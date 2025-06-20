
/*---------------------------------------------------------------------*
 *                                                                     *
 *                           GBC Emulator                              *
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
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "emulator.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define TITLE "Tib's GBC Emul"

#define SCALING_FACTOR 3
#define WIDTH (160 * SCALING_FACTOR)
#define HEIGHT (144 * SCALING_FACTOR)
#define MENU_OFFSET_X (WIDTH / 20)
#define MENU_OFFSET_Y (HEIGHT / 20)
#define FONTSIZE (HEIGHT / 16)

#define MENU_TIMER_INTERVAL_MS 16

#define SAMPLE_RATE 32768
#define BUFFER_SIZE 550

#define USE_PRINTF 1
#if (0 != USE_PRINTF)
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...) do {} while (0)
#endif

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    Uint32 *screen_buffer;
    SDL_Texture *texture;
    SDL_AudioDeviceID audio_device;
    Uint32 frame_draw_event;
    SDL_TimerID menu_timer;
    
    /* menu */
    SDL_Rect menu_overlay;
    TTF_Font *font;
} sdl_rsc_t;

typedef struct
{
    SDL_KeyCode keyCode;
    bool state;
} key_t;

#define GBC_KEY_START  0
#define GBC_KEY_SELECT 1
#define GBC_KEY_A      2
#define GBC_KEY_B      3
#define GBC_KEY_UP     4
#define GBC_KEY_DOWN   5
#define GBC_KEY_LEFT   6
#define GBC_KEY_RIGHT  7

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static SDL_mutex* gEmulatorDataMutex;
static SDL_mutex* gEmulatorJoypadMutex;
static SDL_cond* gEmulatorDataCond;
static int gEmulatorDataCollected = 0;
static int gActiveMenuLine = 0;
static uint8_t gEmulatorSpeed = 10; // 10 ... 20 = 100% ... 200%
static int32_t gVolume = 50;
FILE *gSaveFile = NULL;
long gSaveFileSize = 0;

static key_t keys[8] =
{
    (key_t) { .keyCode = SDLK_RETURN, .state = false },
    (key_t) { .keyCode = SDLK_SPACE,  .state = false },
    (key_t) { .keyCode = SDLK_a,      .state = false },
    (key_t) { .keyCode = SDLK_b,      .state = false },
    (key_t) { .keyCode = SDLK_UP,     .state = false },
    (key_t) { .keyCode = SDLK_DOWN,   .state = false },
    (key_t) { .keyCode = SDLK_LEFT,   .state = false },
    (key_t) { .keyCode = SDLK_RIGHT,  .state = false },
};

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/
static void   audio_callback      (void* userdata, Uint8* stream, int len);
static Uint32 timerCallback       (Uint32 interval, void* param);
static int    emulator_thread_fn  (void *data);
static int    init_sdl_rsc        (sdl_rsc_t *p_sdl_rsc);
static void   destroy_sdl_rsc     (sdl_rsc_t *p_sdl_rsc);
static void   handle_menu         (sdl_rsc_t *p_sdl_rsc, SDL_Event event);
static void   render              (sdl_rsc_t *p_sdl_rsc);
static void   fps                 (sdl_rsc_t *p_sdl_rsc);
static int    save_emulator_state (char *fname);

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
            int32_t local_l, local_r;
            local_l = (int32_t) ((((Sint16) l[i]) - 30) << 10);
            local_r = (int32_t) ((((Sint16) r[i]) - 30) << 10);
            out[i*2+0] = (Sint16) ((local_l * gVolume) / 100);
            out[i*2+1] = (Sint16) ((local_r * gVolume) / 100);
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

    SDL_LockMutex(gEmulatorDataMutex);
    gEmulatorDataCollected = 1;
    SDL_CondSignal(gEmulatorDataCond);  // Signal the waiting thread
    SDL_UnlockMutex(gEmulatorDataMutex);

    return;
}

