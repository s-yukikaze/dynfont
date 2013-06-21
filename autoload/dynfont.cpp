#if defined(__MINGW32__)
#define __MINGW_USE_BROKEN_INTERFACE
#endif

#define _WIN32_WINNT 0x601
#include <windows.h>
#include <set>
#include <vector>
#include <string>

#if defined(__MINGW32__)
#define __CRT_UUID_DECL(type,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)           \
    extern "C++" {                                                      \
    template<> inline const GUID &__mingw_uuidof<type>() {              \
        static const IID __uuid_inst = {l,w1,w2, {b1,b2,b3,b4,b5,b6,b7,b8}}; \
        return __uuid_inst;                                             \
    }                                                                   \
    template<> inline const GUID &__mingw_uuidof<type*>() {             \
        return __mingw_uuidof<type>();                                  \
    }                                                                   \
    }

#define __uuidof(type) __mingw_uuidof<__typeof(type)>()
template<typename type> inline const GUID &__mingw_uuidof();
#endif

#include <dwrite.h>

#if defined(__MINGW32__)
#undef  INTERFACE
#define INTERFACE IDWriteFontFileEnumerator_MinGW
DECLARE_INTERFACE_(IDWriteFontFileEnumerator_MinGW,IUnknown)
{
    BEGIN_INTERFACE

    /* IUnknown methods */
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* IDWriteFontFileEnumerator methods */
    STDMETHOD_(HRESULT,MoveNext)(THIS_ BOOL * hasCurrentFile) PURE;
    STDMETHOD_(HRESULT,GetCurrentFontFile)(THIS_ IDWriteFontFile ** fontFile) PURE;

    END_INTERFACE
};

__CRT_UUID_DECL(IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46)
__CRT_UUID_DECL(IDWriteFontFileEnumerator, 0x72755049, 0x5ff7, 0x435d, 0x83,0x48, 0x4b,0xe9,0x7c,0xfa,0x6c,0x7c)
__CRT_UUID_DECL(IDWriteFontCollectionLoader, 0xcca920e4, 0x52f0, 0x492b, 0xbf,0xa8, 0x29,0xc7,0x2e,0xe0,0xa4,0x68)

#define IDWriteFontFileEnumerator_ IDWriteFontFileEnumerator_MinGW
#else
#define IDWriteFontFileEnumerator_ IDWriteFontFileEnumerator
#endif

#if __cplusplus < 201103L
#define nullptr 0
#endif

#undef OutputDebugString
#define OutputDebugString(x)

typedef FARPROC (WINAPI *GETPROCADDRESS_PROTO)(HMODULE, LPCSTR);
typedef HRESULT (WINAPI *DWRITECREATEFACTORY_PROTO)(DWRITE_FACTORY_TYPE, REFIID, IUnknown **);

bool hooked = false;
std::vector<std::string> fontpaths;
std::set<IDWriteFactory*> factories;
CRITICAL_SECTION fontpathscs;

GETPROCADDRESS_PROTO orig_GetProcAddress;
DWRITECREATEFACTORY_PROTO orig_DWriteCreateFactory;

template <class T> T * iunknown_addref(T *obj)
{
    if (obj != nullptr) {
        obj->AddRef();
    }
    return obj;
}

template <class T> void iunknown_release(T *&obj)
{
    if (obj != nullptr) {
        obj->Release();
    obj = nullptr;
    }
}

class a2w {
private:
    std::vector<WCHAR> buf;
public:
    a2w(const std::string &str)
    {
        int n = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), nullptr, 0);
        if (n > 0) {
            buf.resize(n + 1);
            MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), &buf[0], n);
        } else {
            buf.resize(1);
            buf[0] = 0;
        }
    }

    LPCWSTR c_str()
    {
        return &buf[0];
    }
};

class fontfile_enumerator : public IDWriteFontFileEnumerator_
{
private:
    LONG refnum;
    std::vector<std::string>::iterator curiter;
    IDWriteFontFile *curfile;
    IDWriteFactory *factory_;

public:
    fontfile_enumerator(IDWriteFactory *factory): refnum(0), curiter(fontpaths.begin()), curfile(nullptr), factory_(factory)
    {
        OutputDebugString("fontfile_enumerator::fontfile_enumerator");
    }

