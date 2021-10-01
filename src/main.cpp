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

    int option = MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOCLOSE;
    if (mu_begin_window_ex(ctx, "Base_Window", mu_rect(0, 0, 400, 800), option)) {
        int row[] = { -1 };
        mu_layout_row(ctx, 1, row, 25);
        mu_checkbox(ctx, "Running", &succotash->running);


        // ============ Command Window ============ 
        int row2[] = { 80, -1 };
        mu_layout_row(ctx, 2, row2, 0);
        int32_t option = 0;
        option |= (succotash->running) ? MU_OPT_NOINTERACT : 0;

        // ============ Status Window ============ 
        mu_layout_row(ctx, 1, row, 25);
        if (succotash->running) {
            uint64_t file_last_time = 0; // find_latest_modified_time(pair.watch_dir);

            char log_buffer[2048] = {0};

            if (file_last_time != 0) {
                if (succotash->last_modified_time != file_last_time) {
                    succotash->last_modified_time = file_last_time;
                }
                snprintf(log_buffer, sizeof(log_buffer), "Last File Modified Time: %" PRIu64 "", file_last_time);
            } else {
                snprintf(log_buffer, sizeof(log_buffer), "Error Occurred: %s", strerror(errno));
            } 

            mu_text(ctx, log_buffer);
        }

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
            case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
            case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
        }
    }
    r_present();
}

void testing_threads(void *ptrs) {
    printf("Hello from thread!\n");
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

    const char *directory = "./src";
    const char *command   = "./test_printing_process.exe";

    Process_Handle handle               = create_handle_from_command(command);
    uint64_t       latest_modified_time = find_latest_modified_time((char *)directory);

    if (!handle.valid) {
        printf("Failed to start a program.\n");
        return 0;
    }

    // Buffer.
    Log_Buffer buffer = {0};
    // buffer.buffer_ptr = (char *)malloc(sizeof(char) * 4096);
    // buffer.capacity   = sizeof(char) * 4096;
    // buffer.used       = 0;

    // memset(buffer.buffer_ptr, 0, sizeof(char) * 4096);
    start_process(&handle, &buffer);
    int32_t process_was_alive_previous_frame = 0;
    for (int is_running = 1; is_running;) {
        int32_t process_is_alive = is_process_running(&handle);
        if (!process_is_alive) {
            if (process_was_alive_previous_frame) {
                printf("Process Died! waiting to restart...\n");
            }
        }
        // handle_stdout_for_process(&handle, &buffer);
        uint64_t current_latest_modified_time = find_latest_modified_time((char *)directory);

        if (current_latest_modified_time > latest_modified_time) {
            printf("files changed! restarting... \n");
            latest_modified_time = current_latest_modified_time;
            
            if (!process_is_alive) {
                terminate_process(&handle); // just in case;
                start_process(&handle, &buffer);
            } else {
                restart_process(&handle, &buffer);
            }
        }

        // not working now.
        // process_event(succotash, ctx);
        // process_gui(succotash, ctx);
        // render_gui(succotash, ctx);

        process_was_alive_previous_frame = process_is_alive;
    }  
    
    destroy_handle(&handle);
    // free(buffer.buffer_ptr);
    // free(ctx);
    return 0;
}
