#include "AppFramework.h"
#include "Render.h"
#include <d3dcompiler.h>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

// Формат одной вершины куба
struct CubeVertex
{
    float x, y, z;
    COLORREF rgba;
};

// Содержимое CB для мировой матрицы
struct CBWorld
{
    XMMATRIX worldMatrix;
};

// Содержимое CB для матрицы камеры
struct CBViewProjection
{
    XMMATRIX vpMatrix;
};

// Утилита: присваивает D3D-объекту отладочное имя (видно в RenderDoc и D3D debug output)
template<typename T>
inline void SetObjectLabel(ComPtr<T>& obj, const char* label)
{
    if (obj)
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen(label)), label);
}

Render::Render()
    : m_camPosition(0.0f, 0.0f, -5.0f)
    , m_camYaw(0.0f)
    , m_camPitch(0.0f)
    , m_cubeAngle(0.0f)
    , m_hWnd(nullptr)
{
}

Render::~Render()
{
    Shutdown();
}

HRESULT Render::Initialize(HWND hWnd)
{
    m_hWnd = hWnd;

    HRESULT hr = InitDeviceAndSwapChain(hWnd);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось инициализировать устройство D3D11\n");
        return hr;
    }

    RECT clientArea;
    GetClientRect(hWnd, &clientArea);
    UINT w = clientArea.right - clientArea.left;
    UINT h = clientArea.bottom - clientArea.top;

    hr = CreateDepthBuffer(w, h);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось создать буфер глубины\n");
        return hr;
    }

    hr = InitGeometryBuffers();
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось создать геометрические буферы\n");
        return hr;
    }

    hr = CompileAndCreateShaders();
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось скомпилировать шейдеры\n");
        return hr;
    }

    LabelD3DResources();

    return S_OK;
}

void Render::Shutdown()
{
    if (m_deviceCtx)
        m_deviceCtx->ClearState();
}

HRESULT Render::InitDeviceAndSwapChain(HWND hWnd)
{
    // Получаем DXGI-фабрику для перечисления адаптеров
    ComPtr<IDXGIFactory> dxgiFactory;
    HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), &dxgiFactory);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] CreateDXGIFactory не удался\n");
        return hr;
    }

    // Ищем первый аппаратный адаптер (пропускаем программный рендерер)
    ComPtr<IDXGIAdapter> selectedAdapter;
    UINT idx = 0;
    while (SUCCEEDED(dxgiFactory->EnumAdapters(idx, &selectedAdapter)))
    {
        DXGI_ADAPTER_DESC adapterInfo;
        selectedAdapter->GetDesc(&adapterInfo);

        if (wcscmp(adapterInfo.Description, L"Microsoft Basic Render Driver") != 0)
            break;

        selectedAdapter.Reset();
        ++idx;
    }

    // В Debug-конфигурации включаем отладочный слой D3D11
    UINT deviceFlags = 0;
#if defined(_DEBUG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL requestedLevel[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL obtainedLevel;

    hr = D3D11CreateDevice(
        selectedAdapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        deviceFlags,
        requestedLevel,
        1,
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        &obtainedLevel,
        &m_deviceCtx
    );

    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] D3D11CreateDevice не удался\n");
        return hr;
    }

    // Описание swap chain
    DXGI_SWAP_CHAIN_DESC swapDesc = {};
    swapDesc.BufferCount = 2;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = hWnd;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.Windowed = TRUE;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    hr = dxgiFactory->CreateSwapChain(m_d3dDevice.Get(), &swapDesc, &m_pSwapChain);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] CreateSwapChain не удался\n");
        return hr;
    }

    hr = CreateBackBufferRTV();
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось создать RTV для back buffer\n");
        return hr;
    }

    // Запрашиваем интерфейс аннотаций для пометок в RenderDoc
    hr = m_deviceCtx->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), &m_debugAnnotation);
    if (FAILED(hr))
    {
        OutputDebugString(L"[WARN] ID3DUserDefinedAnnotation недоступен\n");
        m_debugAnnotation = nullptr;
    }

    return S_OK;
}

