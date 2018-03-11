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
        Topo_TriangleList,
    };

public:
    virtual ~NihilUIObject() {}
    virtual bool createVertexStream(NihilUIVertex vertices[], int size) = 0;
    virtual bool createIndexStream(int indices[], int size) = 0;
    virtual bool updateVertexStream(NihilUIVertex vertices[], int size) = 0;

public:
    void setTopology(Topology topo) { m_topology = topo; }

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
    enum ModifierTag
    {
        Mod_None,
        Mod_Translation,
        Mod_Scaling,
        Mod_Rotation,
    };

public:
    virtual ~NihilControl() {}
    virtual bool onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam) = 0; // true: stop false: continue
    void setModifierTag(ModifierTag t) { m_modTag = t; }

protected:
    ModifierTag             m_modTag = Mod_None;
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
    gs::vec3 getTranslationInCurrentView(const gs::vec2& lastpt, const gs::vec2& pt) const;

private:
    float                   m_rot1 = 0.f;           // first rotation angle, in x-z plane, rotate in y-axis
    float                   m_rot2 = 0.f;           // second rotation angle, in rot1 plane
    float                   m_cdis = 20.f;          // camera distance
    gs::vec3                m_viewOffset = gs::vec3(0.f, 0.f, 0.f);
    gs::matrix              m_model;
    gs::matrix              m_viewLookat;
    gs::matrix              m_proj;
};

// todo: add Edge - Face structures later
typedef std::vector<NihilVertex> NihilPointList;
typedef std::vector<int> NihilIndexList;

class __declspec(novtable) NihilObject abstract
{
public:
    enum ObjectType
    {
        OT_Polygon,
        OT_BiCubicBezierPatch,
        OT_BiCubicNURBS,
    };

public:
    virtual ~NihilObject() {}
    virtual ObjectType getType() const = 0;
    virtual void updateBuffers() = 0;
    NihilGeometry* getGeometry() const { return m_geometry; }
    const gs::matrix& getLocalMat() const { return m_localMat; }
    void setLocalMat(const gs::matrix& mat) { m_localMat = mat; }
    void appendTranslation(const gs::vec3& ofs);
    // bridge
    virtual void setSelected(bool b) { if (m_geometry) m_geometry->setSelected(b); }
    virtual bool isSelected() const { return m_geometry ? m_geometry->isSelected() : false; }

protected:
    NihilGeometry*          m_geometry = nullptr;
    gs::matrix              m_localMat;

protected:
    void updateLocalMat();
    int loadLocalSectionFromTextStream(const NihilString& src, int start);
};

class NihilPolygon :
    public NihilObject
{
public:
    NihilPolygon(NihilRenderer* renderer);
    virtual ~NihilPolygon();
    virtual ObjectType getType() const override { return OT_Polygon; }
    int loadPolygonFromTextStream(const NihilString& src, int start);
    NihilPointList& getPointList() { return m_pointList; }
    NihilIndexList& getIndexList() { return m_indexList; }
    virtual void updateBuffers() override;

protected:
    NihilRenderer*          m_renderer = nullptr;
    NihilPointList          m_pointList;
    NihilIndexList          m_indexList;

    friend class NihilBiCubicBezierPatch;
    friend class NihilBiCubicNURBSurface;

protected:
    void calculateNormals();
    bool setupGeometryBuffers();
    int loadPointSectionFromTextStream(const NihilString& src, int start);
    int loadFaceSectionFromTextStream(const NihilString& src, int start);
};

class NihilBiCubicBezierPatch :
    public NihilObject
{
public:
    NihilBiCubicBezierPatch(NihilRenderer* renderer);
    virtual ~NihilBiCubicBezierPatch();
    virtual ObjectType getType() const override { return OT_BiCubicBezierPatch; }
    NihilPolygon* getGridMesh() const { return m_gridMesh; }
    gs::vec3* getCvs() { return m_cvs; }
    int loadBiCubicBezierPatchFromTextStream(const NihilString& src, int start);
    virtual void updateBuffers() override;
    virtual void setSelected(bool b) override;
    virtual bool isSelected() const override;

protected:
    NihilRenderer*          m_renderer = nullptr;
    gs::vec3                m_cvs[16];
    NihilPolygon*           m_gridMesh = nullptr;
    int                     m_ustep, m_vstep;

protected:
    int loadCvsSectionFromTextStream(const NihilString& src, int start);
    void loadFinished();
    void createGridMeshIndices();
    void updateGridMeshPoints();
    void updateGridMesh();
};

class NihilBiCubicNURBSurface :
    public NihilObject
{
public:
    NihilBiCubicNURBSurface(NihilRenderer* renderer);
    virtual ~NihilBiCubicNURBSurface();
    virtual ObjectType getType() const override { return OT_BiCubicNURBS; }
    NihilPolygon* getGridMesh() const { return m_gridMesh; }
    int getUCvs() const { return (int)m_uknots.size() - 4; }
    int getVCvs() const { return (int)m_vknots.size() - 4; }
    int getUDegrees() const { return m_udegrees; }
    int getVDegrees() const { return m_vdegrees; }
    std::vector<gs::vec3>& getCvs() { return m_cvs; }
    virtual void updateBuffers() override;
    virtual void setSelected(bool b) override;
    virtual bool isSelected() const override;
    int loadBiCubicNURBSFromTextStream(const NihilString& src, int start);

protected:
    NihilRenderer*          m_renderer = nullptr;
    std::vector<float>      m_uknots;
    std::vector<float>      m_vknots;
    std::vector<gs::vec3>   m_cvs;
    NihilPolygon*           m_gridMesh = nullptr;
    int                     m_udegrees = 3, m_vdegrees = 3;  // support 3 degree only
    int                     m_ustep, m_vstep;

private:
    int loadCvsSectionFromTextStream(const NihilString& src, int start);
    void loadFinished();
    void calcPoint(gs::vec3& p, float u, float v);
    void updateGridMeshPoints();
    void createGridMeshIndices();
    void updateGridMesh();
};

