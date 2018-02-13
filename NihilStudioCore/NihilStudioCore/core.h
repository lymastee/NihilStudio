#pragma once

#include <windows.h>
#include <unordered_map>
#include <gslib/math.h>
#include <gslib/string.h>
#include <gslib/rtree.h>

struct NihilVertex
{
    gs::vec3                pos;
    gs::vec3                normal;
};

struct NihilUIVertex
{
    gs::vec3                pos;
    gs::vec4                color;
};

typedef gs::string NihilString;
class NihilCore;

class __declspec(novtable) NihilGeometry abstract
{
public:
    virtual ~NihilGeometry() {}
    virtual bool createVertexStream(NihilVertex vertices[], int size) = 0;
    virtual bool createIndexStream(int indices[], int size) = 0;
    virtual bool updateVertexStream(NihilVertex vertices[], int size) = 0;
    virtual void setLocalMat(const gs::matrix& m) = 0;
    void setSelected(bool b) { m_isSelected = b; }
    bool isSelected() const { return m_isSelected; }

protected:
    bool                    m_isSelected = false;
};

class __declspec(novtable) NihilUIObject abstract
{
public:
    enum Topology
    {
        Topo_Points,
        Topo_LineList,
    };

public:
    virtual ~NihilUIObject() {}
    virtual bool createVertexStream(NihilUIVertex vertices[], int size) = 0;
    virtual bool createIndexStream(int indices[], int size) = 0;
    virtual bool updateVertexStream(NihilUIVertex vertices[], int size) = 0;

protected:
    Topology                m_topology = Topo_Points;
};

class __declspec(novtable) NihilRenderer abstract
{
public:
    virtual ~NihilRenderer() {}
    virtual bool setup(HWND hwnd) = 0;
    virtual void render() = 0;
    virtual void notifyResize() = 0;
    virtual void setWorldMat(const gs::matrix& m) = 0;
    virtual NihilGeometry* addGeometry() = 0;
    virtual NihilUIObject* addUIObject() = 0;
    virtual void removeGeometry(NihilGeometry*) = 0;
    virtual void removeUIObject(NihilUIObject*) = 0;
    virtual void showSolidMode() = 0;
    virtual void showWireframeMode() = 0;
};

class __declspec(novtable) NihilControl abstract
{
public:
    virtual ~NihilControl() {}
    virtual bool onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam) = 0; // true: stop false: continue
};

class NihilSceneConfig
{
public:
    void setup(HWND hwnd);
    void updateProjMatrix(HWND hwnd);
    void updateViewMatrix();
    void calcMatrix(gs::matrix& mat);
    void updateRotation(const gs::vec2& lastpt, const gs::vec2& pt);
    void updateZooming(float d);
    void updateTranslation(const gs::vec2& lastpt, const gs::vec2& pt);

private:
    float                   m_rot1 = 0.f;           // first rotation angle, in x-z plane, rotate in y-axis
    float                   m_rot2 = 0.f;           // second rotation angle, in rot1 plane
    float                   m_cdis = 4.f;           // camera distance
    gs::vec3                m_viewOffset = gs::vec3(0.f, 0.f, 0.f);
    gs::matrix              m_model;
    gs::matrix              m_viewLookat;
    gs::matrix              m_proj;
};

// todo: add Edge - Face structures later
typedef std::vector<NihilVertex> NihilPointList;
typedef std::vector<int> NihilIndexList;

class NihilPolygon
{
public:
    NihilPolygon(NihilRenderer* renderer);
    ~NihilPolygon();
    int loadPolygonFromTextStream(const NihilString& src, int start);
    NihilPointList& getPointList() { return m_pointList; }
    NihilIndexList& getIndexList() { return m_indexList; }
    void setSelected(bool b)
    {
        if (m_geometry)
            m_geometry->setSelected(b);
    }

protected:
    NihilRenderer*          m_renderer = nullptr;
    NihilGeometry*          m_geometry = nullptr;
    NihilPointList          m_pointList;
    NihilIndexList          m_indexList;
    gs::matrix              m_localMat;

protected:
    void calculateNormals();
    bool setupGeometryBuffers();
    int loadPointSectionFromTextStream(const NihilString& src, int start);
    int loadFaceSectionFromTextStream(const NihilString& src, int start);
    int loadLocalSectionFromTextStream(const NihilString& src, int start);
};

