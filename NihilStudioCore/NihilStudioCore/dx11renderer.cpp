#include <assert.h>
#include "dx11renderer.h"

#ifdef _NIHIL_USE_DX11

// compiled shader
#include "geometry_vs.h"
#include "geometry_ps.h"
#include "ui_vs.h"
#include "ui_ps.h"

#define ASSERT assert

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { \
    if (p) { \
        p->Release(); \
        p = 0; \
    } \
}
#endif

static UINT PackCbSize(UINT cbsize)
{
    auto x = cbsize / 16;
    if (cbsize % 16)
        x ++;
    return x * 16;
}

NihilDx11Geometry::~NihilDx11Geometry()
{
    SAFE_RELEASE(m_vb);
    SAFE_RELEASE(m_ib);
    m_renderer = nullptr;
}

bool NihilDx11Geometry::createVertexStream(NihilVertex vertices[], int size)
{
    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = sizeof(NihilVertex) * size;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA data;
    ZeroMemory(&data, sizeof(data));
    data.pSysMem = vertices;

    ASSERT(m_renderer);
    ID3D11Device* device = m_renderer->getDevice();
    ASSERT(device && !m_vb);
    HRESULT hr = device->CreateBuffer(&desc, &data, &m_vb);
    if (FAILED(hr))
        return false;

    m_verticeCount = size;
    ASSERT(m_vb);
    return true;
}

bool NihilDx11Geometry::createIndexStream(int indices[], int size)
{
    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.ByteWidth = sizeof(int) * size;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA data;
    ZeroMemory(&data, sizeof(data));
    data.pSysMem = indices;

    ASSERT(m_renderer);
    ID3D11Device* device = m_renderer->getDevice();
    ASSERT(device && !m_ib);
    HRESULT hr = device->CreateBuffer(&desc, &data, &m_ib);
    if (FAILED(hr))
        return false;

    m_indicesCount = size;
    ASSERT(m_ib);
    return true;
}

bool NihilDx11Geometry::updateVertexStream(NihilVertex vertices[], int size)
{
    ASSERT(vertices);
    ASSERT(m_verticeCount == size);
    ASSERT(m_renderer);
    ID3D11DeviceContext* immContext = m_renderer->getImmediateContext();
    ASSERT(immContext);
    D3D11_MAPPED_SUBRESOURCE mappedRes;
    ZeroMemory(&mappedRes, sizeof(mappedRes));
    immContext->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
    memcpy_s(mappedRes.pData, sizeof(NihilVertex) * size, vertices, sizeof(NihilVertex) * size);
    immContext->Unmap(m_vb, 0);
    return true;
}

void NihilDx11Geometry::setLocalMat(const gs::matrix& m)
{
    m_localMat = m;
}

