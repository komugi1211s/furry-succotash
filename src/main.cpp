#include <stdio.h>
#include <assert.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32 
#define SDL_MAIN_HANDLED 1 
#include <SDL.h>
#include <SDL_opengl.h>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#endif

static int WINDOW_WIDTH  = 650;
static int WINDOW_HEIGHT = 300;

extern "C" {
    #include "vendor/microui.h"

    void r_init(int width, int height);
    void r_draw_rect(mu_Rect rect, mu_Color color);
    void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color);
    void r_draw_icon(int id, mu_Rect rect, mu_Color color);
    int r_get_text_width(const char *text, int len);
    int r_get_text_height(void);
    int text_width(mu_Font font, const char *text, int len);
    int text_height(mu_Font font);
    void r_set_clip_rect(mu_Rect rect);
    void r_clear(mu_Color color);
    void r_present(void);
    void r_get_window_size(int *width, int *height);
}

#if _WIN32
/* ======================= */
// Windows. 
/* ======================= */
#include "windows.cpp"
#else
#include "unix.cpp"
#endif



struct Succotash {
    int32_t running;
    uint64_t last_modified_time;
    int32_t  should_process_running;

    int32_t folder_is_invalid;
    char watch_directory[512];
    char work_directory[512];
    char command[512];

    Logger         logger;
    Process_Handle handle;
};

struct Task {
    uint64_t last_modified_time;
    int32_t  should_be_running;
    int32_t  folder_is_invalid;

    char directory[512];
    char command[512];

    Process_Handle handle;
    Task *next;
};

struct Global {
    int32_t running;

    Task    tasks[32];
    Task    *occupied_tasks;
    Task    *free_tasks;
    Logger   logger;
};

static Global global = {0};

void init_tasklist() {
    for (int i = 0; i < 31; ++i) {
        global.tasks[i].next = &global.tasks[i + 1];
    }

    global.occupied_tasks = &global.tasks[0];
}

void watcher_log(const char *message, ...) {
    size_t new_buffer_index = global.logger.logs_end;
    char *buf = global.logger.logs[new_buffer_index];
    memset(buf, 0, LOG_BUFFER_LINE_SIZE);

    va_list list;
    va_start(list, message);
    vsnprintf(buf, LOG_BUFFER_LINE_SIZE-1, message, list);
    buf[LOG_BUFFER_LINE_SIZE-1] = 0;
    va_end(list);

    global.logger.logs_end = (global.logger.logs_end + 1) % LOG_BUFFER_BUCKET_SIZE;
    if (global.logger.logs_end == global.logger.logs_begin) {
        global.logger.logs_begin = (global.logger.logs_begin + 1) % LOG_BUFFER_BUCKET_SIZE;
    }
}

char sdlk_to_microui_key(SDL_Keycode sym) {
    switch(sym) {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            return MU_KEY_SHIFT;

        case SDLK_LCTRL:
        case SDLK_RCTRL:
            return MU_KEY_CTRL;

        case SDLK_LALT:
        case SDLK_RALT:
            return MU_KEY_ALT;

        case SDLK_RETURN:
            return MU_KEY_RETURN;

        case SDLK_BACKSPACE:
            return MU_KEY_BACKSPACE;

        default:
            return 0;
    }
}

void process_event(Succotash *succotash, mu_Context *ctx) {
    /* handle SDL events */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                succotash->running = 0;
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
                int b;
                switch(e.button.button) {
                    case SDL_BUTTON_LEFT:
                        b = MU_MOUSE_LEFT; break;
                    case SDL_BUTTON_RIGHT:
                        b = MU_MOUSE_RIGHT; break;
                    case SDL_BUTTON_MIDDLE:
                        b = MU_MOUSE_MIDDLE; break;
                    default:
                        assert(false && "unsupported mouse");
                }
                if (b && e.type == SDL_MOUSEBUTTONDOWN) { mu_input_mousedown(ctx, e.button.x, e.button.y, b); }
                if (b && e.type ==   SDL_MOUSEBUTTONUP) { mu_input_mouseup(ctx, e.button.x, e.button.y, b);   }
            } break;
            
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                int c = sdlk_to_microui_key(e.key.keysym.sym);
                if (c && e.type == SDL_KEYDOWN) { mu_input_keydown(ctx, c); }
                if (c && e.type ==   SDL_KEYUP) { mu_input_keyup(ctx, c);   }
            } break;
        }
    }
}

