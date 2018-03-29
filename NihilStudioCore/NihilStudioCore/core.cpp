#include <assert.h>
#include <windowsx.h>
#include <algorithm>
#include <gslib/error.h>
#include <pink/utility.h>
#include "core.h"
#include "dx11renderer.h"

#define ASSERT assert
#undef min
#undef max

NihilCore::NihilCore()
{
}

NihilCore::~NihilCore()
{
    destroy();
}

void NihilCore::setup(HWND hwnd)
{
    ASSERT(!m_hwnd && "DONOT setup twice.");
    if (!setupWindow(hwnd) || !setupRenderer())
        destroy();
    m_sceneConfig.setup(m_hwnd);
    navigateScene();
}

void NihilCore::resizeWindow()
{
    m_sceneConfig.updateProjMatrix(m_hwnd);
    if (m_renderer)
    {
        gs::matrix mat;
        m_sceneConfig.calcMatrix(mat);
        m_renderer->setWorldMat(mat);
        m_renderer->notifyResize();
        m_renderer->render();
    }
}

void NihilCore::destroy()
{
    destroyController();
    destroyObjects();
    if (m_hwnd)
    {
        SetWindowLong(m_hwnd, GWL_USERDATA, 0);
        SetWindowLong(m_hwnd, GWL_WNDPROC, (LONG)m_oldWndProc);
        m_hwnd = 0;
        m_oldWndProc = nullptr;
    }
    if (m_renderer)
    {
        delete m_renderer;
        m_renderer = nullptr;
    }
}

void NihilCore::render()
{
    if (m_renderer)
    {
        gs::matrix mat;
        m_sceneConfig.calcMatrix(mat);
        m_renderer->setWorldMat(mat);
        m_renderer->render();
    }
}

void NihilCore::showSolidMode()
{
    if (m_renderer)
        m_renderer->showSolidMode();
}

void NihilCore::showWireframeMode()
{
    if (m_renderer)
        m_renderer->showWireframeMode();
}

void NihilCore::destroyController()
{
    if (m_controller)
    {
        delete m_controller;
        m_controller = nullptr;
    }
}

void NihilCore::navigateScene()
{
    destroyController();
    m_controller = new NihilControl_NavigateScene;
}

void NihilCore::selectObject()
{
    destroyController();
    m_controller = new NihilControl_ObjectLayer(this);
}

void NihilCore::selectPoints()
{
    destroyController();
    m_controller = new NihilControl_PointsLayer(this);
}

void NihilCore::translateModifier()
{
    if (m_controller)
        m_controller->setModifierTag(NihilControl::Mod_Translation);
}

void NihilCore::scaleModifier()
{
    if (m_controller)
        m_controller->setModifierTag(NihilControl::Mod_Scaling);
}

void NihilCore::rotateModifier()
{
    if (m_controller)
        m_controller->setModifierTag(NihilControl::Mod_Rotation);
}

#define NIHIL_BLANKS _t(" \t\v\r\n\f")

static bool badEof(const gs::string& src, int curr)
{
    if (curr < 0)
    {
        ASSERT(!"An error was already existed.");
        return true;
    }
    if (curr >= src.length())
    {
        ASSERT("Unexpected end of file.");
        return true;
    }
    return false;
}

static int skipBlankCharactors(const gs::string& src, int start)
{
    int p = (int)src.find_first_not_of(NIHIL_BLANKS, start);
    if (p == NihilString::npos)
        return src.length();
    return p;
}

static int prereadSectionName(const NihilString& src, NihilString& name, int start)
{
    int p = (int)src.find_first_of(NIHIL_BLANKS "{", start);
    if (p == NihilString::npos)
        return src.length();
    if (p)
        name.assign(src.c_str() + start, p - start);
    return p;
}

static int enterSection(const NihilString& src, int start)
{
    ASSERT(start < src.length());
    if (src.at(start) == _t('{'))
        return ++start;
    int next = skipBlankCharactors(src, start);
    if (badEof(src, next) || src.at(next) != _t('{'))
        return -1;
    return ++next;
}

static int readLineOfSection(const NihilString& src, NihilString& line, int start)
{
    ASSERT(start < src.length());
    int p = (int)src.find_first_of(_t("\r\n}"), start);
    if (badEof(src, p))
        return -1;
    if (src.at(p) == _t('}'))
    {
        ASSERT(!"Unexpected end of section.");
        return -1;
    }
    line.assign(src, start, p - start);
    return (int)src.find_first_not_of(_t("\r\n"), p);
}

bool NihilCore::loadFromTextStream(const NihilString& src)
{
    int start = skipBlankCharactors(src, 0);
    if (badEof(src, start))
        return false;
    NihilString prename;
    int next = prereadSectionName(src, prename, start);
    if (badEof(src, next))
        return false;
    do
    {
        // format start with: Polygon {
        if (prename == _t("Polygon"))
        {
            next = loadPolygonFromTextStream(src, start = next);
            if (next == -1)
                return false;
        }
        else if (prename == _t("BiCubicBezier"))
        {
            next = loadBiCubicBezierPatchFromTextStream(src, start = next);
            if (next == -1)
                return false;
        }
        else if (prename == _t("NURBS"))
        {
            next = loadNurbsFromTextStream(src, start = next);
            if (next == -1)
                return false;
        }
        // todo:
        else
        {
            ASSERT(!"Unknown section name.");
            return false;
        }
        next = skipBlankCharactors(src, start = next);
        if (next >= src.length())
            break;
        next = prereadSectionName(src, prename, start = next);
    } while (next < src.length());
    return true;
}

void NihilCore::destroyObjects()
{
    for (auto* p : m_objectList)
        delete p;
    m_objectList.clear();
}

bool NihilCore::setupWindow(HWND hwnd)
{
    m_hwnd = hwnd;
    LONG userData = GetWindowLong(hwnd, GWL_USERDATA);
    ASSERT(!userData);
    SetWindowLong(hwnd, GWL_USERDATA, (LONG)this);
    WNDPROC wndproc = (WNDPROC)GetWindowLong(hwnd, GWL_WNDPROC);
    m_oldWndProc = wndproc;
    SetWindowLong(hwnd, GWL_WNDPROC, (LONG)wndProc);
    return true;
}

bool NihilCore::setupRenderer()
{
#ifdef _NIHIL_USE_DX11
    ASSERT(!m_renderer);
    m_renderer = new NihilDx11Renderer;
    return m_renderer->setup(m_hwnd);
#else
    // ...
    return false;
#endif
    return false;
}

int NihilCore::loadPolygonFromTextStream(const NihilString& src, int start)
{
    NihilPolygon* polygon = new NihilPolygon(m_renderer);
    ASSERT(polygon);
    m_objectList.push_back(polygon);
    return polygon->loadPolygonFromTextStream(src, start);
}

int NihilCore::loadBiCubicBezierPatchFromTextStream(const NihilString& src, int start)
{
    NihilBiCubicBezierPatch* biCubicBezier = new NihilBiCubicBezierPatch(m_renderer);
    ASSERT(biCubicBezier);
    m_objectList.push_back(biCubicBezier);
    return biCubicBezier->loadBiCubicBezierPatchFromTextStream(src, start);
}

int NihilCore::loadNurbsFromTextStream(const NihilString& src, int start)
{
    NihilBiCubicNURBSurface* biCubicNurbs = new NihilBiCubicNURBSurface(m_renderer);
    ASSERT(biCubicNurbs);
    m_objectList.push_back(biCubicNurbs);
    return biCubicNurbs->loadBiCubicNURBSFromTextStream(src, start);
}

LRESULT NihilCore::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    NihilCore* core = (NihilCore*)GetWindowLong(hwnd, GWL_USERDATA);
    if (!core)
        return DefWindowProc(hwnd, msg, wParam, lParam);
    ASSERT(core);
    WNDPROC oldWndProc = core->getOldWndProc();
    if (!oldWndProc)
        oldWndProc = DefWindowProc;

    NihilControl* ctl = core->getController();
    if (ctl)
    {
        if (ctl->onMsg(core, msg, wParam, lParam))
            return S_OK;
    }

    switch (msg)
    {
    case WM_QUIT:
        core->destroy();
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            break;
        }
    }
    return oldWndProc(hwnd, msg, wParam, lParam);
}

void NihilSceneConfig::setup(HWND hwnd)
{
    m_model.identity();
    updateProjMatrix(hwnd);
    updateViewMatrix();
}

void NihilSceneConfig::updateProjMatrix(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    // const float fovy = 1.570796327f;
    const float fovy = 1.f;
    m_proj.perspectivefovlh(fovy, (float)width / height, 0.01f, 100.f);
}

