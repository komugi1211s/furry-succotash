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

typedef struct {
    int state;
    pthread_mutex_t mutex;
} Channel;

struct Thread_Handle {
    pthread_t inner;
};

struct Process_Handle {
    const char *command;

    int child_pid;
    int reading_pipe[2];

    Thread_Handle stdout_thread;
};

typedef struct {
    int read_fd;
    Logger *output;
} Thread_Info;

// ====================================
// Process handling.

Process_Handle create_handle_from_command(const char *command_as_chars){
    Process_Handle handle = {0};
    if (strlen(command_as_chars) == 0) {
        return handle;
    }

    handle.command   = command_as_chars;
    handle.child_pid = -1;
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

void start_process(Process_Handle *handle, Logger *logger) {
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


    handle->stdout_thread = start_stdout_thread(handle, buffer);
    free(exec_command);
    return;
}

void restart_process(Process_Handle *handle, Logger *logger) {
    if (handle->child_pid == 0 || handle->child_pid == -1) return;

    kill(handle->child_pid, SIGTERM);
    close_pipe(handle);

    handle->child_pid = -1;

    start_process(handle, buffer);
}

int is_process_running(Process_Handle *handle) {
    return handle->child_pid != -1;
}

// repeatedly read stdout from process.
// should be used within the thread.

typedef struct {
    Process_Handle *handle;
    Logger *logger;
} stdout_task_thr_info;

void handle_stdout_for_process(void *ptr) {
    stdout_task_thr_info info = *(stdout_task_thr_info *)ptr;
    delete (stdout_task_thr_info *)(ptr);

    Process_Handle *handle = info.handle;
    Logger *Logger = info.buffer;
    handle->child_pid = -1;

    return;

    char message[1024] = {0};
    size_t read_amount = 0;

    ssize_t read_amount_or_error = read(handle->reading_pipe[0], &message, sizeof(message)-1);
    if (read_amount_or_error < 0) {
        printf("Failed to read: %s\n", strerror(errno));
        return;
    }

    read_amount = (size_t) read_amount_or_error;

    while (read_amount > 0) {
        // Extend buffer accordingly.
        while((Logger->used + read_amount) > Logger->capacity) {
            char *new_ptr = (char *)realloc(Logger->buffer_ptr, Logger->capacity * 2);
            assert(new_ptr && "Realloc failed, shouldn't continue.");

            Logger->buffer_ptr = new_ptr;
            Logger->capacity   = Logger->capacity * 2;
        }

        // Finding a Newline.
        size_t found_newline_index = 0;

        for (size_t index = 0; index < read_amount; ++index) {
            if (message[index] == '\n' || message[index] == 0) {
                found_newline_index = index;
                break;
            }
        }

        char *reading_ptr = message;

        // Write into stdout if you've found a newline.
        if (found_newline_index > 0) {
            strncat(Logger->buffer_ptr, reading_ptr, found_newline_index + 1);
            printf("[Logs] %s", Logger->buffer_ptr);
            reading_ptr += found_newline_index;
            read_amount -= found_newline_index;

            memset(Logger->buffer_ptr, 0, Logger->capacity);
            Logger->used = 0;
        }

        // Concatenate the rest.
        strcat(Logger->buffer_ptr, reading_ptr);
        Logger->used += read_amount;

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

// Create pipe for given handle.
// crashes on invalid handle.
int create_pipe(Process_Handle *handle) {
    assert(handle->child_pid == -1 && "Cannot create pipe for alive handle.");
    const int PIPE_SUCCESS = 0;

    if (pipe(handle->reading_pipe)) {
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

// Close given pipe completely.
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
// Threading.


void *stdout_task(void *arg) {
    printf("begin\n");
    handle_stdout_for_process(arg);
    printf("end\n");

    pthread_exit(NULL);
    return NULL;
}

Thread_Handle start_stdout_thread(Process_Handle *handle, Logger *logger) {
    Thread_Handle thread = {0};

    stdout_task_thr_info *i = new stdout_task_thr_info;
    i->handle = handle;
    i->buffer = buffer;

    pthread_create(&thread.inner, NULL, stdout_task, (void *)i); // problem!
    return thread;
}

// ====================================
// Files.

// Check if given path is forbidden to process.
bool is_forbidden_path(char *path) {
    return (
        strcmp(path, ".")  == 0 ||
        strcmp(path, "..") == 0
    );
}

uint64_t find_latest_modified_time(char *path) {
    if (is_forbidden_path(path)) return 0;
    size_t path_length = strlen(path);


    // Get file's information, returning on failure
    struct stat status;
    if (stat(path, &status) == -1) {
        printf("failed to load path by stat: %s, path: %s\n", strerror(errno), path);
        return 0;
    }

    if (S_ISDIR(status.st_mode)) {
        DIR *dir = opendir(path);

        if (dir) {
            uint64_t current_latest = 0;
            for(struct dirent *file_entry = readdir(dir); file_entry; file_entry = readdir(dir))
            {
                if (is_forbidden_path(file_entry->d_name)) continue;

                size_t name_length  = strlen(file_entry->d_name);
                size_t total_length = name_length + path_length;

                if (total_length < 1024) {
                    char filepath[1024] = {0};
                    snprintf(filepath, 1024, "%s/%s", path, file_entry->d_name);
                    uint64_t file_time = find_latest_modified_time(filepath); // recursive call

                    current_latest = (file_time > current_latest) ? file_time : current_latest;
                } else {
                    printf("failed to open file: file path length too long\n");
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