// TODO: cleanup
void process_gui(Succotash *succotash, mu_Context *ctx) {
    /* process frame */
    mu_begin(ctx);
    int32_t process_status;
    int32_t process_is_running = get_process_status(&succotash->handle, &process_status); // just for display!
    int width, height;
    r_get_window_size(&width, &height);

    if (mu_begin_window_ex(ctx, "Base_Window", mu_rect(0, 0, width, height), MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOCLOSE)) {
        int row[] = { 80, 80, 80 };
        mu_layout_row(ctx, 3, row, 0);
        int32_t should_button_be_active = (succotash->folder_is_invalid) ? MU_OPT_NOINTERACT : 0;
        if(mu_button_ex(ctx, "Start/Stop", 0, should_button_be_active)) {
            succotash->should_process_running = !succotash->should_process_running;
            succotash->last_modified_time = 0;
        }

        int32_t watching_folder       = succotash->should_process_running && !succotash->folder_is_invalid;
        mu_checkbox_ex(ctx, "Running",  &process_is_running,    MU_OPT_NOINTERACT);
        mu_checkbox_ex(ctx, "Watching", &watching_folder, MU_OPT_NOINTERACT);

        // ============ Command Window ============ 
        int row2[] = { 80, -1 };
        mu_layout_row(ctx, 2, row2, 0); 
        int32_t option = 0;
        option |= (process_is_running) ? MU_OPT_NOINTERACT : 0;

        if(mu_button_ex(ctx, "Watch Dir", 0, option)) {
            if(select_new_folder(succotash->watch_directory, sizeof(succotash->watch_directory))) {
                succotash->folder_is_invalid = 0;
            } else {
                watcher_log("Failed to choose a file.");
            }
        }

        if(mu_textbox_ex(ctx, succotash->watch_directory, sizeof(succotash->watch_directory), option) & MU_RES_SUBMIT) {
            succotash->folder_is_invalid = 0;
        }

        if(mu_button_ex(ctx, "Work Dir", 0, option)) {
            if(select_new_folder(succotash->work_directory, sizeof(succotash->work_directory))) {
                // TODO: work_directory check.
            } else {
                watcher_log("Failed to choose a file.");
            }
        }

        if(mu_textbox_ex(ctx, succotash->work_directory, sizeof(succotash->work_directory), option) & MU_RES_SUBMIT) {
            // TODO: work_directory check.
        }


        if(mu_button_ex(ctx, "Command", 0, option)) {
            select_file(succotash->command, sizeof(succotash->command)); 
        }

        mu_textbox_ex(ctx, succotash->command, sizeof(succotash->command), option);


        // ============ Status Window ============ 
        int full_row[] = { -1 };
        mu_layout_row(ctx, 1, full_row, -1);
        mu_begin_panel(ctx, "Logs");
        mu_layout_row(ctx, 1, full_row, r_get_text_height());

        // 
        // Rendering Logs.
        //
        size_t begin = global.logger.logs_begin;
        size_t end   = global.logger.logs_end;

        static size_t static_end = 0;
        if (global.logger.logs_end != static_end) {
            static_end = global.logger.logs_end;
            mu_Container *container = mu_get_current_container(ctx);
            container->scroll.y = container->content_size.y;
        }

        for (size_t i = begin; i != end; i = (i + 1) % LOG_BUFFER_BUCKET_SIZE) {
            char buffer[LOG_BUFFER_LINE_SIZE + 256] = {0};
            snprintf(buffer, LOG_BUFFER_LINE_SIZE + 255, "[LOG] %s", global.logger.logs[i]);
            mu_text(ctx, buffer);
        }

        mu_end_panel(ctx);
        mu_end_window(ctx);
    }

    mu_end(ctx);
}