typedef std::vector<NihilObject*> NihilObjectList;

class NihilUIRectangle
{
public:
    NihilUIRectangle(NihilRenderer* renderer);
    ~NihilUIRectangle();
    void setTopLeft(float left, float top);
    void setBottomRight(float right, float bottom);
    void setupBuffers();
    void updateBuffer();

protected:
    NihilRenderer*          m_renderer = nullptr;
    NihilUIObject*          m_rcObject = nullptr;
    gs::rectf               m_rc;

protected:
    void calcVertexBuffer(NihilUIVertex vertices[4]);
};

typedef std::vector<NihilUIVertex> NihilUIVertices;

struct NihilPointInfo
{
    NihilObject*            object = nullptr;
    int                     index = -1;
    gs::vec3                point;
    bool                    isSelected = false;

    NihilPointInfo(NihilObject* p, int i, const gs::vec3& pt) :
        object(p), index(i), point(pt)
    {}
};
typedef std::list<NihilPointInfo> NihilPointInfoList;
typedef std::vector<NihilPointInfo*> NihilPointInfoRefs;

class NihilUIPoints
{
public:
    NihilUIPoints(NihilRenderer* renderer);
    ~NihilUIPoints();
    void updateBuffer(NihilPointInfoList& infos);

protected:
    NihilRenderer*          m_renderer = nullptr;
    NihilUIObject*          m_rcObject = nullptr;

private:
    void initBuffers(NihilPointInfoList& infos);
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

class NihilControl_ObjectLayer :
    public NihilControl
{
protected:
    struct HittestNode
    {
        gs::vec2            triangle[3];
        int                 index[3];
        NihilGeometry*      geometry;
    };
    typedef std::list<HittestNode> HittestNodeCache;

public:
    NihilControl_ObjectLayer(NihilCore* core);
    virtual ~NihilControl_ObjectLayer();
    virtual bool onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    NihilObjectList&        m_objectList;
    NihilHittestRtree       m_rtree;
    HittestNodeCache        m_cache;
    NihilUIRectangle*       m_selectArea = nullptr;
    NihilRenderer*          m_renderer = nullptr;
    bool                    m_pressed = false;
    gs::vec2                m_startpt;
    gs::vec2                m_lastpt;
    NihilObjectList         m_selectedObjects;

private:
    void setupHittestTable(HWND hwnd, NihilSceneConfig& sceneConfig);
    void setupHittestTableOf(NihilObject* object, const gs::matrix& mat, UINT width, UINT height);
    void setupHittestTableOfPolygon(NihilPolygon* polygon, const gs::matrix& mat, UINT width, UINT height);
    void resetSelectState();
    void startSelecting();
    void endSelecting(const gs::vec2& pt);
    void updateSelecting(const gs::vec2& pt);
    void hitTest(const gs::rectf& rc);
    void startTranslation();
    void updateTranslation(NihilSceneConfig& sceneConfig, const gs::vec2& pt);
};

class NihilControl_PointsLayer :
    public NihilControl
{
protected:
    struct VertexInfo
    {
        gs::vec2            screenPos;
    };

public:
    NihilControl_PointsLayer(NihilCore* core);
    virtual ~NihilControl_PointsLayer();
    virtual bool onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    NihilObjectList         m_selectedObjects;
    NihilUIRectangle*       m_selectArea = nullptr;
    NihilRenderer*          m_renderer = nullptr;
    bool                    m_pressed = false;
    NihilPointInfoList      m_pointInfos;
    NihilUIPoints*          m_uiPoints = nullptr;
    gs::vec2                m_startpt;
    gs::vec2                m_lastpt;
    NihilPointInfoRefs      m_selectedPoints;

private:
    void setupSelectedObjects(const NihilObjectList& polygons);
    void setupHittestInfo(HWND hwnd, NihilSceneConfig& sceneConfig);
    void setupHittestInfoOf(NihilObject* object, const gs::matrix& mat, UINT width, UINT height);
    void setupHittestInfoOfPolygon(NihilPolygon* polygon, const gs::matrix& mat, UINT width, UINT height);
    void setupHittestInfoOfBiCubicBezier(NihilBiCubicBezierPatch* bezierPatch, const gs::matrix& mat, UINT width, UINT height);
    void setupHittestInfoOfBiCubicNurbs(NihilBiCubicNURBSurface* nurbs, const gs::matrix& mat, UINT width, UINT height);
    void updateUIVertices();
    void resetSelectState();
    void startSelecting();
    void endSelecting(const gs::vec2& pt);
    void updateSelecting(const gs::vec2& pt);
    void hitTest(const gs::rectf& rc);
    void startTranslation();
    void updateTranslation(NihilSceneConfig& sceneConfig, HWND hwnd, const gs::vec2& pt);
};

class NihilCore
{
    friend class NihilControl_ObjectLayer;
    friend class NihilControl_PointsLayer;

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
    void selectPoints();
    void translateModifier();
    void scaleModifier();
    void rotateModifier();
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
    NihilObjectList         m_objectList;
    NihilControl*           m_controller = nullptr;

protected:
    void destroyObjects();
    bool setupWindow(HWND hwnd);
    bool setupRenderer();
    int loadPolygonFromTextStream(const NihilString& src, int start);   // -1: failed
    int loadBiCubicBezierPatchFromTextStream(const NihilString& src, int start);
    int loadNurbsFromTextStream(const NihilString& src, int start);
};