    virtual ~fontfile_enumerator()
    {
        OutputDebugString("fontfile_enumerator::~fontfile_enumerator");
        iunknown_release(curfile);
        iunknown_release(factory_);
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
    {
        OutputDebugString("fontfile_enumerator::QueryInterface");
        if (IsEqualIID(riid, __uuidof(IUnknown)) || IsEqualIID(riid, __uuidof(IDWriteFontFileEnumerator))) {
            *ppvObject = iunknown_addref(this);
            return S_OK;
        } else {
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }
    }

    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        OutputDebugString("fontfile_enumerator::AddRef");
        return InterlockedIncrement(&refnum);
    }

    virtual ULONG STDMETHODCALLTYPE Release()
    {
        OutputDebugString("fontfile_enumerator::Release");
        LONG newrefnum = InterlockedDecrement(&refnum);
        if (newrefnum == 0) {
            delete this;
        }
        return newrefnum;
    }

    virtual HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile** fontFile)
    {
        OutputDebugString("fontfile_enumerator::GetCurrentFontFile");
        if(fontFile == nullptr) {
            return E_INVALIDARG;
        } else if (curfile == nullptr) {
            return E_FAIL;
        } else {
            *fontFile = iunknown_addref(curfile);
            return S_OK;
        }
    }

    virtual HRESULT STDMETHODCALLTYPE MoveNext(BOOL* hasCurrentFile)
    {
        OutputDebugString("fontfile_enumerator::MoveNext");
        if(hasCurrentFile == nullptr) {
            return E_INVALIDARG;
        }

        iunknown_release(curfile);
        *hasCurrentFile = FALSE;
        if (curiter != fontpaths.end()) {
            HRESULT result = factory_->CreateFontFileReference(a2w(*curiter).c_str(), nullptr, &curfile);
            if (SUCCEEDED(result)) {
                *hasCurrentFile = TRUE;
            }
            ++curiter;
        }
        return S_OK;
    }
};

class fontfile_collection_loader : public IDWriteFontCollectionLoader
{
private:
    LONG refnum;
    static fontfile_collection_loader *inst;

private:
    fontfile_collection_loader(): refnum(0)
    {
        OutputDebugString("fontfile_collection_loader::fontfile_collection_loader");
    }

    virtual ~fontfile_collection_loader()
    {
        OutputDebugString("fontfile_collection_loader::~fontfile_collection_loader");
    }

public:

    static fontfile_collection_loader &get_instance()
    {
        if (inst == nullptr) {
            fontfile_collection_loader *temp_inst = new(std::nothrow) fontfile_collection_loader();
            if (InterlockedCompareExchange((LONG *)&inst, (LONG)temp_inst, (LONG)nullptr) != (LONG)nullptr) {
                delete temp_inst;
            }
        }
        return *inst;
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
    {
        if (IsEqualIID(riid, __uuidof(IUnknown)) || IsEqualIID(riid, __uuidof(IDWriteFontCollectionLoader))) {
            OutputDebugString("fontfile_collection_loader::QueryInterface");
            *ppvObject = iunknown_addref(this);
            return S_OK;
        } else {
            OutputDebugString("fontfile_collection_loader::QueryInterface failed");
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }
    }

    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        OutputDebugString("fontfile_collection_loader::AddRef");
        return InterlockedIncrement(&refnum);
    }

    virtual ULONG STDMETHODCALLTYPE Release()
    {
        OutputDebugString("fontfile_collection_loader::Release");
        ULONG newrefnum = InterlockedDecrement(&refnum);
        if (newrefnum == 0) {
            delete this;
        }
        return newrefnum;
    }

