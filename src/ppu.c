
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
#include "ppu.h"
#include "bus.h"
#include "debug.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define LCDC        0xFF40   // LCD control
#define STAT        0xFF41   // LCD status
#define SCY         0xFF42   // Viewport Y position (0 ... 255)
#define SCX         0xFF43   // Viewport X position (0 ... 255)
#define LY          0xFF44   // Current LCD Y coordinate (0 ... 153), (144 ... 153)=VBLANK
#define LYC         0xFF45   // LY compare: Sets "LYC==LY"-Flag in STAT-Register (-> STAT-IRQ (if enabled))
#define DMA         0xFF46   // OAM DMA source address & start (Copies 0xXX00-XX9F (with XX=0x00-0xDF) to 0xFE00-0xFE9f (Sprite Attribute Table)) in 160 M-Cycles
#define BGP         0xFF47   // BG palette data
#define OBP0        0xFF48   // OBJ palette 0 data
#define OBP1        0xFF49   // OBJ palette 1 data
#define WY          0xFF4A   // Window Y position: On-Screen-Pos of Window (0 ... 143) (7: Top)
#define WX          0xFF4B   // Window X position plus 7: On-Screen-Pos of Window (0 ... 166) (0: Left)
#define KEY1        0xFF4D   // Prepare speed switch
#define VBK         0xFF4F   // VRAM bank: Bit 0 Selects bank, Bits 1-7 read 0xFE
#define HDMA1       0xFF51   // VRAM DMA source high
#define HDMA2       0xFF52   // VRAM DMA source low (4 Lower Bits ignored (treated as 0))
#define HDMA3       0xFF53   // VRAM DMA destination high (3 higher Bits ignored (treated as 0b100)) |-> 9 middle bits used
#define HDMA4       0xFF54   // VRAM DMA destination low (4 Lower Bits ignored (treated as 0))       |-> dst=(0x8000-0x9FF0)
#define HDMA5       0xFF55   // VRAM DMA length/mode/start; CPU is halted during any transfer
#define BCPS_BGPI   0xFF68   // Background color palette specification / Background palette index
#define BCPD_BGPD   0xFF69   // Background color palette data / Background palette data
#define OCPS_OBPI   0xFF6A   // OBJ color palette specification / OBJ palette index
#define OCPD_OBPD   0xFF6B   // OBJ color palette data / OBJ palette data
#define OPRI        0xFF6C   // Object priority mode

#define LCDC_BG_WNDOW_EN_PRIO   (1<<0)
#define LCDC_OBJ_EN             (1<<1)
#define LCDC_OBJ_SIZE           (1<<2)
#define LCDC_BG_TILE_MAP        (1<<3)
#define LCDC_BG_WINDOW_TILES    (1<<4)
#define LCDC_WINDOW_EN          (1<<5)
#define LCDC_WINDOW_TILE_MAP    (1<<6)
#define LCDC_LCD_PPU_EN         (1<<7)

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

#define HDMA5_DMA_MODE_SEL      (1<<7) // 0: General Purpose DMA, 1: Copy 0x10 Bytes during each HBlank (CPU runs rest of time)
#define HDMA5_DMA_LENGTH        (0x7F) // Length/16 - 1: Length of Transfers = 0x10 ... 0x800 (L=(HDMA5_DMA_LENGTH + 1)*16)

#define CPS_PI_ADDRESS          (0x3F)
#define CPS_PI_AUTO_INC         (1<<7)

#define CPD_PD_RED_MSK          (0x1F<<0)    // implement 64B color palette ram as uint16_t
#define CPD_PD_RED_POS          0            // implement 64B color palette ram as uint16_t
#define CPD_PD_GRN_MSK          (0x1F<<5)    // implement 64B color palette ram as uint16_t
#define CPD_PD_GRN_POS          5            // implement 64B color palette ram as uint16_t
#define CPD_PD_BLU_MSK          (0x1F<<10)   // implement 64B color palette ram as uint16_t
#define CPD_PD_BLU_POS          10           // implement 64B color palette ram as uint16_t

#define OPRI_PRIO_MODE_DMG      (1<<0) // 0: Gameboy Color, 1 Original Gameboy

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
/* internal: one-time init */
void gbc_ppu_init(void)
{

}

/* internal: call this with every clock tick */
void gbc_ppu_tick(void)
{

}

/* internal: only call this for address 0xFF10 - 0xFF3F */
uint8_t gbc_ppu_get_memory(uint16_t addr)
{
    return 0;
}

/* internal: only call this for address 0xFF10 - 0xFF3F */
void gbc_ppu_set_memory(uint16_t addr, uint8_t val)
{

}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