HRESULT Render::CreateBackBufferRTV()
{
    ComPtr<ID3D11Texture2D> backBuf;
    HRESULT hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuf);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] GetBuffer для back buffer не удался\n");
        return hr;
    }

    hr = m_d3dDevice->CreateRenderTargetView(backBuf.Get(), nullptr, &m_backBufferRTV);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] CreateRenderTargetView не удался\n");
        return hr;
    }

    return S_OK;
}

HRESULT Render::CreateDepthBuffer(UINT width, UINT height)
{
    // Текстура глубины
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = m_d3dDevice->CreateTexture2D(&texDesc, nullptr, &m_depthTex);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось создать текстуру глубины\n");
        return hr;
    }

    // Depth Stencil View
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = texDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = m_d3dDevice->CreateDepthStencilView(m_depthTex.Get(), &dsvDesc, &m_depthDSV);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось создать DSV\n");
        return hr;
    }

    // Привязка render target + depth
    m_deviceCtx->OMSetRenderTargets(1, m_backBufferRTV.GetAddressOf(), m_depthDSV.Get());

    // Настройка области вывода
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    m_deviceCtx->RSSetViewports(1, &viewport);

    return S_OK;
}

HRESULT Render::InitGeometryBuffers()
{
    // 8 вершин куба с разными цветами
    static const CubeVertex vertices[] =
    {
        { -1.0f,  1.0f, -1.0f, RGB(255, 20, 147) },
        {  1.0f,  1.0f, -1.0f, RGB(0, 255, 127) },
        {  1.0f,  1.0f,  1.0f, RGB(138, 43, 226) },
        { -1.0f,  1.0f,  1.0f, RGB(255, 215, 0) },
        { -1.0f, -1.0f, -1.0f, RGB(255, 69, 0) },
        {  1.0f, -1.0f, -1.0f, RGB(0, 255, 255) },
        {  1.0f, -1.0f,  1.0f, RGB(186, 85, 211) },
        { -1.0f, -1.0f,  1.0f, RGB(50, 205, 50) }
    };

    // 36 индексов — 12 треугольников на 6 граней
    WORD indices[] =
    {
        3,1,0, 2,1,3,
        0,5,4, 1,5,0,
        3,4,7, 0,4,3,
        1,6,5, 2,6,1,
        2,7,6, 3,7,2,
        6,4,5, 7,4,6,
    };

    // Vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbInit = {};
    vbInit.pSysMem = vertices;

    HRESULT hr = m_d3dDevice->CreateBuffer(&vbDesc, &vbInit, &m_cubeVB);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось создать vertex buffer\n");
        return hr;
    }

    // Index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibInit = {};
    ibInit.pSysMem = indices;

    hr = m_d3dDevice->CreateBuffer(&ibDesc, &ibInit, &m_cubeIB);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось создать index buffer\n");
        return hr;
    }

    // CB для мировой матрицы (обновляется через UpdateSubresource)
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CBWorld);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = m_d3dDevice->CreateBuffer(&cbDesc, nullptr, &m_cbWorld);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось создать CB world\n");
        return hr;
    }

    // CB для view-projection (обновляется через Map/Unmap)
    cbDesc.ByteWidth = sizeof(CBViewProjection);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_d3dDevice->CreateBuffer(&cbDesc, nullptr, &m_cbViewProj);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Не удалось создать CB view-projection\n");
        return hr;
    }

    return S_OK;
}

