#include <Commdlg.h>
#include <ShlObj.h>
#include <Windows.h>
#include "main.h"

struct Thread_Handle {
    HANDLE inner;
};

struct Process_Handle {
    int32_t valid;
    PROCESS_INFORMATION procinfo;

    HANDLE read_pipe;
    HANDLE write_pipe;
};

int create_pipe(Process_Handle *process) {
    return 1; // giving up actually handling stdout for now. I have to think how to do it

    char pipe_name[256] = {0};
    sprintf(pipe_name, "\\\\.\\pipe\\Pipe%08x", GetCurrentProcessId());

    SECURITY_ATTRIBUTES attr = {0};
    attr.nLength = sizeof(attr);
    attr.bInheritHandle = TRUE;
    attr.lpSecurityDescriptor = NULL;
    
    HANDLE read = CreateNamedPipe(pipe_name,
                                   PIPE_ACCESS_INBOUND,
                                   PIPE_TYPE_BYTE | PIPE_WAIT,
                                   1,
                                   1024,
                                   1024,
                                   4 * 1000,
                                   &attr);

    if (read == INVALID_HANDLE_VALUE) {
        printf("failed to create read pipe. Num: %d\n", GetLastError());
        return 0;
    }

    if (!SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0)) {
        printf("failed to set-up information for a pipe.\n");
        CloseHandle(read);
        return 0;
    }

    process->read_pipe  = read;

    HANDLE write = CreateFile(pipe_name,
                              GENERIC_WRITE,
                              0,
                              &attr,
                              OPEN_EXISTING,
                              FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                              NULL);

    if (write == INVALID_HANDLE_VALUE) {
        printf("failed to create write pipe. Num: %d\n", GetLastError());
        CloseHandle(process->read_pipe);
        process->read_pipe = 0;
        return 0;
    }

    process->write_pipe = write;

    return 1;
}

void close_pipe(Process_Handle *process) {
    return; // giving up actually handling stdout for now. I have to think how to do it

    CloseHandle(process->read_pipe);
    process->read_pipe  = 0;

    CloseHandle(process->write_pipe);
    process->write_pipe = 0;
}

Process_Handle create_process_handle() {
    Process_Handle handle = {0};
    ZeroMemory(&handle.procinfo, sizeof(PROCESS_INFORMATION));

    handle.valid = 1;
    return handle;
}

