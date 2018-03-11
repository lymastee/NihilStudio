//
// Copyright (C) NihilIO
// 
// File: NihilIOCmd.cpp
//
// MEL Command: NihilIO
//
// Author: Maya Plug-in Wizard 2.0
//

// Includes everything needed to register a simple MEL command with Maya.
// 
#include <memory>
#include <maya/MSimple.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MDagPath.h>
#include <maya/MFnMesh.h>
#include <maya/MFloatPointArray.h>
#include <maya/MPointArray.h>
#include <maya/MMatrix.h>
#include <maya/MFnTransform.h>
#include <maya/MQuaternion.h>
#include <maya/MFnNurbsSurface.h>

#include <gslib/math.h>
#include <gslib/string.h>
#include <gslib/file.h>

#undef max
#undef min

// Use helper macro to register a command with Maya.  It creates and
// registers a command that does not support undo or redo.  The 
// created class derives off of MPxCommand.
//
DeclareSimpleCommand( NihilIO, "NihilIO", "2013");

// Import & Export to nihil file format ;-)

static void nihilRetrieveTransformation(MDagPath& dp, gs::matrix& mat)
{
	MFnTransform transform(dp);
	MVector translation = transform.translation(MSpace::kTransform);
	MQuaternion rot;
	transform.getRotation(rot, MSpace::kTransform);
	double scale[3];
	transform.getScale(scale);
	gs::matrix matTranslation, matRotation, matScaling;
	matTranslation.translation(translation.x, translation.y, translation.z);
	matRotation.rotation(gs::quat(rot.x, rot.y, rot.z, rot.w));
	matScaling.scaling(scale[0], scale[1], scale[2]);
	mat.multiply(matScaling, matRotation);
	mat.multiply(matTranslation);
}

static bool nihilExportTransformation(MDagPath& dp, gs::file& f)
{
	// export transform
	f.write("\t.Local {\n");
	gs::matrix mat;
	nihilRetrieveTransformation(dp, mat);
	gs::string strMat;
	strMat.format(_t("\t\t%f %f %f %f\n\t\t%f %f %f %f\n\t\t%f %f %f %f\n\t\t%f %f %f %f\n"),
		mat._11, mat._12, mat._13, mat._14,
		mat._21, mat._22, mat._23, mat._24,
		mat._31, mat._32, mat._33, mat._34,
		mat._41, mat._42, mat._43, mat._44
		);
	f.write(strMat.c_str());
	f.write("\t}\n");
	return true;
}

// Polygon {
//		.Points {
//			x y z(optional, index)
//			x y z
//			...
//			}
//		.Faces {	// only triangles supported.
//			x y z(optional, index)
//			...
//			}
//		}

static bool nihilExportPolygon(MDagPath& dp, gs::file& f)
{
	MFnMesh mfnMesh(dp);
	MIntArray triangles, triangleVertices;
	if (MFAIL(mfnMesh.getTriangles(triangles, triangleVertices)))
		return false;
	unsigned int vertices = triangleVertices.length();
	if (vertices <= 0)
		return false;
	std::unique_ptr<int[]> indexData(new int[vertices]);
	if (MFAIL(triangleVertices.get(indexData.get())))
		return false;
	MFloatPointArray points;
	if (MFAIL(mfnMesh.getPoints(points, MSpace::kObject)))
		return false;
	unsigned int ptCount = points.length();
	std::unique_ptr<gs::vec4[]> pointsData(new gs::vec4[ptCount]);
	if (MFAIL(points.get((float (*)[4])pointsData.get())))
		return false;
	// output
	f.write("Polygon {\n");
	// export points
	f.write("\t.Points {\n");
	for (unsigned int i = 0; i < ptCount; i ++)
	{
		gs::string strpt;
		strpt.format(_t("\t\t%f %f %f(%d)\n"), pointsData[i].x / pointsData[i].w, pointsData[i].y / pointsData[i].w, pointsData[i].z / pointsData[i].w, i);
		f.write(strpt);
	}
	f.write("\t}\n");
	// export faces
	f.write("\t.Faces {\n");
	assert(vertices % 3 == 0);
	for (unsigned int i = 0; i < vertices; i += 3)
	{
		gs::string strFaces;
		strFaces.format(_t("\t\t%d %d %d(%d)\n"), indexData[i], indexData[i + 1], indexData[i + 2], i / 3);
		f.write(strFaces);
	}
	f.write("\t}\n");
	nihilExportTransformation(dp, f);
	f.write("}\n");
	return true;
}

