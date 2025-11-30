
/*---------------------------------------------------------------------*
 *                                                                     *
 *                          GBC PPU                                    *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: ppu.c                                                *
 *        author: tstr92                                               *
 *          date: 2025-05-04                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include <stdlib.h>

#include "ppu.h"
#include "bus.h"
#include "debug.h"
#include "emulator.h"

#define DEBUG_SHOW_WINDOW 1

#define ABS_DIFF(_a, _b) ((_a > _b) ? (_a - _b) : (_b - _a))

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define LCDC        0xFF40   // LCD control
#define STAT        0xFF41   // LCD status
#define SCY         0xFF42   // Viewport Y position (0 ... 255)
#define SCX         0xFF43   // Viewport X position (0 ... 255)
#define LY          0xFF44   // Current LCD Y coordinate (0 ... 153), (144 ... 153)=VBLANK
#define LYC         0xFF45   // LY compare: Sets "LYC==LY"-Flag in STAT-Register (-> STAT-IRQ (if enabled))
#define BGP         0xFF47   // BG palette data
#define OBP0        0xFF48   // OBJ palette 0 data
#define OBP1        0xFF49   // OBJ palette 1 data
#define WY          0xFF4A   // Window Y position: On-Screen-Pos of Window (0 ... 143) (7: Top)
#define WX          0xFF4B   // Window X position plus 7: On-Screen-Pos of Window (0 ... 166) (0: Left)
#define VBK         0xFF4F   // VRAM bank: Bit 0 Selects bank, Bits 1-7 read 0xFE
#define BCPS_BGPI   0xFF68   // Background color palette specification / Background palette index
#define BCPD_BGPD   0xFF69   // Background color palette data / Background palette data
#define OCPS_OBPI   0xFF6A   // OBJ color palette specification / OBJ palette index
#define OCPD_OBPD   0xFF6B   // OBJ color palette data / OBJ palette data
#define OPRI        0xFF6C   // Object priority mode

#define LCDC_BG_WNDOW_EN_PRIO   (1<<0)   // DMG: 0: BG and Window become white ; CGB: BG and Window lose prio
#define LCDC_OBJ_EN             (1<<1)   // 0 = Off; 1 = On
#define LCDC_OBJ_SIZE           (1<<2)   // 0 = 8×8; 1 = 8×16
#define LCDC_BG_TILE_MAP        (1<<3)   // 0 = 9800–9BFF; 1 = 9C00–9FFF
#define LCDC_BG_WINDOW_TILES    (1<<4)   // 0 = 8800–97FF; 1 = 8000–8FFF
#define LCDC_WINDOW_EN          (1<<5)   // 0 = Off; 1 = On
#define LCDC_WINDOW_TILE_MAP    (1<<6)   // 0 = 9800–9BFF; 1 = 9C00–9FFF
#define LCDC_LCD_PPU_EN         (1<<7)   // 0 = Off; 1 = On

#define STAT_PPU_MODE           (0x03)
#define STAT_LYC_EQ_LY          (1<<2)
#define STAT_MODE0_INT_SEL      (1<<3)
#define STAT_MODE1_INT_SEL      (1<<4)
#define STAT_MODE2_INT_SEL      (1<<5)
#define STAT_LYC_INT_SEL        (1<<6)

#define BGP_ID0_MSK             (3<<0)
#define BGP_ID0_POS             (0)
#define BGP_ID1_MSK             (3<<2)
#define BGP_ID1_POS             (2)
#define BGP_ID2_MSK             (3<<4)
#define BGP_ID2_POS             (4)
#define BGP_ID3_MSK             (3<<6)
#define BGP_ID3_POS             (6)

#define OBP_ID1_MSK             (3<<2)
#define OBP_ID1_POS             (2)
#define OBP_ID2_MSK             (3<<4)
#define OBP_ID2_POS             (4)
#define OBP_ID3_MSK             (3<<6)
#define OBP_ID3_POS             (6)

#define KEY1_SWITCH_ARMED       (1<<0)
#define KEY1_CURRENT_SPEED      (1<<7)

#define VBK_BANK_SEL            (1<<0)

#define CPS_PI_ADDRESS          (0x3F)
#define CPS_PI_AUTO_INC         (1<<7)

#define CPD_PD_RED_MSK          (0x1F<<0)    // implement 64B color palette ram as uint16_t
#define CPD_PD_RED_POS          0            // implement 64B color palette ram as uint16_t
#define CPD_PD_GRN_MSK          (0x1F<<5)    // implement 64B color palette ram as uint16_t
#define CPD_PD_GRN_POS          5            // implement 64B color palette ram as uint16_t
#define CPD_PD_BLU_MSK          (0x1F<<10)   // implement 64B color palette ram as uint16_t
#define CPD_PD_BLU_POS          10           // implement 64B color palette ram as uint16_t

#define OPRI_PRIO_MODE_DMG      (1<<0) // 0: Gameboy Color, 1 Original Gameboy

#define COLOR_ID_MSK            (0x03)

#define PALETTE_ADDR_MSK        (0x3F) // Bits 0-5: Address, 
#define PALETTE_AUTO_INC_MSK    (0x80) // Bit 7: auto increment
#define PALETTE_SPEC_MSK        (PALETTE_ADDR_MSK | PALETTE_AUTO_INC_MSK)

typedef struct
{
    uint8_t y_pos;
    uint8_t x_pos;
    uint8_t tile_idx;
    union
    {
        uint8_t attributes;
        struct
        {
            uint8_t cgb_palette : 3; // 0-2
            uint8_t bank        : 1; // 3
            uint8_t dmg_palette : 1; // 4
            uint8_t x_flip      : 1; // 5
            uint8_t y_flip      : 1; // 6
            uint8_t priority    : 1; // 7
        };
    };
} obj_attr_t;

typedef struct
{
    obj_attr_t sprites[10];
    uint8_t wr;
    uint8_t rd;
} scanline_objects_t;

typedef struct
{
    uint8_t lcdc;
    uint8_t stat;
    uint8_t scy;
    uint8_t scx;
    uint8_t ly;
    uint8_t lyc;
    uint8_t bgp;
    uint8_t obp0;
    uint8_t obp1;
    uint8_t wy;
    uint8_t wx;
    uint8_t vbk;
    uint8_t bcps_bgpi;
    uint8_t bcpd_bgpd;
    uint8_t ocps_obpi;
    uint8_t ocpd_obpd;
    uint8_t opri;

    union
    {
        uint8_t oam_raw[0xA0];
        obj_attr_t object_attributes[40];
    };

    scanline_objects_t scobj;
    
} ppu_mem_t;

typedef enum
{
    mode0_hblank   = 0,   // 87-204 dots
    mode1_vblank   = 1,   // 4560 dots (10 scanlines)
    mode2_oam_scan = 2,   // 80 dots
    mode3_draw     = 3    // 172-289 dots
} ppu_mode_t;

typedef struct
{
    ppu_mode_t mode;
    uint32_t line_dot_cnt;
    uint32_t lx;
    uint8_t x_discard_count;
    uint8_t pixel_delay;
    uint8_t obj_size;
} ppu_state_t;

typedef struct
{
    uint8_t color_id;       // a value between 0 and 3
    uint8_t dmg_palette;    // only applies to objects
    uint8_t cgb_palette;    // a value between 0 and 7
    uint8_t sprite_prio;    // on CGB this is the OAM index for the object and on DMG this doesn’t exist
    uint8_t bg_prio;        // holds the value of the OBJ-to-BG Priority bit
    uint8_t oam_tile_index; // pixel priorization
} pixel_t;

typedef struct
{
    pixel_t pixels[8];
    uint8_t fill_level;
} pixel_buffer_t;

#define FIFO_NUM_ENTRIES 16
typedef struct
{
    pixel_t data[FIFO_NUM_ENTRIES];
    uint8_t fill_level;
    union
    {
        uint8_t wr_rd_ptr;
        struct
        {
            uint8_t wr : 4;
            uint8_t rd : 4;
        };
    };
} pixel_fifo_t;


typedef enum
{
    pfs_get_tile_0_e,
    pfs_get_tile_1_e,
    pfs_get_tile_data_lo_0_e,
    pfs_get_tile_data_lo_1_e,
    pfs_get_tile_data_hi_0_e,
    pfs_get_tile_data_hi_1_e,
    pfs_push_e,
    pfs_suspended_e,
} pixel_fetcher_state_t;

typedef struct
{
    /* bg data */
    pixel_fetcher_state_t bg_state;
    pixel_fifo_t bg_fifo;
    uint8_t bg_tile_number;
    uint8_t bg_tile_attr;
    uint8_t bg_tile_data[2];

    /* sprite data */
    pixel_fetcher_state_t obj_state;
    uint8_t obj_tile_number;
    pixel_fifo_t obj_fifo;
    uint8_t obj_tile_data[2];

    /* common */
    uint8_t tile_y_offset;
    uint8_t tile_hi_lo;
    uint8_t wy;
    uint32_t x;

} pixel_fetcher_t;

