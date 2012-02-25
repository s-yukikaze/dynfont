#include <windows.h>

#ifndef FR_PRIVATE
#define FR_PRIVATE 0x10
#endif

#ifndef AddFontResourceEx
int __stdcall AddFontResourceExA(const char *, int, void *);
#define AddFontResourceEx AddFontResourceExA
#endif

#undef DLLEXPORT
#ifdef _MSC_VER
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif


#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

extern BOOL exttextout_impl(HDC hdc, int x, int y, UINT options, const RECT *rc, LPCWSTR str, int len, const int *dx);

HMODULE mymodule = NULL;
ULONG ref = 0;

BOOL dwenabled = FALSE;
BOOL dwhooked = FALSE;

DWORD_PTR exttextout_orig;

BOOL WINAPI exttextout_hook(HDC hdc, int x, int y, UINT options, const RECT *rc, LPCWSTR str, int len, const int *dx)
{
    OutputDebugString("TextOut called");
    return exttextout_impl(hdc, x, y, options, rc, str, len, dx);
}

BOOL hook_textout()
{
    HMODULE vim;
    PIMAGE_DOS_HEADER doshdr;
    PIMAGE_NT_HEADERS32 nthdrs;
    PIMAGE_DATA_DIRECTORY impdir;
    PIMAGE_IMPORT_DESCRIPTOR impdesc;
    PIMAGE_IMPORT_DESCRIPTOR gdi32;
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
    gdi32 = NULL;
    for (i = 0; impdesc[i].Characteristics != 0; ++i) {
        LPCSTR libname = (LPCSTR)((char*)vim + impdesc[i].Name);
        if (strcasecmp(libname, "GDI32.DLL") == 0) {
            gdi32 = &impdesc[i];
            break;
        }
    }
    if (gdi32 == NULL) return FALSE;

    lookuptbl = (PIMAGE_THUNK_DATA)((char*)vim + gdi32->OriginalFirstThunk);
    addrtbl = (PIMAGE_THUNK_DATA)((char*)vim + gdi32->FirstThunk);
    for (; lookuptbl->u1.Function != 0; ++lookuptbl, ++addrtbl) {
        if (!IMAGE_SNAP_BY_ORDINAL(lookuptbl->u1.Ordinal)) {
           PIMAGE_IMPORT_BY_NAME impfun = (PIMAGE_IMPORT_BY_NAME)((char*)vim + lookuptbl->u1.AddressOfData);
            if (strcmp((LPCSTR)impfun->Name, "ExtTextOutW") == 0) {
                    break;
            }
        }
    }
    if (lookuptbl->u1.Function == 0) return FALSE;

    exttextout_orig = (DWORD_PTR)addrtbl->u1.Function;

    retsize = VirtualQuery((LPCVOID)&addrtbl->u1.Function, &bufinfo, sizeof(bufinfo));
    if (retsize == 0) return FALSE;
    succeeded = VirtualProtect(bufinfo.BaseAddress, bufinfo.RegionSize, PAGE_READWRITE, &oldprotect);
    if (succeeded == FALSE) return FALSE;
    addrtbl->u1.Function = (DWORD_PTR)exttextout_hook;
    VirtualProtect(bufinfo.BaseAddress, bufinfo.RegionSize, bufinfo.Protect, &oldprotect);
    return TRUE;
}

DLLEXPORT const char * enable_directwrite(const char *arg)
{
	char mymodulepath[MAX_PATH];

        if (strcmp(arg, "0") != 0) {
	    GetModuleFileName(mymodule, mymodulepath, MAX_PATH);
	    LoadLibrary(mymodulepath);
	    InterlockedIncrement(&ref);

            dwenabled = TRUE;
            return hook_textout()? "true" : "false";
        } else {
            dwenabled = FALSE;
	    return "true";
        }
}

DLLEXPORT const char * dynamic_font(const char *arg)
{
    int ret = AddFontResourceEx(arg, FR_PRIVATE, NULL);
    return ret != 0? "true": "false";
}

int WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpUnknown)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
	mymodule = hModule;
	ref = 1;
    }
    return TRUE;
}