void NihilDx11Geometry::setupIndexVertexBuffers(NihilDx11Renderer* renderer)
{
    ASSERT(m_renderer == renderer);
    ID3D11DeviceContext* immContext = renderer->getImmediateContext();
    ASSERT(immContext);
    UINT stride = sizeof(NihilVertex);
    UINT offset = 0;
    immContext->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
    immContext->IASetIndexBuffer(m_ib, DXGI_FORMAT_R32_UINT, 0);
    immContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void NihilDx11Geometry::render(NihilDx11Renderer* renderer)
{
    ASSERT(m_renderer == renderer);
    ID3D11DeviceContext* immContext = renderer->getImmediateContext();
    ASSERT(immContext);
    immContext->DrawIndexed(m_indicesCount, 0, 0);
}

NihilDx11UIObject::~NihilDx11UIObject()
{
    SAFE_RELEASE(m_vb);
    SAFE_RELEASE(m_ib);
}

bool NihilDx11UIObject::createVertexStream(NihilUIVertex vertices[], int size)
{
    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = sizeof(NihilUIVertex) * size;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA data;
    ZeroMemory(&data, sizeof(data));
    data.pSysMem = vertices;

    ASSERT(m_renderer);
    ID3D11Device* device = m_renderer->getDevice();
    ASSERT(device && !m_vb);
    HRESULT hr = device->CreateBuffer(&desc, &data, &m_vb);
    if (FAILED(hr))
        return false;

    m_verticeCount = size;
    ASSERT(m_vb);
    return true;
}

bool NihilDx11UIObject::createIndexStream(int indices[], int size)
{
    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.ByteWidth = sizeof(int) * size;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA data;
    ZeroMemory(&data, sizeof(data));
    data.pSysMem = indices;

    ASSERT(m_renderer);
    ID3D11Device* device = m_renderer->getDevice();
    ASSERT(device && !m_ib);
    HRESULT hr = device->CreateBuffer(&desc, &data, &m_ib);
    if (FAILED(hr))
        return false;

    m_indicesCount = size;
    ASSERT(m_ib);
    return true;
}

bool NihilDx11UIObject::updateVertexStream(NihilUIVertex vertices[], int size)
{
    ASSERT(vertices);
    ASSERT(m_verticeCount == size);
    ASSERT(m_renderer);
    ID3D11DeviceContext* context = m_renderer->getImmediateContext();
    ASSERT(context);
    D3D11_MAPPED_SUBRESOURCE mappedRes;
    ZeroMemory(&mappedRes, sizeof(mappedRes));
    context->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
    memcpy_s(mappedRes.pData, sizeof(NihilUIVertex) * size, vertices, sizeof(NihilUIVertex) * size);
    context->Unmap(m_vb, 0);
    return true;
}

void NihilDx11UIObject::setupIndexVertexBuffers(NihilDx11Renderer* renderer)
{
    ASSERT(renderer);
    ID3D11DeviceContext* context = renderer->getImmediateContext();
    ASSERT(context);
    switch (m_topology)
    {
    case Topo_Points:
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        break;
    case Topo_LineList:
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        break;
    case Topo_TriangleList:
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        break;
    default:
        ASSERT(!"Unexpected topology.");
        break;
    }
    if (m_vb)
    {
        UINT stride = sizeof(NihilUIVertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
    }
    if (m_ib)
        context->IASetIndexBuffer(m_ib, DXGI_FORMAT_R32_UINT, 0);
}

void NihilDx11UIObject::render(NihilDx11Renderer* renderer)
{
    ASSERT(renderer);
    ASSERT(m_vb);
    ID3D11DeviceContext* context = renderer->getImmediateContext();
    ASSERT(context);
    if (!m_ib)
    {
        context->Draw(m_verticeCount, 0);
        return;
    }
    context->DrawIndexed(m_indicesCount, 0, 0);
}

NihilDx11Renderer::NihilDx11Renderer()
{
    m_worldMat.identity();
}

NihilDx11Renderer::~NihilDx11Renderer()
{
    destroy();
}

ID3D11Texture2D* createDepthStencilBuffer(ID3D11Device* dev, UINT width, UINT height)
{
    ASSERT(dev);
    D3D11_TEXTURE2D_DESC depthBufferDesc;
    ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));
    depthBufferDesc.Width = width;
    depthBufferDesc.Height = height;
    depthBufferDesc.MipLevels = 1;
    depthBufferDesc.ArraySize = 1;
    depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthBufferDesc.SampleDesc.Count = 4;       // todo: check?
    depthBufferDesc.SampleDesc.Quality = 0;
    depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthBufferDesc.CPUAccessFlags = 0;
    depthBufferDesc.MiscFlags = 0;
    ID3D11Texture2D* pDepthBuffer = nullptr;
    HRESULT hr = dev->CreateTexture2D(&depthBufferDesc, nullptr, &pDepthBuffer);
    if (FAILED(hr))
    {
        ASSERT(!"Create depth buffer failed.");
        return nullptr;
    }
    return pDepthBuffer;
}

