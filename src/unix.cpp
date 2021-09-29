/* ======================= */
// Linux. 
/* ======================= */
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "main.h"

typedef pthread_t       Thread_Handle;
typedef pthread_mutex_t Mutex;

struct Process_Handle {
    int32_t valid;
    const char *command;

    int child_pid;
    int reading_pipe[2];

};

// ====================================
// Process handling.


Process_Handle create_handle_from_command(const char *command_as_chars){
    Process_Handle handle = {0};
    if (strlen(command_as_chars) == 0) {
        return handle;
    }

    handle.command = command_as_chars;
    handle.valid   = 1;
    return handle;
}

char *separate_command_to_executable_and_args(const char *in, char *out_arg_list[], size_t arg_capacity) {
    char *copied_string = strdup(in);
    char *current_ptr   = copied_string;
    size_t arg_count    = 0;

    char *executable_command = strsep(&current_ptr, " ");
    for(;;) {
        char *argument = strsep(&current_ptr, " ");
        if (!current_ptr || !argument || arg_count >= arg_capacity) break;
        out_arg_list[arg_count++] = argument;
    }

    return executable_command;
}

void run_process(Process_Handle *handle) {
    // Create Argument list.

    if (!create_pipe(handle)) {
        return;
    }

    char *arg_list[32] = {0};
    char *exec_command = separate_command_to_executable_and_args(handle->command, arg_list, 32);

    int pid = fork();
    switch(pid) {
        case -1:
        {
            close_pipe(handle);
            free(exec_command);
            handle->valid = 0;
            return;
        } break;

        case 0:
        {
            close(handle->reading_pipe[0]);
            handle->reading_pipe[0] = 0;

            dup2(handle->reading_pipe[1], STDOUT_FILENO);
            execvp(exec_command, arg_list);

            // Unreachable!
            assert(false && "Unreachable:: Process running failed.");
            exit(127);
        } break;

        default:
        {
            handle->child_pid = pid;
            close(handle->reading_pipe[1]);
            handle->reading_pipe[1] = 0;
        } break;
    }

    free(exec_command);
    return;
}

void restart_process(Process_Handle *handle) {
    if (!handle->valid) return;

    kill(handle->child_pid, SIGTERM);
    handle->child_pid = -1;

    close_pipe(handle);
    run_process(handle);
}

void read_from_process(Process_Handle *handle, char **malloced_log_buffers, size_t *buffer_capacity, size_t *buffer_used) {
    char message[1024] = {0};
    size_t read_amount = 0;

    ssize_t read_amount_or_error = read(handle->reading_pipe[0], &message, sizeof(message)-1);
    if (read_amount_or_error < 0) {
        printf("Failed to read: %s\n", strerror(errno));
        return;
    }

    read_amount = (size_t) read_amount_or_error;

    while (read_amount > 0) {
        size_t found_newline_index = 0;

        // Extend a buffer so it fits.
        while ((read_amount + *buffer_used) > *buffer_capacity) {
            printf("Warning: Buffer size limited: need %" PRIu64 " but has %" PRIu64 ". expanding to power of 2\n", (read_amount + *buffer_used), *buffer_capacity);
            char *new_ptr = (char *)realloc(*malloced_log_buffers, (*buffer_capacity) * 2);
            assert(new_ptr && "Realloc failed, shouldn't continue.");

            *malloced_log_buffers = new_ptr;
            (*buffer_capacity) *= 2;
        }

        // Finding a Newline.
        for (size_t index = 0; index < read_amount; ++index) {
            if (message[index] == '\n' || message[index] == 0) {
                found_newline_index = index;
                break;
            }
        }

        char *reading_ptr = message;
        char *logs = *malloced_log_buffers;

        // Write into stdout if you've found a newline.
        if (found_newline_index > 0) {
            strncat(logs, reading_ptr, found_newline_index + 1);
            printf("[Logs] %s", logs);
            reading_ptr += found_newline_index;
            read_amount -= found_newline_index;

            memset(logs, 0, *buffer_capacity);
            *buffer_used = 0;
        }

        // Concatenate the rest.
        strcat(logs, reading_ptr);
        *buffer_used += read_amount;

        memset(message, 0, sizeof(message));
        read_amount_or_error = read(handle->reading_pipe[0], &message, sizeof(message)-1);
        if (read_amount_or_error < 0) {
            if (errno == EAGAIN) {
                // expected when the result is empty.
                return;
            }
            printf("Failed to read below: %s", strerror(errno));
            return;
        }
        read_amount = (size_t) read_amount_or_error;
    }
}

int create_pipe(Process_Handle *handle) {
    assert(handle->valid && "Cannot create pipe for invalid handle.");
    const int PIPE_SUCCESS = 0;

    if (pipe(handle->reading_pipe)) {
        handle->valid = 0;
        return 0;
    }

    /*
    Let's make it blocking and use mutex instead!
    if (fcntl(handle->reading_pipe[0], F_SETFL, O_NONBLOCK)) {
        handle->valid = 0;
        close(handle->reading_pipe[0]);
        close(handle->reading_pipe[1]);
        return 0;
    }
    */

    return 1;
}

void close_pipe(Process_Handle *handle) {
    if (handle->reading_pipe[0] != 0) {
        close(handle->reading_pipe[0]);
        handle->reading_pipe[0] = 0;
    }
    if (handle->reading_pipe[1] != 0) {
        close(handle->reading_pipe[1]);
        handle->reading_pipe[1] = 0;
    }
}


// ====================================
// Files.


uint64_t find_latest_modified_time(char *path) {
    if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0) return 0;
    size_t file_path_length = strlen(path);

    struct stat status;
    if (stat(path, &status) == -1) {
        printf("failed to load path by stat: %s, path: %s\n", strerror(errno), path);
        return 0;
    }

    if (S_ISDIR(status.st_mode)) {
        DIR *dir = opendir(path);
#define FILEPATH_SIZE 2048

        if (dir) {
            uint64_t current_latest = 0;
            for(struct dirent *file_entry = readdir(dir); file_entry; file_entry = readdir(dir))
            {
                if (file_entry) { // NOTE: do i need to do this?
                    // Skip current / past directory
                    if (strcmp(file_entry->d_name, ".") == 0 ||
                        strcmp(file_entry->d_name, "..") == 0) continue;

                    size_t filename_length = strlen(file_entry->d_name);
                    size_t total_filepath_length = filename_length + file_path_length;

                    if (total_filepath_length < FILEPATH_SIZE) {
                        char filepath[FILEPATH_SIZE] = {0};
                        snprintf(filepath, FILEPATH_SIZE, "%s/%s", path, file_entry->d_name);
                        uint64_t file_time = find_latest_modified_time(filepath); // recursive call

                        current_latest = (file_time > current_latest) ? file_time : current_latest;
                    } else {
                        printf("failed to open file: file path length too long\n");
                    }
                }
            }
            closedir(dir);
            return current_latest;
        } else {
            printf("Not a directory :( : %s\n", strerror(errno));
            return 0;
        }
    } else {
#define ModTime(mStatus) ((uint64_t)((mStatus).st_mtim.tv_sec * 1000000000llu + (mStatus).st_mtim.tv_nsec))
        uint64_t time = ModTime(status);
        return time;
    }
}

void sleep_ms(int ms) {
    usleep(ms * 1000);
}