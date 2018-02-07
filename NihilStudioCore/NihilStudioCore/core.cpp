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

static const gs::gchar nihilBlanks[] = _t(" \t\v\r\n\f");

static int skipBlankCharactors(const gs::string& src, int start)
{
    int p = (int)src.find_first_not_of(nihilBlanks, start);
    if (p == gs::string::npos)
        return src.length();
    return p;
}

bool NihilCore::loadFromTextStream(const gs::string& src)
{
    int start = skipBlankCharactors(src, 0);
    return true;
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
