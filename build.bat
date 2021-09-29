@echo off

if not exist dist mkdir dist
pushd dist

cl.exe /Zi /nologo /I "W:\CppProject\CppLib\SDL2\include" /I "W:\CppProject\CppLib\GL\include" ../src/main.cpp ../src/vendor/microui.c ../src/template_sdl_microui_opengl3.c /link /LIBPATH:"W:\CppProject\CppLib\SDL2\lib\x64" /LIBPATH:"W:\CppProject\CppLib\GL\lib" user32.lib shell32.lib opengl32.lib SDL2.lib

popd
