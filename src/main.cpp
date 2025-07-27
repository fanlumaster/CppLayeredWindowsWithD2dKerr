#include <windows.h>
#include <atlbase.h>
#include <atlwin.h>
#include "atlapp.h"
#include "atlcrack.h"
#include "atltypes.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <winuser.h>

#pragma comment(lib, "d2d1.lib")

// clang-format off
#define HR(_hr_expr)              \
{                                 \
    hr = (_hr_expr);              \
    if (FAILED(hr)) return hr;    \
}
#ifndef VERIFY
#ifdef _DEBUG
#include <cassert>
#define VERIFY(expr) assert(expr)
#else
#define VERIFY(expr) (void)(expr)
#endif
#endif
// clang-format on

class Window : public CWindowImpl<Window, CWindow, CWinTraits<WS_OVERLAPPEDWINDOW>>
{
  public:
    BEGIN_MSG_MAP(Window)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_DISPLAYCHANGE(OnDisplayChange)
    MSG_WM_SIZE(OnSize)
    END_MSG_MAP()

    HRESULT Create()
    {
        VERIFY(__super::Create(nullptr)); // Top-level window

        VERIFY(SetWindowText(L"Alpha Window"));

        VERIFY(SetWindowPos( //
            nullptr,         // No Z-order change
            100, 100,        // Position (x, y)
            600, 400,        // Size (width, height)
            SWP_NOZORDER | SWP_SHOWWINDOW));

        CreateDeviceIndependentResources();
        SetWindowLongPtr( //
            GWL_EXSTYLE,  //
            GetWindowLongPtr(GWL_EXSTYLE) | WS_EX_LAYERED);
        VERIFY(SetLayeredWindowAttributes( //
            m_hWnd,                        //
            0,                             // no color key
            180,                           // alpha value
            LWA_ALPHA));

        VERIFY(UpdateWindow());

        return S_OK;
    }

    HRESULT CreateDeviceIndependentResources()
    {
        HRESULT hr;
        // Create device-independent resources here
        HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_factory));
        return S_OK;
    }

    HRESULT CreateDeviceResources()
    {
        HRESULT hr;
        if (0 == m_target)
        {
            CRect rect;
            VERIFY(GetClientRect(&rect));
            D2D1_SIZE_U size = D2D1::SizeU(rect.Width(), rect.Height());
            HR(m_factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(m_hWnd, size), &m_target));
            HR(m_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &m_brush));
        }
        return S_OK;
    }

    void DiscardDeviceResources()
    {
        m_brush.Release();
        m_target.Release();
    }

    void Render()
    {
        if (SUCCEEDED(CreateDeviceResources()))
        {
            if (0 == (D2D1_WINDOW_STATE_OCCLUDED & m_target->CheckWindowState()))
            {
                m_target->BeginDraw();
                m_target->SetTransform(D2D1::Matrix3x2F::Identity());
                m_target->Clear(D2D1::ColorF(D2D1::ColorF::White));

                // Drawing code here

                if (D2DERR_RECREATE_TARGET == m_target->EndDraw())
                {
                    DiscardDeviceResources();
                }
            }
        }
    }
    void OnPaint(CDCHandle /*dc*/)
    {
        PAINTSTRUCT paint;
        VERIFY(BeginPaint(&paint));
        Render();
        EndPaint(&paint);
    }

    void OnDisplayChange(UINT /*bpp*/, CSize /*resolution*/)
    {
        Render();
    }

    void OnSize(UINT /*type*/, CSize size)
    {
        if (0 != m_target)
        {
            if (FAILED(m_target->Resize(D2D1::SizeU(size.cx, size.cy))))
            {
                DiscardDeviceResources();
                VERIFY(Invalidate(FALSE));
            }
        }
    }

  private:
    void OnDestroy()
    {
        ::PostQuitMessage(1);
    }

    CComPtr<ID2D1Factory> m_factory;
    CComPtr<ID2D1HwndRenderTarget> m_target;
    CComPtr<ID2D1SolidColorBrush> m_brush;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HRESULT hr = S_OK;
    ::CoInitialize(nullptr);

    Window wnd;

    HR(wnd.Create());

    wnd.ShowWindow(nCmdShow);
    wnd.UpdateWindow();

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ::CoUninitialize();
    return static_cast<int>(msg.wParam);
}