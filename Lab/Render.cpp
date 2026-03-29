#include "AppFramework.h"
#include "Render.h"
#include <d3dcompiler.h>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Структура вершины
struct Vertex
{
    float pos[3];
    COLORREF color;
};

// Буферы констант
struct WorldMatrixBuffer
{
    XMMATRIX world;
};

struct ViewProjBuffer
{
    XMMATRIX viewProj;
};

Render::Render()
    : m_cameraPos(0.0f, 0.0f, -5.0f)
    , m_yawAngle(0.0f)
    , m_pitchAngle(0.0f)
    , m_rotationAngle(0.0f)
    , m_hwnd(nullptr)
{
}

Render::~Render()
{
    Shutdown();
}

HRESULT Render::Initialize(HWND hwnd)
{
    m_hwnd = hwnd;

    SetupDevice(hwnd);

    RECT rect;
    GetClientRect(hwnd, &rect);
    UINT width = rect.right - rect.left;
    UINT height = rect.bottom - rect.top;

    SetupDepthStencil(width, height);
    CreateGeometry();
    LoadShaders();

    return S_OK;
}

void Render::Shutdown()
{
    if (m_context)
    {
        m_context->ClearState();
    }
}

HRESULT Render::SetupDevice(HWND hwnd)
{
    // Поиск адаптера
    ComPtr<IDXGIFactory> factory;
    CreateDXGIFactory(__uuidof(IDXGIFactory), &factory);

    ComPtr<IDXGIAdapter> adapter;
    UINT adapterIdx = 0;
    while (SUCCEEDED(factory->EnumAdapters(adapterIdx, &adapter)))
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        // Пропускаем программный рендерер
        if (wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0)
        {
            break;
        }
        adapter.Reset();
        ++adapterIdx;
    }

    // Создание устройства
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;

    D3D11CreateDevice(
        adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        0,
        featureLevels,
        1,
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context
    );

    // Настройка swap chain
    DXGI_SWAP_CHAIN_DESC scDesc = {};
    scDesc.BufferCount = 2;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = hwnd;
    scDesc.SampleDesc.Count = 1;
    scDesc.SampleDesc.Quality = 0;
    scDesc.Windowed = TRUE;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    factory->CreateSwapChain(m_device.Get(), &scDesc, &m_swapChain);

    SetupBackBuffer();

    return S_OK;
}

HRESULT Render::SetupBackBuffer()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTarget);

    return S_OK;
}

HRESULT Render::SetupDepthStencil(UINT width, UINT height)
{
    // Создание текстуры для depth buffer
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthBuffer);

    // Создание depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = depthDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, &m_depthStencil);

    m_context->OMSetRenderTargets(1, m_renderTarget.GetAddressOf(), m_depthStencil.Get());

    // Viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(width);
    vp.Height = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_context->RSSetViewports(1, &vp);

    return S_OK;
}

HRESULT Render::CreateGeometry()
{
    // Вершины куба
    static const Vertex cubeVertices[] =
    {
        { {-1.0f,  1.0f, -1.0f}, RGB(255, 20, 147) },
        { { 1.0f,  1.0f, -1.0f}, RGB(0, 255, 127) },
        { { 1.0f,  1.0f,  1.0f}, RGB(138, 43, 226) },
        { {-1.0f,  1.0f,  1.0f}, RGB(255, 215, 0) },
        { {-1.0f, -1.0f, -1.0f}, RGB(255, 69, 0) },
        { { 1.0f, -1.0f, -1.0f}, RGB(0, 255, 255) },
        { { 1.0f, -1.0f,  1.0f}, RGB(186, 85, 211) },
        { {-1.0f, -1.0f,  1.0f}, RGB(50, 205, 50) }
    };

    // Индексы треугольников
    WORD cubeIndices[] =
    {
        3,1,0, 2,1,3,
        0,5,4, 1,5,0,
        3,4,7, 0,4,3,
        1,6,5, 2,6,1,
        2,7,6, 3,7,2,
        6,4,5, 7,4,6,
    };

    // Создание вершинного буфера
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(cubeVertices);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = cubeVertices;

    m_device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);

    // Создание индексного буфера
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(cubeIndices);
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = cubeIndices;

    m_device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);

    // Константный буфер для world матрицы
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(WorldMatrixBuffer);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    m_device->CreateBuffer(&cbDesc, nullptr, &m_worldBuffer);

    // Константный буфер для view-projection
    cbDesc.ByteWidth = sizeof(ViewProjBuffer);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    m_device->CreateBuffer(&cbDesc, nullptr, &m_viewProjBuffer);

    return S_OK;
}

