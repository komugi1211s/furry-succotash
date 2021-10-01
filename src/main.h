#ifndef MAIN_H
#define MAIN_H
#include <stdlib.h>
#include <stdint.h>

// ====================================
// Shared.

typedef struct Thread_Handle Thread_Handle;

typedef struct Succotash Succotash;

struct Succotash {
    int32_t running;
    uint64_t last_modified_time;
};


typedef struct Log_Buffer Log_Buffer;

struct Log_Buffer {
    char *buffer_ptr;
    size_t capacity;
    size_t used;
};

#define THREAD_TASK(name) void *name(void *arguments);

// ====================================
// Process handling.

struct Process_Handle;
Process_Handle create_handle_from_command(const char *command_as_chars);
char *separate_command_to_executable_and_args(const char *in, char *out_arg_list[], size_t arg_capacity);

void start_process(Process_Handle *handle, Log_Buffer *buffer);
void restart_process(Process_Handle *handle, Log_Buffer *buffer);
void terminate_process(Process_Handle *handle); // try to terminate the process whether it's alive or not.

int  is_process_running(Process_Handle *handle);
void sleep_ms(int ms);


/* Code below are functions that are currently confirmed to be required in Unix. */
int create_pipe(Process_Handle *handle);
void close_pipe(Process_Handle *handle);

// ====================================
// Threading.
typedef struct Thread_Handle Thread_Handle;
Thread_Handle start_stdout_thread(Process_Handle *handle, Log_Buffer *buffer);
void handle_stdout_task(void *ptr);


// ====================================
// Files.

uint64_t find_latest_modified_time(char *path);


#endif
