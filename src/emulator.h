
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         GBC Emulator                                *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: emulator.h                                           *
 *        author: tstr92                                               *
 *          date: 2025-05-10                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

#ifndef _EMULATOR_H_
#define _EMULATOR_H_

/*---------------------------------------------------------------------*
 *  additional includes                                                *
 *---------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------*
 *  global definitions                                                 *
 *---------------------------------------------------------------------*/
#define NUM_AUDIO_SAMPLES_PER_FRAME 550

#define GBC_JOYPAD_A      (1<<0)
#define GBC_JOYPAD_B      (1<<1)
#define GBC_JOYPAD_SELECT (1<<2)
#define GBC_JOYPAD_START  (1<<3)
#define GBC_JOYPAD_RIGHT  (1<<4)
#define GBC_JOYPAD_LEFT   (1<<5)
#define GBC_JOYPAD_UP     (1<<6)
#define GBC_JOYPAD_DOWN   (1<<7)

#define CGB_SCREEN_WIDTH  (160)
#define CGB_SCREEN_HEIGTH (144)

#define PIXEL_FORMAT_RGBA 0
#define PIXEL_FORMAT_ARGB 1
#define PIXEL_FORMAT_ABGR 2

typedef struct
{
	uint32_t rtc_ticker;
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t days_low;
	uint8_t days_hi_ctrl;
	uint8_t latch[5];
} rtc_t;

typedef struct
{
	uint8_t sram[16][8*1024];
} sram_t;


#if (PIXEL_FORMAT == PIXEL_FORMAT_RGBA)
#define PX_COL_OFFS_R (24)
#define PX_COL_OFFS_G (16)
#define PX_COL_OFFS_B ( 8)
#define PX_COL_OFFS_A ( 0)
#elif (PIXEL_FORMAT == PIXEL_FORMAT_ARGB)
#define PX_COL_OFFS_A (24)
#define PX_COL_OFFS_R (16)
#define PX_COL_OFFS_G ( 8)
#define PX_COL_OFFS_B ( 0)
#elif (PIXEL_FORMAT == PIXEL_FORMAT_ABGR)
#define PX_COL_OFFS_A (24)
#define PX_COL_OFFS_B (16)
#define PX_COL_OFFS_G ( 8)
#define PX_COL_OFFS_R ( 0)
#else
#error invalid PIXEL_FORMAT
#endif

#ifndef SET_ALPHA
#define SET_ALPHA 0
#endif

#define countof(_vec) (sizeof(_vec) / sizeof(_vec[0]))
/*---------------------------------------------------------------------*
 *  type declarations                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  function prototypes                                                *
 *---------------------------------------------------------------------*/
int emulator_load_game(uint8_t *rom, size_t rom_size, uint8_t *sram, size_t sram_size, rtc_t *p_rtc);
void emulator_run(void);
void emulator_stop(void);
void emulator_get_audio_data(uint8_t *ch_r, uint8_t *ch_l, size_t *num_samples);
void emulator_get_video_data(uint32_t *data); /* returns "uint32_t screen[144][160]" */
void emulator_debug_get_ppu_data(uint8_t *p_bg_cram, uint8_t *p_obj_cram, uint8_t *p_vram_0, uint8_t *p_vram_1);
void emulator_debug_pixel_draw_event(void);
uint8_t emulator_get_speed(void);

void emulator_cb_audio_ready(void);

void emulator_write_save_file(void);
int emulator_load_save_file(void);

/* 
 */
void emulator_cb_write_to_save_file(const uint8_t *data, size_t size, char *name);
int emulator_cb_read_from_save_file(uint8_t *data, size_t size);

void emulator_tick_cb(void);

void emulator_cb_push_video(uint32_t screen[144][160]);

/* Callback-Function that reads the current Button-states.
 * Use Defines GBC_JOYPAD_* to set joypad data.
 * This function should return immediately with a buffered
 * value. Do not sample the inputs in this function.
 */
uint8_t gbc_joypad_buttons_cb(void);

/*---------------------------------------------------------------------*
 *  callback functions                                                 *
 *---------------------------------------------------------------------*/
uint32_t platform_getSysTick_ms(void);

/*---------------------------------------------------------------------*
 *  global data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  inline functions and function-like macros                          *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/

 #endif /* #ifndef _EMULATOR_H_ */