    virtual HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(
        IDWriteFactory* factory,
        void const* collectionKey,
        UINT32 collectionKeySize,
        IDWriteFontFileEnumerator** fontFileEnumerator
        )
    {
        OutputDebugString("fontfile_collection_loader::CreateEnumeratorFromKey");
        if(factory == nullptr || fontFileEnumerator == nullptr) {
            return E_INVALIDARG;
        }
        fontfile_enumerator* enumerator = new(std::nothrow) fontfile_enumerator(factory);
        if (enumerator != nullptr) {
            BOOL has_file;
            *fontFileEnumerator = (IDWriteFontFileEnumerator*)iunknown_addref(enumerator);
            return S_OK;
        } else {
            return E_OUTOFMEMORY;
        }
    }
};
fontfile_collection_loader *fontfile_collection_loader::inst;

class delegate_dwrite_gdi_interop : public IDWriteGdiInterop
{
private:
    LONG refnum;
    IDWriteGdiInterop *orig_this;
    IDWriteFontCollection *mycoll_;

public:
    delegate_dwrite_gdi_interop(IDWriteGdiInterop *inst, IDWriteFontCollection *mycoll) : refnum(0), orig_this(inst), mycoll_(iunknown_addref(mycoll))
    {
    }

    virtual ~delegate_dwrite_gdi_interop()
    {
        iunknown_release(mycoll_);
        iunknown_release(orig_this);
    }

    /* IUnknown methods */
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
    {
        if (IsEqualIID(riid, __uuidof(IUnknown)) || IsEqualIID(riid, __uuidof(IDWriteGdiInterop))) {
            OutputDebugString("delegate_dwrite_gdi_interop::QueryInterface");
            *ppvObject = iunknown_addref(this);
            return S_OK;
        } else {
            OutputDebugString("delegate_dwrite_gdi_interop::QueryInterface failed");
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }
    }

    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        OutputDebugString("delegate_dwrite_gdi_interop::AddRef");
        return InterlockedIncrement(&refnum);
    }

    virtual ULONG STDMETHODCALLTYPE Release()
    {
        OutputDebugString("delegate_dwrite_gdi_interop::Release");
        ULONG newrefnum = InterlockedDecrement(&refnum); 
        if (newrefnum == 0) {
            delete this;
        }
        return newrefnum;
    }

    /* IDWriteGdiInterop methods */
    virtual HRESULT STDMETHODCALLTYPE CreateFontFromLOGFONT(
        LOGFONTW const *logFont,
        IDWriteFont **font)
    {
        OutputDebugString("delegate_dwrite_gdi_interop::CreateFontFromLOGFONT");
        HRESULT result;
        UINT32 index;
        BOOL exists;
        result = mycoll_->FindFamilyName(logFont->lfFaceName, &index, &exists);
        if (SUCCEEDED(result)) {
            result = E_FAIL;
            if (exists != FALSE) {
                IDWriteFontFamily *family;
                result = mycoll_->GetFontFamily(index, &family);
                if (SUCCEEDED(result)) {
                    result = family->GetFirstMatchingFont(
                        (DWRITE_FONT_WEIGHT)logFont->lfWeight,
                        DWRITE_FONT_STRETCH_NORMAL,
                        DWRITE_FONT_STYLE_NORMAL,
                        font);
                    iunknown_release(family);
                }
            }
        }
	if (FAILED(result)) {
            OutputDebugString("delegate_dwrite_gdi_interop::CreateFontFromLOGFONT -> fallback");
            result = orig_this->CreateFontFromLOGFONT(logFont, font);
        }
        return result; 
    }


    virtual HRESULT STDMETHODCALLTYPE ConvertFontToLOGFONT(
        IDWriteFont *font,
        LOGFONTW *logFont,
        BOOL *isSystemFont)
    {
        return orig_this->ConvertFontToLOGFONT(font, logFont, isSystemFont); 
    }

    virtual HRESULT STDMETHODCALLTYPE ConvertFontFaceToLOGFONT(
        IDWriteFontFace *font,
        LOGFONTW *logFont)
    {
        return orig_this->ConvertFontFaceToLOGFONT(font, logFont); 
    }

    virtual HRESULT STDMETHODCALLTYPE CreateFontFaceFromHdc(
        HDC hdc,
        IDWriteFontFace **fontFace)
    {
        return orig_this->CreateFontFaceFromHdc(hdc, fontFace); 
    }

    virtual HRESULT STDMETHODCALLTYPE CreateBitmapRenderTarget(
        HDC hdc,
        UINT32 width,
        UINT32 height,
        IDWriteBitmapRenderTarget **renderTarget)
    {
        return orig_this->CreateBitmapRenderTarget(hdc, width, height, renderTarget); 
    }
};

