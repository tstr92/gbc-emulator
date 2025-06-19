
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         SM83 Memory Bus                             *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: main.c                                               *
 *        author: tstr92                                               *
 *          date: 2025-04-27                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "emulator.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define TITLE "Tib's GBC Emul"

#define SCALING_FACTOR 2
#define WIDTH (160 * SCALING_FACTOR)
#define HEIGHT (144 * SCALING_FACTOR)

#define SAMPLE_RATE 32768
#define BUFFER_SIZE 550

#define USE_PRINTF 0
#if (0 != USE_PRINTF)
#define debug_printf(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define debug_printf(fmt, ...) do {} while (0)
#endif

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    Uint32* screen_buffer;
    SDL_Texture* texture;
    SDL_AudioDeviceID audio_device;
    Uint32 frame_draw_event;
} sdl_rsc_t;

typedef struct
{
    bool esc;
    bool space;
    bool a;
    bool b;
    bool up;
    bool down;
    bool left;
    bool right;
} keys_t;

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static SDL_mutex* emulator_data_mutex;
static SDL_mutex* emulator_joypad_mutex;
static SDL_cond* emulator_data_cond;
static int emulator_data_collected = 0;
static keys_t keys;

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/
static void audio_callback     (void* userdata, Uint8* stream, int len);
static int  emulator_thread_fn (void *data);
static int  init_sdl_rsc       (sdl_rsc_t *p_sdl_rsc);
static void destroy_sdl_rsc    (sdl_rsc_t *p_sdl_rsc);
static void render             (sdl_rsc_t *p_sdl_rsc);
static void fps                (sdl_rsc_t *p_sdl_rsc);

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
// Callback function for audio
static void audio_callback(void* userdata, Uint8* stream, int len)
{
    sdl_rsc_t *p_sdl_rsc = (sdl_rsc_t *) userdata;
    static float t = 0;
    Sint16* out = (Sint16*) stream;
    int i;

    uint8_t r[1024], l[1024];
    size_t num_samples;
    emulator_get_audio_data(r, l, &num_samples);
    size_t copy_len = (len < num_samples) ? len : num_samples;
    if (copy_len)
    {
        for (i = 0; i < copy_len; i++)
        {
            out[i*2+0] = (((Sint16) l[i]) - 30) << 10;
            out[i*2+1] = (((Sint16) r[i]) - 30) << 10;
        }
    }
    else
    {
        memset(stream, 0, len);
    }

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = p_sdl_rsc->frame_draw_event;
    SDL_PushEvent(&event);

    SDL_LockMutex(emulator_data_mutex);
    emulator_data_collected = 1;
    SDL_CondSignal(emulator_data_cond);  // Signal the waiting thread
    SDL_UnlockMutex(emulator_data_mutex);

    return;
}

static int emulator_thread_fn(void *data)
{
    (void) data;
    emulator_run();
    return 0;
}

