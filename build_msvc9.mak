all: autoload/dynfont.dll

autoload/dynfont.dll: autoload/dynfont.c
	cl /nologo /Os /MD /LD /GL /TC /Feautoload\dynfont.dll autoload\dynfont.c /link gdi32.lib 
        mt -nologo -manifest autoload\dynfont.dll.manifest -outputresource:autoload\dynfont.dll;#2

clean:
	cmd /c "del /F /Q dynfont.obj autoload\dynfont.dll autoload\dynfont.dll.manifest autoload\dynfont.lib autoload\dynfont.exp"