class delegate_dwritefactory : public IDWriteFactory
{
private:
    LONG refnum;
    IDWriteFactory *orig_this;
    IDWriteFontCollection* mycoll;
    CRITICAL_SECTION collcs; 

public:
    delegate_dwritefactory(IDWriteFactory *inst) : refnum(0), orig_this(inst), mycoll(nullptr)
    {
        OutputDebugString("delegate_dwritefactory::delegate_dwritefactory");
	InitializeCriticalSection(&collcs);
        fontfile_collection_loader &loader = fontfile_collection_loader::get_instance();
        if (SUCCEEDED(orig_this->RegisterFontCollectionLoader(&loader))) {
            EnterCriticalSection(&fontpathscs);
            orig_this->CreateCustomFontCollection(&loader, nullptr, 0, &mycoll);
            LeaveCriticalSection(&fontpathscs);
        }
	factories.insert(this);
    }

    virtual ~delegate_dwritefactory()
    {
        OutputDebugString("delegate_dwritefactory::~delegate_dwritefactory");
	factories.erase(this);
        EnterCriticalSection(&collcs);
        iunknown_release(mycoll);
	LeaveCriticalSection(&collcs);
        DeleteCriticalSection(&collcs);
        iunknown_release(orig_this);
    }

    void update_font_collection()
    {
        fontfile_collection_loader &loader = fontfile_collection_loader::get_instance();
        EnterCriticalSection(&collcs);
        EnterCriticalSection(&fontpathscs);
        IDWriteFontCollection* coll;
        if (SUCCEEDED(orig_this->CreateCustomFontCollection(&loader, nullptr, 0, &coll))) {
            InterlockedExchange((LONG*)&mycoll, (LONG)coll);
	    iunknown_release(coll);
	}
        LeaveCriticalSection(&fontpathscs);
	LeaveCriticalSection(&collcs);
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
    {
        if (IsEqualIID(riid, __uuidof(IUnknown)) || IsEqualIID(riid, __uuidof(IDWriteFactory))) {
            OutputDebugString("delegate_dwritefactory::QueryInterface");
            *ppvObject = iunknown_addref(this);
            return S_OK;
        } else {
            OutputDebugString("delegate_dwritefactory::QueryInterface failed");
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }
    }

    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        OutputDebugString("delegate_dwritefactory::AddRef");
        return InterlockedIncrement(&refnum);
    }

    virtual ULONG STDMETHODCALLTYPE Release()
    {
        OutputDebugString("delegate_dwritefactory::Release");
        ULONG newrefnum = InterlockedDecrement(&refnum); 
        if (newrefnum == 0) {
            delete this;
        }
        return newrefnum;
    }

