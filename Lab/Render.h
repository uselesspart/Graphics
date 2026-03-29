#pragma once

#include <d3d11_1.h>
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
    void OnWindowResize(HWND hWnd);

    void MoveCamera(float dx, float dy, float dz);
    void RotateCamera(float deltaYaw, float deltaPitch);

private:
    HRESULT InitDeviceAndSwapChain(HWND hWnd);
    HRESULT CreateBackBufferRTV();
    HRESULT CreateDepthBuffer(UINT width, UINT height);
    HRESULT InitGeometryBuffers();
    HRESULT CompileAndCreateShaders();
    void UpdateMatrices();
    void LabelD3DResources();

    ComPtr<ID3D11Device>              m_d3dDevice;
    ComPtr<ID3D11DeviceContext>       m_deviceCtx;
    ComPtr<IDXGISwapChain>            m_pSwapChain;

    ComPtr<ID3D11RenderTargetView>    m_backBufferRTV;
    ComPtr<ID3D11Texture2D>           m_depthTex;
    ComPtr<ID3D11DepthStencilView>    m_depthDSV;

    ComPtr<ID3DUserDefinedAnnotation> m_debugAnnotation;

    ComPtr<ID3D11Buffer> m_cubeVB;
    ComPtr<ID3D11Buffer> m_cubeIB;

    ComPtr<ID3D11VertexShader> m_mainVS;
    ComPtr<ID3D11PixelShader>  m_mainPS;
    ComPtr<ID3D11InputLayout>  m_vertexLayout;

    ComPtr<ID3D11Buffer> m_cbWorld;
    ComPtr<ID3D11Buffer> m_cbViewProj;

    XMFLOAT3 m_camPosition;
    float    m_camYaw;
    float    m_camPitch;

    float m_cubeAngle;

    HWND m_hWnd;
};