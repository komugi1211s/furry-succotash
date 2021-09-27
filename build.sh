if [ ! -d "dist" ]; then
    mkdir dist
fi
pushd dist
clang -fno-caret-diagnostics -fsanitize=undefined -g -o FurrySccotash ../src/main.c `sdl2-config --cflags --libs` -lGL 
popd
