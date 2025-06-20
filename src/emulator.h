
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
#define GBC_JOYPAD_A      (1<<0)
#define GBC_JOYPAD_B      (1<<1)
#define GBC_JOYPAD_SELECT (1<<2)
#define GBC_JOYPAD_START  (1<<3)
#define GBC_JOYPAD_RIGHT  (1<<4)
#define GBC_JOYPAD_LEFT   (1<<5)
#define GBC_JOYPAD_UP     (1<<6)
#define GBC_JOYPAD_DOWN   (1<<7)

/*---------------------------------------------------------------------*
 *  type declarations                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  function prototypes                                                *
 *---------------------------------------------------------------------*/
int emulator_load_game(char *fileName);
void emulator_run(void);
void emulator_wait_for_data_collection(void);
void emulator_get_audio_data(uint8_t *ch_r, uint8_t *ch_l, size_t *num_samples);
void emulator_get_video_data(uint32_t *data);
uint8_t emulator_get_speed(void);

void emulator_write_save_file(void);
int emulator_load_save_file(void);

/* 
 */
void emulator_cb_write_to_save_file(uint8_t *data, size_t size);
int emulator_cb_read_from_save_file(uint8_t *data, size_t size);

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
