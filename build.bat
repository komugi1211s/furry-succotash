@echo off

if not exist dist    mkdir dist
if not exist tmpfile mkdir tmpfile

set SDL_INCLUDE_PATH="W:\CppProject\CppLib\SDL2\include"
set SDL_LIB_PATH="W:\CppProject\CppLib\SDL2\lib\x64"

REM ==============================
REM compiling dependency as C
REM ==============================
pushd tmpfile
cl.exe /Zi /nologo                      /c /Fo"./microui.obj"                      ../src/vendor/microui.c 
cl.exe /Zi /nologo /I%SDL_INCLUDE_PATH% /c /Fo"./template_sdl_microui_opengl3.obj" ../src/template_sdl_microui_opengl3.c
popd

REM ==============================
REM compiling main file as C++
REM ==============================

pushd dist
set FILES=../src/main.cpp ../tmpfile/microui.obj ../tmpfile/template_sdl_microui_opengl3.obj
set LIBS=user32.lib shell32.lib Comdlg32.lib opengl32.lib SDL2.lib

cl.exe /Zi /nologo /I %SDL_INCLUDE_PATH% %FILES% /link /LIBPATH:%SDL_LIB_PATH% %LIBS%
popd
