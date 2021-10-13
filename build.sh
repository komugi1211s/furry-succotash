if [ ! -d "dist" ]; then
    mkdir dist
fi
clang -fno-caret-diagnostics -c src/vendor/microui.c src/template_sdl_microui_opengl3.c
clang++ -fno-caret-diagnostics -g -o dist/FurrySccotash src/main.cpp microui.o template_sdl_microui_opengl3.o `sdl2-config --cflags --libs` -lGL 
