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

extern "C" {
    #include "vendor/microui.h"

    void r_init(void);
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

    int32_t folder_is_invalid;
    char directory[256];
    char command[256];

    Logger     logger;
    Process_Handle handle;
};


void watcher_log(Logger *logger, char *message, ...) {
    size_t new_buffer_index = logger->logs_end;
    char *buf = logger->logs[new_buffer_index];
    memset(buf, 0, LOG_BUFFER_LINE_SIZE);

    va_list list;
    va_start(list, message);
    vsnprintf(buf, LOG_BUFFER_LINE_SIZE, message, list);
    va_end(list);

    logger->logs_end = (logger->logs_end + 1) % LOG_BUFFER_BUCKET_SIZE;
    if (logger->logs_end == logger->logs_begin) logger->logs_begin = (logger->logs_begin + 1) % LOG_BUFFER_BUCKET_SIZE;
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
    int32_t process_is_running = is_process_running(&succotash->handle); // just for display!


    if (mu_begin_window_ex(ctx, "Base_Window", mu_rect(0, 0, 350, 300), MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOCLOSE)) {
        int row[] = { 80, 80, 80 };
        mu_layout_row(ctx, 3, row, 0);
        int32_t option_for_running_button = (succotash->folder_is_invalid) ? MU_OPT_NOINTERACT : 0;
        if(mu_button_ex(ctx, "Start/Stop", 0, option_for_running_button)) {
            if (!process_is_running) {
                watcher_log(&succotash->logger, "Running a process...");
                close_pipe(&succotash->handle);
                start_process(succotash->command, &succotash->handle, &succotash->logger);
            } else {
                watcher_log(&succotash->logger, "Restarting a process...");
                close_pipe(&succotash->handle);
                terminate_process(&succotash->handle);
            }
        }
        mu_checkbox_ex(ctx, "Running",  &process_is_running,           MU_OPT_NOINTERACT);

        int32_t folder_is_not_invalid = !succotash->folder_is_invalid;
        mu_checkbox_ex(ctx, "Watching", &folder_is_not_invalid, MU_OPT_NOINTERACT);

        // ============ Command Window ============ 
        int row2[] = { 80, -1 };
        mu_layout_row(ctx, 2, row2, 0); 
        int32_t option = 0;
        option |= (process_is_running) ? MU_OPT_NOINTERACT : 0;

        if(mu_button_ex(ctx, "Directory", 0, option)) {
            select_new_folder(succotash->directory, sizeof(succotash->directory));
            succotash->folder_is_invalid = 0;
        }

        if(mu_textbox_ex(ctx, succotash->directory, sizeof(succotash->directory), option) & MU_RES_SUBMIT) {
            succotash->folder_is_invalid = 0;
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
        size_t begin = succotash->logger.logs_begin;
        size_t end   = succotash->logger.logs_end;

        static size_t static_end = 0;
        if (succotash->logger.logs_end != static_end) {
            static_end = succotash->logger.logs_end;
            mu_Container *container = mu_get_current_container(ctx);
            container->scroll.y = container->content_size.y;
        }

        for (size_t i = begin; i != end; i = (i + 1) % LOG_BUFFER_BUCKET_SIZE) {
            char buffer[LOG_BUFFER_LINE_SIZE + 256] = {0};
            snprintf(buffer, LOG_BUFFER_LINE_SIZE + 256, "[LOG] %s", succotash->logger.logs[i]);
            mu_text(ctx, buffer);
        }

        mu_end_panel(ctx);
        mu_end_window(ctx);
    }

    mu_end(ctx);
}

void render_gui(Succotash *succotash, mu_Context *ctx) {
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
    r_init();

    /* init microui */
    Succotash *succotash = (Succotash *)malloc(sizeof(Succotash));
    mu_Context *ctx = (mu_Context *)malloc(sizeof(mu_Context));
    memset(succotash, 0, sizeof(Succotash));

    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    /* main loop */
    strcat(succotash->directory, "./src");
    strcat(succotash->command,   "./test_printing_process.exe");

    to_full_paths(succotash->directory, sizeof(succotash->directory));
    to_full_paths(succotash->command,   sizeof(succotash->command));

    succotash->handle             = create_process_handle();
    succotash->last_modified_time = find_latest_modified_time(&succotash->logger, (char *)succotash->directory);
    succotash->folder_is_invalid  = succotash->last_modified_time == 0;
    watcher_log(&succotash->logger, "Waiting.");

    if (!succotash->handle.valid) {
        printf("Failed to start a program.\n");
        return 0;
    }

    int32_t process_was_alive_previous_frame = 0;

    succotash->running = 1;
    while (succotash->running) {
        int32_t process_is_alive = is_process_running(&succotash->handle);
        if (!process_is_alive) { 
            if (process_was_alive_previous_frame) {
                watcher_log(&succotash->logger, "process exited. waiting for restart(press start stop or modify content in watch folder.)");
            }
        }

        handle_stdout_for_process(&succotash->handle, NULL);
        if (!succotash->folder_is_invalid) {
            uint64_t current_latest_modified_time = find_latest_modified_time(&succotash->logger, (char *)succotash->directory);

            if (current_latest_modified_time > succotash->last_modified_time) {
                watcher_log(&succotash->logger, "File change detected (timestamp %" PRIu64 "). restarting a process", current_latest_modified_time);
                succotash->last_modified_time = current_latest_modified_time;
                
                if (!process_is_alive) {
                    terminate_process(&succotash->handle); // just in case;
                    close_pipe(&succotash->handle); // just in case;
                    start_process(succotash->command, &succotash->handle, &succotash->logger);
                } else {
                    restart_process(succotash->command, &succotash->handle, &succotash->logger);
                }
            } else if (current_latest_modified_time == 0) {
                watcher_log(&succotash->logger, "Folder %s became invalid. disabling restart.", succotash->directory);
                succotash->folder_is_invalid = 1;
            }
        }

        process_event(succotash, ctx);
        process_gui(succotash, ctx);
        render_gui(succotash, ctx);

        process_was_alive_previous_frame = process_is_alive;
    }  
    
    destroy_handle(&succotash->handle);
    free(ctx);
    free(succotash);
    return 0;
}
