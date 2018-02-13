#pragma once

#include "core.h"

#ifdef _NIHIL_USE_DX11

#include <d3d11.h>
#include <dxgi1_2.h>
#include <unordered_set>

class NihilDx11Geometry :
    public NihilGeometry
{
    friend class NihilDx11Renderer;

public:
    NihilDx11Geometry(NihilDx11Renderer* renderer): m_renderer(renderer) {}
    virtual ~NihilDx11Geometry();
    virtual bool createVertexStream(NihilVertex vertices[], int size) override;
    virtual bool createIndexStream(int indices[], int size) override;
    virtual bool updateVertexStream(NihilVertex vertices[], int size) override;
    virtual void setLocalMat(const gs::matrix& m) override;

public:
    const gs::matrix& getLocalMat() const { return m_localMat; }
    void setupIndexVertexBuffers(NihilDx11Renderer* renderer);
    void render(NihilDx11Renderer* renderer);

private:
    NihilDx11Renderer*          m_renderer = nullptr;
    ID3D11Buffer*               m_vb = nullptr;
    ID3D11Buffer*               m_ib = nullptr;
    int                         m_verticeCount = 0;
    int                         m_indicesCount = 0;
    gs::matrix                  m_localMat;
};

class NihilDx11UIObject :
    public NihilUIObject
{
    friend class NihilDx11Renderer;

public:
    NihilDx11UIObject(NihilDx11Renderer* renderer): m_renderer(renderer) {}
    virtual ~NihilDx11UIObject();
    virtual bool createVertexStream(NihilUIVertex vertices[], int size) override;
    virtual bool createIndexStream(int indices[], int size) override;
    virtual bool updateVertexStream(NihilUIVertex vertices[], int size) override;

public:
    void setupIndexVertexBuffers(NihilDx11Renderer* renderer);
    void render(NihilDx11Renderer* renderer);

private:
    NihilDx11Renderer*          m_renderer = nullptr;
    ID3D11Buffer*               m_vb = nullptr;
    ID3D11Buffer*               m_ib = nullptr;
    int                         m_verticeCount = 0;
    int                         m_indicesCount = 0;
};

typedef std::unordered_set<NihilGeometry*> NihilGeometrySet;
typedef std::unordered_set<NihilUIObject*> NihilUIObjectSet;

class NihilDx11Renderer :
    public NihilRenderer
{
public:
    struct GeometryCB
    {
        gs::matrix              mvp;
        gs::vec3                diffuseColor;
    };

    struct UICB
    {
        gs::matrix              mat;
    };

public:
    NihilDx11Renderer();
    virtual ~NihilDx11Renderer();

public:
    virtual bool setup(HWND hwnd) override;
    virtual void render() override;
    virtual void notifyResize() override;
    virtual void setWorldMat(const gs::matrix& m) override;
    virtual NihilGeometry* addGeometry() override;
    virtual NihilUIObject* addUIObject() override;
    virtual void removeGeometry(NihilGeometry* ptr) override;
    virtual void removeUIObject(NihilUIObject* ptr) override;
    virtual void showSolidMode() override;
    virtual void showWireframeMode() override;

public:
    ID3D11Device* getDevice() const { return m_device; }
    ID3D11DeviceContext* getImmediateContext() const { return m_immediateContext; }

protected:
    HWND                        m_hwnd = 0;
    D3D_DRIVER_TYPE             m_driverType = D3D_DRIVER_TYPE_NULL;
    D3D_FEATURE_LEVEL           m_featureLevel = D3D_FEATURE_LEVEL_11_0;
    ID3D11Device*               m_device = nullptr;
    ID3D11DeviceContext*        m_immediateContext = nullptr;
    IDXGISwapChain1*            m_swapChain = nullptr;
    ID3D11RenderTargetView*     m_rtv = nullptr;
    ID3D11DepthStencilView*     m_dsv = nullptr;
    ID3D11VertexShader*         m_geometryVS = nullptr;
    ID3D11PixelShader*          m_geometryPS = nullptr;
    ID3D11InputLayout*          m_geometryInputLayout = nullptr;
    ID3D11Buffer*               m_geometryCB = nullptr;
    ID3D11VertexShader*         m_uiVS = nullptr;
    ID3D11PixelShader*          m_uiPS = nullptr;
    ID3D11InputLayout*          m_uiInputLayout = nullptr;
    ID3D11Buffer*               m_uiCB = nullptr;
    NihilGeometrySet            m_geometrySet;
    NihilUIObjectSet            m_uiSet;
    gs::matrix                  m_worldMat;
    gs::matrix                  m_screenMat;

private:
    void destroy();
    void clearRenderSets();
    void setupViewpoints(UINT width, UINT height);
    void setupScreenMatrix(UINT width, UINT height);
    bool setupShaderOfGeometry();
    bool setupShaderOfUI();
    void beginRender();
    void endRender();
    void renderGeometryBatch();
    void renderUIBatch();
    void setupUIConstantBuffer();
    void setupConstantBufferForGeometry(NihilDx11Geometry* geometry);
};


#endif
