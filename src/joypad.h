
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         GBC Joypad Input                            *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: joypad.h                                             *
 *        author: tstr92                                               *
 *          date: 2025-04-21                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

#ifndef _JOYPAD_H_
#define _JOYPAD_H_

/*---------------------------------------------------------------------*
 *  additional includes                                                *
 *---------------------------------------------------------------------*/
#include <stdint.h>

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
/* internal: only call this for address 0xFF00 */
uint8_t gbc_joypad_get_memory(uint16_t addr);

/* internal: only call this for address 0xFF00 */
void gbc_joypad_set_memory(uint16_t addr, uint8_t val);


/* Callback-Function that reads the current Button-states.
 * Use Defines GBC_JOYPAD_* to set joypad data.
 * This function should return immediately with a buffered
 * value. Do not sample the inputs in this function.
 */
uint8_t gbc_joypad_buttons_cb(void);

/*---------------------------------------------------------------------*
 *  global data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  inline functions and function-like macros                          *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/

 #endif /* #ifndef _JOYPAD_H_ */
