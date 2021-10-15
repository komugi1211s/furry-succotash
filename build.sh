if [ ! -d "dist" ]; then
    mkdir dist
fi

if [ ! -d "tmpfile" ]; then
    mkdir tmpfile 
fi

# ==============================
# compiling dependency as C
# ==============================
pushd tmpfile
clang -fno-caret-diagnostics -c ../src/vendor/microui.c
clang -fno-caret-diagnostics -c ../src/template_sdl_microui_opengl3.c
popd

# ==============================
# compiling main file as C++
# ==============================
clang++ -fno-caret-diagnostics -g -o dist/FurrySccotash src/main.cpp tmpfile/microui.o tmpfile/template_sdl_microui_opengl3.o `sdl2-config --cflags --libs` -lGL 

# ==============================
# Cleanup
# ==============================
rm -r ./tmpfile
