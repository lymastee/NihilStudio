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
#include <maya/MMatrix.h>

#include <gslib/math.h>
#include <gslib/string.h>
#include <gslib/file.h>

// Use helper macro to register a command with Maya.  It creates and
// registers a command that does not support undo or redo.  The 
// created class derives off of MPxCommand.
//
DeclareSimpleCommand( NihilIO, "NihilIO", "2013");

// Import & Export to nihil file format ;-)
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

class NihilPolygonIO
{
public:
	static bool exporta(MDagPath& dp, gs::file& f)
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
		// export transform
		//f.write("\t.Local {\n");
		MMatrix mat = mfnMesh.transformationMatrix();
		//f.write("\t}\n");
		f.write("}\n");
		return true;
	}
};


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
		MObject node = dagPath.node();
		if (node.hasFn(MFn::kNurbsSurface))
		{
		}
		else
		{
			NihilPolygonIO::exporta(dagPath, f);
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