typedef union
{
    uint32_t raw;
    struct
    {
        uint8_t A;
        uint8_t B;
        uint8_t G;
        uint8_t R;
    };
} screen_pixel_t;


typedef union
{
    uint8_t raw[0x2000];
    struct
    {
        uint8_t tile_data[0x1800];
        uint8_t tile_map[2][0x400];
    };
} vram_t;

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static ppu_mem_t ppu;
static ppu_state_t ppu_state;
static pixel_fetcher_t pixel_fetcher;
static vram_t vram[2];
static uint8_t obj_cram[64];
static uint8_t bg_cram[64];

static screen_pixel_t screen0[144][160];
static screen_pixel_t screen1[144][160];
static screen_pixel_t *screen = (screen_pixel_t *) &screen0[0][0];

uint32_t dmg_palette[4] =
{
    0xFFFFFFFF, 0xAAAAAAAA, 0x55555555, 0x00000000
    
    /* *Original* DMG Palette */
    /* credit: https://lospec.com/palette-list/dmg-nso */
    // 0x8cad2800, 0x6c942100, 0x426b2900, 0x21423100
};

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/
// Comparison function for qsort
int compare_sprite_x_offsets(const void *a, const void *b)
{
    const obj_attr_t *arg1 = (const obj_attr_t *)a;
    const obj_attr_t *arg2 = (const obj_attr_t *)b;
    return (arg1->x_pos > arg2->x_pos) - (arg1->x_pos < arg2->x_pos); // returns -1, 0, or 1
}

