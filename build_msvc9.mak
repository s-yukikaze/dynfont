all: autoload/dynfont.dll

autoload/dynfont.dll: autoload/dynfont.c autoload/exttextout.cpp
	cl /nologo /c /Os /MD /LD /GL /Tcautoload\dynfont.c
	cl /nologo /c /Os /MD /LD /GL /EHsc /Tpautoload\exttextout.cpp
	link /dll /LTCG /out:autoload\dynfont.dll dynfont.obj exttextout.obj gdi32.lib dwrite.lib
    mt -nologo -manifest autoload\dynfont.dll.manifest -outputresource:autoload\dynfont.dll;#2

clean:
	cmd /c "del /F /Q dynfont.obj exttextout.obj autoload\dynfont.dll autoload\dynfont.dll.manifest autoload\dynfont.lib autoload\dynfont.exp"

