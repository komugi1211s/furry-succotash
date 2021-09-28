#include "vendor/microui.c"
#include "template_sdl_microui_opengl3.c"

typedef struct Process_Handle Process_Handle;

#if _WIN32
#include <windows.h>
/* ======================= */
// Windows. 
/* ======================= */
typedef HANDLE Thread_Handle;
typedef HANDLE Mutex;

typedef DWORD (*thread_proc)(void *params);

#else
/* ======================= */
// Linux. 
/* ======================= */
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>

typedef pthread_t       Thread_Handle;
typedef pthread_mutex_t Mutex;

struct Process_Handle {
    int32_t valid;
    const char *command;
    FILE *process_stream;
};

Process_Handle create_handle_from_command(const char *command_as_chars) {
    Process_Handle handle = {0};
    if (strlen(command_as_chars) == 0) {
        return handle;
    }

    // Create Actual Initialization command for processes (main process name, and arguments as lists);
    /* NOTE: probably I don't need this monstrosity
    char *copied_string, *current;
    copied_string = current = strdup(command_as_chars);

    char *first_command = strsep(&current, " ");
    char *arg_list[32] = {0};
    int32_t arg_list_count = 0;

    for(;;) {
        char *argument = strsep(&current, " ");
        if (!current || !argument || arg_list_count >= 32) break;

        arg_list[arg_list_count++] = argument;
    }
    free(copied_string);
    */

    handle.command = command_as_chars;
    handle.valid   = 1;
    return handle;
}

void run_process(Process_Handle *handle) {
    assert(handle->process_stream == NULL && "Do not call run_process without closing the previous process.");
    FILE *process_stream = popen(handle->command, "r");
    if (!process_stream) {
        printf("failed to open the process %s: %s\n", handle->command, strerror(errno));
        handle->valid = 0;
        return;
    }

    handle->process_stream = process_stream;
    return;
}

void restart_process(Process_Handle *handle) {
    if (!handle->valid) return;
    assert(handle->process_stream != NULL && "Do not call restart_process without spinning it up first.");

    int32_t result = pclose(handle->process_stream);
    handle->process_stream = 0;
    if(-1 == result) {
        printf("Failed to close process: preventing from restarting.");
        handle->valid = 0;
        return;
    }

    run_process(handle);
}

uint64_t find_latest_modified_time(char *path);

#define ModTime(mStatus) ((uint64_t)((mStatus).st_mtim.tv_sec * 1000000000llu + (mStatus).st_mtim.tv_nsec))
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
        uint64_t time = ModTime(status);
        return time;
    }
}


#endif

typedef struct Succotash Succotash;
typedef struct Directory_Command_Pair Directory_Command_Pair;

struct Directory_Command_Pair {
    char watch_dir[256];
    char command[256];
};

struct Succotash {
    int32_t running;

    uint64_t last_modified_time;
    Directory_Command_Pair pairs[32];
    size_t command_pair_count;
};

void process_event(Succotash *succotash, mu_Context *ctx) {
    /* handle SDL events */
    SDL_Event e;
    int32_t is_running = 0; // todo: remove
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                is_running = 0;
                break;

            case SDL_MOUSEMOTION:
                mu_input_mousemove(ctx, e.motion.x, e.motion.y);
                break;

            case SDL_MOUSEWHEEL:
                mu_input_scroll(ctx, 0, e.wheel.y * -30);
                break;

            case SDL_TEXTINPUT:
                mu_input_text(ctx, e.text.text);
                break;
            
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                int b = button_map[e.button.button & 0xff];
                if (b && e.type == SDL_MOUSEBUTTONDOWN) { mu_input_mousedown(ctx, e.button.x, e.button.y, b); }
                if (b && e.type ==   SDL_MOUSEBUTTONUP) { mu_input_mouseup(ctx, e.button.x, e.button.y, b);   }
            } break;
            
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                int c = key_map[e.key.keysym.sym & 0xff];
                if (c && e.type == SDL_KEYDOWN) { mu_input_keydown(ctx, c); }
                if (c && e.type ==   SDL_KEYUP) { mu_input_keyup(ctx, c);   }
            } break;
        }
    }
}

// TODO: cleanup
void process_gui(Succotash *succotash, mu_Context *ctx) {
    /* process frame */
    Directory_Command_Pair pair = {0};
    mu_begin(ctx);

    int option = MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOCLOSE;
    if (mu_begin_window_ex(ctx, "Base_Window", mu_rect(0, 0, 400, 800), option)) {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 25);
        mu_checkbox(ctx, "Running", &succotash->running);


        // ============ Command Window ============ 
        mu_layout_row(ctx, 2, (int[]) { 80, -1 }, 0);
        int32_t option = 0;
        option |= (succotash->running) ? MU_OPT_NOINTERACT : 0;
        {
            mu_text(ctx, "Directory: ");
            mu_textbox_ex(ctx, pair.watch_dir, sizeof(pair.watch_dir), option);
            mu_text(ctx, "Command: ");
            mu_textbox_ex(ctx, pair.command, sizeof(pair.command), option);
        }

        // ============ Status Window ============ 
        mu_layout_row(ctx, 1, (int[]) { -1 }, 25);
        if (succotash->running) {
            char log_buffer[2048] = {0};
            uint64_t file_last_time = find_latest_modified_time(pair.watch_dir);
            if (file_last_time == 0) {
                snprintf(log_buffer, sizeof(log_buffer), "Error Occurred: %s", strerror(errno));
            } else {
                if (succotash->last_modified_time != file_last_time) {
                    succotash->last_modified_time = file_last_time;
                }
                snprintf(log_buffer, sizeof(log_buffer), "Last File Modified Time: %" PRIu64 "", file_last_time);
            } 
            mu_text(ctx, log_buffer);
        }

        mu_end_window(ctx);
    }

    mu_end(ctx);
}

void render_gui(Succotash *succotash, mu_Context *ctx) {
    r_clear(mu_color(bg[0], bg[1], bg[2], 255));
    mu_Command *cmd = NULL;
    while (mu_next_command(ctx, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
            case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
            case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
        }
    }
    r_present();
}


int main(int argc, char **argv) {
    /*
    SDL_Init(SDL_INIT_EVERYTHING);
    r_init();
    */

    /* init microui */
    // Succotash *succotash = malloc(sizeof(Succotash));
    // mu_Context *ctx = malloc(sizeof(mu_Context));

    // memset(succotash, 0, sizeof(Succotash));

    // mu_init(ctx);
    // ctx->text_width = text_width;
    // ctx->text_height = text_height;

    /* main loop */

    char *directory = "./src";
    char *command = "notify-send \"Hello here from commands!\"";

    Process_Handle handle               = create_handle_from_command(command);
    uint64_t       latest_modified_time = find_latest_modified_time(directory);

    run_process(&handle);
    for (int is_running = 1; is_running;) {
        uint64_t current_latest_modified_time = find_latest_modified_time(directory);

        if (current_latest_modified_time > latest_modified_time) {
            latest_modified_time = current_latest_modified_time;
            restart_process(&handle);
        }

        // not working now.
        // process_event(succotash, ctx);
        // process_gui(succotash, ctx);
        // render_gui(succotash, ctx);
    }
    
    // free(ctx);
    return 0;
}
