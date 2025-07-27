#include <windows.h>
#include <atlbase.h>
#include <atlwin.h>
#include "atlapp.h"
#include "atlcrack.h"
#include "atltypes.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <winnt.h>
#include <winuser.h>
#include <new>
#include <gdiplus.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "gdiplus.lib")

// clang-format off
#define HR(_hr_expr)              \
{                                 \
    hr = (_hr_expr);              \
    if (FAILED(hr)) return hr;    \
}
#define HRVOID(_hr_expr)          \
{                                 \
    hr = (_hr_expr);              \
    if (FAILED(hr)) return;       \
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

using namespace std;
using namespace Gdiplus;

class LayeredWindowInfo
{
    const POINT m_sourcePosition;
    POINT m_windowPosition;
    CSize m_size;
    BLENDFUNCTION m_blend;
    UPDATELAYEREDWINDOWINFO m_info;

  public:
    LayeredWindowInfo(__in UINT width, __in UINT height) : m_sourcePosition(), m_windowPosition(), m_size(width, height), m_blend(), m_info()
    {

        m_info.cbSize = sizeof(UPDATELAYEREDWINDOWINFO);
        m_info.pptSrc = &m_sourcePosition;
        m_info.pptDst = &m_windowPosition;
        m_info.psize = &m_size;
        m_info.pblend = &m_blend;
        m_info.dwFlags = ULW_ALPHA;

        m_blend.SourceConstantAlpha = 255;
        m_blend.AlphaFormat = AC_SRC_ALPHA;
    }

    void Update(__in HWND window, __in HDC source)
    {

        m_info.hdcSrc = source;

        VERIFY(UpdateLayeredWindowIndirect(window, &m_info));
    }

    UINT GetWidth() const
    {
        return m_size.cx;
    }

    UINT GetHeight() const
    {
        return m_size.cy;
    }
};

class GdiBitmap
{
    const UINT m_width;
    const UINT m_height;
    const UINT m_stride;
    void *m_bits;
    HBITMAP m_oldBitmap;

    CDC m_dc;
    CBitmap m_bitmap;

  public:
    GdiBitmap(__in UINT width, __in UINT height) : m_width(width), m_height(height), m_stride((width * 32 + 31) / 32 * 4), m_bits(0), m_oldBitmap(0)
    {

        BITMAPINFO bitmapInfo = {};
        bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
        bitmapInfo.bmiHeader.biWidth = width;
        bitmapInfo.bmiHeader.biHeight = 0 - height;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        m_bitmap.Attach(CreateDIBSection( //
            0,                            // device context
            &bitmapInfo, DIB_RGB_COLORS, &m_bits,
            0,   // file mapping object
            0)); // file offset
        if (0 == m_bits)
        {
            throw bad_alloc();
        }

        if (0 == m_dc.CreateCompatibleDC())
        {
            throw bad_alloc();
        }

        m_oldBitmap = m_dc.SelectBitmap(m_bitmap);
    }

    ~GdiBitmap()
    {
        m_dc.SelectBitmap(m_oldBitmap);
    }

    UINT GetWidth() const
    {
        return m_width;
    }

    UINT GetHeight() const
    {
        return m_height;
    }

    UINT GetStride() const
    {
        return m_stride;
    }

    void *GetBits() const
    {
        return m_bits;
    }

    HDC GetDC() const
    {
        return m_dc;
    }
};

class LayeredWindow : public CWindowImpl<LayeredWindow, CWindow, CWinTraits<WS_POPUP, WS_EX_LAYERED>>
{

    LayeredWindowInfo m_info;
    GdiBitmap m_bitmap;
    CComPtr<ID2D1Factory> factory;
    CComPtr<ID2D1DCRenderTarget> target;
    CComPtr<ID2D1SolidColorBrush> brush;

  public:
    BEGIN_MSG_MAP(LayeredWindow)
    MSG_WM_DESTROY(OnDestroy)
    END_MSG_MAP()

    LayeredWindow() : m_info(600, 400), m_bitmap(m_info.GetWidth(), m_info.GetHeight())
    {
        VERIFY(0 != __super::Create(0)); // parent

        const D2D1_PIXEL_FORMAT format = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
        const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, format);

        HRESULT hr;
        HRVOID(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory));
        HRVOID(factory->CreateDCRenderTarget(&properties, &target));

        const RECT rect = {0, 0, static_cast<LONG>(m_bitmap.GetWidth()), static_cast<LONG>(m_bitmap.GetHeight())};
        HRVOID(target->BindDC(m_bitmap.GetDC(), &rect));

        HRVOID(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &brush));

        ShowWindow(SW_SHOW);
        Render();
    }

    void Render()
    {
        // Do some drawing here
        target->BeginDraw();

        target->Clear(D2D1::ColorF(0, 0, 0, 0));

        brush->SetColor(D2D1::ColorF(0, 0, 0, 0.627f));
        target->FillRectangle(D2D1::RectF(0, 0, (FLOAT)m_info.GetWidth(), (FLOAT)m_info.GetHeight()), brush);

        brush->SetColor(D2D1::ColorF(D2D1::ColorF::Green));
        brush->SetOpacity(1.0);
        target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(300.0f, 200.0f), 100.0f, 100.0f), brush);

        HRESULT hr;
        HRVOID(target->EndDraw());

        m_info.Update(m_hWnd, m_bitmap.GetDC());
    }

    void OnDestroy()
    {
        PostQuitMessage(1);
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    {
        LayeredWindow window;

        MSG msg = {};
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}