static inline bool ppu_pixel_fifo_empty(pixel_fifo_t *pFifo)
{
    return (0 == pFifo->fill_level);
}

static inline void ppu_pixel_fifo_push(pixel_fifo_t *pFifo, pixel_t px)
{
    pFifo->data[pFifo->wr++] = px;
    pFifo->fill_level++;
    return;
}

static inline bool ppu_pixel_fifo_pop(pixel_fifo_t *pFifo, pixel_t *p_px)
{
    bool ret = false;

    if (!ppu_pixel_fifo_empty(pFifo))
    {
        *p_px = pFifo->data[pFifo->rd++];
        pFifo->fill_level--;
        ret = true;
    }
    
     return ret;
}

static inline uint8_t ppu_pixel_fifo_num_empty_slots(pixel_fifo_t *pFifo)
{
    return FIFO_NUM_ENTRIES - pFifo->fill_level;
}

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
void ppu_pixel_fetcher_do(void)
{
    if (pfs_suspended_e == pixel_fetcher.obj_state)
    {
        if (ppu.scobj.wr > ppu.scobj.rd)
        {
            if (ppu.scobj.sprites[ppu.scobj.rd].x_pos <= (ppu_state.lx + 8))
            {
                /* found a matching sprite */
                pixel_fetcher.bg_state = pfs_suspended_e;
                pixel_fetcher.obj_state = pfs_get_tile_0_e;
            }
        }
    }

    switch (pixel_fetcher.obj_state)
    {
    case pfs_get_tile_0_e:
    {
        pixel_fetcher.tile_y_offset = ppu.ly - (ppu.scobj.sprites[ppu.scobj.rd].y_pos - 16);
        if (16 == ppu_state.obj_size)
        {
            if (8 <= pixel_fetcher.tile_y_offset)
            {
                /* bottom tile */
                pixel_fetcher.obj_tile_number = (ppu.scobj.sprites[ppu.scobj.rd].tile_idx & 0xFE);
                pixel_fetcher.tile_y_offset -= 8;
            }
            else
            {
                /* top tile */
                pixel_fetcher.obj_tile_number = (ppu.scobj.sprites[ppu.scobj.rd].tile_idx | 0x01);
            }
        }
        else
        {
            pixel_fetcher.obj_tile_number = ppu.scobj.sprites[ppu.scobj.rd].tile_idx;
        }
        pixel_fetcher.tile_hi_lo = 0;

        pixel_fetcher.obj_state++;
    }
    break;

    case pfs_get_tile_data_hi_0_e:
    case pfs_get_tile_data_lo_0_e:
    {
        uint8_t *p_window_tile_data = &(vram[ppu.vbk & 0x01].tile_data[0]);
        uint8_t data;
        if (ppu.scobj.sprites[ppu.scobj.rd].y_flip)
        {
            data = p_window_tile_data[pixel_fetcher.obj_tile_number * 16 + (16 - 2 * pixel_fetcher.tile_y_offset) + pixel_fetcher.tile_hi_lo];
        }
        else
        {
            data = p_window_tile_data[pixel_fetcher.obj_tile_number * 16 + 2 * pixel_fetcher.tile_y_offset + pixel_fetcher.tile_hi_lo];
        }
        pixel_fetcher.obj_tile_data[pixel_fetcher.tile_hi_lo] = data;
        pixel_fetcher.tile_hi_lo++;

        pixel_fetcher.obj_state++;
    }
    break;

    case pfs_get_tile_data_hi_1_e:
        pixel_fetcher.obj_state++;
        /* no break */
    case pfs_push_e:
    {
        pixel_buffer_t newPixels = {0};
        pixel_buffer_t oldPixels = {0};
        pixel_t pixel;

        int numPixels = 8 - ((ppu_state.lx + 8) - ppu.scobj.sprites[ppu.scobj.rd].x_pos);
        if (ppu.scobj.sprites[ppu.scobj.rd].x_flip)
        {
            for (int i = 0; i <= (numPixels - 1); i++)
                {
                    pixel = (pixel_t)
                    {
                        .color_id       = ((pixel_fetcher.obj_tile_data[0] & (1<<i)) ? 0x01 : 0x00) |
                                        ((pixel_fetcher.obj_tile_data[1] & (1<<i)) ? 0x02 : 0x00) ,
                        .sprite_prio    = ppu.scobj.sprites[ppu.scobj.rd].priority,
                        .oam_tile_index = ppu.scobj.sprites[ppu.scobj.rd].tile_idx,
                        .dmg_palette    = ppu.scobj.sprites[ppu.scobj.rd].dmg_palette,
                        .cgb_palette    = ppu.scobj.sprites[ppu.scobj.rd].cgb_palette
                    };
                newPixels.pixels[newPixels.fill_level++] = pixel;
            }
        }
        else
        {
            for (int i = (numPixels - 1); i >= 0; i--)
            {
                pixel = (pixel_t)
                {
                    .color_id    = ((pixel_fetcher.obj_tile_data[0] & (1<<i)) ? 0x01 : 0x00) |
                                    ((pixel_fetcher.obj_tile_data[1] & (1<<i)) ? 0x02 : 0x00) ,
                    .sprite_prio = ppu.scobj.sprites[ppu.scobj.rd].priority,
                    .oam_tile_index = ppu.scobj.sprites[ppu.scobj.rd].tile_idx,
                    .dmg_palette    = ppu.scobj.sprites[ppu.scobj.rd].dmg_palette,
                    .cgb_palette    = ppu.scobj.sprites[ppu.scobj.rd].cgb_palette
                };
                newPixels.pixels[newPixels.fill_level++] = pixel;
            }
        }
        
        while (ppu_pixel_fifo_pop(&pixel_fetcher.obj_fifo, &pixel))
        {
            oldPixels.pixels[oldPixels.fill_level++] = pixel;
        }

        for (int i = 0; i < 8; i++)
        {
            if ((i < newPixels.fill_level) && (i < oldPixels.fill_level))
            {
                if (newPixels.pixels[i].oam_tile_index < oldPixels.pixels[i].oam_tile_index)
                {
                    ppu_pixel_fifo_push(&pixel_fetcher.obj_fifo, newPixels.pixels[i]);
                }
                else
                {
                    ppu_pixel_fifo_push(&pixel_fetcher.obj_fifo, oldPixels.pixels[i]);
                }
            }
            else if (i < newPixels.fill_level)
            {
                ppu_pixel_fifo_push(&pixel_fetcher.obj_fifo, newPixels.pixels[i]);
            }
            else if (i < oldPixels.fill_level)
            {
                ppu_pixel_fifo_push(&pixel_fetcher.obj_fifo, oldPixels.pixels[i]);
            }
            else
            {
                break;
            }
        }

        ppu.scobj.rd++;
        pixel_fetcher.obj_state = pfs_suspended_e;
        pixel_fetcher.bg_state = pfs_get_tile_0_e;
    }
    break;

    case pfs_suspended_e:
    {
        /* dont change state here */
    }
    break;

    case pfs_get_tile_1_e:
    case pfs_get_tile_data_lo_1_e:
    default:
    {
        pixel_fetcher.obj_state++;
    }
    break;
    }


    switch (pixel_fetcher.bg_state)
    {
    case pfs_get_tile_0_e:
    {
        uint8_t *p_tile_map, *p_attr_map;
        bool inWindow;
        uint8_t tileX, tileY;

        p_tile_map = &(vram[0].tile_map[0][0]);
        p_attr_map = &(vram[1].tile_map[0][0]);
        inWindow = (ppu.ly >= ppu.wy) && (pixel_fetcher.x >= (ppu.wx - 7));

        if ((!inWindow && (ppu.lcdc & LCDC_BG_TILE_MAP)) ||
            ( inWindow && (ppu.lcdc & LCDC_WINDOW_TILE_MAP)))
        {
            p_tile_map = &(vram[0].tile_map[1][0]);
        }
        
        if (inWindow && (ppu.lcdc & LCDC_WINDOW_EN))
        {
            tileX = ((pixel_fetcher.x - (ppu.wx - 7)) / 8);
            tileY = ((      ppu.ly -  ppu.wy     ) / 8);
            pixel_fetcher.tile_y_offset = 2 * ((ppu.ly -  ppu.wy) & 7);
        }
        else
        {
            tileX = ((ppu.scx / 8) + (pixel_fetcher.x / 8)) & 0x1f;
            tileY = ((ppu.ly + ppu.scy) & 0xFF) / 8;
            pixel_fetcher.tile_y_offset = 2 * ((ppu.scy + ppu.ly) & 7);
        }

        pixel_fetcher.bg_tile_number = p_tile_map[tileY * 32 + tileX];
        pixel_fetcher.bg_tile_attr = p_attr_map[tileY * 32 + tileX];
        pixel_fetcher.tile_hi_lo = 0;
        
        pixel_fetcher.bg_state++;
    }
    break;

    case pfs_get_tile_data_hi_0_e:
    case pfs_get_tile_data_lo_0_e:
    {
        uint8_t vram_idx = pixel_fetcher.bg_tile_attr & 0x80 ? 1 : 0;
        uint8_t *p_window_tile_data = &(vram[vram_idx].tile_data[0]);
        uint8_t data;
        if (pixel_fetcher.bg_tile_number & 0x80)
        {
            data = p_window_tile_data[0x800 + (pixel_fetcher.bg_tile_number & 0x7F) * 16 + pixel_fetcher.tile_y_offset + pixel_fetcher.tile_hi_lo];
        }
        else
        {
            if (ppu.lcdc & LCDC_BG_WINDOW_TILES)
            {
                data = p_window_tile_data[pixel_fetcher.bg_tile_number * 16 + pixel_fetcher.tile_y_offset + pixel_fetcher.tile_hi_lo];
            }
            else
            {
                data = p_window_tile_data[0x1000 + pixel_fetcher.bg_tile_number * 16 + pixel_fetcher.tile_y_offset + pixel_fetcher.tile_hi_lo];
            }
        }

        pixel_fetcher.bg_tile_data[pixel_fetcher.tile_hi_lo] = data;
        pixel_fetcher.tile_hi_lo++;

        pixel_fetcher.bg_state++;
    }
    break;


    case pfs_get_tile_data_hi_1_e:
        pixel_fetcher.bg_state++;
        /* no break */
    case pfs_push_e:
    {
        if (ppu_pixel_fifo_empty(&pixel_fetcher.bg_fifo))
        {
            for (int i = 7; i >= 0; i--)
            {
                pixel_t pixel = (pixel_t)
                {
                    .color_id = ((pixel_fetcher.bg_tile_data[0] & (1<<i)) ? 0x01 : 0x00) |
                                ((pixel_fetcher.bg_tile_data[1] & (1<<i)) ? 0x02 : 0x00) ,
                    .dmg_palette    = 0,
                    .cgb_palette    = pixel_fetcher.bg_tile_attr & 0x07,
                };
                ppu_pixel_fifo_push(&pixel_fetcher.bg_fifo, pixel);
            }
            pixel_fetcher.x += 8;
            pixel_fetcher.bg_state = pfs_get_tile_0_e;
        }
    }
    break;

    case pfs_suspended_e:
    {
        /* dont change state here */
    }
    break;
    
    case pfs_get_tile_1_e:
    case pfs_get_tile_data_lo_1_e:
    default:
    {
        pixel_fetcher.bg_state++;
    }
    break;
    }
}


