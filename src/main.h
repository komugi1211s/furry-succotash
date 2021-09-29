
#ifndef MAIN_H
#define MAIN_H

// ====================================
// Process handling.

struct Process_Handle;
Process_Handle create_handle_from_command(const char *command_as_chars);
char *separate_command_to_executable_and_args(const char *in, char *out_arg_list[], size_t arg_capacity);

void run_process(Process_Handle *handle);
void restart_process(Process_Handle *handle);
void read_from_process(Process_Handle *handle, char **malloced_log_buffers, size_t *buffer_capacity, size_t *buffer_count);
void sleep_ms(int ms);

/* Code below are functions that are currently confirmed to be required in Unix. */
int create_pipe(Process_Handle *handle);
void close_pipe(Process_Handle *handle);

// ====================================
// Files.

uint64_t find_latest_modified_time(char *path);


// ====================================
// Shared.

typedef struct Succotash Succotash;

struct Succotash {
    int32_t running;
    uint64_t last_modified_time;
};


typedef struct Log_Buffer Log_Buffer;
struct Log_Buffer {
    Mutex read_write_mtx;
    char *buffer_ptr;

    size_t capacity;
    size_t used;
}

#endif