void NihilSceneConfig::updateViewMatrix()
{
    using namespace gs;
    vec3 eye, at, up;
    at = vec3(0.f, 0.f, 0.f);
    vec4 rotaxis;
    vec3(1.f, 0.f, 0.f).transform(rotaxis, matrix().rotation_y(m_rot1));
    vec3 v;
    v.cross((vec3&)rotaxis, vec3(0.f, 1.f, 0.f));
    vec4 d;
    v.transform(d, matrix().rotation((vec3&)rotaxis, m_rot2));
    vec3 t(d.x, d.y, d.z);
    t.normalize();
    t.scale(m_cdis);
    eye = t;
    v.transform(d, matrix().rotation((vec3&)rotaxis, m_rot2 + PI / 2.f));
    up = vec3(d.x, d.y, d.z);
    up.normalize();
    m_viewLookat.lookatlh(eye, at, up);
}

void NihilSceneConfig::calcMatrix(gs::matrix& mat)
{
    gs::matrix matOffset;
    matOffset.translation(m_viewOffset.x, m_viewOffset.y, m_viewOffset.z);
    mat.multiply(m_model, matOffset);
    mat.multiply(m_viewLookat);
    mat.multiply(m_proj);
}

static void nihilKeepRotInDomain(float& r)
{
    if (r >= PI * 2.f)
        r -= PI * 2.f;
    if (r < 0.f)
        r += PI * 2.f;
}

void NihilSceneConfig::updateRotation(const gs::vec2& lastpt, const gs::vec2& pt)
{
    gs::vec2 d;
    d.sub(pt, lastpt);
    float rot1 = m_rot1;
    float rot2 = m_rot2;
    rot1 += d.x * 0.005f;
    rot2 += d.y * 0.005f;
    nihilKeepRotInDomain(rot1);
    nihilKeepRotInDomain(rot2);
    m_rot1 = rot1;
    m_rot2 = rot2;
    updateViewMatrix();
}

void NihilSceneConfig::updateZooming(float d)
{
    m_cdis += (d * 0.8f);
    updateViewMatrix();
}

void NihilSceneConfig::updateTranslation(const gs::vec2& lastpt, const gs::vec2& pt)
{
    auto t = getTranslationInCurrentView(lastpt, pt);
    m_viewOffset.add(m_viewOffset, t);
    updateViewMatrix();
}

gs::vec3 NihilSceneConfig::getTranslationInCurrentView(const gs::vec2& lastpt, const gs::vec2& pt) const
{
    gs::matrix mat;
    float det;
    mat.inverse(&det, m_viewLookat);
    gs::vec2 d;
    d.sub(pt, lastpt);
    d.scale(0.05f);
    gs::vec4 t, t0;
    t.multiply(gs::vec4(d.x, -d.y, 0.f, 1.f), mat);
    t0.multiply(gs::vec4(0.f, 0.f, 0.f, 1.f), mat);
    t.scale(1.f / t.w);
    t0.scale(1.f / t.w);
    t.sub(t, t0);
    return (const gs::vec3&)t;
}

void NihilObject::appendTranslation(const gs::vec3& ofs)
{
    gs::matrix mat;
    mat.translation(ofs.x, ofs.y, ofs.z);
    m_localMat.multiply(mat);
    updateLocalMat();
}

void NihilObject::updateLocalMat()
{
    if (m_geometry)
        m_geometry->setLocalMat(m_localMat);
}

int NihilObject::loadLocalSectionFromTextStream(const NihilString& src, int start)
{
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, next))
        return -1;
    std::vector<float> readFloats;
    while (src.at(next) != _t('}'))
    {
        NihilString line;
        next = readLineOfSection(src, line, start = next);
        if (badEof(src, next))
            return -1;
        float a, b, c, d;
        int ct = gs::strtool::sscanf(line.c_str(), _t("%f %f %f %f"), &a, &b, &c, &d);
        if (ct != 4)
        {
            ASSERT(!"Bad format about line in section...");
            return -1;
        }
        readFloats.push_back(a);
        readFloats.push_back(b);
        readFloats.push_back(c);
        readFloats.push_back(d);
        // step on
        next = skipBlankCharactors(src, start = next);
        if (badEof(src, next))
            return -1;
    }
    if (readFloats.size() != 16)
        return -1;
    for (int i = 0, k = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++, k++)
            m_localMat.m[i][j] = readFloats.at(k);
    }
    ASSERT(src.at(next) == _t('}'));
    return ++next;
}

NihilPolygon::NihilPolygon(NihilRenderer* renderer)
{
    ASSERT(renderer);
    m_renderer = renderer;
    m_geometry = renderer->addGeometry();
    ASSERT(m_geometry);
}

NihilPolygon::~NihilPolygon()
{
    ASSERT(m_renderer && m_geometry);
    m_renderer->removeGeometry(m_geometry);
    m_renderer = nullptr;
    m_geometry = nullptr;
}

int NihilPolygon::loadPolygonFromTextStream(const NihilString& src, int start)
{
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, next))
        return -1;
    UINT fulfilled = 0;
    enum
    {
        PointSectionFulfilled = 0x01,
        FaceSectionFulfilled = 0x02,
        LocalSectionFulfilled = 0x04,
        NecessarySectionsFulfilled = PointSectionFulfilled | FaceSectionFulfilled,
    };
    while (src.at(next) != _t('}'))
    {
        NihilString prename;
        next = prereadSectionName(src, prename, start = next);
        if (badEof(src, next))
            return -1;
        // format start with: .Points {
        if (prename == _t(".Points"))
        {
            next = loadPointSectionFromTextStream(src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= PointSectionFulfilled;
        }
        // format start with: .Faces {
        else if (prename == _t(".Faces"))
        {
            next = loadFaceSectionFromTextStream(src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= FaceSectionFulfilled;
        }
        // format start with: .Local {
        else if (prename == _t(".Local"))
        {
            next = loadLocalSectionFromTextStream(src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= LocalSectionFulfilled;
        }
        else
        {
            ASSERT(!"Unexpected section.");
            return -1;
        }
        next = skipBlankCharactors(src, start = next);
        if (badEof(src, next))
            return -1;
    }
    if ((fulfilled & NecessarySectionsFulfilled) != NecessarySectionsFulfilled)
        return -1;
    calculateNormals();
    if (!(fulfilled & LocalSectionFulfilled))
    {
        // setup a default matrix
        m_localMat.identity();
    }
    // so far so good, finally setup the buffers to geometry
    if (!setupGeometryBuffers())
        return -1;
    ASSERT(src.at(next) == _t('}'));
    return ++ next;
}

void NihilPolygon::calculateNormals()
{
    // 1.set all the normals to 0
    for (NihilVertex& v : m_pointList)
        v.normal = gs::vec3(0.f, 0.f, 0.f);
    // 2.calculate normals for each face, normalize it, add it up to the normals of each point
    ASSERT(m_indexList.size() % 3 == 0);
    for (int m = 0; m < (int)m_indexList.size(); m += 3)
    {
        int i = m_indexList.at(m);
        int j = m_indexList.at(m + 1);
        int k = m_indexList.at(m + 2);
        NihilVertex& v1 = m_pointList.at(i);
        NihilVertex& v2 = m_pointList.at(j);
        NihilVertex& v3 = m_pointList.at(k);
        gs::vec3 normal;
        normal.cross(gs::vec3().sub(v2.pos, v1.pos), gs::vec3().sub(v3.pos, v2.pos)).normalize();
        v1.normal += normal;
        v2.normal += normal;
        v3.normal += normal;
    }
    // 3.normalize each of the normals
    for (NihilVertex& v : m_pointList)
        v.normal.normalize();
}

bool NihilPolygon::setupGeometryBuffers()
{
    ASSERT(m_geometry);
    // create vertex buffer
    if (!m_geometry->createVertexStream(&m_pointList.front(), (int)m_pointList.size()))
    {
        ASSERT(!"Create vertex buffer failed.");
        return false;
    }
    // create index buffer
    if (!m_geometry->createIndexStream(&m_indexList.front(), (int)m_indexList.size()))
    {
        ASSERT(!"Create index buffer failed.");
        return false;
    }
    // set local transformations
    m_geometry->setLocalMat(m_localMat);
    return true;
}

void NihilPolygon::updateBuffers()
{
    if (m_geometry)
    {
        calculateNormals();
        m_geometry->updateVertexStream(&m_pointList.front(), (int)m_pointList.size());
    }
}

int NihilPolygon::loadPointSectionFromTextStream(const NihilString& src, int start)
{
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, next))
        return -1;
    while (src.at(next) != _t('}'))
    {
        // the format was: float float float[optional](index)\r\n
        NihilString line;
        next = readLineOfSection(src, line, start = next);
        if (badEof(src, next))
            return -1;
        float x, y, z;
        int c = gs::strtool::sscanf(line.c_str(), _t("%f %f %f"), &x, &y, &z);
        if (c != 3)
        {
            ASSERT(!"Bad format about line in section...");
            return -1;
        }
        NihilVertex v;
        v.pos = gs::vec3(x, y, z);
        v.normal = gs::vec3(0.f, 0.f, 0.f);
        m_pointList.push_back(v);
        // step on
        next = skipBlankCharactors(src, start = next);
        if (badEof(src, next))
            return -1;
    }
    ASSERT(src.at(next) == _t('}'));
    return ++ next;
}