bool NihilDx11Renderer::setup(HWND hwnd)
{
    HRESULT hr = S_OK;
    m_hwnd = hwnd;
    RECT rc;
    GetClientRect(hwnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    IDXGIFactory* factory = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIOutput* adapterOutput = nullptr;
    UINT numerator, denominator, modes;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory)) ||
        FAILED(factory->EnumAdapters(0, &adapter)) ||
        FAILED(adapter->EnumOutputs(0, &adapterOutput)) ||
        FAILED(adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &modes, 0))
        )
        return false;
    DXGI_MODE_DESC* displayModes = new DXGI_MODE_DESC[modes];
    ASSERT(displayModes);
    if (FAILED(adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &modes, displayModes)))
    {
        delete[] displayModes;
        return false;
    }
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    for (UINT i = 0; i < modes; i++)
    {
        if ((displayModes[i].Width == (UINT)screenWidth) &&
            (displayModes[i].Height = (UINT)screenHeight)
            )
        {
            numerator = displayModes[i].RefreshRate.Numerator;
            denominator = displayModes[i].RefreshRate.Denominator;
            break;
        }
    }
    delete[] displayModes;

    // create device & immediateContext
    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
        m_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDevice(nullptr, m_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
            D3D11_SDK_VERSION, &m_device, &m_featureLevel, &m_immediateContext
            );
        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return false;
    ASSERT(m_device && m_immediateContext);

    // check MSAA sampler counts
    UINT samplerCount, samplerQuality = 0;
    static const UINT ExpectedSampleCount[] = { 4, 2, 1 };
    bool bMSAA_Available = false;
    for (int i = 0; i < _countof(ExpectedSampleCount); i++)
    {
        samplerCount = ExpectedSampleCount[i];
        hr = m_device->CheckMultisampleQualityLevels(DXGI_FORMAT_B8G8R8A8_UNORM, samplerCount, &samplerQuality);
        if (samplerQuality != 0)
        {
            bMSAA_Available = true;
            break;
        }
    }
    if (!bMSAA_Available)
    {
        samplerCount = 1;
        samplerQuality = 0;
    }
    else
    {
        samplerQuality--;
        ASSERT(samplerQuality >= 0);
    }

    // create SwapChain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
    swapChainDesc.BufferCount = 1;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SampleDesc.Count = samplerCount;
    swapChainDesc.SampleDesc.Quality = samplerQuality;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.Flags = 0;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc;
    ZeroMemory(&fullScreenDesc, sizeof(fullScreenDesc));
    fullScreenDesc.RefreshRate.Numerator = numerator;
    fullScreenDesc.RefreshRate.Denominator = denominator;
    fullScreenDesc.Windowed = TRUE;

    IDXGIFactory2* factory2 = nullptr;
    factory->QueryInterface(__uuidof(IDXGIFactory2), (void**)&factory2);
    if (!factory2)
        return false;
    hr = factory2->CreateSwapChainForHwnd(m_device, hwnd, &swapChainDesc, &fullScreenDesc, nullptr, &m_swapChain);
    factory2->Release();
    if (FAILED(hr))
        return false;

    ID3D11Texture2D* pBackBuffer = NULL;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr))
        return false;

    hr = m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_rtv);
    pBackBuffer->Release();
    if (FAILED(hr))
        return false;

    // Create depth-stencil view
    ID3D11Texture2D* pDepthBuffer = createDepthStencilBuffer(m_device, width, height);
    if (!pDepthBuffer)
        return false;

    hr = m_device->CreateDepthStencilView(pDepthBuffer, nullptr, &m_dsv);
    if (FAILED(hr))
    {
        ASSERT(!"Create depth stencil view failed.");
        return false;
    }
    pDepthBuffer->Release();

    setupViewpoints(width, height);
    setupScreenMatrix(width, height);
    return setupShaderOfGeometry() && setupShaderOfUI();
}

void NihilDx11Renderer::render()
{
    beginRender();
    renderGeometryBatch();
    renderUIBatch();
    endRender();
}

void NihilDx11Renderer::notifyResize()
{
    m_immediateContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_immediateContext->Flush();
    SAFE_RELEASE(m_rtv);
    SAFE_RELEASE(m_dsv);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    // resize swap chain & buffers
    HRESULT hr = m_swapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr))
    {
        ASSERT(!"Resize swap chain buffers failed.");
        return;
    }
    ID3D11Texture2D* pBackBuffer = NULL;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_rtv);
    pBackBuffer->Release();

    ID3D11Texture2D* pDepthBuffer = createDepthStencilBuffer(m_device, width, height);
    ASSERT(pDepthBuffer);
    m_device->CreateDepthStencilView(pDepthBuffer, nullptr, &m_dsv);
    pDepthBuffer->Release();

    m_immediateContext->OMSetRenderTargets(1, &m_rtv, m_dsv);

    setupViewpoints(width, height);
    setupScreenMatrix(width, height);
}

void NihilDx11Renderer::setWorldMat(const gs::matrix& m)
{
    m_worldMat = m;
}

NihilGeometry* NihilDx11Renderer::addGeometry()
{
    auto* p = new NihilDx11Geometry(this);
    ASSERT(p);
    m_geometrySet.insert(p);
    return p;
}

