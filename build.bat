@echo off

set VSTOOLS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
call %VSTOOLS%

set INCLUDE_DIR=/I"C:\Program Files (x86)\Microsoft DirectX SDK (August 2006)"
set LIB_DIR=/link /LIBPATH:"C:\Program Files (x86)\Microsoft DirectX SDK (August 2006)\Lib\x86"
set FLAGS=/Fe:valhalla.dll
set LIBS=User32.lib Kernel32.lib detours.lib d3d8.lib

cl.exe /LD main.cpp %LIBS% %FLAGS% %INCLUDE_DIR% %LIB_DIR% 