int NihilPolygon::loadFaceSectionFromTextStream(const NihilString& src, int start)
{
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, next))
        return -1;
    while (src.at(next) != _t('}'))
    {
        // the format was: index1 index2 index3[optional](index)\r\n
        NihilString line;
        next = readLineOfSection(src, line, start = next);
        if (badEof(src, next))
            return -1;
        int i, j, k;
        int c = gs::strtool::sscanf(line.c_str(), _t("%d %d %d"), &i, &j, &k);
        if (c != 3)
        {
            ASSERT(!"Bad format about line in section...");
            return -1;
        }
        // write index
        m_indexList.push_back(i);
        m_indexList.push_back(j);
        m_indexList.push_back(k);
        // step on
        next = skipBlankCharactors(src, start = next);
        if (badEof(src, next))
            return -1;
    }
    ASSERT(src.at(next) == _t('}'));
    return ++next;
}

NihilBiCubicBezierPatch::NihilBiCubicBezierPatch(NihilRenderer* renderer)
{
    ASSERT(renderer);
    m_renderer = renderer;
}

NihilBiCubicBezierPatch::~NihilBiCubicBezierPatch()
{
    ASSERT(m_renderer && m_gridMesh);
    m_renderer = nullptr;
    if (m_gridMesh)
    {
        delete m_gridMesh;
        m_gridMesh = nullptr;
    }
}

int NihilBiCubicBezierPatch::loadBiCubicBezierPatchFromTextStream(const NihilString& src, int start)
{
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, start = next))
        return -1;
    UINT fulfilled = 0;
    enum
    {
        CvsSectionFulfilled = 0x01,
        LocalSectionFulFilled = 0x02,
        NecessarySectionsFulfilled = CvsSectionFulfilled,
    };
    while (src.at(next) != _t('}'))
    {
        NihilString prename;
        next = prereadSectionName(src, prename, start = next);
        if (badEof(src, next))
            return -1;
        // format start with: .Cvs {
        if (prename == _t(".Cvs"))
        {
            next = loadCvsSectionFromTextStream(src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= CvsSectionFulfilled;
        }
        else if (prename == _t(".Local"))
        {
            next = loadLocalSectionFromTextStream(src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= LocalSectionFulFilled;
        }
        else
        {
            ASSERT(!"Unexpected section.");
            return -1;
        }
        next = skipBlankCharactors(src, start = next);
        if (badEof(src, next))
            return -1;
    }
    if ((fulfilled & NecessarySectionsFulfilled) != NecessarySectionsFulfilled)
        return -1;
    if (!(fulfilled & LocalSectionFulFilled))
    {
        // setup a default matrix
        m_localMat.identity();
    }
    loadFinished();
    return ++ next;
}

void NihilBiCubicBezierPatch::updateBuffers()
{
    updateGridMesh();
}

void NihilBiCubicBezierPatch::setSelected(bool b)
{
    if (m_gridMesh)
        m_gridMesh->setSelected(b);
}

bool NihilBiCubicBezierPatch::isSelected() const
{
    return m_gridMesh ? m_gridMesh->isSelected() : false;
}

void NihilBiCubicBezierPatch::appendTranslation(const gs::vec3& ofs)
{
    __super::appendTranslation(ofs);
    if (m_gridMesh)
        m_gridMesh->appendTranslation(ofs);
}

int NihilBiCubicBezierPatch::loadCvsSectionFromTextStream(const NihilString& src, int start)
{
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, next))
        return -1;
    int cvsCount = 0;
    while (src.at(next) != _t('}'))
    {
        NihilString line;
        next = readLineOfSection(src, line, start = next);
        if (badEof(src, next))
            return -1;
        float i, j, k;
        int c = gs::strtool::sscanf(line.c_str(), _t("%f %f %f"), &i, &j, &k);
        if (c != 3)
        {
            ASSERT(!"Bad format about line in section...");
            return -1;
        }
        m_cvs[cvsCount++] = gs::vec3(i, j, k);
        // step on
        next = skipBlankCharactors(src, start = next);
        if (badEof(src, next))
            return -1;
    }
    if (cvsCount != 16)
    {
        ASSERT(!"It takes 16 cvs to make a bi-cubic bezier patch.");
        return -1;
    }
    ASSERT(src.at(next) == _t('}'));
    return ++ next;
}

using namespace gs;

static float getCubicControlLength(const vec3& a, const vec3& b, const vec3& c, const vec3& d)
{
    vec3 d1, d2, d3;
    d1.sub(b, a);
    d2.sub(c, b);
    d3.sub(d, c);
    return d1.length() + d2.length() + d3.length();
}

static int nihilGetBiCubicBezierInterpolationStep(const gs::vec3& a, const gs::vec3& b, const gs::vec3& c, const gs::vec3& d)
{
    float fstep = getCubicControlLength(a, b, c, d) * 10.f;
    int step = (int)(fstep + 0.5f);     // round
    return step < 2 ? 2 : step;
}

static void nihilPrepareBiCubicSampleMatrix(matrix& matX, matrix& matY, matrix& matZ, const vec3 cvs[16])
{
    matrix mb(
        1.f, 0.f, 0.f, 0.f,
        -3.f, 3.f, 0.f, 0.f,
        3.f, -6.f, 3.f, 0.f,
        -1.f, 3.f, -3.f, 1.f
        );
    matrix mbt;
    mbt.transpose(mb);
    matX.multiply(mb, matrix(
        cvs[0].x, cvs[1].x, cvs[2].x, cvs[3].x,
        cvs[4].x, cvs[5].x, cvs[6].x, cvs[7].x,
        cvs[8].x, cvs[9].x, cvs[10].x, cvs[11].x,
        cvs[12].x, cvs[13].x, cvs[14].x, cvs[15].x
        ));
    matX.multiply(mbt);
    matY.multiply(mb, matrix(
        cvs[0].y, cvs[1].y, cvs[2].y, cvs[3].y,
        cvs[4].y, cvs[5].y, cvs[6].y, cvs[7].y,
        cvs[8].y, cvs[9].y, cvs[10].y, cvs[11].y,
        cvs[12].y, cvs[13].y, cvs[14].y, cvs[15].y
        ));
    matY.multiply(mbt);
    matZ.multiply(mb, matrix(
        cvs[0].z, cvs[1].z, cvs[2].z, cvs[3].z,
        cvs[4].z, cvs[5].z, cvs[6].z, cvs[7].z,
        cvs[8].z, cvs[9].z, cvs[10].z, cvs[11].z,
        cvs[12].z, cvs[13].z, cvs[14].z, cvs[15].z
        ));
    matZ.multiply(mbt);
}

static void nihilSampleBiCubicPatch(vec3 vertices[], int ustep, int vstep, const matrix& matX, const matrix& matY, const matrix& matZ)
{
    ASSERT(vertices);
    ASSERT(ustep >= 2 && vstep >= 2);
    float uchord = 1.f / (ustep - 1);
    float vchord = 1.f / (vstep - 1);
    float u, v;
    int i, j, k;
    for (i = k = 0, u = 0.f; i < ustep; i ++, u += uchord)
    {
        vec4 m(1.f, u, u * u, u * u * u);
        for (j = 0, v = 0.f; j < vstep; j ++, v += vchord)
        {
            vec4 n(1.f, v, v * v, v * v * v);
            float x = vec4().multiply(m, matX).dot(n);
            float y = vec4().multiply(m, matY).dot(n);
            float z = vec4().multiply(m, matZ).dot(n);
            vertices[k ++] = vec3(x, y, z);
#ifdef _DEBUG
            trace(_t("curve -d 3 -p %f %f %f;\n"), x, y, z);
#endif
        }
    }
}

void NihilBiCubicBezierPatch::loadFinished()
{
    ASSERT(!m_gridMesh);
    m_gridMesh = new NihilPolygon(m_renderer);
    ASSERT(m_gridMesh);
    m_gridMesh->setLocalMat(m_localMat);
    m_ustep = std::max(
        nihilGetBiCubicBezierInterpolationStep(m_cvs[0], m_cvs[1], m_cvs[2], m_cvs[3]),
        nihilGetBiCubicBezierInterpolationStep(m_cvs[12], m_cvs[13], m_cvs[14], m_cvs[15])
        );
    m_vstep = std::max(
        nihilGetBiCubicBezierInterpolationStep(m_cvs[0], m_cvs[4], m_cvs[8], m_cvs[12]),
        nihilGetBiCubicBezierInterpolationStep(m_cvs[3], m_cvs[7], m_cvs[11], m_cvs[15])
        );
    m_ustep = m_vstep = 8;
    updateGridMeshPoints();
    createGridMeshIndices();
    m_gridMesh->calculateNormals();
    m_gridMesh->setupGeometryBuffers();
}