NihilUIObject* NihilDx11Renderer::addUIObject()
{
    auto* p = new NihilDx11UIObject(this);
    ASSERT(p);
    m_uiSet.insert(p);
    return p;
}

void NihilDx11Renderer::removeGeometry(NihilGeometry* ptr)
{
    ASSERT(ptr);
    m_geometrySet.erase(ptr);
    delete ptr;
}

void NihilDx11Renderer::removeUIObject(NihilUIObject* ptr)
{
    ASSERT(ptr);
    m_uiSet.erase(ptr);
    delete ptr;
}

void NihilDx11Renderer::showSolidMode()
{
    ASSERT(m_device && m_immediateContext);
    D3D11_RASTERIZER_DESC rd;
    ZeroMemory(&rd, sizeof(rd));
    rd.CullMode = D3D11_CULL_BACK;
    rd.FillMode = D3D11_FILL_SOLID;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable = TRUE;
    rd.MultisampleEnable = TRUE;
    rd.AntialiasedLineEnable = TRUE;
    ID3D11RasterizerState* pRasterState = nullptr;
    m_device->CreateRasterizerState(&rd, &pRasterState);
    m_immediateContext->RSSetState(pRasterState);
    pRasterState->Release();
}

void NihilDx11Renderer::showWireframeMode()
{
    ASSERT(m_device && m_immediateContext);
    D3D11_RASTERIZER_DESC rd;
    ZeroMemory(&rd, sizeof(rd));
    rd.CullMode = D3D11_CULL_NONE;
    rd.FillMode = D3D11_FILL_WIREFRAME;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable = TRUE;
    rd.MultisampleEnable = TRUE;
    rd.AntialiasedLineEnable = TRUE;
    ID3D11RasterizerState* pRasterState = nullptr;
    m_device->CreateRasterizerState(&rd, &pRasterState);
    m_immediateContext->RSSetState(pRasterState);
    pRasterState->Release();
}

void NihilDx11Renderer::destroy()
{
    clearRenderSets();
    if (m_immediateContext)
        m_immediateContext->ClearState();
    SAFE_RELEASE(m_uiInputLayout);
    SAFE_RELEASE(m_uiPS);
    SAFE_RELEASE(m_uiVS);
    SAFE_RELEASE(m_uiCB);
    SAFE_RELEASE(m_geometryInputLayout);
    SAFE_RELEASE(m_geometryCB);
    SAFE_RELEASE(m_geometryPS);
    SAFE_RELEASE(m_geometryVS);
    SAFE_RELEASE(m_rtv);
    SAFE_RELEASE(m_dsv);
    SAFE_RELEASE(m_swapChain);
    SAFE_RELEASE(m_immediateContext);
#ifdef _DEBUG
    ID3D11Debug* pDebug = nullptr;
    HRESULT hr = m_device->QueryInterface(__uuidof(ID3D11Debug), (void**)&pDebug);
    if (SUCCEEDED(hr) && pDebug)
        pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    SAFE_RELEASE(pDebug);
#endif
    SAFE_RELEASE(m_device);
}

void NihilDx11Renderer::setupViewpoints(UINT width, UINT height)
{
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    ASSERT(m_immediateContext);
    m_immediateContext->RSSetViewports(1, &vp);
}

void NihilDx11Renderer::setupScreenMatrix(UINT width, UINT height)
{
    m_screenMat.multiply(gs::matrix().scaling(2.f / width, -2.f / height, 0.f), gs::matrix().translation(-1.f, 1.f, 0.f));
}

bool NihilDx11Renderer::setupShaderOfGeometry()
{
    ASSERT(m_device);
    HRESULT hr = m_device->CreateVertexShader(g_VS, sizeof(g_VS), nullptr, &m_geometryVS);
    if (FAILED(hr))
        return false;
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);
    hr = m_device->CreateInputLayout(layout, numElements, g_VS, sizeof(g_VS), &m_geometryInputLayout);
    if (FAILED(hr))
        return false;
    hr = m_device->CreatePixelShader(g_PS, sizeof(g_PS), nullptr, &m_geometryPS);
    if (FAILED(hr))
        return false;
    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = PackCbSize(sizeof(GeometryCB));
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_geometryCB);
    return SUCCEEDED(hr);
}