static int nihilConvertArrayIndex(int u, int v, int utotal, int vtotal)
{
	return u * vtotal + v;
}

static void nihilExportCvs1(const gs::vec4& cvs, gs::file& f)
{
	gs::string strpt;
	strpt.format(_t("\t\t%f %f %f\n"), cvs.x / cvs.w, cvs.y / cvs.w, cvs.z / cvs.w);
	f.write(strpt);
}

static bool nihilExportBiCubicBezier(MDagPath& dp, gs::file& f)
{
	MFnNurbsSurface nurbs(dp);
	if (!nurbs.isBezier())
		return false;
	int uspans = nurbs.numSpansInU();
	int vspans = nurbs.numSpansInV();
	int udegree = nurbs.degreeU();
	int vdegree = nurbs.degreeV();
	if (udegree != 3 || vdegree != 3)
	{
		// NihilIO support BiCubicBezier only.
		return false;
	}
	// is bezier
	MPointArray cvs;
	nurbs.getCVs(cvs, MSpace::kObject);
	unsigned int ptCount = cvs.length();
	std::unique_ptr<gs::vec4[]> pointsData(new gs::vec4[ptCount]);	// v * u
	if (MFAIL(cvs.get((float (*)[4])pointsData.get())))
		return false;
	// convert the cvs for convenience
	gs::matrix mat;
	nihilRetrieveTransformation(dp, mat);
	for (unsigned int i = 0; i < ptCount; i ++)
		pointsData[i].multiply(mat);
	// do export
	int upts = uspans + 3;
	int vpts = vspans + 3;
	int usegs = (upts - 1) / 3;
	int vsegs = (vpts - 1) / 3;
	for (int i = 0; i < usegs; i ++)
	{
		for (int j = 0; j < vsegs; j ++)
		{
			f.write("BiCubicBezier {\n");
			f.write("\t.Cvs {\n");
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 0, j * 3, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 1, j * 3, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 2, j * 3, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 3, j * 3, upts, vpts)], f);

			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 0, j * 3 + 1, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 1, j * 3 + 1, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 2, j * 3 + 1, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 3, j * 3 + 1, upts, vpts)], f);

			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 0, j * 3 + 2, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 1, j * 3 + 2, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 2, j * 3 + 2, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 3, j * 3 + 2, upts, vpts)], f);

			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 0, j * 3 + 3, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 1, j * 3 + 3, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 2, j * 3 + 3, upts, vpts)], f);
			nihilExportCvs1(pointsData[nihilConvertArrayIndex(i * 3 + 3, j * 3 + 3, upts, vpts)], f);
			f.write("\t}\n");
			//nihilExportTransformation(dp, f);
			f.write("}\n");
		}
	}
	return true;
}

static void nihilWriteKnots(MDoubleArray& knots, gs::file& f)
{
	unsigned int length = knots.length() + 2;
	std::unique_ptr<double[]> knotsData(new double[length]);
	knots.get(knotsData.get() + 1);
	// ASSERT(length >= 4);
	knotsData[0] = knotsData[1] - (knotsData[2] - knotsData[1]);
	knotsData[length - 1] = knotsData[length - 2] + (knotsData[length - 2] - knotsData[length - 3]);
	const unsigned int lineKnots = 10;
	for (unsigned int i = 0; i < length; i += lineKnots)
	{
		f.write(_t("\t\t"));
		unsigned int knotsLeft = std::min(lineKnots, length - i);
		for (unsigned int j = 0; j < knotsLeft; j ++)
		{
			gs::string str;
			str.format(_t("%f "), knotsData[i + j]);
			f.write(str);
		}
		f.write(_t("\n"));
	}
}

