#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>
#define Slp(n) (Sleep((n)*1000))
#else
#include <unistd.h>
#define Slp(n) sleep((n))
#endif

int main(void) {
    printf("Started Running!\n");
    for (int i = 0; i < 5; ++i) {
        Slp(2);
        printf("Hello!\n");
    }
    return 0;
}
