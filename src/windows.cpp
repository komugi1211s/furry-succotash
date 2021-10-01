#include <windows.h>
#include "main.h"

struct Thread_Handle {
    HANDLE inner;
};

struct Process_Handle {
    const char          *command;
    PROCESS_INFORMATION procinfo;
};

Process_Handle create_handle_from_command(const char *command_as_chars) {
    Process_Handle handle = {0};

    handle.command = command_as_chars;
    ZeroMemory(&handle.procinfo, sizeof(PROCESS_INFORMATION));

    return handle;
}

char *strsep(char **stringp, const char *delim) {
    for (char *current = *stringp; *current; ++current) {
        for (char *delimiter = (char *)delim; *delimiter; ++delimiter) {
            if (*current == *delimiter) {
                *current = '\0';
                *stringp = current + 1;
                return current;
            }
        }
    }

    char *not_found = *stringp;
    *stringp = NULL;
    return not_found;
}

int is_process_running(Process_Handle *handle) {
    PROCESS_INFORMATION empty = {0};

    return (
        handle->procinfo.hProcess != empty.hProcess
    );
}

char *separate_command_to_executable_and_args(const char *in, char *out_arg_list[], size_t arg_capacity) {
    char *copied_string = strdup(in);
    char *current_ptr   = copied_string;
    size_t arg_count    = 0;

    char *executable_command = strsep(&current_ptr, " ");
    if (!current_ptr) {
        return executable_command;
    }

    for(;;) {
        char *argument = strsep(&current_ptr, " ");
        if (!current_ptr || !argument || arg_count >= arg_capacity) break;
        out_arg_list[arg_count++] = argument;
    }

    return executable_command;
}

void start_process(Process_Handle *handle, Log_Buffer *buffer) {
    STARTUPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.cb = sizeof(info);

    char *arg_list[32] = {0};
    size_t arg_capacity = 32; 

    char *exec_command = separate_command_to_executable_and_args(handle->command,
                                                                 arg_list,
                                                                 arg_capacity);
    printf("Running process: %s\n", exec_command);
    BOOL created = CreateProcess(exec_command,
                                 (TCHAR *)arg_list,
                                 NULL,
                                 NULL,
                                 FALSE,
                                 0,
                                 NULL,
                                 NULL,
                                 &info,
                                 &handle->procinfo);

    free(exec_command);
    if (!created) {
        printf("error code: %d\n", GetLastError());
        ZeroMemory(&handle->procinfo, sizeof(handle->procinfo));
    }
    return;
}


uint64_t find_latest_modified_time(char *filepath) {
    return 0;
}

void sleep_ms(int ms) {
    Sleep(ms);
}
