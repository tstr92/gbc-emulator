#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "emulator.h"

#define TITLE "Tibors Gameboy Emulator"

#define WIDTH 800
#define HEIGHT 600

#define SAMPLE_RATE 32768
#define BUFFER_SIZE 549

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    Uint32* screen_buffer;
    SDL_Texture* texture;
    SDL_AudioDeviceID audio_device;
    Uint32 frame_draw_event;
} sdl_rsc_t;

SDL_mutex* emulator_data_mutex;
SDL_cond* emulator_data_cond;
int emulator_data_collected = 0;

// This function creates a simple buffer filled with some colors (e.g., a gradient)
void generateBuffer(Uint32* buffer, Uint32 modifier)
{
    bool XorY = true;
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            Uint32 color = ((x+modifier) % 255) << 24 | ((y-modifier) % 255) << 16 | ((XorY ? x : y) * modifier % 255) << 8;
            buffer[y * WIDTH + x] = color;
            XorY = !XorY;
        }
    }
}

// Callback function for audio
void audio_callback(void* userdata, Uint8* stream, int len)
{
    sdl_rsc_t *p_sdl_rsc = (sdl_rsc_t *) userdata;
    static float t = 0;
    Sint16* out = (Sint16*) stream;
    int i;

    static bool firstTime = true;
    if (firstTime)
    {
        firstTime = false;
        printf("len=%d\n", len);
    }

    // int num_samples = len / sizeof(Sint16);
    // for (i = 0; i < num_samples; i++)
    // {
    //     float sample = sinf(2.0f * M_PI * 440.0 * t);
    //     out[i] = (Sint16)(sample * 32767.0f);
    //     t += 1.0 / (float) SAMPLE_RATE;
    // }

    uint8_t r[1024], l[1024];
    size_t num_samples;
    emulator_get_audio_data(r, l, &num_samples);
    size_t copy_len = (len < num_samples) ? len : num_samples;
    // printf("%ld samples: %u, %u, %u, %u, %u, ... %u, %u, %u, %u, %u\n", num_samples,
    //         r[0], r[1], r[2], r[3], r[4], r[num_samples - 5], r[num_samples - 4], r[num_samples - 3], r[num_samples - 2], r[num_samples - 1]);
    if (copy_len)
    {
        for (i = 0; i < copy_len; i++)
        {
            out[i*2+0] = (((Sint16) l[i]) - 30) * (1<<14);
            out[i*2+1] = (((Sint16) r[i]) - 30) * (1<<14);
        }
        printf("%ld samples: %d, %d, %d, %d, %d, ... %d, %d, %d, %d, %d\n", copy_len,
            out[1], out[3], out[5], out[7], out[9], out[copy_len - 10], out[copy_len - 8], out[copy_len - 6], out[copy_len - 4], out[copy_len - 2]);
    }
    else
    {
        memset(stream, 0, len);
    }

    SDL_LockMutex(emulator_data_mutex);
    emulator_data_collected = 1;
    SDL_CondSignal(emulator_data_cond);  // Signal the waiting thread
    SDL_UnlockMutex(emulator_data_mutex);

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = p_sdl_rsc->frame_draw_event;
    SDL_PushEvent(&event);

    return;
}

int emulator_thread_fn(void *data)
{
    (void) data;
    emulator_run();
    return 0;
}

void emulator_wait_for_data_collection(void)
{
    SDL_LockMutex(emulator_data_mutex);
    while (!emulator_data_collected)
    {
        SDL_CondWait(emulator_data_cond, emulator_data_mutex);  // Wait until signaled
    }
    emulator_data_collected = 0;
    SDL_UnlockMutex(emulator_data_mutex);
}

uint32_t platform_getSysTick_ms(void)
{
    return SDL_GetTicks();
}

int init_sdl_rsc(sdl_rsc_t *p_sdl_rsc)
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
        printf("AudioSpec:\n");
        printf("     freq: %d:\n", want.freq);
        printf("   format: %d:\n", want.format);
        printf(" channels: %d:\n", want.channels);
        printf("  samples: %d:\n", want.samples);
    }

    p_sdl_rsc->frame_draw_event = SDL_RegisterEvents(1);
    if (p_sdl_rsc->frame_draw_event == (Uint32)-1)
    {
        fprintf(stderr, "SDL_RegisterEvents Error: %s\n", SDL_GetError());
        return 1;
    }

    return 0;
}

void destroy_sdl_rsc(sdl_rsc_t *p_sdl_rsc)
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

void render(sdl_rsc_t *p_sdl_rsc)
{
    static Uint32 modifier = 0;

    generateBuffer(p_sdl_rsc->screen_buffer, modifier++);
    SDL_UpdateTexture(p_sdl_rsc->texture, NULL, p_sdl_rsc->screen_buffer, WIDTH * sizeof(Uint32));

    // Clear the renderer
    SDL_RenderClear(p_sdl_rsc->renderer);
    // Copy the texture to the renderer
    SDL_RenderCopy(p_sdl_rsc->renderer, p_sdl_rsc->texture, NULL, NULL);
    // Present the renderer
    SDL_RenderPresent(p_sdl_rsc->renderer);

    return;
}

void fps(sdl_rsc_t *p_sdl_rsc)
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

    emulator_data_mutex = SDL_CreateMutex();
    emulator_data_cond = SDL_CreateCond();

    SDL_PauseAudioDevice(sdl_rsc.audio_device, 0); // Start playing

    SDL_Thread* emulator_thread = SDL_CreateThread(emulator_thread_fn, "EmulatorThread", NULL);

    // Main loop
    SDL_Event e;

    // int quit = 0;
    // while (!quit)
    // {
    //     while (SDL_PollEvent(&e))
    //     {
    //         if (e.type == SDL_QUIT)
    //         {
    //             quit = 1;
    //         }
    //     }
      
    //     render(&sdl_rsc);
    //     fps(&sdl_rsc);
    // }
    
    while (SDL_WaitEvent(&e))
    {
        if (e.type == sdl_rsc.frame_draw_event)
        {
            render(&sdl_rsc);
            fps(&sdl_rsc);
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

    return 0;
}
