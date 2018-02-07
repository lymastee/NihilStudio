#include <assert.h>
#include "core.h"
#include "dx11renderer.h"

#define ASSERT assert

NihilCore::NihilHwndCoreMap NihilCore::m_instMap;

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
}

void NihilCore::resizeWindow()
{
    if (m_renderer)
    {
        m_renderer->notifyResize();
        m_renderer->render();
    }
}

void NihilCore::destroy()
{
    destroyObjects();
    if (m_hwnd)
    {
        m_instMap.erase(m_hwnd);
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
        m_renderer->render();
}

#define NIHIL_BLANKS _t(" \t\v\r\n\f")

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
        name.assign(src.c_str() + start, p);
    return p + start;
}

bool NihilCore::loadFromTextStream(const NihilString& src)
{
    int start = skipBlankCharactors(src, 0);
    if (start == src.length())  // eof
        return false;
    NihilString prename;
    int next = prereadSectionName(src, prename, start);
    if (next == src.length())   // eof
        return false;
    do
    {
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
        next = prereadSectionName(src, prename, start = next);
    } while (next != src.length());
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
    WNDPROC wndproc = (WNDPROC)GetWindowLong(hwnd, GWL_WNDPROC);
    m_oldWndProc = wndproc;
    m_instMap.insert(std::make_pair(hwnd, this));
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
    auto f = m_instMap.find(hwnd);
    if (f == m_instMap.end())
        return DefWindowProc(hwnd, msg, wParam, lParam);
    NihilCore* core = f->second;
    ASSERT(core);
    WNDPROC oldWndProc = core->getOldWndProc();
    if (!oldWndProc)
        oldWndProc = DefWindowProc;
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
    return -1;
}