void destroy_handle(Process_Handle *handle) {
    terminate_process(handle);
    close_pipe(handle);
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
    HANDLE empty = {0};
    if(handle->procinfo.hProcess == empty) return 0;

    DWORD exit_code;
    GetExitCodeProcess(handle->procinfo.hProcess, &exit_code);

    if (exit_code == STILL_ACTIVE) {
        switch(WaitForSingleObject(handle->procinfo.hProcess, 0)) {
            case WAIT_TIMEOUT:
                return 1;

            default:
                return 0;
        }
    } else {
        return 0;
    }
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

void terminate_process(Process_Handle *process) { // try to terminate the process whether it's alive or not.
    TerminateProcess(process->procinfo.hProcess, 0);

    WaitForSingleObject(process->procinfo.hProcess, INFINITE);
    WaitForSingleObject(process->procinfo.hThread,  INFINITE);

    CloseHandle(process->procinfo.hProcess);
    CloseHandle(process->procinfo.hThread);
}

void restart_process(const char *command, Process_Handle *handle, Logger *logger) {
    assert(is_process_running(handle) && "Process is not running");
    terminate_process(handle);

    assert(!is_process_running(handle) && "Process is still runnning despite of terminate process");
    ZeroMemory(&handle->procinfo, sizeof(handle->procinfo));

    start_process(command, handle, logger);
}

int32_t start_process(const char *command, Process_Handle *handle, Logger *logger) {
    // if(!create_pipe(handle)) {
    //     printf("Failed to start a process.\n");
    //     return 0;
    // }

    STARTUPINFO info = {0};
    info.cb = sizeof(info);
    // standard_info->hStdOutput = handle->write_pipe;
    // standard_info->dwFlags    = STARTF_USESTDHANDLES;
    BOOL inherit_pipe = FALSE;

    TCHAR process_command[1024] = {0};
    ua_tcscpy_s(process_command, sizeof(process_command), command);
    BOOL created = CreateProcess(NULL,
                                 process_command,
                                 NULL,
                                 NULL,
                                 inherit_pipe,
                                 0,
                                 NULL,
                                 NULL,
                                 &info,
                                 &handle->procinfo);

    if (created == 0) {
        watcher_log(logger, "Failed to run process: GetLastError() = %d", GetLastError());
        ZeroMemory(&handle->procinfo, sizeof(handle->procinfo));
        return 0;
    }   

    // CloseHandle(handle->write_pipe);
    return 1;
}

void handle_stdout_for_process(Process_Handle *process, Logger *Logger) {
    return; // giving up actually handling stdout for now. I have to think how to do it

    if (!is_process_running(process)) return;

    // DWORD current_pipe_occupied_amount = 0;
    char buffer[1024] = {0};
    DWORD bytes_read = 0;
    DWORD read = 0;

    BOOL succeed = ReadFile(process->read_pipe, &buffer[bytes_read], 1, &read, 0);

    while (succeed && read > 0) {
        // while((bytes_read + Logger->used) > Logger->capacity) {
        //     void *new_buffer = realloc(Logger->buffer_ptr, Logger->capacity * 2);
        //     assert(new_buffer && "failed to realloc new buffer.");

        //     Logger->buffer_ptr = (char *)new_buffer;
        //     Logger->capacity   = Logger->capacity * 2;
        // }


        // int begin = 0, end = 0;
        // for (; end < bytes_read; ++end) {
        //     if (buffer[end] == '\n') {
        //         strncat(Logger->buffer_ptr, &buffer[begin],(end + 1) - begin);
        //         printf("[Logs] %s", Logger->buffer_ptr);
        //         
        //         memset(Logger->buffer_ptr, 0, Logger->capacity);
        //         Logger->used = 0;
        //         begin = end + 1;
        //     }
        // }

        // if (begin != end) {
        //     strncat(Logger->buffer_ptr, &buffer[begin], end - begin);
        //     Logger->used += end - begin;
        // }


        if (buffer[bytes_read] == '\n') {
            buffer[bytes_read + 1] = '\0';
            printf("[Logs] %s", buffer);
            memset(buffer, 0, sizeof(buffer));
            bytes_read = 0;
        } else {
            bytes_read += read;
        }

        if (bytes_read < sizeof(buffer)) {
            succeed = ReadFile(process->read_pipe, &buffer[bytes_read], 1, &read, 0);
        } else {
            succeed = 0;
        }
    }
}

// Check if given path is forbidden to process.
bool is_forbidden_path(char *path) {
    return (
        strcmp(path, ".")  == 0 ||
        strcmp(path, "..") == 0
    );
}

static int CALLBACK
SHBrowseProc(HWND hwnd, UINT umsg, LPARAM lParam, LPARAM lpData) {
    if (umsg == BFFM_INITIALIZED) {
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }
    return 0;
}

void select_new_folder(char *folder_buffer, size_t folder_buffer_size) {
    BROWSEINFO info = {0};
    char buffer[MAX_PATH] = {0};
    strncpy(buffer, folder_buffer, MAX_PATH);
    
    info.hwndOwner = NULL;
    info.lpszTitle = "Select a target directory...";
    info.ulFlags   = BIF_DONTGOBELOWDOMAIN | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
    info.lParam    = (LPARAM)&buffer;
    info.lpfn      = SHBrowseProc;

    PIDLIST_ABSOLUTE Pidlist = SHBrowseForFolderA(&info);
    if (Pidlist) {
        SHGetPathFromIDListA(Pidlist, buffer);
        IMalloc *allocator;
        SHGetMalloc(&allocator);

        if (allocator) {
            allocator->Free(Pidlist);
            allocator->Release();
        }

        if (strlen(buffer) < folder_buffer_size) {
            memset(folder_buffer, 0,folder_buffer_size);
            strncpy(folder_buffer, buffer, strlen(buffer));
        }
    }
}

void select_file(char *file_buffer, size_t file_buffer_size) {
    char buffer[MAX_PATH] = {0};
    strncpy(buffer, file_buffer, MAX_PATH);

    OPENFILENAME filename;
    ZeroMemory(&filename, sizeof(filename));
    filename.lStructSize = sizeof(filename);
    filename.lpstrFilter = ".exe;.com;.bat;.sh;";
    filename.lpstrFile   = buffer;
    filename.nMaxFile    = MAX_PATH;
    filename.lpstrTitle  = "Select a process to run...";

    filename.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NONETWORKBUTTON;

    if (GetOpenFileNameA(&filename)) {
        if (strlen(buffer) < file_buffer_size) {
            memset(file_buffer, 0,file_buffer_size);
            strncpy(file_buffer, buffer, strlen(buffer));
        }
    }
}

void to_full_paths(char *path_buffer, size_t path_buffer_size) {
    char *buffer = (char *)VirtualAlloc(0, path_buffer_size+1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    strncpy(buffer, path_buffer, path_buffer_size);

    GetFullPathName(buffer, path_buffer_size, path_buffer, 0);
    path_buffer[path_buffer_size-1] = 0;

    VirtualFree(buffer, 0, MEM_RELEASE);
}


uint64_t find_latest_modified_time(Logger *logger, char *filepath) {
    if (is_forbidden_path(filepath)) return 0;
    WIN32_FIND_DATA data = {0};
    HANDLE handle = FindFirstFile(filepath, &data);

    if (handle == INVALID_HANDLE_VALUE) {
        watcher_log(logger, "Failed to find a folder: attempt to open %s resulted in INVALID_HANDLE_VALUE", filepath);
        return 0;
    }

    uint64_t result = 0;
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        char dir_search_term[1024] = {0};
        snprintf(dir_search_term, 1024, "%s\\*", filepath);

        WIN32_FIND_DATA dir_data = {0};
        HANDLE directory_handle = FindFirstFile(dir_search_term, &dir_data);

        do {
            if (!is_forbidden_path(dir_data.cFileName)) {
                memset(dir_search_term, 0, sizeof(dir_search_term));
                snprintf(dir_search_term, sizeof(dir_search_term), "%s\\%s", filepath, dir_data.cFileName);
                uint64_t write_time_for_given_file = find_latest_modified_time(logger, dir_search_term);

                if (write_time_for_given_file > result) {
                    result = write_time_for_given_file;
                }
            }
        } while(FindNextFile(directory_handle, &dir_data));

        FindClose(directory_handle);
    } else {
        ULARGE_INTEGER lg = {};
        lg.u.HighPart = data.ftLastWriteTime.dwHighDateTime;
        lg.u.LowPart  = data.ftLastWriteTime.dwLowDateTime;
        result = lg.QuadPart;
    }

    FindClose(handle);
    return result;
}

void sleep_ms(int ms) {
    Sleep(ms);
}
