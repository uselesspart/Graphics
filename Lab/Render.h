#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Render
{
public:
    Render();
    ~Render();

    HRESULT Initialize(HWND hWnd);
    void Shutdown();
    void RenderFrame();

    // Управление камерой
    void MoveCamera(float dx, float dy, float dz);
    void RotateCamera(float deltaYaw, float deltaPitch);

private:
    HRESULT InitDeviceAndSwapChain(HWND hWnd);
    HRESULT CreateBackBufferRTV();
    HRESULT CreateDepthBuffer(UINT width, UINT height);
    HRESULT InitGeometryBuffers();
    HRESULT CompileAndCreateShaders();
    void UpdateMatrices();

    // Устройство и контекст
    ComPtr<ID3D11Device>           m_d3dDevice;
    ComPtr<ID3D11DeviceContext>    m_deviceCtx;
    ComPtr<IDXGISwapChain>         m_pSwapChain;

    // Render targets и глубина
    ComPtr<ID3D11RenderTargetView> m_backBufferRTV;
    ComPtr<ID3D11Texture2D>        m_depthTex;
    ComPtr<ID3D11DepthStencilView> m_depthDSV;

    // Геометрия куба
    ComPtr<ID3D11Buffer> m_cubeVB;
    ComPtr<ID3D11Buffer> m_cubeIB;

    // Шейдеры и layout
    ComPtr<ID3D11VertexShader> m_mainVS;
    ComPtr<ID3D11PixelShader>  m_mainPS;
    ComPtr<ID3D11InputLayout>  m_vertexLayout;

    // Константные буферы
    ComPtr<ID3D11Buffer> m_cbWorld;
    ComPtr<ID3D11Buffer> m_cbViewProj;

    // Камера
    XMFLOAT3 m_camPosition;
    float    m_camYaw;
    float    m_camPitch;

    // Анимация куба
    float m_cubeAngle;

    HWND m_hWnd;
};