HRESULT Render::CompileAndCreateShaders()
{
    // В Debug дополнительно включаем отладочную информацию в шейдерах
    UINT shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    shaderFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vsCode;
    ComPtr<ID3DBlob> compileErrors;

    // --- Vertex Shader ---
    HRESULT hr = D3DCompileFromFile(
        L"VertexShader.vs",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "vs_5_0",
        shaderFlags,
        0,
        &vsCode,
        &compileErrors
    );
    if (FAILED(hr))
    {
        if (compileErrors)
            OutputDebugStringA(static_cast<const char*>(compileErrors->GetBufferPointer()));
        OutputDebugString(L"[ERROR] Компиляция vertex shader не удалась\n");
        return hr;
    }

    hr = m_d3dDevice->CreateVertexShader(
        vsCode->GetBufferPointer(),
        vsCode->GetBufferSize(),
        nullptr,
        &m_mainVS
    );
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] CreateVertexShader не удался\n");
        return hr;
    }

    // --- Pixel Shader ---
    ComPtr<ID3DBlob> psCode;
    hr = D3DCompileFromFile(
        L"PixelShader.ps",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_5_0",
        shaderFlags,
        0,
        &psCode,
        &compileErrors
    );
    if (FAILED(hr))
    {
        if (compileErrors)
            OutputDebugStringA(static_cast<const char*>(compileErrors->GetBufferPointer()));
        OutputDebugString(L"[ERROR] Компиляция pixel shader не удалась\n");
        return hr;
    }

    hr = m_d3dDevice->CreatePixelShader(
        psCode->GetBufferPointer(),
        psCode->GetBufferSize(),
        nullptr,
        &m_mainPS
    );
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] CreatePixelShader не удался\n");
        return hr;
    }

    // --- Input Layout ---
    D3D11_INPUT_ELEMENT_DESC inputDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = m_d3dDevice->CreateInputLayout(
        inputDesc,
        2,
        vsCode->GetBufferPointer(),
        vsCode->GetBufferSize(),
        &m_vertexLayout
    );
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] CreateInputLayout не удался\n");
        return hr;
    }

    return S_OK;
}

void Render::LabelD3DResources()
{
#if defined(_DEBUG)
    SetObjectLabel(m_d3dDevice, "D3D Device");
    SetObjectLabel(m_deviceCtx, "Immediate Context");
    SetObjectLabel(m_pSwapChain, "Primary SwapChain");
    SetObjectLabel(m_backBufferRTV, "BackBuffer RTV");
    SetObjectLabel(m_depthDSV, "Depth DSV");
    SetObjectLabel(m_cubeVB, "Cube VB");
    SetObjectLabel(m_cubeIB, "Cube IB");
    SetObjectLabel(m_cbWorld, "CB World");
    SetObjectLabel(m_cbViewProj, "CB ViewProj");
    SetObjectLabel(m_mainVS, "Main VS");
    SetObjectLabel(m_mainPS, "Main PS");
#endif
}