void NihilBiCubicBezierPatch::updateGridMeshPoints()
{
    matrix matX, matY, matZ;
    nihilPrepareBiCubicSampleMatrix(matX, matY, matZ, m_cvs);
    int size = m_ustep * m_vstep;
    std::unique_ptr<vec3[]> vertices(new vec3[size]);
    nihilSampleBiCubicPatch(vertices.get(), m_ustep, m_vstep, matX, matY, matZ);
    ASSERT(m_gridMesh);
    NihilPointList& ptList = m_gridMesh->getPointList();
    ptList.resize(size);
    for (int i = 0; i < size; i ++)
        ptList.at(i).pos = vertices[i];
}

void NihilBiCubicBezierPatch::updateGridMesh()
{
    updateGridMeshPoints();
    ASSERT(m_gridMesh);
    m_gridMesh->updateBuffers();
}

void NihilBiCubicBezierPatch::createGridMeshIndices()
{
    int size = (m_ustep - 1) * (m_vstep - 1);
    NihilIndexList& indexList = m_gridMesh->getIndexList();
    indexList.resize(size * 6);
    int* indices = &indexList.front();
    int i, j, k;
    for (i = k = 0; i < m_vstep - 1; i ++)
    {
        for (j = 0; j < m_ustep - 1; j ++)
        {
            int a = j * m_vstep + i, b = a + 1;
            int c = (j + 1) * m_vstep + i, d = c + 1;
            indices[k ++] = a;
            indices[k ++] = d;
            indices[k ++] = c;
            indices[k ++] = a;
            indices[k ++] = b;
            indices[k ++] = d;
        }
    }
}

static int nihilGet2IntsFromTextStream(int& i1, int& i2, const NihilString& src, int start)
{
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, next))
        return -1;
    NihilString line;
    next = readLineOfSection(src, line, start = next);
    if (badEof(src, next))
        return -1;
    int c = gs::strtool::sscanf(line.c_str(), _t("%d %d"), &i1, &i2);
    if (c != 2)
    {
        ASSERT(!"Bad format about line in section...");
        return -1;
    }
    // step on
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, next))
        return -1;
    ASSERT(src.at(next) == _t('}'));
    return ++next;
}

static int nihilLoadFloatVectorFromTextStream(std::vector<float>& floats, const NihilString& src, int start)
{
    ASSERT(floats.empty());
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, next))
        return -1;
    while (src.at(next) != _t('}'))
    {
        int p = (int)src.find_first_of(NIHIL_BLANKS, next);
        if (p == NihilString::npos)
            return -1;
        float f;
        int c = gs::strtool::sscanf(&src.at(next), _t("%f"), &f);
        if (c != 1)
            return -1;
        floats.push_back(f);
        next = p;
        next = skipBlankCharactors(src, start = next);
        if (badEof(src, next))
            return -1;
    }
    ASSERT(src.at(next) == _t('}'));
    return ++ next;
}

NihilBiCubicNURBSurface::NihilBiCubicNURBSurface(NihilRenderer* renderer)
{
    ASSERT(renderer);
    m_renderer = renderer;
}

NihilBiCubicNURBSurface::~NihilBiCubicNURBSurface()
{
    ASSERT(m_renderer && m_gridMesh);
    m_renderer = nullptr;
    if (m_gridMesh)
    {
        delete m_gridMesh;
        m_gridMesh = nullptr;
    }
}

void NihilBiCubicNURBSurface::updateBuffers()
{
    updateGridMesh();
}

void NihilBiCubicNURBSurface::setSelected(bool b)
{
    if (m_gridMesh)
        m_gridMesh->setSelected(b);
}

bool NihilBiCubicNURBSurface::isSelected() const
{
    return m_gridMesh ? m_gridMesh->isSelected() : false;
}

void NihilBiCubicNURBSurface::appendTranslation(const gs::vec3& ofs)
{
    __super::appendTranslation(ofs);
    if (m_gridMesh)
        m_gridMesh->appendTranslation(ofs);
}

