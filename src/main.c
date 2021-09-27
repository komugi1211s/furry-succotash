#include "vendor/microui.c"
#include "template_sdl_microui_opengl3.c"

#if _WIN32
#include <windows.h>
typedef HANDLE Thread_Handle;
typedef HANDLE Mutex;

typedef DWORD (*thread_proc)(void *params);
#else
#include <pthread.h>
typedef pthread_t       Thread_Handle;
typedef pthread_mutex_t Mutex;

#endif


int main(int argc, char **argv) {
    SDL_Init(SDL_INIT_EVERYTHING);
    r_init();
    /* init microui */
    mu_Context *ctx = malloc(sizeof(mu_Context));
    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;
    
    /* main loop */
    for (int is_running = 1; is_running;) {
        /* handle SDL events */
        SDL_Event e;
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
        
        /* process frame */
        mu_begin(ctx);
        int option = MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOCLOSE;
        if (mu_begin_window_ex(ctx, "Base_Window", mu_rect(0, 0, 400, 800), option)) {
            mu_layout_row(ctx, 2, (int[]) { 50, 50 }, 25);
            if (mu_button(ctx, "Add")) {
                printf("Added\n");
            }

            if (mu_button(ctx, "Delete")) {
                printf("Added\n");
            }

            mu_layout_row(ctx, 1, (int[]) { -1 }, 25);
            mu_text(ctx, "whatever whatever");

            mu_end_window(ctx);
        }

        mu_end(ctx);

        // process_frame(ctx);
        
        /* render */
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
    
    return 0;
}