bool NihilDx11Renderer::setupShaderOfUI()
{
    ASSERT(m_device);
    HRESULT hr = m_device->CreateVertexShader(g_UIVS, sizeof(g_UIVS), nullptr, &m_uiVS);
    if (FAILED(hr))
        return false;
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);
    hr = m_device->CreateInputLayout(layout, numElements, g_UIVS, sizeof(g_UIVS), &m_uiInputLayout);
    if (FAILED(hr))
        return false;
    hr = m_device->CreatePixelShader(g_UIPS, sizeof(g_UIPS), nullptr, &m_uiPS);
    if (FAILED(hr))
        return false;
    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = PackCbSize(sizeof(UICB));
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateBuffer(&desc, nullptr, &m_uiCB);
    return SUCCEEDED(hr);
}

void NihilDx11Renderer::clearRenderSets()
{
    for (auto* p : m_geometrySet)
        delete p;
    for (auto * p : m_uiSet)
        delete p;
    m_geometrySet.clear();
    m_uiSet.clear();
}

void NihilDx11Renderer::beginRender()
{
    ASSERT(m_immediateContext);
    m_immediateContext->OMSetRenderTargets(1, &m_rtv, m_dsv);

    static float ccr[] = { 0.275f, 0.31f, 0.349f, 1.f };
    m_immediateContext->ClearRenderTargetView(m_rtv, ccr);
    m_immediateContext->ClearDepthStencilView(m_dsv, D3D11_CLEAR_DEPTH, 1.f, 0);
}

void NihilDx11Renderer::endRender()
{
    ASSERT(m_swapChain);
    m_swapChain->Present(0, 0);
}

void NihilDx11Renderer::renderGeometryBatch()
{
    ASSERT(m_immediateContext);
    m_immediateContext->IASetInputLayout(m_geometryInputLayout);
    m_immediateContext->VSSetShader(m_geometryVS, nullptr, 0);
    m_immediateContext->PSSetShader(m_geometryPS, nullptr, 0);
    for (auto* p : m_geometrySet)
    {
        ASSERT(p);
        auto* pGeometry = static_cast<NihilDx11Geometry*>(p);
        pGeometry->setupIndexVertexBuffers(this);
        // setup constant buffer
        setupConstantBufferForGeometry(pGeometry);
        pGeometry->render(this);
    }
}

void NihilDx11Renderer::renderUIBatch()
{
    ASSERT(m_immediateContext);
    m_immediateContext->IASetInputLayout(m_uiInputLayout);
    m_immediateContext->VSSetShader(m_uiVS, nullptr, 0);
    m_immediateContext->PSSetShader(m_uiPS, nullptr, 0);
    setupUIConstantBuffer();
    for (auto * p : m_uiSet)
    {
        ASSERT(p);
        auto* pUI = static_cast<NihilDx11UIObject*>(p);
        pUI->setupIndexVertexBuffers(this);
        pUI->render(this);
    }
}

void NihilDx11Renderer::setupConstantBufferForGeometry(NihilDx11Geometry* geometry)
{
    ASSERT(geometry);
    gs::matrix m;
    m.multiply(geometry->getLocalMat(), m_worldMat);
    m.transpose();
    D3D11_MAPPED_SUBRESOURCE mappedRes;
    ZeroMemory(&mappedRes, sizeof(mappedRes));
    m_immediateContext->Map(m_geometryCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
    auto* data = (GeometryCB*)mappedRes.pData;
    data->mvp = m;
    if (geometry->isSelected())
        data->diffuseColor = gs::vec3(0.6f, 0.6f, 0.5f);
    else
        data->diffuseColor = gs::vec3(0.6f, 0.f, 0.f);
    m_immediateContext->Unmap(m_geometryCB, 0);
    m_immediateContext->VSSetConstantBuffers(0, 1, &m_geometryCB);
}

void NihilDx11Renderer::setupUIConstantBuffer()
{
    gs::matrix m = m_screenMat;
    m.transpose();
    D3D11_MAPPED_SUBRESOURCE mappedRes;
    ZeroMemory(&mappedRes, sizeof(mappedRes));
    m_immediateContext->Map(m_uiCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
    auto* data = (UICB*)mappedRes.pData;
    data->mat = m;
    m_immediateContext->Unmap(m_uiCB, 0);
    m_immediateContext->VSSetConstantBuffers(0, 1, &m_uiCB);
}

#endif
