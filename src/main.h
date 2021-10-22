#ifndef MAIN_H
#define MAIN_H
#include <stdlib.h>
#include <stdint.h>

// ====================================
// Shared.

typedef struct Succotash Succotash;
typedef struct Logger Logger;

#define LOG_BUFFER_LINE_SIZE   2048
#define LOG_BUFFER_BUCKET_SIZE 256

struct Logger {
    char   logs[LOG_BUFFER_LINE_SIZE][LOG_BUFFER_BUCKET_SIZE];
    size_t logs_begin;
    size_t logs_end;
};

void watcher_log(const char *message, ...);

int32_t platform_app_should_close();
void platform_init();

// ====================================
// Process handling.

struct Process_Handle;

Process_Handle create_process_handle();
void destroy_process_handle(Process_Handle *handle);
char *separate_command_to_executable_and_args(const char *in, char *out_arg_list[], size_t arg_capacity);

int32_t start_process(const char *working_dir, const char *command, Process_Handle *handle, Logger *logger);
void terminate_process(Process_Handle *handle); // try to terminate the process whether it's alive or not.

int  get_process_status(Process_Handle *handle, int *process_status);
void sleep_ms(int ms);

enum {
    PROCESS_NOT_RUNNING,
    PROCESS_STILL_ALIVE,
    PROCESS_DIED_ERROR,
    PROCESS_DIED_KILLED,
    PROCESS_DIED_CORRECLTLY,
    PROCESS_MAX,
};

size_t handle_stdout_for_process(Process_Handle *handle, char *buffer, size_t buffer_open);

int create_pipe(Process_Handle *handle);
void close_pipe(Process_Handle *handle);

// ====================================
// Threading.
/*
typedef struct Thread_Handle Thread_Handle;
Thread_Handle start_stdout_thread(Process_Handle *handle, Logger *logger);
void handle_stdout_task(void *ptr);
*/

// ====================================
// Files.

uint64_t find_latest_modified_time(Logger *logger, char *path);
int32_t select_new_folder(char *folder_buffer, size_t folder_buffer_size);
int32_t select_file(char *file_buffer, size_t file_buffer_size);
int32_t to_full_paths(char *path_buffer, size_t path_buffer_size);

#endif
