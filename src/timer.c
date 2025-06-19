
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         GBC Timer                                   *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: timer.c                                              *
 *        author: tstr92                                               *
 *          date: 2025-04-21                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include "timer.h"
#include "bus.h"
#include "debug.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define TIMER_ADDR_DIV    0xFF04
#define TIMER_ADDR_TIMA   0xFF05
#define TIMER_ADDR_TMA    0xFF06
#define TIMER_ADDR_TAC    0xFF07

#define TAC_CLKSEL_MSK    0x03
#define TAC_EN_MSK        0x04

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static uint8_t DIVA = 0;
static uint8_t DIVA_PRESC_CNT = 0;

static uint8_t TIMA = 0;
static uint32_t TIMA_PRESC_CNT = 0;
static uint8_t TMA = 0;
static uint8_t TAC = 0;

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
void gbc_timer_tick(void)
{
    static const uint32_t TAC_CLKSEL_LUT[4] = {(256 * 4), (4 * 4), (16 * 4), (64 * 4)};
    const uint32_t TIMA_PRESCALER = TAC_CLKSEL_LUT[(TAC & TAC_CLKSEL_MSK)];

    if (0 == ++DIVA_PRESC_CNT)
    {
        DIVA++;
    }

    if (TAC & TAC_EN_MSK)
    {
        if (TIMA_PRESCALER <= ++TIMA_PRESC_CNT)
        {
            TIMA_PRESC_CNT = 0;
            TIMA++;
            if (0 == TIMA)
            {
                TIMA = TMA;
                BUS_SET_IRQ(IRQ_TIMER);
            }
        }
    }

    return;
}

void gbc_timer_diva_reset(void)
{
    DIVA = 0;
}

uint8_t gbc_timer_get_memory(uint16_t addr)
{
    uint8_t ret;

    ret = 0;

    switch (addr)
    {
        case TIMER_ADDR_DIV:
        {
            ret = DIVA;
        }
        break;

        case TIMER_ADDR_TIMA:
        {
            ret = TIMA;
        }
        break;

        case TIMER_ADDR_TMA:
        {
            ret = TMA;
        }
        break;

        case TIMER_ADDR_TAC:
        {
            ret = TAC;
        }
        break;

        default:
        DBG_ERROR();
        break;
    }

    return ret;
}

void gbc_timer_set_memory(uint16_t addr, uint8_t val)
{
    switch (addr)
    {
        case TIMER_ADDR_DIV:
        {
            DIVA = 0;
        }
        break;

        case TIMER_ADDR_TIMA:
        {
            TIMA = val;
        }
        break;

        case TIMER_ADDR_TMA:
        {
            TMA = val;
        }
        break;

        case TIMER_ADDR_TAC:
        {
            TAC = val;
        }
        break;

        default:
        DBG_ERROR();
        break;
    }
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