    virtual HRESULT STDMETHODCALLTYPE GetSystemFontCollection(
        IDWriteFontCollection** fontCollection,
        BOOL checkForUpdates = FALSE
        )
    {
        return orig_this->GetSystemFontCollection(fontCollection, checkForUpdates);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateCustomFontCollection(
        IDWriteFontCollectionLoader* collectionLoader,
        void const* collectionKey,
        UINT32 collectionKeySize,
        IDWriteFontCollection** fontCollection
        )
    {
        return orig_this->CreateCustomFontCollection(collectionLoader, collectionKey, collectionKeySize, fontCollection);
    }

    virtual HRESULT STDMETHODCALLTYPE RegisterFontCollectionLoader(
        IDWriteFontCollectionLoader* fontCollectionLoader
        )
    {
        return orig_this->RegisterFontCollectionLoader(fontCollectionLoader);
    }

    virtual HRESULT STDMETHODCALLTYPE UnregisterFontCollectionLoader(
        IDWriteFontCollectionLoader* fontCollectionLoader
        )
    {
        return orig_this->UnregisterFontCollectionLoader(fontCollectionLoader);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateFontFileReference(
        WCHAR const* filePath,
        FILETIME const* lastWriteTime,
        IDWriteFontFile** fontFile
        )
    {
        return orig_this->CreateFontFileReference(filePath, lastWriteTime, fontFile);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateCustomFontFileReference(
        void const* fontFileReferenceKey,
        UINT32 fontFileReferenceKeySize,
        IDWriteFontFileLoader* fontFileLoader,
        IDWriteFontFile** fontFile
        )
    {
        return orig_this->CreateCustomFontFileReference(fontFileReferenceKey, fontFileReferenceKeySize, fontFileLoader, fontFile);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateFontFace(
        DWRITE_FONT_FACE_TYPE fontFaceType,
        UINT32 numberOfFiles,
        IDWriteFontFile* const* fontFiles,
        UINT32 faceIndex,
        DWRITE_FONT_SIMULATIONS fontFaceSimulationFlags,
        IDWriteFontFace** fontFace
        )
    {
        return orig_this->CreateFontFace(fontFaceType, numberOfFiles, fontFiles, faceIndex, fontFaceSimulationFlags, fontFace);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateRenderingParams(
        IDWriteRenderingParams** renderingParams
        )
    {
        return orig_this->CreateRenderingParams(renderingParams);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateMonitorRenderingParams(
        HMONITOR monitor,
        IDWriteRenderingParams** renderingParams
        )
    {
        return orig_this->CreateMonitorRenderingParams(monitor, renderingParams);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateCustomRenderingParams(
        FLOAT gamma,
        FLOAT enhancedContrast,
        FLOAT clearTypeLevel,
        DWRITE_PIXEL_GEOMETRY pixelGeometry,
        DWRITE_RENDERING_MODE renderingMode,
        IDWriteRenderingParams** renderingParams
        )
    {
        return orig_this->CreateCustomRenderingParams(gamma, enhancedContrast, clearTypeLevel, pixelGeometry, renderingMode, renderingParams);
    }

    virtual HRESULT STDMETHODCALLTYPE RegisterFontFileLoader(
        IDWriteFontFileLoader* fontFileLoader
        )
    {
        return orig_this->RegisterFontFileLoader(fontFileLoader);
    }

    virtual HRESULT STDMETHODCALLTYPE UnregisterFontFileLoader(
        IDWriteFontFileLoader* fontFileLoader
        )
    {
        return orig_this->UnregisterFontFileLoader(fontFileLoader);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateTextFormat(
        WCHAR const* fontFamilyName,
        IDWriteFontCollection* fontCollection,
        DWRITE_FONT_WEIGHT fontWeight,
        DWRITE_FONT_STYLE fontStyle,
        DWRITE_FONT_STRETCH fontStretch,
        FLOAT fontSize,
        WCHAR const* localeName,
        IDWriteTextFormat** textFormat
        )
    {
        UINT32 index;
        BOOL exists;
        HRESULT result;
        OutputDebugString("delegate_dwritefactory::CreateTextFormat");
        OutputDebugStringW(fontFamilyName);
        EnterCriticalSection(&collcs);
        result = this->mycoll->FindFamilyName(fontFamilyName, &index, &exists);
        if (SUCCEEDED(result)) {
            result = E_FAIL;
            if (exists != FALSE) {
                OutputDebugString("delegate_dwritefactory::CreateTextFormat -> mycoll");
                result = orig_this->CreateTextFormat(fontFamilyName, this->mycoll, fontWeight, fontStyle, fontStretch, fontSize, localeName, textFormat);
            }
        }
        LeaveCriticalSection(&collcs);
	if (FAILED(result)) {
            OutputDebugString("delegate_dwritefactory::CreateTextFormat -> fallback");
            result = orig_this->CreateTextFormat(fontFamilyName, fontCollection, fontWeight, fontStyle, fontStretch, fontSize, localeName, textFormat);
        }
        return result;
    }

    virtual HRESULT STDMETHODCALLTYPE CreateTypography(
        IDWriteTypography** typography
        )
    {
        return orig_this->CreateTypography(typography);
    }

    virtual HRESULT STDMETHODCALLTYPE GetGdiInterop(
        IDWriteGdiInterop** gdiInterop
        )
    {
        HRESULT result;
        result = orig_this->GetGdiInterop(gdiInterop);
        if (SUCCEEDED(result)) {
            IDWriteGdiInterop *delegate = new(std::nothrow) delegate_dwrite_gdi_interop(*gdiInterop, mycoll);
            if (delegate != nullptr) {
                *gdiInterop = iunknown_addref(delegate);
            }
        }
        return result;
    }

    virtual HRESULT STDMETHODCALLTYPE CreateTextLayout(
        WCHAR const* string,
        UINT32 stringLength,
        IDWriteTextFormat* textFormat,
        FLOAT maxWidth,
        FLOAT maxHeight,
        IDWriteTextLayout** textLayout
        )
    {
        return orig_this->CreateTextLayout(string, stringLength, textFormat, maxWidth, maxHeight, textLayout);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateGdiCompatibleTextLayout(
        WCHAR const* string,
        UINT32 stringLength,
        IDWriteTextFormat* textFormat,
        FLOAT layoutWidth,
        FLOAT layoutHeight,
        FLOAT pixelsPerDip,
        DWRITE_MATRIX const* transform,
        BOOL useGdiNatural,
        IDWriteTextLayout** textLayout
        )
    {
        return orig_this->CreateGdiCompatibleTextLayout(string, stringLength, textFormat, layoutWidth, layoutHeight, pixelsPerDip, transform, useGdiNatural, textLayout);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateEllipsisTrimmingSign(
        IDWriteTextFormat* textFormat,
        IDWriteInlineObject** trimmingSign
        )
    {
        return orig_this->CreateEllipsisTrimmingSign(textFormat, trimmingSign);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateTextAnalyzer(
        IDWriteTextAnalyzer** textAnalyzer
        )
    {
        return orig_this->CreateTextAnalyzer(textAnalyzer);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateNumberSubstitution(
        DWRITE_NUMBER_SUBSTITUTION_METHOD substitutionMethod,
        WCHAR const* localeName,
        BOOL ignoreUserOverride,
        IDWriteNumberSubstitution** numberSubstitution
        )
    {
        return orig_this->CreateNumberSubstitution(substitutionMethod, localeName, ignoreUserOverride, numberSubstitution);
    }

    virtual HRESULT STDMETHODCALLTYPE CreateGlyphRunAnalysis(
        DWRITE_GLYPH_RUN const* glyphRun,
        FLOAT pixelsPerDip,
        DWRITE_MATRIX const* transform,
        DWRITE_RENDERING_MODE renderingMode,
        DWRITE_MEASURING_MODE measuringMode,
        FLOAT baselineOriginX,
        FLOAT baselineOriginY,
        IDWriteGlyphRunAnalysis** glyphRunAnalysis
        )
    {
        return orig_this->CreateGlyphRunAnalysis(glyphRun, pixelsPerDip, transform, renderingMode, measuringMode, baselineOriginX, baselineOriginY, glyphRunAnalysis);
    }
};

HRESULT WINAPI oncall_DWriteCreateFactory(DWRITE_FACTORY_TYPE factorytype, REFIID iid, IUnknown **factory)
{
    HRESULT result = orig_DWriteCreateFactory(factorytype, iid, factory);
    if (SUCCEEDED(result) && *factory != nullptr) {
        IUnknown *delegate = new(std::nothrow) delegate_dwritefactory((IDWriteFactory *)*factory);
        if (delegate != nullptr) {
            *factory = iunknown_addref(delegate);
        }
    }
    return result;
}

FARPROC WINAPI oncall_GetProcAddress(HMODULE module, LPCSTR procname)
{
    FARPROC result = GetProcAddress(module, procname);

    if (result != nullptr && lstrcmpi(procname, "DWriteCreateFactory") == 0) {
        orig_DWriteCreateFactory = (DWRITECREATEFACTORY_PROTO)result;
        return (FARPROC)oncall_DWriteCreateFactory;
    } else {
        return result;
    }
}

BOOL hook_getprocaddress()
{
    HMODULE vim;
    PIMAGE_DOS_HEADER doshdr;
    PIMAGE_NT_HEADERS32 nthdrs;
    PIMAGE_DATA_DIRECTORY impdir;
    PIMAGE_IMPORT_DESCRIPTOR impdesc;
    PIMAGE_IMPORT_DESCRIPTOR kernel32;
    PIMAGE_THUNK_DATA lookuptbl;
    PIMAGE_THUNK_DATA addrtbl;
    MEMORY_BASIC_INFORMATION bufinfo;
    BOOL succeeded;
    DWORD oldprotect;
    int retsize;
    int i;

    vim = GetModuleHandle(NULL);
    doshdr = (PIMAGE_DOS_HEADER)vim;
    if (doshdr->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    nthdrs = (PIMAGE_NT_HEADERS32)((char*)doshdr + doshdr->e_lfanew);
    if (nthdrs->Signature != IMAGE_NT_SIGNATURE) return FALSE;
    if (nthdrs->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) return FALSE;

    impdir = &nthdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impdir->Size == 0) return FALSE;

    impdesc = (PIMAGE_IMPORT_DESCRIPTOR)((char*)vim + impdir->VirtualAddress);
    kernel32 = NULL;
    for (i = 0; impdesc[i].Characteristics != 0; ++i) {
        LPCSTR libname = (LPCSTR)((char*)vim + impdesc[i].Name);
        if (lstrcmpi(libname, "KERNEL32.DLL") == 0) {
            kernel32 = &impdesc[i];
            break;
        }
    }
    if (kernel32 == NULL) return FALSE;

    lookuptbl = (PIMAGE_THUNK_DATA)((char*)vim + kernel32->OriginalFirstThunk);
    addrtbl = (PIMAGE_THUNK_DATA)((char*)vim + kernel32->FirstThunk);
    for (; lookuptbl->u1.Function != 0; ++lookuptbl, ++addrtbl) {
        if (!IMAGE_SNAP_BY_ORDINAL(lookuptbl->u1.Ordinal)) {
           PIMAGE_IMPORT_BY_NAME impfun = (PIMAGE_IMPORT_BY_NAME)((char*)vim + lookuptbl->u1.AddressOfData);
            if (strcmp((LPCSTR)impfun->Name, "GetProcAddress") == 0) {
                break;
            }
        }
    }
    if (lookuptbl->u1.Function == 0) return FALSE;

    orig_GetProcAddress = (GETPROCADDRESS_PROTO)addrtbl->u1.Function;
    retsize = VirtualQuery((LPCVOID)&addrtbl->u1.Function, &bufinfo, sizeof(bufinfo));
    if (retsize == 0) return FALSE;
    succeeded = VirtualProtect(bufinfo.BaseAddress, bufinfo.RegionSize, PAGE_READWRITE, &oldprotect);
    if (succeeded == FALSE) return FALSE;
    addrtbl->u1.Function = (DWORD_PTR)oncall_GetProcAddress;
    VirtualProtect(bufinfo.BaseAddress, bufinfo.RegionSize, bufinfo.Protect, &oldprotect);
    return TRUE; 
}

extern "C" {
HMODULE mymodule;

const char * dynamic_font(const char *arg)
{
    if (!hooked) {
        hook_getprocaddress();
        hooked = true;
        char mypath[MAX_PATH];
        GetModuleFileName(mymodule, mypath, sizeof mypath);
        LoadLibrary(mypath);
	InitializeCriticalSection(&fontpathscs);
    }
    int ret = AddFontResourceEx(arg, FR_PRIVATE, NULL);
    if (ret != 0) {
        EnterCriticalSection(&fontpathscs);
        fontpaths.push_back(arg);
	LeaveCriticalSection(&fontpathscs);
	for (std::set<IDWriteFactory*>::iterator iter = factories.begin(); iter != factories.end(); ++iter) {
		((delegate_dwritefactory*)*iter)->update_font_collection();
	}
        return "true";
    } else {
        return "false";
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpUnused)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        mymodule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}
}