/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
void gbc_ppu_init(void)
{
    ppu_state.mode = mode2_oam_scan;
}

void gbc_ppu_tick(void)
{
    uint8_t *window_tile_map;
    ppu_mode_t current_ppu_mode = ppu_state.mode;

    if (ppu.lcdc & LCDC_LCD_PPU_EN)
    {
        window_tile_map = (ppu.lcdc & LCDC_WINDOW_TILE_MAP)      ?
                          &(vram[ppu.vbk & 0x01].tile_map[1][0]) :
                          &(vram[ppu.vbk & 0x01].tile_map[0][0]) ;
    }

    switch (ppu_state.mode)
    {
        case mode2_oam_scan:
        {
            if (0 == ppu_state.line_dot_cnt)
            {
                if (ppu.lcdc & LCDC_OBJ_SIZE)
                {
                    printf("todo LCDC_OBJ_SIZE\n");
                    ppu_state.obj_size = 16;
                }
                else
                {
                    ppu_state.obj_size = 8;
                }
            }

            if (0 == (ppu_state.line_dot_cnt & 1))
            {
                int idx = (ppu_state.line_dot_cnt >> 1);
                if (( 10 > ppu.scobj.wr)                                          &&
                    ((ppu.object_attributes[idx].y_pos - 16 +                  0) <= ppu.ly) &&
                    ((ppu.object_attributes[idx].y_pos - 16 + ppu_state.obj_size) >  ppu.ly))
                {
                    ppu.scobj.sprites[ppu.scobj.wr++] = ppu.object_attributes[idx];
                }
            }

            if ((80 - 1) <= ppu_state.line_dot_cnt)
            {
                qsort(ppu.scobj.sprites, ppu.scobj.wr, sizeof(obj_attr_t), compare_sprite_x_offsets);

                pixel_fetcher.bg_fifo.wr_rd_ptr = 0;
                pixel_fetcher.obj_fifo.wr_rd_ptr = 0;
                pixel_fetcher.bg_fifo.fill_level = 0;
                pixel_fetcher.obj_fifo.fill_level = 0;
                pixel_fetcher.bg_state = pfs_get_tile_0_e;
                pixel_fetcher.obj_state = pfs_suspended_e;
                pixel_fetcher.x = 0;
                
                ppu_state.lx = 0;
                ppu_state.pixel_delay = 12;
                ppu_state.pixel_delay += ppu.scx & 0x07;
                ppu_state.x_discard_count = ppu.scx & 0x07;
                ppu_state.mode = mode3_draw;
            }
        }
        break;

        case mode3_draw:
        {
            ppu_pixel_fetcher_do();
            if (0 == --ppu_state.pixel_delay)
            {
                pixel_t pixel;
                bool pixel_valid = false;

                ppu_state.pixel_delay = 1;

                if (pfs_suspended_e == pixel_fetcher.obj_state)
                {
                    if (0 != ppu_state.x_discard_count)
                    {
                        if (ppu_pixel_fifo_pop(&pixel_fetcher.bg_fifo , &pixel))
                        {
                            ppu_pixel_fifo_pop(&pixel_fetcher.obj_fifo, &pixel);
                            ppu_state.x_discard_count--;
                        }
                    }
                    else if (ppu_pixel_fifo_pop(&pixel_fetcher.bg_fifo , &pixel))
                    {
                        uint8_t *cram = &bg_cram[0];
                        pixel_t sprite_pixel;
                        uint8_t palette;

                        palette = ppu.bgp;

                        if (ppu_pixel_fifo_pop(&pixel_fetcher.obj_fifo, &sprite_pixel))
                        {
                            if (!((0 == sprite_pixel.color_id) ||
                                  (sprite_pixel.sprite_prio && (0 != pixel.color_id))))
                            {
                                pixel = sprite_pixel;
                                palette = sprite_pixel.dmg_palette ? ppu.obp1 : ppu.obp0;
                                cram = &obj_cram[0];
                            }
                        }

                        if (bus_DMG_mode())
                        {
                            uint8_t id = (pixel.color_id & 0x03) * 2; // 0, 2, 4, 6
                            uint8_t color_index = (palette & (COLOR_ID_MSK << id)) >> id;
                            screen[ppu.ly * 160 + ppu_state.lx].raw = dmg_palette[color_index];
                        }
                        else
                        {
                            // uint16_t color = ((uint16_t) cram[(pixel.palette << 3) + (color_index << 1) + 0]) << 0 |
                            //                 ((uint16_t) cram[(pixel.palette << 3) + (color_index << 1) + 1]) << 8;
                            uint16_t color = ((uint16_t) cram[(pixel.cgb_palette << 3) + (pixel.color_id << 1) + 0]) << 0 |
                                            ((uint16_t) cram[(pixel.cgb_palette << 3) + (pixel.color_id << 1) + 1]) << 8;
                            
                            screen[ppu.ly * 160 + ppu_state.lx].R = ((color & 0x001f) >>  0) << 3;
                            screen[ppu.ly * 160 + ppu_state.lx].G = ((color & 0x03e0) >>  5) << 3;
                            screen[ppu.ly * 160 + ppu_state.lx].B = ((color & 0x7c00) >> 10) << 3;
                            
#if (0 != DEBUG_SHOW_WINDOW)
                            /* draw green frame around window */
                            if ((ppu.lcdc & LCDC_WINDOW_EN) && (((ppu.wx - 7) == ppu_state.lx) || (((ppu.wx - 7) <= ppu_state.lx) && (ppu.ly == ppu.wy))))
                            {
                                screen[ppu.ly * 160 + ppu_state.lx] = (screen_pixel_t) { .G = 0xff };
                            }
#endif
                        }

                        ppu_state.lx++;
                    }
                }
            }

            if (160 <= ppu_state.lx)
            {
                bus_HBlank_cb();
                ppu_state.mode = mode0_hblank;
            }
        }
        break;

        case mode0_hblank:
        {
            /* transition below */
        }
        break;
        
        case mode1_vblank:
        default:
        {
            /* transition below */
        }
        break;
    }

    if (ppu.ly == ppu.lyc)
    {
        if (0 == (ppu.stat & STAT_LYC_EQ_LY))
        {
            ppu.stat |= STAT_LYC_EQ_LY;
            if (ppu.stat & STAT_LYC_INT_SEL)
            {
                BUS_SET_IRQ(IRQ_LCD);
            }
        }
    }
    else
    {
        ppu.stat &= ~STAT_LYC_EQ_LY;
    }

    ppu.stat &= ~STAT_PPU_MODE;
    ppu.stat |= ppu_state.mode;

    if ((0 == ppu_state.line_dot_cnt) && (144 == ppu.ly))
    {
        screen = ((&screen0[0][0] == screen) ? &screen1[0][0] : &screen0[0][0]);
        BUS_SET_IRQ(IRQ_VBLANK);
    }

    /* check for end of scanline */
    if (456 <= ++ppu_state.line_dot_cnt)
    {
        ppu_state.line_dot_cnt = 0;
        ppu.ly++;
        if ((ppu.lcdc & LCDC_WINDOW_EN)    &&
            (0 < ppu.wx) && (166 > ppu.wx) &&
            (0 < ppu.wy) && (143 > ppu.wy))
        {
            pixel_fetcher.wy++;
        }
        if (144 > ppu.ly)
        {
            ppu.scobj.wr = 0;
            ppu.scobj.rd = 0;
            ppu_state.mode = mode2_oam_scan;
        }
        else if (154 > ppu.ly)
        {
            pixel_fetcher.wy = 0;
            ppu_state.mode = mode1_vblank;
        }
        else
        {
            ppu.ly = 0;
            ppu.scobj.wr = 0;
            ppu.scobj.rd = 0;
            ppu_state.mode = mode2_oam_scan;
        }
    }

    if (ppu_state.mode != current_ppu_mode)
    {
        /* eval STAT IRQ state */
        if (((ppu.stat & STAT_MODE0_INT_SEL) && (mode0_hblank   == ppu_state.mode)) ||
            ((ppu.stat & STAT_MODE1_INT_SEL) && (mode1_vblank   == ppu_state.mode)) ||
            ((ppu.stat & STAT_MODE2_INT_SEL) && (mode2_oam_scan == ppu_state.mode)))
        {
            BUS_SET_IRQ(IRQ_LCD);
        }
    }

    return;
}