typedef std::vector<NihilPolygon*> NihilPolygonList;

class NihilUIRectangle
{
public:
    NihilUIRectangle(NihilRenderer* renderer);
    ~NihilUIRectangle();
    void setTopLeft(float left, float top);
    void setBottomRight(float right, float bottom);

protected:
    NihilRenderer*          m_renderer = nullptr;
    NihilUIObject*          m_rcObject = nullptr;
    gs::rectf               m_rc;
};

// controllers
class NihilControl_NavigateScene :
    public NihilControl
{
public:
    NihilControl_NavigateScene();
    virtual ~NihilControl_NavigateScene();
    virtual bool onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    bool                    m_lmbPressed = false;
    bool                    m_rmbPressed = false;
    gs::vec2                m_lastpt;
};

typedef gs::rtree_entity<UINT> NihilHittestEntity;
typedef gs::rtree_node<NihilHittestEntity> NihilHittestNode;
typedef gs::_tree_allocator<NihilHittestNode> NihilHittestAlloc;
typedef gs::tree<NihilHittestEntity, NihilHittestNode, NihilHittestAlloc> NihilHittestTree;
typedef gs::rtree<NihilHittestEntity, gs::quadratic_split_alg<5, 2, NihilHittestTree>, NihilHittestNode, NihilHittestAlloc> NihilHittestRtree;

class NihilControl_SelectObject :
    public NihilControl
{
protected:
    struct HittestNode
    {
        gs::vec2            triangle[3];
        int                 index[3];
    };
    typedef std::list<HittestNode> HittestNodeCache;

public:
    NihilControl_SelectObject(NihilCore* core);
    virtual ~NihilControl_SelectObject();
    virtual bool onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    NihilPolygonList&       m_polygonList;
    NihilHittestRtree       m_rtree;
    HittestNodeCache        m_cache;
    NihilUIRectangle*       m_selectArea = nullptr;
    NihilRenderer*          m_renderer = nullptr;
    bool                    m_pressed = false;
    gs::vec2                m_startpt;

private:
    void setupHittestTable(HWND hwnd, NihilSceneConfig& sceneConfig);
    void setupHittestTableOf(NihilPolygon* polygon, const gs::matrix& mat, UINT width, UINT height);
    void resetSelectState();
    void startSelecting();
    void endSelecting(const gs::vec2& pt);
    void updateSelecting(const gs::vec2& pt);
};

class NihilCore
{
    friend class NihilControl_SelectObject;

public:
    NihilCore();
    virtual ~NihilCore();

public:
    void setup(HWND hwnd);
    void resizeWindow();
    void destroy();
    void render();
    void showSolidMode();
    void showWireframeMode();
    void destroyController();
    void navigateScene();
    void selectObject();
    NihilRenderer* getRenderer() const { return m_renderer; }
    NihilControl* getController() const { return m_controller; }
    NihilSceneConfig& getSceneConfig() { return m_sceneConfig; }
    HWND getHwnd() const { return m_hwnd; }
    WNDPROC getOldWndProc() const { return m_oldWndProc; }
    bool loadFromTextStream(const NihilString& src);

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

protected:
    HWND                    m_hwnd = 0;             // valid if non-zero
    WNDPROC                 m_oldWndProc = nullptr; // old windowproc
    NihilRenderer*          m_renderer = nullptr;
    NihilSceneConfig        m_sceneConfig;
    NihilPolygonList        m_polygonList;
    NihilControl*           m_controller = nullptr;

protected:
    void destroyObjects();
    bool setupWindow(HWND hwnd);
    bool setupRenderer();
    int loadPolygonFromTextStream(const NihilString& src, int start);   // -1: failed
};
