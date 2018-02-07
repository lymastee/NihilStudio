#pragma once

#include <windows.h>
#include <unordered_map>
#include <gslib/math.h>
#include <gslib/string.h>

struct NihilVertex
{
    gs::vec3            pos;
    gs::vec3            normal;
};

struct NihilUIVertex
{
    gs::vec4            pos;
    gs::vec4            color;
};

typedef gs::string NihilString;

class __declspec(novtable) NihilGeometry abstract
{
public:
    virtual ~NihilGeometry() {}
    virtual bool createVertexStream(NihilVertex vertices[], int size) = 0;
    virtual bool createIndexStream(int indices[], int size) = 0;
    virtual bool updateVertexStream(NihilVertex vertices[], int size) = 0;
};

class __declspec(novtable) NihilUIObject abstract
{
public:
    virtual ~NihilUIObject() {}
    virtual bool createVertexStream(NihilUIVertex vertices[], int size) = 0;
    virtual bool createIndexStream(int indices[], int size) = 0;
    virtual bool updateVertexStream(NihilUIVertex vertices[], int size) = 0;
};

class __declspec(novtable) NihilRenderer abstract
{
public:
    virtual ~NihilRenderer() {}
    virtual bool setup(HWND hwnd) = 0;
    virtual void render() = 0;
    virtual void notifyResize() = 0;
    virtual NihilGeometry* addGeometry() = 0;
    virtual NihilUIObject* addUIObject() = 0;
    virtual void removeGeometry(NihilGeometry*) = 0;
    virtual void removeUIObject(NihilUIObject*) = 0;
};

// todo: add Edge - Face structures later
typedef std::list<gs::vec3> NihilPointList;
typedef std::vector<int> NihilIndexList;
typedef struct { std::vector<int> linkedIndices; } NihilLinkedPoints;
typedef std::unordered_map<int, NihilLinkedPoints> NihilPointLinkage;


class NihilPolygon
{
public:
    NihilPolygon(NihilRenderer* renderer);
    ~NihilPolygon();
    int loadPolygonFromTextStream(const NihilString& src, int start);

protected:
    NihilRenderer*          m_renderer = nullptr;
    NihilGeometry*          m_geometry = nullptr;
    NihilPointList          m_pointList;
    NihilIndexList          m_indexList;
    NihilPointLinkage       m_pointLinkage;     // used for calculate the points normal
    gs::matrix              m_wvp;
};

typedef std::vector<NihilPolygon*> NihilPolygonList;

class NihilCore
{
    typedef std::unordered_map<HWND, NihilCore*> NihilHwndCoreMap;

public:
    NihilCore();
    virtual ~NihilCore();

public:
    void setup(HWND hwnd);
    void resizeWindow();
    void destroy();
    void render();
    NihilRenderer* getRenderer() const { return m_renderer; }
    HWND getHwnd() const { return m_hwnd; }
    WNDPROC getOldWndProc() const { return m_oldWndProc; }
    bool loadFromTextStream(const NihilString& src);

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    static NihilHwndCoreMap m_instMap;

protected:
    HWND                    m_hwnd = 0;             // valid if non-zero
    WNDPROC                 m_oldWndProc = nullptr; // old windowproc
    NihilRenderer*          m_renderer = nullptr;
    NihilPolygonList        m_polygonList;

protected:
    void destroyObjects();
    bool setupWindow(HWND hwnd);
    bool setupRenderer();
    int loadPolygonFromTextStream(const NihilString& src, int start);   // -1: failed
};
