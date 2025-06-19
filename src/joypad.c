
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         GBC Joypad Input                            *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: joypad.c                                             *
 *        author: tstr92                                               *
 *          date: 2025-04-21                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include "joypad.h"
#include "debug.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define JOYPAD_ADDRESS      0xFF00
#define KEY_SELECTION_MSK   0x30
#define KEY_SELECTION_BTN   0x10
#define KEY_SELECTION_D_PAD 0x20
#define KEY_SELECTION_ALL   0x00
#define KEY_SELECTION_NONE  0x30
#define BUTTONS_MSK         0x0F
#define JOYPAD_A_RIGHT      (1<<0)
#define JOYPAD_B_LEFT       (1<<1)
#define JOYPAD_SELECT_UP    (1<<2)
#define JOYPAD_START_DOWN   (1<<3)

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static uint8_t JOYP = 0x3F; /* all buttons released */

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
uint8_t gbc_joypad_get_memory(uint16_t addr)
{
    uint8_t ret;
    uint8_t buttons;

    ret = 0;

    if (JOYPAD_ADDRESS == addr)
    {
        buttons = gbc_joypad_buttons_cb();

        JOYP |= BUTTONS_MSK; /* all buttons released */

        switch (JOYP & KEY_SELECTION_MSK)
        {
            case KEY_SELECTION_BTN:
            {
                JOYP &= ~((buttons >> 0) & BUTTONS_MSK);
            }
            break;

            case KEY_SELECTION_D_PAD:
            {
                JOYP &= ~((buttons >> 4) & BUTTONS_MSK);
            }
            break;

            case KEY_SELECTION_ALL:
            {
                JOYP &= (~((buttons >> 0) & BUTTONS_MSK)) & (~((buttons >> 4) & BUTTONS_MSK));
            }
            break;

            case KEY_SELECTION_NONE:
            default:
            /* nothing to do */
            break;
        }

        ret = JOYP;
    }
    else
    {
        DBG_ERROR();
    }

    return ret;
}

void gbc_joypad_set_memory(uint16_t addr, uint8_t val)
{
    if (JOYPAD_ADDRESS == addr)
    {
        JOYP &= ~KEY_SELECTION_MSK;         /* clear selection */
        JOYP |= (val & KEY_SELECTION_MSK);  /* set selection */
    }
    else
    {
        DBG_ERROR();
    }

    return;
}

__attribute__((weak))
uint8_t gbc_joypad_buttons_cb(void)
{
    return 0x00;
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