HRESULT Render::LoadShaders()
{
    // Компиляция вершинного шейдера
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> errorBlob;

    D3DCompileFromFile(
        L"VertexShader.vs",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &vsBlob,
        &errorBlob
    );

    m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &m_vertexShader
    );

    // Компиляция пиксельного шейдера
    ComPtr<ID3DBlob> psBlob;
    D3DCompileFromFile(
        L"PixelShader.ps",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &psBlob,
        &errorBlob
    );

    m_device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &m_pixelShader
    );

    // Input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    m_device->CreateInputLayout(
        layout,
        2,
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &m_inputLayout
    );

    return S_OK;
}

void Render::DrawScene()
{
    // Очистка буферов (темно-фиолетовый космический фон)
    float clearColor[4] = { 0.1f, 0.05f, 0.2f, 1.0f };
    m_context->ClearRenderTargetView(m_renderTarget.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthStencil.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    UpdateTransforms();

    // Настройка pipeline
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_inputLayout.Get());

    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    // Рендеринг
    m_context->DrawIndexed(36, 0, 0);

    m_swapChain->Present(1, 0);
}

void Render::UpdateTransforms()
{
    // Обновление угла вращения куба
    m_rotationAngle += 0.005f;
    if (m_rotationAngle > XM_2PI)
        m_rotationAngle -= XM_2PI;

    XMMATRIX world = XMMatrixRotationY(m_rotationAngle);

    // Обновление матрицы мира
    XMMATRIX worldTranspose = XMMatrixTranspose(world);
    m_context->UpdateSubresource(m_worldBuffer.Get(), 0, nullptr, &worldTranspose, 0, 0);

    // Вычисление направления взгляда
    XMVECTOR direction = XMVectorSet(
        cosf(m_pitchAngle) * sinf(m_yawAngle),
        sinf(m_pitchAngle),
        cosf(m_pitchAngle) * cosf(m_yawAngle),
        0.0f
    );

    XMVECTOR eye = XMLoadFloat3(&m_cameraPos);
    XMVECTOR focus = XMVectorAdd(eye, direction);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(eye, focus, up);

    // Перспективная проекция
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    float aspect = static_cast<float>(rect.right - rect.left) / static_cast<float>(rect.bottom - rect.top);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);

    XMMATRIX viewProj = view * proj;
    XMMATRIX vpTranspose = XMMatrixTranspose(viewProj);

    // Обновление view-projection буфера
    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_viewProjBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &vpTranspose, sizeof(XMMATRIX));
    m_context->Unmap(m_viewProjBuffer.Get(), 0);

    m_context->VSSetConstantBuffers(0, 1, m_worldBuffer.GetAddressOf());
    m_context->VSSetConstantBuffers(1, 1, m_viewProjBuffer.GetAddressOf());
}

void Render::MoveView(float dx, float dy, float dz)
{
    m_cameraPos.x += dx;
    m_cameraPos.y += dy;
    m_cameraPos.z += dz;
}

void Render::RotateView(float yaw, float pitch)
{
    m_yawAngle += yaw;
    m_pitchAngle += pitch;

    // Ограничение углов
    if (m_yawAngle > XM_2PI)
        m_yawAngle -= XM_2PI;
    if (m_yawAngle < -XM_2PI)
        m_yawAngle += XM_2PI;

    if (m_pitchAngle > XM_PIDIV2)
        m_pitchAngle = XM_PIDIV2;
    if (m_pitchAngle < -XM_PIDIV2)
        m_pitchAngle = -XM_PIDIV2;
}