uint8_t gbc_ppu_get_memory(uint16_t addr)
{
    uint8_t ret;

    ret = 0;

    switch (addr)
    {
        case 0x8000 ... 0x9FFF:   // Video RAM
        {
            ret = vram[ppu.vbk & 0x01].raw[addr & 0x1FFF];
        }
        break;

        case LCDC:
        {
            ret = ppu.lcdc;
        }
        break;

        case STAT:
        {
            ret = ppu.stat;
        }
        break;

        case SCY:
        {
            ret = ppu.scy;
        }
        break;

        case SCX:
        {
            ret = ppu.scx;
        }
        break;

        case LY:
        {
            ret = ppu.ly;
        }
        break;

        case LYC:
        {
            ret = ppu.lyc;
        }

        case BGP:
        {
            ret = ppu.bgp;
        }
        break;

        case OBP0:
        {
            ret = ppu.obp0;
        }
        break;

        case OBP1:
        {
            ret = ppu.obp1;
        }
        break;

        case WY:
        {
            ret = ppu.wy;
        }
        break;

        case WX:
        {
            ret = ppu.wx;
        }
        break;

        case VBK:
        {
            ret = ppu.vbk | 0xFE;
        }
        break;

        case BCPS_BGPI:
        {
            ret = ppu.bcps_bgpi;
        }
        break;

        case BCPD_BGPD:
        {
            if (mode3_draw != ppu_state.mode)
            {
                ret = bg_cram[ppu.bcps_bgpi & PALETTE_ADDR_MSK];
            }
        }
        break;

        case OCPS_OBPI:
        {
            ret = ppu.ocps_obpi;
        }
        break;

        case OCPD_OBPD:
        {
            if (mode3_draw != ppu_state.mode)
            {
                ret = obj_cram[ppu.ocps_obpi & PALETTE_ADDR_MSK];
            }
        }
        break;

        case OPRI:
        {
            ret = ppu.opri;
        }
        break;

        case 0xFE00 ... 0xFE9F:   // Sprite Attribute Table
        {
            ret = ppu.oam_raw[addr & 0x00FF];
        }
        break;
        
        default:
            {
                DBG_ERROR();
            }
        break;
    }

    return ret;
}