static Uint32 timerCallback(Uint32 interval, void* param)
{
    sdl_rsc_t *p_sdl_rsc = (sdl_rsc_t *) param;
    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = p_sdl_rsc->frame_draw_event;
    SDL_PushEvent(&event);
    return MENU_TIMER_INTERVAL_MS; // repeat
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
    if (0 != SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (0 != TTF_Init())
    {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
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

    p_sdl_rsc->font = TTF_OpenFont("assets/Flexi_IBM_VGA_False.ttf", FONTSIZE);
    if (!p_sdl_rsc->font)
    {
        fprintf(stderr, "Failed to open font: %s\n", TTF_GetError());
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

    if(p_sdl_rsc->font)
    {
        TTF_CloseFont(p_sdl_rsc->font);
        p_sdl_rsc->font = NULL;
    }

    // no cleanup needed for p_sdl_rsc->frame_draw_event

    SDL_Quit();

    return;
}


void renderTextLine(sdl_rsc_t *p_sdl_rsc, const char* text, SDL_Color color, int lineNum)
{
    int x, y;
    SDL_Surface* surface = TTF_RenderText_Blended(p_sdl_rsc->font, text, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(p_sdl_rsc->renderer, surface);

    if (0 == lineNum)
    {
        /* center header */
        x = p_sdl_rsc->menu_overlay.x + p_sdl_rsc->menu_overlay.w / 2 - surface->w / 2;
    }
    else
    {
        /* left align options */
        x = p_sdl_rsc->menu_overlay.x + MENU_OFFSET_X;
    }
    y = p_sdl_rsc->menu_overlay.y + (lineNum + 1) * FONTSIZE;

    SDL_Rect dst = { x, y, surface->w, surface->h };

    SDL_RenderCopy(p_sdl_rsc->renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);

    return;
}

static void handle_menu(sdl_rsc_t *p_sdl_rsc, SDL_Event event)
{
    char menu_txt[4][64];
    static uint32_t last_save = -501;
    uint32_t now = SDL_GetTicks();

    #define TXT_SETTINGS "Settings"
    #define TXT_SPEED    "Emulator Speed        %1.1f"
    #define TXT_VOLUME   "Volume                %3u"
    #define TXT_SAVE0    "Save Game"
    #define TXT_SAVE1    "Save Game           done!"

    /* prepare menu text */
    snprintf(&menu_txt[0][0], sizeof(menu_txt[0]), TXT_SETTINGS);
    snprintf(&menu_txt[1][0], sizeof(menu_txt[1]), TXT_SPEED, ((float)gEmulatorSpeed / 10.0));
    snprintf(&menu_txt[2][0], sizeof(menu_txt[1]), TXT_VOLUME, gVolume);
    if ((uint32_t) (now - last_save) < 500)
    {
        snprintf(&menu_txt[3][0], sizeof(menu_txt[0]), TXT_SAVE1);
    }
    else
    {
        snprintf(&menu_txt[3][0], sizeof(menu_txt[0]), TXT_SAVE0);
    }
    

    /* copy last screen */
    SDL_RenderClear(p_sdl_rsc->renderer);
    SDL_RenderCopy(p_sdl_rsc->renderer, p_sdl_rsc->texture, NULL, NULL);

    /* menu box */
    SDL_SetRenderDrawColor(p_sdl_rsc->renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(p_sdl_rsc->renderer, &p_sdl_rsc->menu_overlay);
    for (int i = 0; i < _countof(menu_txt); i++)
    {
        SDL_Color color;
        color = (gActiveMenuLine == i)            ? 
                (SDL_Color) { 255, 255,   0, 255 } :
                (SDL_Color) { 255, 255, 255, 255 } ;
        renderTextLine(p_sdl_rsc, menu_txt[i], color, i);
    }
    
    if (SDL_KEYDOWN == event.type)
    {
        if (1 == gActiveMenuLine)
        {
            if ((SDLK_LEFT == event.key.keysym.sym) && (10 < gEmulatorSpeed))
            {
                gEmulatorSpeed--;
            }
            else if ((SDLK_RIGHT == event.key.keysym.sym) && (20 > gEmulatorSpeed))
            {
                gEmulatorSpeed++;
            }
        }

        if (2 == gActiveMenuLine)
        {
            if ((SDLK_LEFT == event.key.keysym.sym) && (0 < gVolume))
            {
                gVolume--;
            }
            else if ((SDLK_RIGHT == event.key.keysym.sym) && (100 > gVolume))
            {
                gVolume++;
            }
        }

        if (3 == gActiveMenuLine)
        {
            if (NULL == gSaveFile)
            {
#if 0
                char fileName[64];
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                snprintf(fileName, sizeof(fileName), "savegame_%4u-%02u-%02u_%02u-%02u-%02u.bin",
                    1900+t->tm_year, t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
                save_emulator_state(fileName);
#else
                if (0 == save_emulator_state("savegame.bin"))
                {
                    last_save = now;
                }
#endif
            }
        }

        if ((SDLK_UP == event.key.keysym.sym) && (1 < gActiveMenuLine))
        {
            gActiveMenuLine--;
        }
        else if ((SDLK_DOWN == event.key.keysym.sym) && (3 > gActiveMenuLine))
        {
            gActiveMenuLine++;
        }
    }

    SDL_RenderPresent(p_sdl_rsc->renderer);
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

    SDL_RenderClear(p_sdl_rsc->renderer);
    SDL_RenderCopy(p_sdl_rsc->renderer, p_sdl_rsc->texture, NULL, NULL);
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

static int save_emulator_state(char *fname)
{
    if (NULL == fname)
    {
        return 1;
    }

    gSaveFile = fopen(fname, "wb");

    if (NULL == gSaveFile)
    {
        return 1;
    }

    emulator_write_save_file();

    fclose(gSaveFile);

    gSaveFile = NULL;
    gSaveFileSize = 0;

    return 0;
}

/*---------------------------------------------------------------------*
 *  Emulator Callbacks                                                 *
 *---------------------------------------------------------------------*/
void emulator_wait_for_data_collection(void)
{
    // return;
    SDL_LockMutex(gEmulatorDataMutex);
    while (!gEmulatorDataCollected)
    {
        /* Wait until signaled */
        SDL_CondWait(gEmulatorDataCond, gEmulatorDataMutex);
    }
    gEmulatorDataCollected = 0;
    SDL_UnlockMutex(gEmulatorDataMutex);
}

uint32_t platform_getSysTick_ms(void)
{
    return SDL_GetTicks();
}

uint8_t gbc_joypad_buttons_cb(void)
{
    uint8_t ret = 0;

    SDL_LockMutex(gEmulatorJoypadMutex);
    ret |= keys[GBC_KEY_START ].state ? GBC_JOYPAD_START  : 0;
    ret |= keys[GBC_KEY_SELECT].state ? GBC_JOYPAD_SELECT : 0;
    ret |= keys[GBC_KEY_A     ].state ? GBC_JOYPAD_A      : 0;
    ret |= keys[GBC_KEY_B     ].state ? GBC_JOYPAD_B      : 0;
    ret |= keys[GBC_KEY_UP    ].state ? GBC_JOYPAD_UP     : 0;
    ret |= keys[GBC_KEY_DOWN  ].state ? GBC_JOYPAD_DOWN   : 0;
    ret |= keys[GBC_KEY_LEFT  ].state ? GBC_JOYPAD_LEFT   : 0;
    ret |= keys[GBC_KEY_RIGHT ].state ? GBC_JOYPAD_RIGHT  : 0;
    SDL_UnlockMutex(gEmulatorJoypadMutex);

    return ret;
}


uint8_t emulator_get_speed(void)
{
    return gEmulatorSpeed;
}


void emulator_cb_write_to_save_file(uint8_t *data, size_t size)
{
    if (gSaveFile)
    {
        fwrite(data, 1, size, gSaveFile);
    }
}

int emulator_cb_read_from_save_file(uint8_t *data, size_t size)
{
    int ret = 1;
    if (gSaveFile)
    {
        long current_pos = ftell(gSaveFile);
        if ((-1 != current_pos) && (size <= (gSaveFileSize - current_pos)))
        {
            size_t read_cnt = fread(data, 1, size, gSaveFile);
            if (read_cnt == size)
            {
                ret = 0;
            }
        }
    }
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

    if (2 < argc)
    {
        gSaveFile = fopen(argv[2], "rb");
        if (NULL != gSaveFile)
        {
            fseek(gSaveFile, 0, SEEK_END);
            gSaveFileSize = ftell(gSaveFile);
            if (-1 != gSaveFileSize)
            {
                fseek(gSaveFile, 0, SEEK_SET);
                emulator_load_save_file();
            }
        }
        fclose(gSaveFile);
        gSaveFile = NULL;
        gSaveFileSize = 0;
    }

    sdl_rsc_t sdl_rsc = (sdl_rsc_t)
    {
        .window        = NULL,
        .renderer      = NULL,
        .screen_buffer = NULL,
        .texture       = NULL,
        .menu_overlay  = { MENU_OFFSET_X, MENU_OFFSET_Y, WIDTH - 2 * MENU_OFFSET_X, HEIGHT - 2 * MENU_OFFSET_Y },
    };

    if (init_sdl_rsc(&sdl_rsc))
    {
        fprintf(stderr, "Error: Could not initialize all sdl components.\n");
        destroy_sdl_rsc(&sdl_rsc);
        return 1;
    }

    gEmulatorJoypadMutex = SDL_CreateMutex();
    gEmulatorDataMutex = SDL_CreateMutex();
    gEmulatorDataCond = SDL_CreateCond();

    SDL_Thread* emulator_thread = SDL_CreateThread(emulator_thread_fn, "EmulatorThread", NULL);
    SDL_PauseAudioDevice(sdl_rsc.audio_device, 0); // Start playing

    // Main loop
    SDL_Event e;
    bool menu = false;
    while (SDL_WaitEvent(&e))
    {
        if (menu)
        {
            handle_menu(&sdl_rsc, e);
            fps(&sdl_rsc);
        }

        if (e.type == sdl_rsc.frame_draw_event)
        {
            if (!menu)
            {
                render(&sdl_rsc);
                fps(&sdl_rsc);
            }
        }
        else if ((e.type == SDL_KEYDOWN) && (SDLK_ESCAPE == e.key.keysym.sym))
        {
            if (menu)
            {
                menu = false;
                SDL_PauseAudioDevice(sdl_rsc.audio_device, 0); // Start Emulator
                SDL_SetRenderDrawBlendMode(sdl_rsc.renderer, SDL_BLENDMODE_NONE);
                SDL_RemoveTimer(sdl_rsc.menu_timer);
            }
            else
            {
                menu = true;
                gActiveMenuLine = 1;
                SDL_PauseAudioDevice(sdl_rsc.audio_device, 1); // Pause Emulator
                SDL_SetRenderDrawBlendMode(sdl_rsc.renderer, SDL_BLENDMODE_BLEND);
                sdl_rsc.menu_timer = SDL_AddTimer(MENU_TIMER_INTERVAL_MS, timerCallback, &sdl_rsc);
            }
        }
        else if ((e.type == SDL_KEYDOWN) ||
                 (e.type == SDL_KEYUP  ))
        {
            bool keyPressed = (e.type == SDL_KEYDOWN);
            SDL_LockMutex(gEmulatorJoypadMutex);
            for (int key = 0; key < _countof(keys); key++)
            {
                if (keys[key].keyCode == e.key.keysym.sym)
                {
                    keys[key].state = keyPressed;
                    break;
                }
            }
            SDL_UnlockMutex(gEmulatorJoypadMutex);
        }
        else if (e.type == SDL_QUIT)
        {
            break;
        }
    }

    // Cleanup
    destroy_sdl_rsc(&sdl_rsc);
    SDL_DestroyCond(gEmulatorDataCond);
    SDL_DestroyMutex(gEmulatorDataMutex);
    SDL_DestroyMutex(gEmulatorJoypadMutex);

    return 0;
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
