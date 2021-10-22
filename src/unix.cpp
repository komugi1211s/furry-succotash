/* ======================= */
// Linux. 
/* ======================= */
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "main.h"

struct Process_Handle {
    pid_t child_pid;
    FILE *output_pipe;
};

volatile sig_atomic_t force_stop = 0;

void handle_signal(int signal) {
    force_stop = 1;
}

int32_t platform_app_should_close() {
    return force_stop == 1;
}

void platform_init() {
    struct sigaction action = {0};
    action.sa_handler = handle_signal;
    
    if (sigaction(SIGINT, &action, NULL) == -1) {
        int err = errno;
        perror("failed on platform_init() -> sigaction(SIGINT...))");
        fprintf(stderr, "failed on platform_init() -> sigaction(SIGINT...)): %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &action, NULL) == -1) {
        int err = errno;
        perror("failed on platform_init() -> sigaction(SIGTERM...))");
        fprintf(stderr, "failed on platform_init() -> sigaction(SIGTERM...)): %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
}

// ====================================
// Process handling.

Process_Handle create_process_handle() {
    Process_Handle handle = {0};
    handle.child_pid = -1;
    return handle;
}

void destroy_process_handle(Process_Handle *handle) {
    terminate_process(handle);
}

char *separate_command_to_executable_and_args(const char *in, char **out_arg_list, size_t arg_capacity) {
    char *copied_string = strdup(in);
    char *current_ptr   = copied_string;
    size_t arg_count    = 0;

    char *executable_command = strsep(&current_ptr, " ");
    out_arg_list[arg_count++] = executable_command;
    while(current_ptr && *current_ptr) {
        char *argument = strsep(&current_ptr, " ");
        if (!argument || arg_count >= arg_capacity)  {
            break;
        }
        out_arg_list[arg_count++] = argument;
    }

    return executable_command;
}

int32_t start_process(const char *working_dir, const char *command, Process_Handle *handle, Logger *logger) {
    int stdout[2] = {0};

    if (pipe(stdout) != 0) {
        int err = errno;
        watcher_log("failed to create a pipe: errno %s", strerror(err));
        return 0;
    }

    // Create Argument list.
    char *arg_list[32] = {0};
    char *exec_command = separate_command_to_executable_and_args(command, arg_list, 32);

    pid_t pid = fork();
    int err = errno;

    if (pid == -1) {
        watcher_log("Failed to create a process via fork: errno %s", strerror(err));
        close(stdout[0]);
        close(stdout[1]);
        free(exec_command);
        return 0;
    }

    // Child Process.
    if (pid == 0) {
        int chresult = chdir(working_dir);
        int cherr = errno;
        if (chresult == -1) {
            fprintf(stderr, "Failed to set a working directory. errno = %s\n", strerror(cherr));
            sleep_ms(500);
            exit(EXIT_FAILURE);
        }

        // Set the process group to new distinct one so that
        // I can specify the entire group to kill the grandchildren process altogether.
        int process_group_set_result = setpgid(0, 0);
        int pgerr = errno;

        if (process_group_set_result == -1) {
            fprintf(stderr, "Failed to set setpgid. errno = %s\n", strerror(pgerr));
            // Wait for a little bit so that I can prevent the process creation spam.
            sleep_ms(500);
            exit(EXIT_FAILURE);
        }

        // Setup the pipe.
        close(stdout[0]);
        dup2(stdout[1], STDERR_FILENO);
        dup2(STDERR_FILENO, STDOUT_FILENO);

        execvp(exec_command, (char *const *)arg_list); // arg_list);

        // Failed to run a process. I shouldn't be here!
        int err = errno;
        fprintf(stderr, "Failed to start a process. errno = %s\n", strerror(err));

        // Wait for a little bit so that I can prevent the process creation spam.
        sleep_ms(500);
        exit(EXIT_FAILURE);
    }

    // Parent process!

    close(stdout[1]);
    free(exec_command);

    handle->output_pipe = fdopen(stdout[0], "rb");
    handle->child_pid   = pid;
    watcher_log("started a new process: pid = %d", pid);

    return 1;
}

/*
 * Kills the child process if alive and releases the process handle.
 * */
void terminate_process(Process_Handle *handle) {
    if (handle->child_pid == 0 || handle->child_pid == -1) return;
    int kill_result = kill(-handle->child_pid, SIGTERM);
    int err = errno;

    if (kill_result == -1) {
        // sanitize handle if the process is already dead and does not exist.
        if (err == ESRCH) {
            goto clear_handle;
        }

        fprintf(stderr, "Failed to kill a process. error: %d\n", err);
        exit(EXIT_FAILURE);
    }

    {
        int status;
        int wait_result = waitpid(-handle->child_pid, &status, 0);
        int werr = errno;

        if (wait_result == -1) {
            fprintf(stderr, "Failed to wait a process. error: %d\n", werr);
            exit(EXIT_FAILURE);
        }
    }

clear_handle:
    fclose(handle->output_pipe);

    handle->child_pid = -1;
    handle->output_pipe = NULL;
}

int get_process_status(Process_Handle *handle, int *process_status) {
    if (handle->child_pid == -1) {
        *process_status = PROCESS_NOT_RUNNING;
        return 0;
    }

    int status = 0;
    int result = waitpid(-handle->child_pid, &status, WNOHANG);
    int err = errno;

    int32_t retry_count = 0;
    int32_t retry_cap = 3;
    while (result == -1) {
        if (err == ECHILD) {
            // This can happen if the parent process checks the status of child process
            // immediately after the process has been created.
            // wait a little bit and check again, re-try it for few more times,
            // then bail out.
            retry_count++;
            if (retry_count > retry_cap) {
                fprintf(stderr, "error occurred while checking the status of child process: %s\n, pid = %d\n", strerror(err), handle->child_pid);
                exit(EXIT_FAILURE);
            }

            sleep_ms(50);
            result = waitpid(-handle->child_pid, &status, WNOHANG);
        } else {
            fprintf(stderr, "error occurred while checking if the process is running: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }
    }

    if (result == 0) {
        *process_status = PROCESS_STILL_ALIVE;
        return 1;
    }

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) == EXIT_SUCCESS) {
            *process_status = PROCESS_DIED_CORRECLTLY;
        } else {
            *process_status = PROCESS_DIED_ERROR;
        }
        return 0;
    } else if (WIFSIGNALED(status)) {
        *process_status = PROCESS_DIED_KILLED;
        return 0;
    }
    fprintf(stderr, "still alive for some reason.\n");
    return 1;
}


int32_t zenity_to_select_file_or_folder(char *outbuf, size_t outbuf_size, int32_t folder_selection) {
    if (outbuf_size < 512) {
        return 0;
    }

    char buffer[512] = {0};
    FILE *zenity = 0;
    if (folder_selection) {
        zenity = popen("zenity --file-selection --directory --title=\"Select a Folder...\"", "r");
    } else {
        zenity = popen("zenity --file-selection --title=\"Select a File...\"", "r");
    }

    if (zenity) {
        fread(buffer, 511, 1, zenity);
        pclose(zenity);
        memset(outbuf, 0, outbuf_size);
        memcpy(outbuf, buffer, 512);

        size_t last_char = strlen(outbuf);
        if(outbuf[last_char - 1] == '\n') {
            outbuf[last_char - 1] = '\0';
        }
        return 1;
    }
    return 0;
}

int32_t select_new_folder(char *folder_buffer, size_t folder_buffer_size) {
    return zenity_to_select_file_or_folder(folder_buffer, folder_buffer_size, 1);
}

int32_t select_file(char *file_buffer, size_t file_buffer_size) {
    return zenity_to_select_file_or_folder(file_buffer, file_buffer_size, 0);
}

int32_t to_full_paths(char *paths_buffer, size_t paths_buffer_size) {
    return 1;
}

size_t handle_stdout_for_process(Process_Handle *handle, char *buffer, size_t buffer_open) {
    if (!handle->output_pipe) {
        return 0;
    }

    int32_t file_no = fileno(handle->output_pipe);
    fd_set sets;
    FD_ZERO(&sets);
    FD_SET(file_no, &sets);
    struct timeval timeout = {0};
    timeout.tv_usec = 100;
    int ready = select(file_no + 1, &sets, 0, 0, &timeout);
    int rerr = errno;

    if (ready == -1) {
        fprintf(stderr, "Failed to select a file discriptor: errno = %s\n", strerror(rerr));
        exit(EXIT_FAILURE);
    }

    if (ready == 0 || !(FD_ISSET(file_no, &sets))) {
        return 0;
    }

    ssize_t bytes_read = read(file_no, buffer, buffer_open - 1);
    if (bytes_read < 0) {
        return 0;
    } else {
        return (size_t)bytes_read;
    }
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

uint64_t find_latest_modified_time(Logger *logger, char *filepath) {
    if (is_forbidden_path(filepath)) return 0;
    size_t path_length = strlen(filepath);


    // Get file's information, returning on failure
    struct stat status;
    if (stat(filepath, &status) == -1) {
        watcher_log("failed to load path by stat: %s, path: %s\n", strerror(errno), filepath);
        return 0;
    }

    if (S_ISDIR(status.st_mode)) {
        DIR *dir = opendir(filepath);

        if (dir) {
            uint64_t current_latest = 0;
            for(struct dirent *file_entry = readdir(dir); file_entry; file_entry = readdir(dir))
            {
                if (is_forbidden_path(file_entry->d_name)) continue;

                size_t name_length  = strlen(file_entry->d_name);
                size_t total_length = name_length + path_length;

                if (total_length < 1024) {
                    char new_filepath[1024] = {0};
                    snprintf(new_filepath, 1023, "%s/%s", filepath, file_entry->d_name);
                    uint64_t file_time = find_latest_modified_time(logger, new_filepath);

                    current_latest = (file_time > current_latest) ? file_time : current_latest;
                }
            }

            closedir(dir);
            return current_latest;
        } else {
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
