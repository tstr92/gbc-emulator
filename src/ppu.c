
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
    uint8_t pixel_delay;
    uint8_t obj_size;
} ppu_state_t;

typedef struct
{
    uint8_t color;        // a value between 0 and 3
    uint8_t palette;      // on CGB a value between 0 and 7 and on DMG this only applies to objects
    uint8_t sprite_prio;  // on CGB this is the OAM index for the object and on DMG this doesn’t exist
    uint8_t bg_prio;      // holds the value of the OBJ-to-BG Priority bit
} pixel_t;

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
static uint8_t window_tile_map_0x9800[0x400];
static uint8_t window_tile_map_0x9C00[0x400];
static uint8_t window_tile_data_0[0x1800];
static uint8_t window_tile_data_1[0x1800];

static screen_pixel_t screen0[144][160];
static screen_pixel_t screen1[144][160];
static screen_pixel_t *screen = (screen_pixel_t *) &screen0[0][0];

uint32_t debug_palette[4] =
{
    0xFFFFFFFF, 0xAAAAAAAA, 0x55555555, 0x00000000
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
            else
            {
                pixel_fetcher.obj_state = pfs_suspended_e;
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
        uint8_t *p_window_tile_data = window_tile_data_0;
        uint8_t data;
        data = p_window_tile_data[pixel_fetcher.obj_tile_number * 16 + 2 * pixel_fetcher.tile_y_offset + pixel_fetcher.tile_hi_lo];
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
        if (ppu_pixel_fifo_empty(&pixel_fetcher.obj_fifo))
        {
            int numPixels = 8 - ((pixel_fetcher.x + 8) - ppu.scobj.sprites[ppu.scobj.rd].x_pos);
            if (ppu.scobj.sprites[ppu.scobj.rd].x_flip)
            {
                for (int i = 0; i <= (numPixels - 1); i++)
                {
                    pixel_t pixel = (pixel_t)
                    {
                        .color       = ((pixel_fetcher.obj_tile_data[0] & (1<<i)) ? 0x01 : 0x00) |
                                       ((pixel_fetcher.obj_tile_data[1] & (1<<i)) ? 0x02 : 0x00) ,
                        .sprite_prio = ppu.scobj.sprites[ppu.scobj.rd].priority,
                    };
                    ppu_pixel_fifo_push(&pixel_fetcher.obj_fifo, pixel);
                }
            }
            else
            {
                for (int i = (numPixels - 1); i >= 0; i--)
                {
                    pixel_t pixel = (pixel_t)
                    {
                        .color       = ((pixel_fetcher.obj_tile_data[0] & (1<<i)) ? 0x01 : 0x00) |
                                       ((pixel_fetcher.obj_tile_data[1] & (1<<i)) ? 0x02 : 0x00) ,
                        .sprite_prio = ppu.scobj.sprites[ppu.scobj.rd].priority,
                    };
                    ppu_pixel_fifo_push(&pixel_fetcher.obj_fifo, pixel);
                }
            }
            if (ppu.scobj.sprites[ppu.scobj.rd].y_flip) printf("yf\n");
            ppu.scobj.rd++;
            pixel_fetcher.obj_state = pfs_suspended_e;
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
        pixel_fetcher.obj_state++;
    }
    break;
    }


    switch (pixel_fetcher.bg_state)
    {
    case pfs_get_tile_0_e:
    {
        uint8_t *p_tile_map;
        bool inWindow;
        uint8_t tileX, tileY;

        p_tile_map = window_tile_map_0x9800;
        inWindow = (ppu.ly >= ppu.wy) && (pixel_fetcher.x >= (ppu.wx - 7));

        if ((!inWindow && (ppu.lcdc & LCDC_BG_TILE_MAP)) ||
            ( inWindow && (ppu.lcdc & LCDC_WINDOW_TILE_MAP)))
        {
            p_tile_map = window_tile_map_0x9C00;
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
        pixel_fetcher.tile_hi_lo = 0;
        
        pixel_fetcher.bg_state++;
    }
    break;

    case pfs_get_tile_data_hi_0_e:
    case pfs_get_tile_data_lo_0_e:
    {
        uint8_t *p_window_tile_data = window_tile_data_0;
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
                    .color       = ((pixel_fetcher.bg_tile_data[0] & (1<<i)) ? 0x01 : 0x00) |
                                   ((pixel_fetcher.bg_tile_data[1] & (1<<i)) ? 0x02 : 0x00) ,
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

    if (ppu.lcdc & LCDC_LCD_PPU_EN)
    {
        window_tile_map = (ppu.lcdc & LCDC_WINDOW_TILE_MAP) ? window_tile_map_0x9C00 : window_tile_map_0x9800;
    }

    switch (ppu_state.mode)
    {
        case mode2_oam_scan:
        {
            if (0 == ppu_state.line_dot_cnt)
            {
                if (ppu.lcdc & LCDC_OBJ_SIZE)
                {
                    printf("16\n");
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
                    if (ppu_pixel_fifo_pop(&pixel_fetcher.bg_fifo , &pixel))
                    {
                        pixel_t sprite_pixel;
                        if (ppu_pixel_fifo_pop(&pixel_fetcher.obj_fifo, &sprite_pixel))
                        {
                            if (!((0 == sprite_pixel.color) ||
                                  (sprite_pixel.bg_prio && (0 != pixel.color))))
                            {
                                pixel = sprite_pixel;
                            }
                        }
                        screen[ppu.ly * 160 + ppu_state.lx].raw = debug_palette[pixel.color & 0x03];
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
        ppu.stat |= STAT_LYC_EQ_LY;
    }
    else
    {
        ppu.stat &= ~STAT_LYC_EQ_LY;
    }

    ppu.stat &= ~STAT_PPU_MODE;
    ppu.stat |= ppu_state.mode;

    /* eval STAT IRQ state */
    if (((ppu.stat & STAT_MODE0_INT_SEL) && (mode0_hblank   == ppu_state.mode)) ||
        ((ppu.stat & STAT_MODE1_INT_SEL) && (mode1_vblank   == ppu_state.mode)) ||
        ((ppu.stat & STAT_MODE2_INT_SEL) && (mode2_oam_scan == ppu_state.mode)) ||
        ((ppu.stat & STAT_LYC_INT_SEL  ) && (ppu.stat & STAT_LYC_EQ_LY)))
    {
        BUS_SET_IRQ(IRQ_LCD);
    }

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

    return;
}

uint8_t gbc_ppu_get_memory(uint16_t addr)
{
    uint8_t ret;

    ret = 0;

    switch (addr)
    {
        case 0x8000 ... 0x97FF:   // Video RAM: Tile Data
        {
            ret = window_tile_data_0[addr - 0x8000];
        }
        break;

        case 0x9800 ... 0x9BFF:   // Video RAM: Tile Map 0
        {
            ret = window_tile_map_0x9800[addr - 0x9800];
        }
        break;

        case 0x9C00 ... 0x9FFF:   // Video RAM: Tile Map 1
        {
            ret = window_tile_map_0x9C00[addr - 0x9C00];
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
            ret = ppu.vbk;
        }
        break;

        case BCPS_BGPI:
        {
            ret = ppu.bcps_bgpi;
        }
        break;

        case BCPD_BGPD:
        {
            ret = ppu.bcpd_bgpd;
        }
        break;

        case OCPS_OBPI:
        {
            ret = ppu.ocps_obpi;
        }
        break;

        case OCPD_OBPD:
        {
            ret = ppu.ocpd_obpd;
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
        case 0x8000 ... 0x97FF:   // Video RAM: Tile Data
        {
            window_tile_data_0[addr - 0x8000] = val;
        }
        break;

        case 0x9800 ... 0x9BFF:   // Video RAM: Tile Map 0
        {
            window_tile_map_0x9800[addr - 0x9800] = val;
        }
        break;

        case 0x9C00 ... 0x9FFF:   // Video RAM: Tile Map 1
        {
            window_tile_map_0x9C00[addr - 0x9C00] = val;
        }
        break;

        case LCDC:
        {
            printf("lcdc=%02x\n", val);
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
            ppu.vbk = val;
        }
        break;

        case BCPS_BGPI:
        {
            ppu.bcps_bgpi = val;
        }
        break;

        case BCPD_BGPD:
        {
            ppu.bcpd_bgpd = val;
        }
        break;

        case OCPS_OBPI:
        {
            ppu.ocps_obpi = val;
        }
        break;

        case OCPD_OBPD:
        {
            ppu.ocpd_obpd = val;
        }
        break;

        case OPRI:
        {
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

void gbc_ppu_write_internal_state(void)
{
    emulator_cb_write_to_save_file((uint8_t*) &ppu                   , sizeof(ppu                   ));
    emulator_cb_write_to_save_file((uint8_t*) &ppu_state             , sizeof(ppu_state             ));
    emulator_cb_write_to_save_file((uint8_t*) &pixel_fetcher         , sizeof(pixel_fetcher         ));
    emulator_cb_write_to_save_file((uint8_t*) &window_tile_map_0x9800, sizeof(window_tile_map_0x9800));
    emulator_cb_write_to_save_file((uint8_t*) &window_tile_map_0x9C00, sizeof(window_tile_map_0x9C00));
    emulator_cb_write_to_save_file((uint8_t*) &window_tile_data_0    , sizeof(window_tile_data_0    ));
    emulator_cb_write_to_save_file((uint8_t*) &window_tile_data_1    , sizeof(window_tile_data_1    ));
    return;
}

int gbc_ppu_set_internal_state(void)
{
    int ret = 0;

    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &ppu                   , sizeof(ppu                   ));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &ppu_state             , sizeof(ppu_state             ));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &pixel_fetcher         , sizeof(pixel_fetcher         ));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &window_tile_map_0x9800, sizeof(window_tile_map_0x9800));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &window_tile_map_0x9C00, sizeof(window_tile_map_0x9C00));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &window_tile_data_0    , sizeof(window_tile_data_0    ));
    }
    if (0 == ret)
    {
        emulator_cb_read_from_save_file((uint8_t*) &window_tile_data_1    , sizeof(window_tile_data_1    ));
    }

    return ret;
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
