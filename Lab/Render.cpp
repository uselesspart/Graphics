#include "AppFramework.h"
#include "Render.h"
#include <d3dcompiler.h>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

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

    InitDeviceAndSwapChain(hWnd);

    RECT clientArea;
    GetClientRect(hWnd, &clientArea);
    UINT w = clientArea.right - clientArea.left;
    UINT h = clientArea.bottom - clientArea.top;

    CreateDepthBuffer(w, h);
    InitGeometryBuffers();
    CompileAndCreateShaders();

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
    CreateDXGIFactory(__uuidof(IDXGIFactory), &dxgiFactory);

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

    // Создание устройства
    D3D_FEATURE_LEVEL requestedLevel[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL obtainedLevel;

    D3D11CreateDevice(
        selectedAdapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        0,
        requestedLevel,
        1,
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        &obtainedLevel,
        &m_deviceCtx
    );

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

    dxgiFactory->CreateSwapChain(m_d3dDevice.Get(), &swapDesc, &m_pSwapChain);

    CreateBackBufferRTV();

    return S_OK;
}

HRESULT Render::CreateBackBufferRTV()
{
    ComPtr<ID3D11Texture2D> backBuf;
    m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuf);
    m_d3dDevice->CreateRenderTargetView(backBuf.Get(), nullptr, &m_backBufferRTV);

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

    m_d3dDevice->CreateTexture2D(&texDesc, nullptr, &m_depthTex);

    // Depth Stencil View
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = texDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    m_d3dDevice->CreateDepthStencilView(m_depthTex.Get(), &dsvDesc, &m_depthDSV);

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

    m_d3dDevice->CreateBuffer(&vbDesc, &vbInit, &m_cubeVB);

    // Index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibInit = {};
    ibInit.pSysMem = indices;

    m_d3dDevice->CreateBuffer(&ibDesc, &ibInit, &m_cubeIB);

    // CB для мировой матрицы (обновляется через UpdateSubresource)
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CBWorld);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    m_d3dDevice->CreateBuffer(&cbDesc, nullptr, &m_cbWorld);

    // CB для view-projection (обновляется через Map/Unmap)
    cbDesc.ByteWidth = sizeof(CBViewProjection);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    m_d3dDevice->CreateBuffer(&cbDesc, nullptr, &m_cbViewProj);

    return S_OK;
}

HRESULT Render::CompileAndCreateShaders()
{
    // Компиляция вершинного шейдера
    ComPtr<ID3DBlob> vsCode;
    ComPtr<ID3DBlob> compileErrors;

    D3DCompileFromFile(
        L"VertexShader.vs",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &vsCode,
        &compileErrors
    );

    m_d3dDevice->CreateVertexShader(
        vsCode->GetBufferPointer(),
        vsCode->GetBufferSize(),
        nullptr,
        &m_mainVS
    );

    // Компиляция пиксельного шейдера
    ComPtr<ID3DBlob> psCode;
    D3DCompileFromFile(
        L"PixelShader.ps",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &psCode,
        &compileErrors
    );

    m_d3dDevice->CreatePixelShader(
        psCode->GetBufferPointer(),
        psCode->GetBufferSize(),
        nullptr,
        &m_mainPS
    );

    // Input Layout
    D3D11_INPUT_ELEMENT_DESC inputDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    m_d3dDevice->CreateInputLayout(
        inputDesc,
        2,
        vsCode->GetBufferPointer(),
        vsCode->GetBufferSize(),
        &m_vertexLayout
    );

    return S_OK;
}

void Render::RenderFrame()
{
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

    // Отрисовка куба
    m_deviceCtx->DrawIndexed(36, 0, 0);

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
    m_deviceCtx->Map(m_cbViewProj.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapData);
    memcpy(mapData.pData, &vpT, sizeof(XMMATRIX));
    m_deviceCtx->Unmap(m_cbViewProj.Get(), 0);

    m_deviceCtx->VSSetConstantBuffers(0, 1, m_cbWorld.GetAddressOf());
    m_deviceCtx->VSSetConstantBuffers(1, 1, m_cbViewProj.GetAddressOf());
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