void Render::RenderFrame()
{
    // Маркер начала кадра
    if (m_debugAnnotation)
        m_debugAnnotation->BeginEvent(L"Frame Render");

    // Заливка фона
    float bgColor[4] = { 0.1f, 0.05f, 0.2f, 1.0f };
    m_deviceCtx->ClearRenderTargetView(m_backBufferRTV.Get(), bgColor);
    m_deviceCtx->ClearDepthStencilView(m_depthDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    UpdateMatrices();

    // Сборка графического конвейера
    UINT vertexStride = sizeof(CubeVertex);
    UINT vertexOffset = 0;
    m_deviceCtx->IASetVertexBuffers(0, 1, m_cubeVB.GetAddressOf(), &vertexStride, &vertexOffset);
    m_deviceCtx->IASetIndexBuffer(m_cubeIB.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_deviceCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_deviceCtx->IASetInputLayout(m_vertexLayout.Get());

    m_deviceCtx->VSSetShader(m_mainVS.Get(), nullptr, 0);
    m_deviceCtx->PSSetShader(m_mainPS.Get(), nullptr, 0);

    // Отрисовка куба с меткой
    if (m_debugAnnotation)
        m_debugAnnotation->BeginEvent(L"Draw Cube");

    m_deviceCtx->DrawIndexed(36, 0, 0);



    if (m_debugAnnotation)
        m_debugAnnotation->EndEvent();

    // Конец кадра
    if (m_debugAnnotation)
        m_debugAnnotation->EndEvent();

    m_pSwapChain->Present(1, 0);
}

void Render::UpdateMatrices()
{
    // Вращение куба вокруг оси Y
    m_cubeAngle += 0.005f;
    if (m_cubeAngle > XM_2PI)
        m_cubeAngle -= XM_2PI;

    XMMATRIX worldMtx = XMMatrixRotationY(m_cubeAngle);
    XMMATRIX worldT = XMMatrixTranspose(worldMtx);
    m_deviceCtx->UpdateSubresource(m_cbWorld.Get(), 0, nullptr, &worldT, 0, 0);

    // Направление взгляда камеры из углов yaw/pitch
    XMVECTOR lookDir = XMVectorSet(
        cosf(m_camPitch) * sinf(m_camYaw),
        sinf(m_camPitch),
        cosf(m_camPitch) * cosf(m_camYaw),
        0.0f
    );

    XMVECTOR eyePos = XMLoadFloat3(&m_camPosition);
    XMVECTOR target = XMVectorAdd(eyePos, lookDir);
    XMVECTOR upDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX viewMtx = XMMatrixLookAtLH(eyePos, target, upDir);

    // Перспективная проекция
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    float aspectRatio = static_cast<float>(rc.right - rc.left) / static_cast<float>(rc.bottom - rc.top);
    XMMATRIX projMtx = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 100.0f);

    XMMATRIX vpMtx = viewMtx * projMtx;
    XMMATRIX vpT = XMMatrixTranspose(vpMtx);

    // Запись view-projection через Map/Unmap
    D3D11_MAPPED_SUBRESOURCE mapData;
    HRESULT hr = m_deviceCtx->Map(m_cbViewProj.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapData);
    if (SUCCEEDED(hr))
    {
        memcpy(mapData.pData, &vpT, sizeof(XMMATRIX));
        m_deviceCtx->Unmap(m_cbViewProj.Get(), 0);
    }

    m_deviceCtx->VSSetConstantBuffers(0, 1, m_cbWorld.GetAddressOf());
    m_deviceCtx->VSSetConstantBuffers(1, 1, m_cbViewProj.GetAddressOf());
}

void Render::OnWindowResize(HWND hWnd)
{
    if (!m_pSwapChain)
        return;

    // Отвязываем текущие render targets перед пересозданием
    m_deviceCtx->OMSetRenderTargets(0, nullptr, nullptr);
    m_backBufferRTV.Reset();
    m_depthDSV.Reset();
    m_depthTex.Reset();

    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT newW = rc.right - rc.left;
    UINT newH = rc.bottom - rc.top;

    // Перестраиваем буферы swap chain под новый размер окна
    HRESULT hr = m_pSwapChain->ResizeBuffers(0, newW, newH, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] ResizeBuffers не удался\n");
        return;
    }

    hr = CreateBackBufferRTV();
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Пересоздание RTV не удалось\n");
        return;
    }

    hr = CreateDepthBuffer(newW, newH);
    if (FAILED(hr))
    {
        OutputDebugString(L"[ERROR] Пересоздание буфера глубины не удалось\n");
        return;
    }
}

void Render::MoveCamera(float dx, float dy, float dz)
{
    m_camPosition.x += dx;
    m_camPosition.y += dy;
    m_camPosition.z += dz;
}

void Render::RotateCamera(float deltaYaw, float deltaPitch)
{
    m_camYaw += deltaYaw;
    m_camPitch += deltaPitch;

    if (m_camYaw > XM_2PI)   m_camYaw -= XM_2PI;
    if (m_camYaw < -XM_2PI)  m_camYaw += XM_2PI;

    if (m_camPitch > XM_PIDIV2)  m_camPitch = XM_PIDIV2;
    if (m_camPitch < -XM_PIDIV2) m_camPitch = -XM_PIDIV2;
}