int NihilBiCubicNURBSurface::loadBiCubicNURBSFromTextStream(const NihilString& src, int start)
{
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, start = next))
        return -1;
    UINT fulfilled = 0;
    enum
    {
        CvsSectionFulfilled = 0x01,
        SpanSectionFulfilled = 0x02,
        DegreeSectionFulfilled = 0x04,
        UKnotSectionFulfilled = 0x08,
        VKnotSectionFulfilled = 0x10,
        LocalSectionFulfilled = 0x20,
        NecessarySectionsFulfilled = CvsSectionFulfilled | SpanSectionFulfilled | DegreeSectionFulfilled | UKnotSectionFulfilled | VKnotSectionFulfilled,
    };
    int ucvs, vcvs, udegree, vdegree;
    while (src.at(next) != _t('}'))
    {
        NihilString prename;
        next = prereadSectionName(src, prename, start = next);
        if (badEof(src, next))
            return -1;
        if (prename == _t(".Cvs"))
        {
            next = loadCvsSectionFromTextStream(src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= CvsSectionFulfilled;
        }
        else if (prename == _t(".NumCVs"))
        {
            next = nihilGet2IntsFromTextStream(ucvs, vcvs, src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= SpanSectionFulfilled;
        }
        else if (prename == _t(".Degrees"))
        {
            next = nihilGet2IntsFromTextStream(udegree, vdegree, src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= DegreeSectionFulfilled;
            if (udegree != 3 || vdegree != 3)
            {
                ASSERT(!"Only cubic NURBS was supported.");
                return -1;
            }
        }
        else if (prename == _t(".UKnots"))
        {
            next = nihilLoadFloatVectorFromTextStream(m_uknots, src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= UKnotSectionFulfilled;
        }
        else if (prename == _t(".VKnots"))
        {
            next = nihilLoadFloatVectorFromTextStream(m_vknots, src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= VKnotSectionFulfilled;
        }
        else if (prename == _t(".Local"))
        {
            next = loadLocalSectionFromTextStream(src, start = next);
            if (badEof(src, next))
                return -1;
            fulfilled |= LocalSectionFulfilled;
        }
        else
        {
            ASSERT(!"Unexpected section.");
            return -1;
        }
        next = skipBlankCharactors(src, start = next);
        if (badEof(src, next))
            return -1;
    }
    if ((fulfilled & NecessarySectionsFulfilled) != NecessarySectionsFulfilled)
        return -1;
    if ((ucvs + udegree + 1 != (int)m_uknots.size()) || (vcvs + vdegree + 1 != (int)m_vknots.size()))
    {
        ASSERT(!"Bad format of NURBS.");
        return -1;
    }
    int uspans = ucvs - udegree;
    int vspans = vcvs - vdegree;
    const int steps = 6;
    m_ustep = uspans * steps;
    m_vstep = vspans * steps;
    if (!(fulfilled & LocalSectionFulfilled))
    {
        // setup a default matrix
        m_localMat.identity();
    }
    loadFinished();
    return ++ next;
}

int NihilBiCubicNURBSurface::loadCvsSectionFromTextStream(const NihilString& src, int start)
{
    int next = enterSection(src, start);
    if (badEof(src, next))
        return -1;
    next = skipBlankCharactors(src, start = next);
    if (badEof(src, next))
        return -1;
    ASSERT(m_cvs.empty());
    while (src.at(next) != _t('}'))
    {
        NihilString line;
        next = readLineOfSection(src, line, start = next);
        if (badEof(src, next))
            return -1;
        float i, j, k;
        int c = gs::strtool::sscanf(line.c_str(), _t("%f %f %f"), &i, &j, &k);
        if (c != 3)
        {
            ASSERT(!"Bad format about line in section...");
            return -1;
        }
        m_cvs.push_back(gs::vec3(i, j, k));
        // step on
        next = skipBlankCharactors(src, start = next);
        if (badEof(src, next))
            return -1;
    }
    ASSERT(src.at(next) == _t('}'));
    return ++ next;
}

void NihilBiCubicNURBSurface::loadFinished()
{
    ASSERT(!m_gridMesh);
    m_gridMesh = new NihilPolygon(m_renderer);
    ASSERT(m_gridMesh);
    m_gridMesh->setLocalMat(m_localMat);
    updateGridMeshPoints();
    createGridMeshIndices();
    m_gridMesh->calculateNormals();
    m_gridMesh->setupGeometryBuffers();
}

static int nihilFindNurbsSpan(int numCvs, int degree, float t, const std::vector<float>& knots)
{
    if (t == knots.at(numCvs))
        return numCvs - 1;
    int low = degree;
    int high = numCvs;
    int mid = (low + high) / 2;
    while (t < knots.at(mid) || t >= knots.at(mid + 1))
    {
        if (t < knots.at(mid))
            high = mid;
        else
            low = mid;
        mid = (low + high) / 2;
    }
    return mid;
}

static void nihilNurbsBasisFunc(int i, float u, int degree, const std::vector<float>& knots, float N[])
{
    ASSERT(N);
    N[0] = 1.f;
    ASSERT(degree == 3);
    float left[4], right[4];
    for (int j = 1; j <= degree; j ++)
    {
        left[j] = u - knots.at(i + 1 - j);
        right[j] = knots.at(i + j) - u;
        float saved = 0.f;
        for (int r = 0; r < j; r ++)
        {
            float temp = N[r] / (right[r + 1] + left[j - r]);
            N[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        N[j] = saved;
    }
}

void NihilBiCubicNURBSurface::calcPoint(gs::vec3& p, float u, float v)
{
    int udegree = getUDegrees();
    int vdegree = getVDegrees();
    int uspan = nihilFindNurbsSpan(getUCvs(), udegree, u, m_uknots);
    int vspan = nihilFindNurbsSpan(getVCvs(), vdegree, v, m_vknots);
    ASSERT(udegree == 3 && vdegree == 3);
    float Nu[4], Nv[4];
    nihilNurbsBasisFunc(uspan, u, udegree, m_uknots, Nu);
    nihilNurbsBasisFunc(vspan, v, vdegree, m_vknots, Nv);
    gs::vec3 temp[4];
    for (int l = 0; l <= vdegree; l ++)
    {
        temp[l] = gs::vec3(0.f, 0.f, 0.f);
        for (int k = 0; k <= udegree; k ++)
        {
            int uni = (uspan - udegree + k) * getVCvs() + (vspan - vdegree + l);
            temp[l] += Nu[k] * m_cvs.at(uni);
        }
    }
    gs::vec3 sw(0.f, 0.f, 0.f);
    for (int l = 0; l <= vdegree; l ++)
        sw += gs::vec3().scale(temp[l], Nv[l]);
    p = sw;
}

void NihilBiCubicNURBSurface::updateGridMeshPoints()
{
    int size = (m_ustep + 1) * (m_vstep + 1);
    ASSERT(m_gridMesh);
    NihilPointList& ptList = m_gridMesh->getPointList();
    ptList.resize(size);
    int k = 0;
    for (int i = 0; i <= m_vstep; i++)
    {
        float v = ((float)i / m_vstep) * (m_vknots.at(getVCvs()) - m_vknots.at(getVDegrees())) + m_vknots.at(getVDegrees());
        for (int j = 0; j <= m_ustep; j++)
        {
            gs::vec3 p;
            float u = ((float)j / m_ustep) * (m_uknots.at(getUCvs()) - m_uknots.at(getUDegrees())) + m_uknots.at(getUDegrees());
            calcPoint(p, u, v);
#ifdef _DEBUG
            gs::trace(_t("curve -d 3 -p %f %f %f;\n"), p.x, p.y, p.z);
#endif
            ptList.at(k++).pos = p;
        }
    }
}

void NihilBiCubicNURBSurface::updateGridMesh()
{
    updateGridMeshPoints();
    ASSERT(m_gridMesh);
    m_gridMesh->updateBuffers();
}

void NihilBiCubicNURBSurface::createGridMeshIndices()
{
    int size = m_ustep * m_vstep;
    NihilIndexList& indexList = m_gridMesh->getIndexList();
    indexList.resize(size * 6);
    int* indices = &indexList.front();
    int i, j, k;
    for (i = k = 0; i < m_vstep; i ++)
    {
        for (j = 0; j < m_ustep; j ++)
        {
            int a = i * (m_ustep + 1) + j, b = a + 1;
            int c = (i + 1) * (m_ustep + 1) + j, d = c + 1;
            indices[k ++] = a;
            indices[k ++] = d;
            indices[k ++] = c;
            indices[k ++] = a;
            indices[k ++] = b;
            indices[k ++] = d;
        }
    }
}

NihilUIRectangle::NihilUIRectangle(NihilRenderer* renderer)
{
    ASSERT(renderer);
    m_renderer = renderer;
    m_rcObject = renderer->addUIObject();
    ASSERT(m_rcObject);
    m_rcObject->setTopology(NihilUIObject::Topo_LineList);
}

NihilUIRectangle::~NihilUIRectangle()
{
    ASSERT(m_renderer && m_rcObject);
    m_renderer->removeUIObject(m_rcObject);
    m_renderer = nullptr;
    m_rcObject = nullptr;
}

void NihilUIRectangle::setTopLeft(float left, float top)
{
    m_rc.set_rect(left, top, 0.f, 0.f);
}

void NihilUIRectangle::setBottomRight(float right, float bottom)
{
    m_rc.right = right;
    m_rc.bottom = bottom;
}

void NihilUIRectangle::setupBuffers()
{
    NihilUIVertex vertices[4];
    calcVertexBuffer(vertices);
    m_rcObject->createVertexStream(vertices, 4);
    static int indices[] = { 0, 1, 1, 2, 2, 3, 3, 0 };
    m_rcObject->createIndexStream(indices, _countof(indices));
}

void NihilUIRectangle::updateBuffer()
{
    NihilUIVertex vertices[4];
    calcVertexBuffer(vertices);
    m_rcObject->updateVertexStream(vertices, 4);
}

void NihilUIRectangle::calcVertexBuffer(NihilUIVertex vertices[4])
{
    ASSERT(vertices);
    static const gs::vec4 cr(0.5f, 0.6f, 0.5f, 1.f);
    // topLeft -> topRight -> bottomRight -> bottomLeft
    vertices[0].pos = gs::vec3(m_rc.left, m_rc.top, 0.f);
    vertices[1].pos = gs::vec3(m_rc.right, m_rc.top, 0.f);
    vertices[2].pos = gs::vec3(m_rc.right, m_rc.bottom, 0.f);
    vertices[3].pos = gs::vec3(m_rc.left, m_rc.bottom, 0.f);
    vertices[0].color = cr;
    vertices[1].color = cr;
    vertices[2].color = cr;
    vertices[3].color = cr;
}

NihilUIPoints::NihilUIPoints(NihilRenderer* renderer)
{
    ASSERT(renderer);
    m_renderer = renderer;
}

NihilUIPoints::~NihilUIPoints()
{
    ASSERT(m_renderer && m_rcObject);
    if (m_rcObject)
        m_renderer->removeUIObject(m_rcObject);
    m_renderer = nullptr;
    m_rcObject = nullptr;
}

static void nihilPrepareUIPointVertexBuffer(NihilUIVertices& vertices, NihilPointInfoList& infos)
{
    const float radius = 1.f;
    const gs::vec4 unselectedColor(1.f, 0.9f, 0.f, 1.f), selectedColor(0.f, 1.f, 0.f, 1.f);
    for (auto& info : infos)
    {
        NihilUIVertex v1, v2, v3, v4;
        v1.color = v2.color = v3.color = v4.color = info.isSelected ? selectedColor : unselectedColor;
        v1.pos = gs::vec3(info.point.x - radius, info.point.y - radius, info.point.z);
        v2.pos = gs::vec3(info.point.x + radius, info.point.y - radius, info.point.z);
        v3.pos = gs::vec3(info.point.x + radius, info.point.y + radius, info.point.z);
        v4.pos = gs::vec3(info.point.x - radius, info.point.y + radius, info.point.z);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);
        vertices.push_back(v4);
    }
}

void NihilUIPoints::updateBuffer(NihilPointInfoList& infos)
{
    if (!m_rcObject)
    {
        ASSERT(m_renderer);
        m_rcObject = m_renderer->addUIObject();
        ASSERT(m_rcObject);
        m_rcObject->setTopology(NihilUIObject::Topo_TriangleList);
        return initBuffers(infos);
    }
    NihilUIVertices vertices;
    nihilPrepareUIPointVertexBuffer(vertices, infos);
    m_rcObject->updateVertexStream(&vertices.front(), (int)vertices.size());
}

void NihilUIPoints::initBuffers(NihilPointInfoList& infos)
{
    // create index buffer
    int size = infos.size() * 6;
    int* indices = new int[size];
    for (int i = 0; i < size; i += 6)
    {
        int j = i / 6 * 4;
        indices[i] = j;
        indices[i + 1] = j + 1;
        indices[i + 2] = j + 2;
        indices[i + 3] = j;
        indices[i + 4] = j + 2;
        indices[i + 5] = j + 3;
    }
    m_rcObject->createIndexStream(indices, size);
    delete[] indices;
    // create vertex buffer
    NihilUIVertices vertices;
    nihilPrepareUIPointVertexBuffer(vertices, infos);
    m_rcObject->createVertexStream(&vertices.front(), (int)vertices.size());
}

NihilControl_NavigateScene::NihilControl_NavigateScene()
{
}

NihilControl_NavigateScene::~NihilControl_NavigateScene()
{
    if (m_lmbPressed || m_rmbPressed)
    {
        m_lmbPressed = false;
        m_rmbPressed = false;
        ReleaseCapture();
    }
}

bool NihilControl_NavigateScene::onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam)
{
    ASSERT(core);
    switch (message)
    {
    case WM_LBUTTONDOWN:
        SetCapture(core->getHwnd());
        m_lmbPressed = true;
        m_lastpt.x = (float)GET_X_LPARAM(lParam);
        m_lastpt.y = (float)GET_Y_LPARAM(lParam);
        break;
    case WM_LBUTTONUP:
        if (m_lmbPressed)
        {
            ReleaseCapture();
            m_lmbPressed = false;
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            core->getSceneConfig().updateRotation(m_lastpt, pt);
            m_lastpt = pt;
        }
        break;
    case WM_RBUTTONDOWN:
        SetCapture(core->getHwnd());
        m_rmbPressed = true;
        m_lastpt.x = (float)GET_X_LPARAM(lParam);
        m_lastpt.y = (float)GET_Y_LPARAM(lParam);
        break;
    case WM_RBUTTONUP:
        if (m_rmbPressed)
        {
            ReleaseCapture();
            m_rmbPressed = false;
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            core->getSceneConfig().updateTranslation(m_lastpt, pt);
            m_lastpt = pt;
        }
        break;
    case WM_MOUSEMOVE:
        if (m_lmbPressed)
        {
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            core->getSceneConfig().updateRotation(m_lastpt, pt);
            m_lastpt = pt;
        }
        if (m_rmbPressed)
        {
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            core->getSceneConfig().updateTranslation(m_lastpt, pt);
            m_lastpt = pt;
        }
        break;
    case WM_MOUSEWHEEL:
    {
        float d = (float)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        core->getSceneConfig().updateZooming(d);
        break;
    }
    }
    return false;
}

NihilControl_ObjectLayer::NihilControl_ObjectLayer(NihilCore* core)
    : m_objectList(core->m_objectList)
{
    ASSERT(core);
    m_renderer = core->getRenderer();
    setupHittestTable(core->getHwnd(), core->getSceneConfig());
}

NihilControl_ObjectLayer::~NihilControl_ObjectLayer()
{
    m_renderer = nullptr;
    if (m_pressed)
    {
        m_pressed = false;
        ReleaseCapture();
    }
    if (m_selectArea)
    {
        delete m_selectArea;
        m_selectArea = nullptr;
    }
}

bool NihilControl_ObjectLayer::onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam)
{
    ASSERT(core);
    switch (message)
    {
    case WM_LBUTTONDOWN:
        SetCapture(core->getHwnd());
        m_pressed = true;
        m_startpt.x = (float)GET_X_LPARAM(lParam);
        m_startpt.y = (float)GET_Y_LPARAM(lParam);
        m_lastpt = m_startpt;
        switch (m_modTag)
        {
        case Mod_None:
            startSelecting();
            break;
        case Mod_Translation:
            startTranslation();
            break;
        }
        break;
    case WM_LBUTTONUP:
        if (m_pressed)
        {
            ReleaseCapture();
            m_pressed = false;
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            switch (m_modTag)
            {
            case Mod_None:
                endSelecting(pt);
                break;
            case Mod_Translation:
                updateTranslation(core->getSceneConfig(), pt);
                break;
            }
            m_lastpt = pt;
        }
        break;
    case WM_MOUSEMOVE:
        if (m_pressed)
        {
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            switch (m_modTag)
            {
            case Mod_None:
                updateSelecting(pt);
                break;
            case Mod_Translation:
                updateTranslation(core->getSceneConfig(), pt);
                break;
            }
            m_lastpt = pt;
        }
        break;
    }
    return false;
}

static void nihilBoundaryRect(gs::rectf& rc, const gs::vec2& p1, const gs::vec2& p2)
{
    float left, right, top, bottom;
    left = top = FLT_MAX;
    right = bottom = -FLT_MAX;
    left = std::min(p1.x, p2.x);
    right = std::max(p1.x, p2.x);
    top = std::min(p1.y, p2.y);
    bottom = std::max(p1.y, p2.y);
    rc.set_ltrb(left, top, right, bottom);
}

static void nihilBoundaryRect(gs::rectf& rc, const gs::vec2& p1, const gs::vec2& p2, const gs::vec2& p3)
{
    float left, right, top, bottom;
    left = top = FLT_MAX;
    right = bottom = -FLT_MAX;
    left = std::min(std::min(p1.x, p2.x), p3.x);
    top = std::min(std::min(p1.y, p2.y), p3.y);
    right = std::max(std::max(p1.x, p2.x), p3.x);
    bottom = std::max(std::max(p1.y, p2.y), p3.y);
    rc.set_ltrb(left, top, right, bottom);
}

void NihilControl_ObjectLayer::setupHittestTableOf(NihilObject* object, const gs::matrix& mat, UINT width, UINT height)
{
    ASSERT(object);
    switch (object->getType())
    {
    case NihilObject::OT_Polygon:
        return setupHittestTableOfPolygon(static_cast<NihilPolygon*>(object), mat, width, height);
    case NihilObject::OT_BiCubicBezierPatch:
        return setupHittestTableOfPolygon(static_cast<NihilBiCubicBezierPatch*>(object)->getGridMesh(), mat, width, height);
    case NihilObject::OT_BiCubicNURBS:
        return setupHittestTableOfPolygon(static_cast<NihilBiCubicNURBSurface*>(object)->getGridMesh(), mat, width, height);
    }
}

void NihilControl_ObjectLayer::setupHittestTableOfPolygon(NihilPolygon* polygon, const gs::matrix& mat, UINT width, UINT height)
{
    ASSERT(polygon);
    // 1.ndc => screen space
    gs::matrix ssm;
    ssm.multiply(polygon->getLocalMat(), mat);
    ssm.multiply(gs::matrix().scaling(0.5f * width, -0.5f * height, 0.f));
    ssm.multiply(gs::matrix().translation(0.5f * width, 0.5f * height, 0.f));
    // 2.transform points
    NihilPointList& pointList = polygon->getPointList();
    std::vector<gs::vec2> dupPoints;
    dupPoints.resize(pointList.size());
    int i = 0;
    for (NihilVertex& v : pointList)
    {
        gs::vec4 t;
        v.pos.transform(t, ssm);
        t.scale(1.f / t.w);
        dupPoints.at(i ++) = (const gs::vec2&)t;
    }
    // 3.setup rtree
    NihilIndexList& indexList = polygon->getIndexList();
    ASSERT(indexList.size() % 3 == 0);
    for (i = 0; i < (int)indexList.size(); i += 3)
    {
        int a = indexList.at(i);
        int b = indexList.at(i + 1);
        int c = indexList.at(i + 2);
        const gs::vec2& p1 = dupPoints.at(a);
        const gs::vec2& p2 = dupPoints.at(b);
        const gs::vec2& p3 = dupPoints.at(c);
        bool isccw = gs::vec2().sub(p2, p1).ccw(gs::vec2().sub(p3, p2)) > 0.f;
        if (isccw)  // flip y already make counter clockwised.
        {
            m_cache.push_back(HittestNode());
            HittestNode& node = m_cache.back();
            node.triangle[0] = p1;
            node.triangle[1] = p2;
            node.triangle[2] = p3;
#if 0
            gs::trace(_t("@moveTo %f, %f;\n"), p1.x, p1.y);
            gs::trace(_t("@lineTo %f, %f;\n"), p2.x, p2.y);
            gs::trace(_t("@lineTo %f, %f;\n"), p3.x, p3.y);
            gs::trace(_t("@lineTo %f, %f;\n"), p1.x, p1.y);
#endif
            node.index[0] = a;
            node.index[1] = b;
            node.index[2] = c;
            node.geometry = polygon->getGeometry();
            gs::rectf rc;
            nihilBoundaryRect(rc, p1, p2, p3);
            m_rtree.insert((UINT)&node, rc);
        }
    }
}

void NihilControl_ObjectLayer::setupHittestTable(HWND hwnd, NihilSceneConfig& sceneConfig)
{
    m_rtree.destroy();
    m_cache.clear();
    gs::matrix mat;
    sceneConfig.calcMatrix(mat);
    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    for (auto* p : m_objectList)
    {
        ASSERT(p);
        setupHittestTableOf(p, mat, width, height);
    }
}

void NihilControl_ObjectLayer::resetSelectState()
{
    for (auto* p : m_objectList)
    {
        ASSERT(p);
        p->setSelected(false);
    }
}

void NihilControl_ObjectLayer::startSelecting()
{
    if (m_selectArea)
        delete m_selectArea;
    m_selectArea = new NihilUIRectangle(m_renderer);
    ASSERT(m_selectArea);
    m_selectArea->setTopLeft(m_startpt.x, m_startpt.y);
    m_selectArea->setupBuffers();
}

void NihilControl_ObjectLayer::endSelecting(const gs::vec2& pt)
{
    ASSERT(m_selectArea);
    delete m_selectArea;
    m_selectArea = nullptr;
    resetSelectState();
    gs::rectf rc;
    nihilBoundaryRect(rc, m_startpt, pt);
    hitTest(rc);
}

void NihilControl_ObjectLayer::updateSelecting(const gs::vec2& pt)
{
    ASSERT(m_selectArea);
    m_selectArea->setBottomRight(pt.x, pt.y);
    m_selectArea->updateBuffer();
}

void NihilControl_ObjectLayer::hitTest(const gs::rectf& rc)
{
    // 1.hit through rtree
    std::vector<UINT> hittable;
    m_rtree.query(rc, hittable);
    // 2.todo: test if the rect overlaps these triangles then filter them again.
    // 3.tag them
    for (UINT u : hittable)
    {
        auto* node = reinterpret_cast<HittestNode*>(u);
        node->geometry->setSelected(true);
    }
}

static void collectSelectedObjects(NihilObjectList& selectedObjects, const NihilObjectList& objects)
{
    selectedObjects.clear();
    for (const NihilObject* object : objects)
    {
        ASSERT(object);
        if (object->isSelected())
            selectedObjects.push_back(const_cast<NihilObject*>(object));
    }
}

void NihilControl_ObjectLayer::startTranslation()
{
    // get selected polygons
    collectSelectedObjects(m_selectedObjects, m_objectList);
}

void NihilControl_ObjectLayer::updateTranslation(NihilSceneConfig& sceneConfig, const gs::vec2& pt)
{
    if (m_selectedObjects.empty())
        return;
    auto t = sceneConfig.getTranslationInCurrentView(m_lastpt, pt);
    for (auto* polygon : m_selectedObjects)
    {
        ASSERT(polygon);
        polygon->appendTranslation(t);
    }
}

NihilControl_PointsLayer::NihilControl_PointsLayer(NihilCore* core)
{
    ASSERT(core);
    m_renderer = core->getRenderer();
    setupSelectedObjects(core->m_objectList);
    setupHittestInfo(core->getHwnd(), core->getSceneConfig());
}

NihilControl_PointsLayer::~NihilControl_PointsLayer()
{
    m_renderer = nullptr;
    if (m_pressed)
    {
        m_pressed = false;
        ReleaseCapture();
    }
    if (m_selectArea)
    {
        delete m_selectArea;
        m_selectArea = nullptr;
    }
    if (m_uiPoints)
    {
        delete m_uiPoints;
        m_uiPoints = nullptr;
    }
}

bool NihilControl_PointsLayer::onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam)
{
    ASSERT(core);
    switch(message)
    {
    case WM_LBUTTONDOWN:
        SetCapture(core->getHwnd());
        m_pressed = true;
        m_startpt.x = (float)GET_X_LPARAM(lParam);
        m_startpt.y = (float)GET_Y_LPARAM(lParam);
        m_lastpt = m_startpt;
        switch (m_modTag)
        {
        case Mod_None:
            startSelecting();
            break;
        case Mod_Translation:
            startTranslation();
            break;
        }
        break;
    case WM_LBUTTONUP:
        if (m_pressed)
        {
            ReleaseCapture();
            m_pressed = false;
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            switch (m_modTag)
            {
            case Mod_None:
                endSelecting(pt);
                break;
            case Mod_Translation:
                updateTranslation(core->getSceneConfig(),core->getHwnd(), pt);
                break;
            }
            m_lastpt = pt;
        }
        break;
    case WM_MOUSEMOVE:
        if (m_pressed)
        {
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            switch (m_modTag)
            {
            case Mod_None:
                updateSelecting(pt);
                break;
            case Mod_Translation:
                updateTranslation(core->getSceneConfig(), core->getHwnd(), pt);
                break;
            }
            m_lastpt = pt;
        }
        break;
    }
    return false;
}

void NihilControl_PointsLayer::setupSelectedObjects(const NihilObjectList& objects)
{
    collectSelectedObjects(m_selectedObjects, objects);
}

void NihilControl_PointsLayer::setupHittestInfo(HWND hwnd, NihilSceneConfig& sceneConfig)
{
    m_pointInfos.clear();
    if (m_selectedObjects.empty())
        return;
    ASSERT(!m_uiPoints);
    m_uiPoints = new NihilUIPoints(m_renderer);
    gs::matrix mat;
    sceneConfig.calcMatrix(mat);
    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    for (NihilObject* p : m_selectedObjects)
    {
        ASSERT(p && p->isSelected());
        setupHittestInfoOf(p, mat, width, height);
    }
    updateUIVertices();
}

void NihilControl_PointsLayer::setupHittestInfoOf(NihilObject* object, const gs::matrix& mat, UINT width, UINT height)
{
    ASSERT(object);
    switch (object->getType())
    {
    case NihilObject::OT_Polygon:
        return setupHittestInfoOfPolygon(static_cast<NihilPolygon*>(object), mat, width, height);
    case NihilObject::OT_BiCubicBezierPatch:
        return setupHittestInfoOfBiCubicBezier(static_cast<NihilBiCubicBezierPatch*>(object), mat, width, height);
    case NihilObject::OT_BiCubicNURBS:
        return setupHittestInfoOfBiCubicNurbs(static_cast<NihilBiCubicNURBSurface*>(object), mat, width, height);
    }
}

void NihilControl_PointsLayer::setupHittestInfoOfPolygon(NihilPolygon* polygon, const gs::matrix& mat, UINT width, UINT height)
{
    ASSERT(polygon);
    // 1.ndc => screen space
    gs::matrix ssm;
    ssm.multiply(polygon->getLocalMat(), mat);
    ssm.multiply(gs::matrix().scaling(0.5f * width, -0.5f * height, 0.f));
    ssm.multiply(gs::matrix().translation(0.5f * width, 0.5f * height, 0.f));
    // 2.transform points
    NihilPointList& pointList = polygon->getPointList();
    std::vector<gs::vec3> dupPoints;
    dupPoints.resize(pointList.size());
    int i = 0;
    for (NihilVertex& v : pointList)
    {
        gs::vec4 t;
        v.pos.transform(t, ssm);
        t.scale(1.f / t.w);
        dupPoints.at(i ++) = (const gs::vec3&)t;
    }
    // 3.test if the point was visible
    std::unordered_set<int> visibleIndices;
    NihilIndexList& indexList = polygon->getIndexList();
    ASSERT(indexList.size() % 3 == 0);
    for (i = 0; i < (int)indexList.size(); i += 3)
    {
        int a = indexList.at(i);
        int b = indexList.at(i + 1);
        int c = indexList.at(i + 2);
        gs::vec2 p1 = dupPoints.at(a);
        gs::vec2 p2 = dupPoints.at(b);
        gs::vec2 p3 = dupPoints.at(c);
        bool isccw = gs::vec2().sub(p2, p1).ccw(gs::vec2().sub(p3, p2)) > 0.f;
        if (isccw)  // flip y already make counter clockwised.
        {
            visibleIndices.insert(a);
            visibleIndices.insert(b);
            visibleIndices.insert(c);
        }
    }
    // 4.record the points info
    for (int i : visibleIndices)
        m_pointInfos.push_back(NihilPointInfo(polygon, i, dupPoints.at(i)));
}

void NihilControl_PointsLayer::setupHittestInfoOfBiCubicBezier(NihilBiCubicBezierPatch* bezierPatch, const gs::matrix& mat, UINT width, UINT height)
{
    ASSERT(bezierPatch);
    // 1.ndc => screen space
    gs::matrix ssm;
    ssm.multiply(bezierPatch->getLocalMat(), mat);
    ssm.multiply(gs::matrix().scaling(0.5f * width, -0.5f * height, 0.f));
    ssm.multiply(gs::matrix().translation(0.5f * width, 0.5f * height, 0.f));
    // 2.transform points
    gs::vec3* cvs = bezierPatch->getCvs();
    ASSERT(cvs);
    std::vector<gs::vec3> dupPoints;
    dupPoints.resize(16);
    for (int i = 0; i < 16; i ++)
    {
        gs::vec4 t;
        cvs[i].transform(t, ssm);
        t.scale(1.f / t.w);
        dupPoints.at(i) = (const gs::vec3&)t;
    }
    // 3.test if the point was visible
    std::unordered_set<int> visibleIndices;
    static int indexArray[4][4] =
    {
        { 0, 1, 2, 3 },
        { 4, 5, 6, 7 },
        { 8, 9, 10, 11 },
        { 12, 13, 14, 15 }
    };
    for (int i = 0; i < 3; i ++)
    {
        for (int j = 0; j < 3; j ++)
        {
            int a = indexArray[i][j];
            int b = indexArray[i + 1][j];
            int c = indexArray[i][j + 1];
            int d = indexArray[i + 1][j + 1];
            gs::vec2 p1 = dupPoints.at(a);  // a
            gs::vec2 p2 = dupPoints.at(b);  // b
            gs::vec2 p3 = dupPoints.at(c);  // c
            gs::vec2 p4 = dupPoints.at(d);  // d
            // a - c - d
            if (gs::vec2().sub(p3, p1).ccw(gs::vec2().sub(p4, p3)) > 0.f)
            {
                visibleIndices.insert(a);
                visibleIndices.insert(c);
                visibleIndices.insert(d);
            }
            // a - d - b
            if (gs::vec2().sub(p4, p1).ccw(gs::vec2().sub(p2, p4)) > 0.f)
            {
                visibleIndices.insert(a);
                visibleIndices.insert(d);
                visibleIndices.insert(b);
            }
        }
    }
    // 4.record the points info
    for (int i : visibleIndices)
        m_pointInfos.push_back(NihilPointInfo(bezierPatch, i, dupPoints.at(i)));
}

void NihilControl_PointsLayer::setupHittestInfoOfBiCubicNurbs(NihilBiCubicNURBSurface* nurbs, const gs::matrix& mat, UINT width, UINT height)
{
    ASSERT(nurbs);
    // 1.ndc => screen space
    gs::matrix ssm;
    ssm.multiply(nurbs->getLocalMat(), mat);
    ssm.multiply(gs::matrix().scaling(0.5f * width, -0.5f * height, 0.f));
    ssm.multiply(gs::matrix().translation(0.5f * width, 0.5f * height, 0.f));
    // 2.transform points
    auto& cvs = nurbs->getCvs();
    std::vector<gs::vec3> dupPoints;
    dupPoints.resize(cvs.size());
    for (int i = 0; i < (int)cvs.size(); i ++)
    {
        gs::vec4 t;
        cvs.at(i).transform(t, ssm);
        t.scale(1.f / t.w);
        dupPoints.at(i) = (const gs::vec3&)t;
    }
    // 3.test if the point was visible
    std::unordered_set<int> visibleIndices;
    int ucvs = nurbs->getUCvs(), vcvs = nurbs->getVCvs();
    for (int v = 0; v < vcvs - 1; v ++)
    {
        for (int u = 0; u < ucvs - 1; u ++)
        {
            int a = v * ucvs + u, b = a + 1;
            int c = (v + 1) * ucvs + u, d = c + 1;
            gs::vec2 p1 = dupPoints.at(a);  // a
            gs::vec2 p2 = dupPoints.at(b);  // b
            gs::vec2 p3 = dupPoints.at(c);  // c
            gs::vec2 p4 = dupPoints.at(d);  // d
            // a - c - d
            if (gs::vec2().sub(p3, p1).ccw(gs::vec2().sub(p4, p3)) > 0.f)
            {
                visibleIndices.insert(a);
                visibleIndices.insert(c);
                visibleIndices.insert(d);
            }
            // a - d - b
            if (gs::vec2().sub(p4, p1).ccw(gs::vec2().sub(p2, p4)) > 0.f)
            {
                visibleIndices.insert(a);
                visibleIndices.insert(d);
                visibleIndices.insert(b);
            }
        }
    }
    // 4.record the points info
    for (int i : visibleIndices)
        m_pointInfos.push_back(NihilPointInfo(nurbs, i, dupPoints.at(i)));
}

void NihilControl_PointsLayer::updateUIVertices()
{
    if (m_uiPoints)
        m_uiPoints->updateBuffer(m_pointInfos);
}

void NihilControl_PointsLayer::resetSelectState()
{
    for (auto& info : m_pointInfos)
        info.isSelected = false;
}

void NihilControl_PointsLayer::startSelecting()
{
    if (m_selectArea)
        delete m_selectArea;
    m_selectArea = new NihilUIRectangle(m_renderer);
    ASSERT(m_selectArea);
    m_selectArea->setTopLeft(m_startpt.x, m_startpt.y);
    m_selectArea->setupBuffers();
}

void NihilControl_PointsLayer::endSelecting(const gs::vec2& pt)
{
    ASSERT(m_selectArea);
    delete m_selectArea;
    m_selectArea = nullptr;
    resetSelectState();
    gs::rectf rc;
    nihilBoundaryRect(rc, m_startpt, pt);
    hitTest(rc);
    updateUIVertices();
}

void NihilControl_PointsLayer::updateSelecting(const gs::vec2& pt)
{
    ASSERT(m_selectArea);
    m_selectArea->setBottomRight(pt.x, pt.y);
    m_selectArea->updateBuffer();
}

void NihilControl_PointsLayer::hitTest(const gs::rectf& rc)
{
    for (auto& info : m_pointInfos)
    {
        if (rc.in_rect(gs::pointf(info.point.x, info.point.y)))
            info.isSelected = true;
    }
}

static void collectSelectedPoints(NihilPointInfoRefs& selectedPoints, NihilPointInfoList& infos)
{
    selectedPoints.clear();
    for (auto& info : infos)
    {
        if (info.isSelected)
            selectedPoints.push_back(&info);
    }
}

void NihilControl_PointsLayer::startTranslation()
{
    collectSelectedPoints(m_selectedPoints, m_pointInfos);
}

static NihilObject* nihilUpdateTranslationPoint(NihilPointInfo* info, const gs::matrix& mat, UINT width, UINT height, const gs::vec3& offset)
{
    ASSERT(info);
    // ndc => screen space
    gs::matrix ssm;
    ssm.multiply(info->object->getLocalMat(), mat);
    ssm.multiply(gs::matrix().scaling(0.5f * width, -0.5f * height, 0.f));
    ssm.multiply(gs::matrix().translation(0.5f * width, 0.5f * height, 0.f));
    // offset the origin point
    ASSERT(info->object->getType() == NihilObject::OT_Polygon);
    auto& ptList = static_cast<NihilPolygon*>(info->object)->getPointList();
    auto& v = ptList.at(info->index).pos;
    v += offset;
    // modify the point info
    gs::vec4 t;
    v.transform(t, ssm);
    t.scale(1.f / t.w);
    info->point = (const gs::vec3&)t;
    return info->object;
}

static NihilObject* nihilUpdateTranslationCvs(NihilPointInfo* info, const gs::matrix& mat, UINT width, UINT height, const gs::vec3& offset)
{
    ASSERT(info);
    // ndc => screen space
    gs::matrix ssm;
    ssm.multiply(info->object->getLocalMat(), mat);
    ssm.multiply(gs::matrix().scaling(0.5f * width, -0.5f * height, 0.f));
    ssm.multiply(gs::matrix().translation(0.5f * width, 0.5f * height, 0.f));
    //
    gs::vec3* v = nullptr;
    // offset the origin point
    if (info->object->getType() == NihilObject::OT_BiCubicBezierPatch)
    {
        auto* cvs = static_cast<NihilBiCubicBezierPatch*>(info->object)->getCvs();
        v = &cvs[info->index];
        (*v) += offset;
    }
    else if (info->object->getType() == NihilObject::OT_BiCubicNURBS)
    {
        auto& cvs = static_cast<NihilBiCubicNURBSurface*>(info->object)->getCvs();
        v = &cvs.at(info->index);
        (*v) += offset;
    }
    else
    {
        ASSERT(!"Unexpecd type.");
        return nullptr;
    }
    // modify the point info
    gs::vec4 t;
    v->transform(t, ssm);
    t.scale(1.f / t.w);
    info->point = (const gs::vec3&)t;
    return info->object;
}

void NihilControl_PointsLayer::updateTranslation(NihilSceneConfig& sceneConfig, HWND hwnd, const gs::vec2& pt)
{
    if (m_selectedPoints.empty())
        return;
    auto t = sceneConfig.getTranslationInCurrentView(m_lastpt, pt);
    gs::matrix mat;
    sceneConfig.calcMatrix(mat);
    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    std::unordered_set<NihilObject*> modifiedObjects;
    for (auto* info : m_selectedPoints)
    {
        ASSERT(info);
        switch (info->object->getType())
        {
        case NihilObject::OT_Polygon:
            modifiedObjects.insert(nihilUpdateTranslationPoint(info, mat, width, height, t));
            break;
        case NihilObject::OT_BiCubicBezierPatch:
        case NihilObject::OT_BiCubicNURBS:
            modifiedObjects.insert(nihilUpdateTranslationCvs(info, mat, width, height, t));
            break;
        }
    }
    for (auto* object : modifiedObjects)
    {
        ASSERT(object);
        object->updateBuffers();
    }
    updateUIVertices();
}
