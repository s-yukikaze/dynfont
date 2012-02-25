#include <windows.h>
#include <dwrite.h>
#include <d2d1.h>
#include <atlbase.h>
#include <vector>

class gditextrenderer : public IDWriteTextRenderer {
private:
    LONG ref;
    IDWriteBitmapRenderTarget *target;
    IDWriteRenderingParams *params;
    COLORREF color;
 
public:
    gditextrenderer(IDWriteBitmapRenderTarget* target, IDWriteRenderingParams *params, COLORREF color) : ref(1), target(target), params(params), color(color)
    {
        target->AddRef();
        params->AddRef(); 
    }

    virtual ~gditextrenderer()
    {
        target->Release();
        params->Release();
    }

public:
    STDMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&ref);
    }
    STDMETHODIMP_(ULONG) Release() {
        ULONG newref = InterlockedDecrement(&ref);
        if (ref <= 1) {
            delete this;
        }
        return newref;
    }
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) {
        if (IsEqualIID(riid, __uuidof(IUnknown))) {
            *ppvObject = (IUnknown*)this;
        } else if (IsEqualIID(riid, __uuidof(IDWriteTextRenderer))) {
            *ppvObject = (IDWriteTextRenderer*)this;
        } else if (IsEqualIID(riid, __uuidof(IDWritePixelSnapping))) {
            *ppvObject = (IDWritePixelSnapping*)this;
        }
        return E_NOINTERFACE;
    }

public:
    STDMETHODIMP DrawGlyphRun(
        void * clientDrawingContext,
        FLOAT  baselineOriginX,
        FLOAT  baselineOriginY,
        DWRITE_MEASURING_MODE  measuringMode,
        const DWRITE_GLYPH_RUN * glyphRun,
        const DWRITE_GLYPH_RUN_DESCRIPTION * glyphRunDescription,
        IUnknown * clientDrawingEffect
        )
    {
        RECT rc = { 0 };
        return target->DrawGlyphRun(baselineOriginX, baselineOriginY, measuringMode, glyphRun, params, color, &rc);
    }
    
    STDMETHODIMP DrawInlineObject(
        void * clientDrawingContext,
        FLOAT  originX,
        FLOAT  originY,
        IDWriteInlineObject * inlineObject,
        BOOL  isSideways,
        BOOL  isRightToLeft,
        IUnknown * clientDrawingEffect
        )
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP DrawStrikethrough(
        void * clientDrawingContext,
        FLOAT  baselineOriginX,
        FLOAT  baselineOriginY,
        const DWRITE_STRIKETHROUGH * strikethrough,
        IUnknown * clientDrawingEffect
        )
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP DrawUnderline(
        void * clientDrawingContext,
        FLOAT  baselineOriginX,
        FLOAT  baselineOriginY,
        const DWRITE_UNDERLINE * underline,
        IUnknown * clientDrawingEffect
        )
    {
        return E_NOTIMPL;
    }

    STDMETHODIMP GetCurrentTransform(
         void * clientDrawingContext,
         DWRITE_MATRIX * transform
         )
    {
        target->GetCurrentTransform(transform);
        return S_OK;
    }
 
    STDMETHODIMP GetPixelsPerDip(
         void * clientDrawingContext,
         FLOAT * pixelsPerDip
         )
    {
        *pixelsPerDip = target->GetPixelsPerDip();
        return S_OK;
    }

    STDMETHODIMP IsPixelSnappingDisabled(
          void * clientDrawingContext,
          BOOL * isDisabled
          )
    {
        *isDisabled = FALSE;
        return S_OK;
    }
};