void render_gui(mu_Context *ctx) {
    r_clear(mu_color(0, 0, 0, 255));
    mu_Command *cmd = NULL;
    while (mu_next_command(ctx, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
            case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color);               break;
            case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect);                            break;
        }
    }
    r_present();
}

int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_EVERYTHING);
    r_init(WINDOW_WIDTH, WINDOW_HEIGHT);
    platform_init();

    /* init microui */
    Succotash *succotash = (Succotash *)malloc(sizeof(Succotash));
    mu_Context *ctx = (mu_Context *)malloc(sizeof(mu_Context));
    memset(succotash, 0, sizeof(Succotash));

    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    succotash->handle             = create_process_handle();
    succotash->last_modified_time = find_latest_modified_time(&succotash->logger, (char *)succotash->watch_directory);
    succotash->folder_is_invalid  = succotash->last_modified_time == 0;
    watcher_log("Waiting.");

    int32_t process_was_alive_previous_frame = 0;
    succotash->running = 1;
    while (!platform_app_should_close() && succotash->running) {
        int32_t process_status;
        int32_t process_is_alive = get_process_status(&succotash->handle, &process_status);
        if (!process_is_alive) { 
            // Process is already dead, but have to reset the handle.
            terminate_process(&succotash->handle);

            switch(process_status) {
                case PROCESS_NOT_RUNNING:
                    if (process_was_alive_previous_frame) {
                        watcher_log("Process not running at all.");
                    }
                    break;

                case PROCESS_DIED_ERROR:
                    watcher_log("Process died due to an error. check your process/command, and try again.");
                    succotash->should_process_running = 0;
                    break;

                case PROCESS_DIED_CORRECLTLY:
                    watcher_log("Process exited successfully.");
                    break;

                case PROCESS_DIED_KILLED:
                    watcher_log("Process got killed by a signal, etc.");
                    break;
            }

            if (process_was_alive_previous_frame) {
                watcher_log("process exited. waiting for restart(press start stop or modify content in watch folder.)");
            }
        }

        process_event(succotash, ctx);
        process_gui(succotash, ctx);

        // handle_stdout_for_process(&succotash->handle, NULL);
        if (succotash->should_process_running) {
            int32_t process_running_trigger = 0;

            if (succotash->folder_is_invalid) {
                watcher_log("Folder %s became invalid. cannot start/restart the process", succotash->watch_directory);
                succotash->should_process_running = 0;
                continue;
            }

            uint64_t modified_time = find_latest_modified_time(&succotash->logger,
                                                               (char *)succotash->watch_directory);

            // Modified time returned 0: means whatever happened while checking the folder.
            // Stop searching.
            if (modified_time == 0) {
                succotash->folder_is_invalid = 1;
                continue;
            }

            // Modified time successfully captured and is in fact earlier than the current hold time:
            if (modified_time > succotash->last_modified_time) {
                if (succotash->last_modified_time != 0) {
                    watcher_log("File change detected (timestamp %" PRIu64 "). restarting a process", modified_time);
                }
                succotash->last_modified_time = modified_time;
                process_running_trigger = 1;
            }

            // Restart / start process depending on the current situation.
            if (process_running_trigger) {
                if (process_is_alive) {
                    restart_process(succotash->work_directory, succotash->command, &succotash->handle, &succotash->logger);
                } else {
                    if(!start_process(succotash->work_directory, succotash->command, &succotash->handle, &succotash->logger)) {
                        watcher_log("Failed to run a process. specify the correct executables and try again.");
                        succotash->should_process_running = 0;
                    }
                }
            }
        } else {
            if (process_is_alive) {
                terminate_process(&succotash->handle);
            }
        }

        // NOTE(fuzzy):
        // Placing render_gui forces renderer to sync to 60hz -- I'm using this 16ms lag to ensure that
        // handle->pid will be a valid ID once we start the process at the same frame.
        // otherwise the is_process_running at the top will return false because of ECHILD error, despite the process itself still running.
        render_gui(ctx);

        process_was_alive_previous_frame = process_is_alive;
    }  
    
    watcher_log("Ending the application.");
    destroy_process_handle(&succotash->handle);
    free(ctx);
    free(succotash);
    return 0;
}