void gbc_ppu_set_memory(uint16_t addr, uint8_t val)
{
    switch (addr)
    {
        case 0x8000 ... 0x9FFF:   // Video RAM
        {
            vram[ppu.vbk & 0x01].raw[addr & 0x1FFF] = val;
        }
        break;

        case LCDC:
        {
            // if (!(val & LCDC_BG_WNDOW_EN_PRIO)) printf("window prio\n");
            // if (!(val & LCDC_OBJ_EN)) printf("obj off\n");
            
            // printf("lcdc=%02x\n", val);

            ppu.lcdc = val;
        }
        break;

        case STAT:
        {
            ppu.stat &= (STAT_PPU_MODE | STAT_LYC_EQ_LY);
            ppu.stat |= val & ~(STAT_PPU_MODE | STAT_LYC_EQ_LY);
        }
        break;

        case SCY:
        {
            ppu.scy = val;
        }
        break;

        case SCX:
        {
            ppu.scx = val;
        }
        break;

        case LY:
        {
            ppu.ly = val;
        }
        break;

        case LYC:
        {
            ppu.lyc = val;
        }
        break;

        case BGP:
        {
            ppu.bgp = val;
        }
        break;

        case OBP0:
        {
            ppu.obp0 = val;
        }
        break;

        case OBP1:
        {
            ppu.obp1 = val;
        }
        break;

        case WY:
        {
            ppu.wy = val;
        }
        break;

        case WX:
        {
            ppu.wx = val;
        }
        break;

        case VBK:
        {
            ppu.vbk = val & 0x01;
        }
        break;

        case BCPS_BGPI:
        {
            ppu.bcps_bgpi = val;
        }
        break;

        case BCPD_BGPD:
        {
            if (mode3_draw != ppu_state.mode)
            {
                bg_cram[ppu.bcps_bgpi & PALETTE_ADDR_MSK] = val;
            }
            if (ppu.bcps_bgpi & PALETTE_AUTO_INC_MSK)
            {
                ppu.bcps_bgpi = (ppu.bcps_bgpi + 1) & PALETTE_SPEC_MSK;
            }
        }
        break;

        case OCPS_OBPI:
        {
            ppu.ocps_obpi = val;
        }
        break;

        case OCPD_OBPD:
        {
            if (mode3_draw != ppu_state.mode)
            {
                obj_cram[ppu.ocps_obpi & PALETTE_ADDR_MSK] = val;
            }
            if (ppu.ocps_obpi & PALETTE_AUTO_INC_MSK)
            {
                ppu.ocps_obpi = (ppu.ocps_obpi + 1) & PALETTE_SPEC_MSK;
            }
        }
        break;

        case OPRI:
        {
            printf("todo opri\n");
            ppu.opri = val;
        }
        break;

        case 0xFE00 ... 0xFE9F:   // Sprite Attribute Table
        {
            ppu.oam_raw[addr & 0x00FF] = val;
        }
        break;
        
        default:
        {
            DBG_ERROR();
        }
        break;
    }

    return;
}

