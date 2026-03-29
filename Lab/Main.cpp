#include "AppFramework.h"
#include "Render.h"
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

constexpr int MAX_STR_LEN = 100;

WCHAR g_title[MAX_STR_LEN] = L"Lab1 DirectX11";
WCHAR g_wndClass[MAX_STR_LEN] = L"Lab1WindowClass";

Render* g_renderer = nullptr;

// Прототипы
ATOM RegisterWindowClass(HINSTANCE hInst);
BOOL InitializeWindow(HINSTANCE hInst, int showCmd);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    RegisterWindowClass(hInstance);
    InitializeWindow(hInstance, nCmdShow);

    // Цикл сообщений с рендером в idle
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            if (g_renderer)
                g_renderer->RenderFrame();
        }
    }

    // Корректное завершение
    if (g_renderer)
    {
        g_renderer->Shutdown();
        delete g_renderer;
        g_renderer = nullptr;
    }

    return static_cast<int>(msg.wParam);
}

ATOM RegisterWindowClass(HINSTANCE hInst)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_LAB1));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = g_wndClass;
    wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wc);
}

BOOL InitializeWindow(HINSTANCE hInst, int showCmd)
{
    HWND hWnd = CreateWindowW(g_wndClass, g_title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600,
        nullptr, nullptr, hInst, nullptr);

    if (!hWnd)
        return FALSE;

    g_renderer = new Render();
    g_renderer->Initialize(hWnd);

    ShowWindow(hWnd, showCmd);
    UpdateWindow(hWnd);

    return TRUE;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (g_renderer)
        {
            switch (wParam)
            {
            case 'W':
                g_renderer->RotateCamera(0.0f, 0.02f);
                break;
            case 'S':
                g_renderer->RotateCamera(0.0f, -0.02f);
                break;
            case 'A':
                g_renderer->RotateCamera(-0.02f, 0.0f);
                break;
            case 'D':
                g_renderer->RotateCamera(0.02f, 0.0f);
                break;
            case VK_UP:
                g_renderer->MoveCamera(0.0f, 0.1f, 0.0f);
                break;
            case VK_DOWN:
                g_renderer->MoveCamera(0.0f, -0.1f, 0.0f);
                break;
            case VK_LEFT:
                g_renderer->MoveCamera(-0.1f, 0.0f, 0.0f);
                break;
            case VK_RIGHT:
                g_renderer->MoveCamera(0.1f, 0.0f, 0.0f);
                break;
            case VK_ADD:
            case 0xBB:
                g_renderer->MoveCamera(0.0f, 0.0f, 0.1f);
                break;
            case VK_SUBTRACT:
            case 0xBD:
                g_renderer->MoveCamera(0.0f, 0.0f, -0.1f);
                break;
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}