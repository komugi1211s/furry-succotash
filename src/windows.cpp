#include <windows.h>
#include "main.h"

typedef HANDLE Thread_Handle;
typedef HANDLE Mutex;

typedef DWORD (*thread_proc)(void *params);

void sleep_ms(int ms) {
    Sleep(ms);
}

struct Process_Handle {
    int valid;
};
