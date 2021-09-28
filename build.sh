if [ ! -d "dist" ]; then
    mkdir dist
fi
clang -fno-caret-diagnostics -fsanitize=undefined -g -o dist/FurrySccotash src/main.c `sdl2-config --cflags --libs` -lGL 
