#include <assert.h>
#include <windowsx.h>
#include <algorithm>
#include <gslib/error.h>
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
    m_controller = new NihilControl_SelectObject(this);
}

void NihilCore::selectPoints()
{
    destroyController();
    m_controller = new NihilControl_SelectPoints(this);
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
    for (auto* p : m_polygonList)
        delete p;
    m_polygonList.clear();
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
    m_polygonList.push_back(polygon);
    return polygon->loadPolygonFromTextStream(src, start);
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
    gs::matrix mat;
    float det;
    mat.inverse(&det, m_viewLookat);
    gs::vec2 d;
    d.sub(pt, lastpt);
    d.scale(0.01f);
    gs::vec4 t, t0;
    t.multiply(gs::vec4(d.x, -d.y, 0.f, 1.f), mat);
    t0.multiply(gs::vec4(0.f, 0.f, 0.f, 1.f), mat);
    t.scale(1.f / t.w);
    t0.scale(1.f / t.w);
    t.sub(t, t0);
    m_viewOffset.add(m_viewOffset, gs::vec3(t.x, t.y, t.z));
    updateViewMatrix();
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

int NihilPolygon::loadLocalSectionFromTextStream(const NihilString& src, int start)
{
    return 666;
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
    m_rcObject = renderer->addUIObject();
    ASSERT(m_rcObject);
    m_rcObject->setTopology(NihilUIObject::Topo_Points);
}

NihilUIPoints::~NihilUIPoints()
{
    ASSERT(m_renderer && m_rcObject);
    m_renderer->removeUIObject(m_rcObject);
    m_renderer = nullptr;
    m_rcObject = nullptr;
}

template<class _Cont>
void NihilUIPoints::setupBuffers(const _Cont& src, const gs::vec4& cr)
{
}

template<class _Cont>
void NihilUIPoints::updateBuffer(const _Cont& src)
{
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

NihilControl_SelectObject::NihilControl_SelectObject(NihilCore* core)
    : m_polygonList(core->m_polygonList)
{
    ASSERT(core);
    m_renderer = core->getRenderer();
    setupHittestTable(core->getHwnd(), core->getSceneConfig());
}

NihilControl_SelectObject::~NihilControl_SelectObject()
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

bool NihilControl_SelectObject::onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam)
{
    ASSERT(core);
    switch (message)
    {
    case WM_LBUTTONDOWN:
        SetCapture(core->getHwnd());
        m_pressed = true;
        m_startpt.x = (float)GET_X_LPARAM(lParam);
        m_startpt.y = (float)GET_Y_LPARAM(lParam);
        startSelecting();
        break;
    case WM_LBUTTONUP:
        if (m_pressed)
        {
            ReleaseCapture();
            m_pressed = false;
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            endSelecting(pt);
        }
        break;
    case WM_MOUSEMOVE:
        if (m_pressed)
        {
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            updateSelecting(pt);
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

void NihilControl_SelectObject::setupHittestTableOf(NihilPolygon* polygon, const gs::matrix& mat, UINT width, UINT height)
{
    ASSERT(polygon);
    // 1.ndc => screen space
    gs::matrix ssm;
    ssm.multiply(mat, gs::matrix().scaling(0.5f * width, -0.5f * height, 0.f));
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

void NihilControl_SelectObject::setupHittestTable(HWND hwnd, NihilSceneConfig& sceneConfig)
{
    m_rtree.destroy();
    m_cache.clear();
    gs::matrix mat;
    sceneConfig.calcMatrix(mat);
    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    for (NihilPolygon* p : m_polygonList)
    {
        ASSERT(p);
        setupHittestTableOf(p, mat, width, height);
    }
}

void NihilControl_SelectObject::resetSelectState()
{
    for (NihilPolygon* p : m_polygonList)
    {
        ASSERT(p);
        p->setSelected(false);
    }
}

void NihilControl_SelectObject::startSelecting()
{
    if (m_selectArea)
        delete m_selectArea;
    m_selectArea = new NihilUIRectangle(m_renderer);
    ASSERT(m_selectArea);
    m_selectArea->setTopLeft(m_startpt.x, m_startpt.y);
    m_selectArea->setupBuffers();
}

void NihilControl_SelectObject::endSelecting(const gs::vec2& pt)
{
    ASSERT(m_selectArea);
    delete m_selectArea;
    m_selectArea = nullptr;
    resetSelectState();
    gs::rectf rc;
    nihilBoundaryRect(rc, m_startpt, pt);
    hitTest(rc);
}

void NihilControl_SelectObject::updateSelecting(const gs::vec2& pt)
{
    ASSERT(m_selectArea);
    m_selectArea->setBottomRight(pt.x, pt.y);
    m_selectArea->updateBuffer();
}

void NihilControl_SelectObject::hitTest(const gs::rectf& rc)
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

NihilControl_SelectPoints::NihilControl_SelectPoints(NihilCore* core)
{
    ASSERT(core);
    m_renderer = core->getRenderer();
}

NihilControl_SelectPoints::~NihilControl_SelectPoints()
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

bool NihilControl_SelectPoints::onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam)
{
    ASSERT(core);
    switch(message)
    {
    case WM_LBUTTONDOWN:
        SetCapture(core->getHwnd());
        m_pressed = true;
        m_startpt.x = (float)GET_X_LPARAM(lParam);
        m_startpt.y = (float)GET_Y_LPARAM(lParam);
        startSelecting();
        break;
    case WM_LBUTTONUP:
        if (m_pressed)
        {
            ReleaseCapture();
            m_pressed = false;
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            endSelecting(pt);
        }
        break;
    case WM_MOUSEMOVE:
        if (m_pressed)
        {
            gs::vec2 pt;
            pt.x = (float)GET_X_LPARAM(lParam);
            pt.y = (float)GET_Y_LPARAM(lParam);
            updateSelecting(pt);
        }
        break;
    }
    return false;
}

void NihilControl_SelectPoints::startSelecting()
{
    if (m_selectArea)
        delete m_selectArea;
    m_selectArea = new NihilUIRectangle(m_renderer);
    ASSERT(m_selectArea);
    m_selectArea->setTopLeft(m_startpt.x, m_startpt.y);
    m_selectArea->setupBuffers();
}

void NihilControl_SelectPoints::endSelecting(const gs::vec2& pt)
{
    ASSERT(m_selectArea);
    delete m_selectArea;
    m_selectArea = nullptr;

    gs::rectf rc;
    nihilBoundaryRect(rc, m_startpt, pt);
}

void NihilControl_SelectPoints::updateSelecting(const gs::vec2& pt)
{
    ASSERT(m_selectArea);
    m_selectArea->setBottomRight(pt.x, pt.y);
    m_selectArea->updateBuffer();
}

void NihilControl_SelectPoints::hitTest(const gs::rectf& rc)
{
}
