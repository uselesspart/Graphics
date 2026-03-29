#include "AppFramework.h"
#include "Render.h"
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

constexpr int MAX_STRING_LENGTH = 100;

// Глобальные данные приложения
WCHAR g_AppTitle[MAX_STRING_LENGTH] = L"Lab1 DirectX11";
WCHAR g_WindowClassName[MAX_STRING_LENGTH] = L"Lab1WindowClass";

Render* g_pGraphics = nullptr;

// Объявления функций
ATOM RegisterAppWindow(HINSTANCE hInst);
BOOL CreateAppWindow(HINSTANCE hInst, int showCmd);
LRESULT CALLBACK WndProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    RegisterAppWindow(hInstance);
    CreateAppWindow(hInstance, nCmdShow);

    // Главный цикл обработки сообщений
    MSG message = {};
    while (message.message != WM_QUIT)
    {
        if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        else
        {
            // Рендеринг сцены
            if (g_pGraphics)
            {
                g_pGraphics->DrawScene();
            }
        }
    }

    // Освобождение ресурсов
    if (g_pGraphics)
    {
        g_pGraphics->Shutdown();
        delete g_pGraphics;
        g_pGraphics = nullptr;
    }

    return static_cast<int>(message.wParam);
}

ATOM RegisterAppWindow(HINSTANCE hInst)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProcedure;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_LAB1));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = g_WindowClassName;
    wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wc);
}

BOOL CreateAppWindow(HINSTANCE hInst, int showCmd)
{
    HWND hwnd = CreateWindowW(g_WindowClassName, g_AppTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600,
        nullptr, nullptr, hInst, nullptr);

    if (!hwnd)
        return FALSE;

    // Инициализация графического движка
    g_pGraphics = new Render();
    g_pGraphics->Initialize(hwnd);

    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    return TRUE;
}

LRESULT CALLBACK WndProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (g_pGraphics)
        {
            switch (wp)
            {
            case 'W':
                g_pGraphics->RotateView(0.0f, 0.02f);
                break;
            case 'S':
                g_pGraphics->RotateView(0.0f, -0.02f);
                break;
            case 'A':
                g_pGraphics->RotateView(-0.02f, 0.0f);
                break;
            case 'D':
                g_pGraphics->RotateView(0.02f, 0.0f);
                break;
            case VK_UP:
                g_pGraphics->MoveView(0.0f, 0.1f, 0.0f);
                break;
            case VK_DOWN:
                g_pGraphics->MoveView(0.0f, -0.1f, 0.0f);
                break;
            case VK_LEFT:
                g_pGraphics->MoveView(-0.1f, 0.0f, 0.0f);
                break;
            case VK_RIGHT:
                g_pGraphics->MoveView(0.1f, 0.0f, 0.0f);
                break;
            case VK_ADD:
            case 0xBB:
                g_pGraphics->MoveView(0.0f, 0.0f, 0.1f);
                break;
            case VK_SUBTRACT:
            case 0xBD:
                g_pGraphics->MoveView(0.0f, 0.0f, -0.1f);
                break;
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}