static int init_sdl_rsc(sdl_rsc_t *p_sdl_rsc)
{
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window
    p_sdl_rsc->window = SDL_CreateWindow(TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (!p_sdl_rsc->window)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a renderer
    p_sdl_rsc->renderer = SDL_CreateRenderer(p_sdl_rsc->window, -1, SDL_RENDERER_ACCELERATED);
    if (!p_sdl_rsc->renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a buffer of pixels
    p_sdl_rsc->screen_buffer = (Uint32*)malloc(WIDTH * HEIGHT * sizeof(Uint32));
    if (!p_sdl_rsc->screen_buffer)
    {
        fprintf(stderr, "malloc error\n");
        return 1;
    }

    // Create a texture to hold the buffer
    p_sdl_rsc->texture = SDL_CreateTexture(p_sdl_rsc->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if (!p_sdl_rsc->texture)
    {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want = {0}, have = {0};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2; // Mono
    want.samples = BUFFER_SIZE;
    want.callback = audio_callback;
    want.userdata = p_sdl_rsc;
    p_sdl_rsc->audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!p_sdl_rsc->audio_device)
    {
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
        return 1;
    }
    else
    {
        debug_printf("AudioSpec:\n");
        debug_printf("     freq: %d:\n", want.freq);
        debug_printf("   format: %d:\n", want.format);
        debug_printf(" channels: %d:\n", want.channels);
        debug_printf("  samples: %d:\n", want.samples);
    }

    p_sdl_rsc->frame_draw_event = SDL_RegisterEvents(1);
    if (p_sdl_rsc->frame_draw_event == (Uint32)-1)
    {
        fprintf(stderr, "SDL_RegisterEvents Error: %s\n", SDL_GetError());
        return 1;
    }

    return 0;
}

static void destroy_sdl_rsc(sdl_rsc_t *p_sdl_rsc)
{
    if (p_sdl_rsc->window)
    {
        SDL_DestroyWindow(p_sdl_rsc->window);
        p_sdl_rsc->window = NULL;
    }

    if (p_sdl_rsc->renderer)
    {
        SDL_DestroyRenderer(p_sdl_rsc->renderer);
        p_sdl_rsc->renderer = NULL;
    }

    if (p_sdl_rsc->screen_buffer)
    {
        free(p_sdl_rsc->screen_buffer);
        p_sdl_rsc->screen_buffer = NULL;
    }

    if (p_sdl_rsc->texture)
    {
        SDL_DestroyTexture(p_sdl_rsc->texture);
        p_sdl_rsc->texture = NULL;
    }

    if (p_sdl_rsc->audio_device)
    {
        SDL_CloseAudioDevice(p_sdl_rsc->audio_device);
        p_sdl_rsc->audio_device = 0;
    }

    // no cleanup needed for p_sdl_rsc->frame_draw_event

    SDL_Quit();

    return;
}

static void render(sdl_rsc_t *p_sdl_rsc)
{
    uint32_t screen[144][160];

    emulator_get_video_data((uint32_t*)screen);
    
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            int yy = y / SCALING_FACTOR;
            int xx = x / SCALING_FACTOR;

            p_sdl_rsc->screen_buffer[y * WIDTH + x] = screen[yy][xx];
        }
    }

    SDL_UpdateTexture(p_sdl_rsc->texture, NULL, p_sdl_rsc->screen_buffer, WIDTH * sizeof(Uint32));

    // Clear the renderer
    SDL_RenderClear(p_sdl_rsc->renderer);
    // Copy the texture to the renderer
    SDL_RenderCopy(p_sdl_rsc->renderer, p_sdl_rsc->texture, NULL, NULL);
    // Present the renderer
    SDL_RenderPresent(p_sdl_rsc->renderer);

    return;
}

static void fps(sdl_rsc_t *p_sdl_rsc)
{
    char title_buffer[512];
    static uint32_t fps = 0;
    static uint32_t timer = 0;
    uint32_t now;

    fps++;
    now = SDL_GetTicks();

    if ((uint32_t) (now - timer) >= 333)
    {
        snprintf(title_buffer, sizeof(title_buffer), "%s - %d fps", TITLE, (fps * 3));
        SDL_SetWindowTitle(p_sdl_rsc->window, title_buffer);
        timer = now;
        fps = 0;
    }

    return;
}

/*---------------------------------------------------------------------*
 *  Emulator Callbacks                                                 *
 *---------------------------------------------------------------------*/
void emulator_wait_for_data_collection(void)
{
    // return;
    SDL_LockMutex(emulator_data_mutex);
    while (!emulator_data_collected)
    {
        /* Wait until signaled */
        SDL_CondWait(emulator_data_cond, emulator_data_mutex);
    }
    emulator_data_collected = 0;
    SDL_UnlockMutex(emulator_data_mutex);
}

uint32_t platform_getSysTick_ms(void)
{
    return SDL_GetTicks();
}

uint8_t gbc_joypad_buttons_cb(void)
{
    uint8_t ret = 0;
    keys_t keys_local;

    SDL_LockMutex(emulator_joypad_mutex);
    keys_local = keys;
    SDL_UnlockMutex(emulator_joypad_mutex);

    ret |= keys.esc   ? GBC_JOYPAD_START  : 0;
    ret |= keys.space ? GBC_JOYPAD_SELECT : 0;
    ret |= keys.a     ? GBC_JOYPAD_A      : 0;
    ret |= keys.b     ? GBC_JOYPAD_B      : 0;
    ret |= keys.left  ? GBC_JOYPAD_LEFT   : 0;
    ret |= keys.right ? GBC_JOYPAD_RIGHT  : 0;
    ret |= keys.up    ? GBC_JOYPAD_UP     : 0;
    ret |= keys.down  ? GBC_JOYPAD_DOWN   : 0;

    return ret;
}

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
    if (2 > argc)
    {
        fprintf(stderr, "Error: expecting game as input argument.\n");
        return 1;
    }

    if (emulator_load_game(argv[1]))
    {
        fprintf(stderr, "Error: Could not initialize emulator.\n");
        return 1;
    }

    sdl_rsc_t sdl_rsc = (sdl_rsc_t)
    {
        .window        = NULL,
        .renderer      = NULL,
        .screen_buffer = NULL,
        .texture       = NULL,
    };

    if (init_sdl_rsc(&sdl_rsc))
    {
        fprintf(stderr, "Error: Could not initialize all sdl components.\n");
        destroy_sdl_rsc(&sdl_rsc);
        return 1;
    }

    emulator_joypad_mutex = SDL_CreateMutex();
    emulator_data_mutex = SDL_CreateMutex();
    emulator_data_cond = SDL_CreateCond();

    SDL_PauseAudioDevice(sdl_rsc.audio_device, 0); // Start playing

    SDL_Thread* emulator_thread = SDL_CreateThread(emulator_thread_fn, "EmulatorThread", NULL);

    // Main loop
    SDL_Event e;
    while (SDL_WaitEvent(&e))
    {
        if (e.type == sdl_rsc.frame_draw_event)
        {
            render(&sdl_rsc);
            fps(&sdl_rsc);
        }
        else if (e.type == SDL_KEYDOWN)
        {
            SDL_LockMutex(emulator_joypad_mutex);
            switch (e.key.keysym.sym)
            {
            case SDLK_ESCAPE: keys.esc   = true; break;
            case SDLK_SPACE : keys.space = true; break;
            case SDLK_a     : keys.a     = true; break;
            case SDLK_b     : keys.b     = true; break;
            case SDLK_LEFT  : keys.left  = true; break;
            case SDLK_RIGHT : keys.right = true; break;
            case SDLK_UP    : keys.up    = true; break;
            case SDLK_DOWN  : keys.down  = true; break;
            default:
                break;
            }
            SDL_UnlockMutex(emulator_joypad_mutex);
        }
        else if (e.type == SDL_KEYUP)
        {
            SDL_LockMutex(emulator_joypad_mutex);
            switch (e.key.keysym.sym)
            {
            case SDLK_ESCAPE: keys.esc   = false; break;
            case SDLK_SPACE : keys.space = false; break;
            case SDLK_a     : keys.a     = false; break;
            case SDLK_b     : keys.b     = false; break;
            case SDLK_LEFT  : keys.left  = false; break;
            case SDLK_RIGHT : keys.right = false; break;
            case SDLK_UP    : keys.up    = false; break;
            case SDLK_DOWN  : keys.down  = false; break;
            default:
                break;
            }
            SDL_UnlockMutex(emulator_joypad_mutex);
        }
        else if (e.type == SDL_QUIT)
        {
            break;
        }
    }

    // Cleanup
    destroy_sdl_rsc(&sdl_rsc);
    SDL_DestroyCond(emulator_data_cond);
    SDL_DestroyMutex(emulator_data_mutex);
    SDL_DestroyMutex(emulator_joypad_mutex);

    return 0;
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
