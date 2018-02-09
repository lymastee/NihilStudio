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
class NihilCore;

class __declspec(novtable) NihilGeometry abstract
{
public:
    virtual ~NihilGeometry() {}
    virtual bool createVertexStream(NihilVertex vertices[], int size) = 0;
    virtual bool createIndexStream(int indices[], int size) = 0;
    virtual bool updateVertexStream(NihilVertex vertices[], int size) = 0;
    virtual void setLocalMat(const gs::matrix& m) = 0;
};

class __declspec(novtable) NihilUIObject abstract
{
public:
    virtual ~NihilUIObject() {}
    virtual bool createVertexStream(NihilUIVertex vertices[], int size) = 0;
    virtual bool createIndexStream(int indices[], int size) = 0;
    virtual bool updateVertexStream(NihilUIVertex vertices[], int size) = 0;
    //virtual void setLocalMat(const gs::) = 0;
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

// todo: add Edge - Face structures later
typedef std::vector<gs::vec3> NihilPointList;
typedef std::vector<int> NihilIndexList;
// if the (index + 1) was negative, then the linkage direction was inversed
typedef struct { std::vector<int> linkedIndices; } NihilLinkedPoints;
typedef std::unordered_map<int, NihilLinkedPoints> NihilPointLinkage;

class NihilSceneConfig
{
public:
    float                   m_rot1 = 0.f;           // first rotation angle, in x-z plane, rotate in y-axis
    float                   m_rot2 = 0.f;           // second rotation angle, in rot1 plane
    float                   m_cdis = 4.f;           // camera distance
    gs::matrix              m_model;
    gs::matrix              m_view;
    gs::matrix              m_proj;

public:
    void setup(HWND hwnd);
    void updateViewMatrix();
    void calcMatrix(gs::matrix& mat);
    void updateRotation(const gs::vec2& lastpt, const gs::vec2& pt);
    void updateScaling(float d);
};

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
    gs::matrix              m_localMat;

protected:
    bool setupGeometryBuffers();
    void calculateNormal(gs::vec3& normal, int i);
    int loadPointSectionFromTextStream(const NihilString& src, int start);
    int loadFaceSectionFromTextStream(const NihilString& src, int start);
    int loadLocalSectionFromTextStream(const NihilString& src, int start);
};

typedef std::vector<NihilPolygon*> NihilPolygonList;

// controllers
class NihilControl_RotateScene:
    public NihilControl
{
public:
    NihilControl_RotateScene();
    virtual ~NihilControl_RotateScene();
    virtual bool onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    bool                    m_pressed = false;
    gs::vec2                m_lastpt;
};

class NihilControl_ScaleScene:
    public NihilControl
{
public:
    virtual bool onMsg(NihilCore* core, UINT message, WPARAM wParam, LPARAM lParam) override;
};

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
    void showSolidMode();
    void showWireframeMode();
    void destroyController();
    void rotateScene();
    void scaleScene();
    NihilRenderer* getRenderer() const { return m_renderer; }
    NihilControl* getController() const { return m_controller; }
    NihilSceneConfig& getSceneConfig() { return m_sceneConfig; }
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
    NihilSceneConfig        m_sceneConfig;
    NihilPolygonList        m_polygonList;
    NihilControl*           m_controller = nullptr;

protected:
    void destroyObjects();
    bool setupWindow(HWND hwnd);
    bool setupRenderer();
    int loadPolygonFromTextStream(const NihilString& src, int start);   // -1: failed
};