// NURBS {
//		.Cvs {}
//		.Degrees { u, v }
//		.NumCVs { u, v }
//		.UKnots {}
//		.VKnots {}
// }
static bool nihilExportNURBS(MDagPath& dp, gs::file& f)
{
	MFnNurbsSurface nurbs(dp);
	if (nurbs.isBezier())
		return false;
	f.write(_t("NURBS {\n"));
	// write numCVs
	int ucvs = nurbs.numCVsInU();
	int vcvs = nurbs.numCVsInV();
	f.write(_t("\t.NumCVs {\n"));
	gs::string strNumCvs;
	strNumCvs.format(_t("\t\t%d %d\n"), ucvs, vcvs);
	f.write(strNumCvs);
	f.write(_t("\t}\n"));
	// write degrees
	int udegree = nurbs.degreeU();
	int vdegree = nurbs.degreeV();
	f.write(_t("\t.Degrees {\n"));
	gs::string strDegrees;
	strDegrees.format(_t("\t\t%d %d\n"), udegree, vdegree);
	f.write(strDegrees);
	f.write(_t("\t}\n"));
	// write points
	MPointArray cvs;
	nurbs.getCVs(cvs, MSpace::kObject);
	unsigned int ptCount = cvs.length();
	std::unique_ptr<gs::vec4[]> pointsData(new gs::vec4[ptCount]);
	if (MFAIL(cvs.get((float (*)[4])pointsData.get())))
		return false;
	gs::matrix mat;
	nihilRetrieveTransformation(dp, mat);
	for (unsigned int i = 0; i < ptCount; i ++)
		pointsData[i].multiply(mat);
	f.write(_t("\t.Cvs {\n"));
	for (unsigned int i = 0; i < ptCount; i ++)
	{
		gs::string strpt;
		strpt.format(_t("\t\t%f %f %f(%d)\n"), pointsData[i].x / pointsData[i].w, pointsData[i].y / pointsData[i].w, pointsData[i].z / pointsData[i].w, i);
		f.write(strpt);
	}
	f.write(_t("\t}\n"));
	// write knots
	MDoubleArray uKnots, vKnots;
	nurbs.getKnotsInU(uKnots);
	nurbs.getKnotsInV(vKnots);
	f.write(_t("\t.UKnots {\n"));
	nihilWriteKnots(uKnots, f);
	f.write(_t("\t}\n"));
	f.write(_t("\t.VKnots {\n"));
	nihilWriteKnots(vKnots, f);
	f.write(_t("\t}\n"));
	f.write(_t("}"));
	return true;
}

static bool nihilExportSurfaces(MDagPath& dp, gs::file& f)
{
	MFnNurbsSurface nurbs(dp);
	if (nurbs.isBezier())
		return nihilExportBiCubicBezier(dp, f);
	return nihilExportNURBS(dp, f);
}

MStatus NihilIO::doIt( const MArgList& args )
//
//	Description:
//		implements the MEL NihilIO command.
//
//	Arguments:
//		args - the argument list that was passes to the command from MEL
//
//	Return Value:
//		MS::kSuccess - command succeeded
//		MS::kFailure - command failed (returning this value will cause the 
//                     MEL script that is being run to terminate unless the
//                     error is caught using a "catch" statement.
//
{
	MStatus stat = MS::kSuccess;

	// Get selection.
	MSelectionList sel;
	stat = MGlobal::getActiveSelectionList(sel);
	if (MFAIL(stat))
	{
		setResult("NihilIO command failed: selection invalid.\n");
		return MS::kFailure;
	}

	// Create file
	char szDefaultPath[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, szDefaultPath);
	strcat_s(szDefaultPath, MAX_PATH, "\\nihilIOSaves.txt");
	const char* resultFileName = szDefaultPath;
	if (args.length() >= 1)
		resultFileName = args.asString(0).asChar();
	gs::file f;
	f.open_text(resultFileName, false);

	// Output
	appendToResult("NihilIO command executed:\n");

	// For each selected
	for (unsigned int i = 0; i < sel.length(); i ++)
	{
		MDagPath dagPath;
		sel.getDagPath(i, dagPath);
		if (dagPath.hasFn(MFn::kNurbsSurface))
		{
			nihilExportSurfaces(dagPath, f);
		}
		else if (dagPath.hasFn(MFn::kMesh))
		{
			nihilExportPolygon(dagPath, f);
		}
	}

	// Since this class is derived off of MPxCommand, you can use the 
	// inherited methods to return values and set error messages
	//
	gs::string s;
	s.format(_t("successfully write to \"%s\".\n"), resultFileName);
	appendToResult(s.c_str());

	return stat;
}
