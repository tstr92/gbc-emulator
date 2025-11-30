
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         GBC PPU DEBUG                               *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: ppu_debug.c                                          *
 *        author: tstr92                                               *
 *          date: 2025-11-30                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

#if (0 != DEBUG)
/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>

#include "ppu_debug.h"
#include "emulator.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define SCALING_FACTOR 3
#define PALETTE_SIZE 6
#define PALETTE_WIDTH (PALETTE_SIZE * 32 * SCALING_FACTOR)
#define PALETTE_HEIGHT ((PALETTE_SIZE * 2 + 1) * SCALING_FACTOR)
#define VRAM_WIDTH (160 * SCALING_FACTOR)
#define VRAM_HEIGHT (160 * SCALING_FACTOR)

#define TITLE_LEN 64

typedef struct
{
    uint8_t tile_data[0x1800];
    uint8_t tile_map[2][0x400];
} vram_t;

typedef struct
{
    uint8_t bg_cram[64];
    uint8_t obj_cram[64];
    vram_t vram_0;
    vram_t vram_1;
} ppu_data_t;

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    Uint32 *screen_buffer;
    SDL_Texture *texture;
    char title[TITLE_LEN];
    int width;
    int height;
    int pos_x;
    int pos_y;
} window_data_t;

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static bool init_done = false;
window_data_t wnd_palette = (window_data_t)
{
    .title = "Palette Data",
    .width = PALETTE_WIDTH,
    .height = PALETTE_HEIGHT,
    .pos_x = 100,
    .pos_y = 100,
};
window_data_t wnd_vram_0 = (window_data_t)
{
    .title = "VRAM_0 Tile Data",
    .width = VRAM_WIDTH,
    .height = VRAM_HEIGHT,
    .pos_x = 100,
    .pos_y = 100 + PALETTE_HEIGHT + 50,
};
window_data_t wnd_vram_1 = (window_data_t)
{
    .title = "VRAM_1 Tile Data",
    .width = VRAM_WIDTH,
    .height = VRAM_HEIGHT,
    .pos_x = 100 + VRAM_WIDTH + 10,
    .pos_y = 100 + PALETTE_HEIGHT + 50,
};
static ppu_data_t ppu_data;
static int next_window_pos_x = 100;
static int next_window_pos_y = 100;

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/
static int init_window_data(window_data_t *p_window_data);
static void destroy_window_data(window_data_t *p_window_data);
static void render_palettes(window_data_t *p_window_data);
static void render_tile_data(window_data_t *p_window_data, vram_t *p_vram);
static void render_window(window_data_t *p_window_data);

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
static int init_window_data(window_data_t *p_window_data)
{
    // Create a window
    p_window_data->window = SDL_CreateWindow(p_window_data->title, p_window_data->pos_x, p_window_data->pos_y, p_window_data->width, p_window_data->height, SDL_WINDOW_SHOWN);
    if (!p_window_data->window)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a renderer
    p_window_data->renderer = SDL_CreateRenderer(p_window_data->window, -1, SDL_RENDERER_ACCELERATED);
    if (!p_window_data->renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a buffer of pixels
    p_window_data->screen_buffer = (Uint32*)malloc(p_window_data->width * p_window_data->height * sizeof(Uint32));
    if (!p_window_data->screen_buffer)
    {
        fprintf(stderr, "malloc error\n");
        return 1;
    }
    for (int y = 0; y < p_window_data->height; y++)
    {
        for (int x = 0; x < p_window_data->width; x++)
        {
            p_window_data->screen_buffer[y * p_window_data->width + x] = 0xFFFFFF00;
        }
    }

    // Create a texture to hold the buffer
    p_window_data->texture = SDL_CreateTexture(p_window_data->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, p_window_data->width, p_window_data->height);
    if (!p_window_data->texture)
    {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        return 1;
    }

    return 0;
}

static void destroy_window_data(window_data_t *p_window_data)
{
    if (p_window_data->window)
    {
        SDL_DestroyWindow(p_window_data->window);
        p_window_data->window = NULL;
    }

    if (p_window_data->renderer)
    {
        SDL_DestroyRenderer(p_window_data->renderer);
        p_window_data->renderer = NULL;
    }

    if (p_window_data->screen_buffer)
    {
        free(p_window_data->screen_buffer);
        p_window_data->screen_buffer = NULL;
    }

    if (p_window_data->texture)
    {
        SDL_DestroyTexture(p_window_data->texture);
        p_window_data->texture = NULL;
    }

    return;
}

/* resources must be initialized! */
static void render_palettes(window_data_t *p_window_data)
{
    const uint8_t *p_cram[] = { ppu_data.bg_cram, ppu_data.obj_cram };
    // uint32_t cram[2][32];

    for (int b = 0; b < 2; b++)
    {
        for (int c = 0; c < 32; c++)
        {
            uint16_t gbc_color = (((uint16_t) p_cram[b][c * 2 + 0]) << 0) | (((uint16_t) p_cram[b][c * 2 + 1]) << 8);
            uint32_t color = (((((uint32_t) gbc_color) & 0x001f) >>  0) << (24 + 3)) |
                             (((((uint32_t) gbc_color) & 0x03e0) >>  5) << (16 + 3)) |
                             (((((uint32_t) gbc_color) & 0x7c00) >> 10) << ( 8 + 3)) ;
            // cram[b][c] = color;
            for (int y = 0; y < (PALETTE_SIZE - 1) * SCALING_FACTOR; y++)
            {
                for (int x = 0; x < (PALETTE_SIZE - 1) * SCALING_FACTOR; x++)
                {
                    int yy = (b * PALETTE_SIZE * SCALING_FACTOR) + y;
                    int xx = (c * PALETTE_SIZE * SCALING_FACTOR) + x;

                    p_window_data->screen_buffer[yy * PALETTE_WIDTH + xx] = color;
                }
            }
        }
    }

    return;
}

/* resources must be initialized! */
static void render_tile_data(window_data_t *p_window_data, vram_t *p_vram)
{
    static uint32_t dmg_palette[4] = { 0xFFFFFFFF, 0xAAAAAAAA, 0x55555555, 0x00000000 };
    //static  uint32_t dmg_palette[4] = { 0x8cad2800, 0x6c942100, 0x426b2900, 0x21423100 };

    for (int t = 0; t < 0x1800; t += 0x10)
    {
        uint32_t tile_pixels[64];
        for (int i = 0; i < 0x10; i += 2)
        {
            uint8_t lsb = p_vram->tile_data[t + i + 0];
            uint8_t msb = p_vram->tile_data[t + i + 1];
            for (int j = 7; j >= 0; j--)
            {
                uint8_t px = 0;
                px += ((0 != (lsb & (1<<j))) ? 1 : 0);
                px += ((0 != (msb & (1<<j))) ? 2 : 0);
                tile_pixels[(i * 4) + 7 - j] = dmg_palette[px];
            }
        }

        int tileY = (t / 16) / 20;
        int tileX = (t / 16) % 20;
        for (int y = 0; y < 8 * SCALING_FACTOR; y++)
        {
            for (int x = 0; x < 8 * SCALING_FACTOR; x++)
            {
                int yy = tileY * 8 * SCALING_FACTOR + y;
                int xx = tileX * 8 * SCALING_FACTOR + x;
                int py = y / SCALING_FACTOR;
                int px = x / SCALING_FACTOR;
                p_window_data->screen_buffer[yy * p_window_data->width + xx] = tile_pixels[py * 8 + px];
            }
        }
    }

    return;
}

static void render_window(window_data_t *p_window_data)
{
    SDL_UpdateTexture(p_window_data->texture, NULL, p_window_data->screen_buffer, p_window_data->width * sizeof(Uint32));
    SDL_RenderClear(p_window_data->renderer);
    SDL_RenderCopy(p_window_data->renderer, p_window_data->texture, NULL, NULL);
    SDL_RenderPresent(p_window_data->renderer);
    return;
}

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
int ppu_debug_init(void)
{
    int error = 0;

    if (!error)
    {
        error = init_window_data(&wnd_palette);
    }
    if(!error)
    {
        error = init_window_data(&wnd_vram_0);
    }
    if(!error)
    {
        error = init_window_data(&wnd_vram_1);
    }
    if (!error)
    {
        init_done = true;
    }

    return error;
}

void ppu_debug_destroy(void)
{
    destroy_window_data(&wnd_palette);
    destroy_window_data(&wnd_vram_0);
    destroy_window_data(&wnd_vram_1);

    init_done = false;

    return;
}

void ppu_debug_render(void)
{
    if (init_done)
    {
        emulator_debug_get_ppu_data(
            ppu_data.bg_cram,
            ppu_data.obj_cram,
            (uint8_t*)&ppu_data.vram_0,
            (uint8_t*)&ppu_data.vram_1
        );

        render_palettes(&wnd_palette);
        render_tile_data(&wnd_vram_0, &ppu_data.vram_0);
        render_tile_data(&wnd_vram_1, &ppu_data.vram_1);

        render_window(&wnd_palette);
        render_window(&wnd_vram_0);
        render_window(&wnd_vram_1);
    }

    return;
}
#endif /* #if (0 != DEBUG) */
/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