void emulator_get_video_data(uint32_t *data)
{
    if (screen == &screen0[0][0])
    {
        memcpy(data, screen1, sizeof(screen1));
    }
    else
    {
        memcpy(data, screen0, sizeof(screen0));
    }
}

void emulator_debug_get_ppu_data(uint8_t *p_bg_cram, uint8_t *p_obj_cram, uint8_t *p_vram_0, uint8_t *p_vram_1)
{
    memcpy(p_bg_cram, bg_cram, sizeof(bg_cram));
    memcpy(p_obj_cram, obj_cram, sizeof(obj_cram));
    memcpy(p_vram_0, &vram[0], sizeof(vram[0]));
    memcpy(p_vram_1, &vram[1], sizeof(vram[1]));
}

void gbc_ppu_write_internal_state(void)
{
    emulator_cb_write_to_save_file((uint8_t*) &ppu          , sizeof(ppu          ), "ppu"          );
    emulator_cb_write_to_save_file((uint8_t*) &ppu_state    , sizeof(ppu_state    ), "ppu_state"    );
    emulator_cb_write_to_save_file((uint8_t*) &pixel_fetcher, sizeof(pixel_fetcher), "pixel_fetcher");
    emulator_cb_write_to_save_file((uint8_t*) &vram         , sizeof(vram         ), "vram"         );
    emulator_cb_write_to_save_file((uint8_t*) &bg_cram      , sizeof(bg_cram      ), "bg_cram"      );
    emulator_cb_write_to_save_file((uint8_t*) &obj_cram     , sizeof(obj_cram     ), "obj_cram"     );
    return;
}

int gbc_ppu_set_internal_state(void)
{
    int ret = 0;

    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &ppu, sizeof(ppu));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &ppu_state, sizeof(ppu_state));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &pixel_fetcher, sizeof(pixel_fetcher));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &vram, sizeof(vram));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &bg_cram, sizeof(bg_cram));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &obj_cram, sizeof(obj_cram));
    }

    return ret;
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
