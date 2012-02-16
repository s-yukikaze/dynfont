#include <windows.h>

#ifndef FR_PRIVATE
#define FR_PRIVATE 0x10
#endif

#ifndef AddFontResourceEx
int __stdcall AddFontResourceExA(const char *, int, void *);
#define AddFontResourceEx AddFontResourceExA
#endif

const char * dynamic_font(const char *arg)
{
    int ret = AddFontResourceEx(arg, FR_PRIVATE, NULL);
    return ret != 0: "true": "false";
}

