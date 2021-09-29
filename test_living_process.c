#include <raylib.h>

int main(int argc, char **argv) {
    InitWindow(200, 300, "testwindow");

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(WHITE);
        EndDrawing();
    }

    CloseWindow();
}