extern "C"
BOOL exttextout_impl(HDC hdc, int x, int y, UINT options, const RECT *rc, LPCWSTR str, int len, const int *dx)
{
 
    HRESULT hr;

    CComPtr<IDWriteFactory> dwfactory;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&dwfactory));
    if(FAILED(hr)) return FALSE;

    CComPtr<IDWriteGdiInterop> interop;
    hr = dwfactory->GetGdiInterop(&interop);
    if(FAILED(hr)) return FALSE;

    HFONT gdifont;
    gdifont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    LOGFONTW fontinfo;
    GetObjectW(gdifont, sizeof(LOGFONT), &fontinfo);

    OutputDebugStringW(fontinfo.lfFaceName);
    CComPtr<IDWriteFont> font;
    hr = interop->CreateFontFromLOGFONT(&fontinfo, &font);
    if (FAILED(hr)) {
        wcscpy(fontinfo.lfFaceName, L"MS Gothic");
        hr = interop->CreateFontFromLOGFONT(&fontinfo, &font);
        if (FAILED(hr)) return FALSE;
    }

    CComPtr<IDWriteFontFamily> fontfamily;
    hr = font->GetFontFamily(&fontfamily);
    if (FAILED(hr)) return FALSE;

    CComPtr<IDWriteLocalizedStrings> familynames;
    hr = fontfamily->GetFamilyNames(&familynames);
    if (FAILED(hr)) return FALSE;

    UINT length;
    hr = familynames->GetStringLength(0, &length);
    if (FAILED(hr)) return FALSE;

    std::vector<wchar_t> familyname(length + 1);
    familynames->GetString(0, &familyname[0], length + 1);
    if (FAILED(hr)) return FALSE;

    float fontsize;
    fontsize = (float)-MulDiv(fontinfo.lfHeight, 96, GetDeviceCaps(hdc, LOGPIXELSY));

    OutputDebugStringW(&familyname[0]);
    CComPtr<IDWriteTextFormat> textfmt;
    hr = dwfactory->CreateTextFormat(&familyname[0], NULL, font->GetWeight(), font->GetStyle(), font->GetStretch(), fontsize, L"ja-jp", &textfmt);
    if (FAILED(hr)) return FALSE;

    CComPtr<IDWriteTextLayout> layout;
    hr = dwfactory->CreateTextLayout(str, len, textfmt, 1024.f, 768.f, &layout);

    if (fontinfo.lfUnderline) {
        DWRITE_TEXT_RANGE range = { 0, len };
        layout->SetUnderline(true, range);
    }

    if (fontinfo.lfStrikeOut) {
        DWRITE_TEXT_RANGE range = { 0, len };
        layout->SetStrikethrough(true, range);
    }

    DWRITE_TEXT_METRICS metrics;
    layout->GetMetrics(&metrics);

    RECT lbox;
    lbox.left  = x;
    lbox.top   = y;
    lbox.right = metrics.widthIncludingTrailingWhitespace + metrics.left + x - 1;
    lbox.bottom = metrics.height + metrics.top + y - 1;
    RECT bbox;
    if (rc != NULL) {
	bbox.left  = max(rc->left, lbox.left);
	bbox.top   = max(rc->top, lbox.top);
	bbox.right = min(rc->right, lbox.right);
	bbox.bottom = min(rc->bottom, lbox.bottom);
    } else {
	bbox = lbox;
    }

    CComPtr<IDWriteBitmapRenderTarget> target;
    hr = interop->CreateBitmapRenderTarget(hdc, lbox.right - lbox.left , lbox.bottom - lbox.top, &target);
    if (FAILED(hr)) return FALSE;

    HDC memdc = target->GetMemoryDC();
    if (GetBkMode(hdc) == OPAQUE) {
        COLORREF bkcolor = GetBkColor(hdc);
        HBRUSH bkbrush = CreateSolidBrush(bkcolor);
        HBRUSH oldbrush = (HBRUSH)SelectObject(memdc, bkbrush);
        PatBlt(memdc, 0, 0, lbox.right - lbox.left, lbox.bottom - lbox.top, PATCOPY);
        SelectObject(memdc, oldbrush);
        DeleteObject(bkbrush);
    } else {
        BitBlt(memdc, 0, 0, lbox.right - lbox.left, lbox.bottom - lbox.top, hdc, x, y, SRCCOPY);
    }

    CComPtr<IDWriteRenderingParams> params;
    hr = dwfactory->CreateCustomRenderingParams(1.0f, 0.3f, 7.5f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC, &params);
    if (FAILED(hr)) return FALSE;

    IDWriteTextRenderer *renderer = new (std::nothrow) gditextrenderer(target, params,GetTextColor(hdc));
    layout->Draw(NULL, renderer, 0, 0);
    renderer->Release();

    BitBlt(hdc, x, y, bbox.right - bbox.left, bbox.bottom - bbox.top, memdc, bbox.left - lbox.left, bbox.top - lbox.top, SRCCOPY);

    return TRUE;
}

