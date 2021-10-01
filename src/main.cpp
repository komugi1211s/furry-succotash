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

    char directory[256];
    char command[256];

    Process_Handle handle;
};

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

    if (mu_begin_window_ex(ctx, "Base_Window", mu_rect(0, 0, 400, 800), MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOCLOSE)) {
        int row[] = { 80, -1 };
        mu_layout_row(ctx, 2, row, 25);
        if(mu_button(ctx, "Start/Stop")) {
            if (!process_is_running) {
                start_process(succotash->command, &succotash->handle, NULL);
            } else {
                terminate_process(&succotash->handle);
            }
        }
        mu_checkbox_ex(ctx, "Running", &process_is_running, MU_OPT_NOINTERACT);

        // ============ Command Window ============ 
        int row2[] = { 80, -1 };
        mu_layout_row(ctx, 2, row2, 0);
        int32_t option = 0;
        option |= (process_is_running) ? MU_OPT_NOINTERACT : 0;

        mu_text(ctx, "Directory");
        mu_textbox_ex(ctx, succotash->directory, sizeof(succotash->directory), option);

        mu_text(ctx, "Command");
        mu_textbox_ex(ctx, succotash->command, sizeof(succotash->command), option);

        // ============ Status Window ============ 
        int full_row[] = { -1 };
        mu_layout_row(ctx, 1, full_row, 25);
        char log_buffer[2048] = {0};
        snprintf(log_buffer, sizeof(log_buffer), "Last File Modified Time: %" PRIu64 "", succotash->last_modified_time);
        mu_text(ctx, log_buffer);

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
    strcat(succotash->command,  "./test_printing_process.exe");

    succotash->handle               = create_process_handle();
    succotash->last_modified_time = find_latest_modified_time((char *)succotash->directory);

    if (!succotash->handle.valid) {
        printf("Failed to start a program.\n");
        return 0;
    }

    // Buffer.
    Log_Buffer buffer = {0};
    // buffer.buffer_ptr = (char *)malloc(sizeof(char) * 4096);
    // buffer.capacity   = sizeof(char) * 4096;
    // buffer.used       = 0;

    // memset(buffer.buffer_ptr, 0, sizeof(char) * 4096);
    int32_t process_was_alive_previous_frame = 0;

    succotash->running = 1;
    while (succotash->running) {
        int32_t process_is_alive = is_process_running(&succotash->handle);
        if (!process_is_alive) { 
            if (process_was_alive_previous_frame) {
                printf("Process Died! waiting to restart...\n");
            }
        }

        uint64_t current_latest_modified_time = find_latest_modified_time((char *)succotash->directory);
        if (current_latest_modified_time > succotash->last_modified_time) {
            printf("files changed! restarting... \n");
            succotash->last_modified_time = current_latest_modified_time;
            
            if (!process_is_alive) {
                terminate_process(&succotash->handle); // just in case;
                start_process(succotash->command, &succotash->handle, &buffer);
            } else {
                restart_process(succotash->command, &succotash->handle, &buffer);
            }
        }

        // not working now.
        process_event(succotash, ctx);
        process_gui(succotash, ctx);
        render_gui(succotash, ctx);

        process_was_alive_previous_frame = process_is_alive;
    }  
    
    destroy_handle(&succotash->handle);
    // free(buffer.buffer_ptr);
    free(ctx);
    free(succotash);
    return 0;
}
