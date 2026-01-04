
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
#include "emulator.h"
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

typedef struct
{
    uint8_t DIVA;
    uint8_t DIVA_PRESC_CNT;
    uint8_t TIMA;
    uint32_t TIMA_PRESC_CNT;
    uint8_t TMA;
    uint8_t TAC;
} timer_t;


/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
timer_t timer = 
{
    .TIMA           = 0x00,
    .TMA            = 0x00,
    .TAC            = 0xF8,
};

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
    const uint32_t TIMA_PRESCALER = TAC_CLKSEL_LUT[(timer.TAC & TAC_CLKSEL_MSK)];

    if (0 == ++timer.DIVA_PRESC_CNT)
    {
        timer.DIVA++;
    }

    if (timer.TAC & TAC_EN_MSK)
    {
        if (TIMA_PRESCALER <= ++timer.TIMA_PRESC_CNT)
        {
            timer.TIMA_PRESC_CNT = 0;
            timer.TIMA++;
            if (0 == timer.TIMA)
            {
                timer.TIMA = timer.TMA;
                BUS_SET_IRQ(IRQ_TIMER);
            }
        }
    }

    return;
}

void gbc_timer_diva_reset(void)
{
    timer.DIVA = 0;
}

uint8_t gbc_timer_get_memory(uint16_t addr)
{
    uint8_t ret;

    ret = 0;

    switch (addr)
    {
        case TIMER_ADDR_DIV:
        {
            ret = timer.DIVA;
        }
        break;

        case TIMER_ADDR_TIMA:
        {
            ret = timer.TIMA;
        }
        break;

        case TIMER_ADDR_TMA:
        {
            ret = timer.TMA;
        }
        break;

        case TIMER_ADDR_TAC:
        {
            ret = timer.TAC;
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
            timer.DIVA = 0;
        }
        break;

        case TIMER_ADDR_TIMA:
        {
            timer.TIMA = val;
        }
        break;

        case TIMER_ADDR_TMA:
        {
            timer.TMA = val;
        }
        break;

        case TIMER_ADDR_TAC:
        {
            timer.TAC = val;
        }
        break;

        default:
        DBG_ERROR();
        break;
    }
}

void gbc_tim_write_internal_state(void)
{
	emulator_cb_write_to_save_file((uint8_t*) &timer, sizeof(timer_t), "tim");
	return;
}

int gbc_tim_set_internal_state(void)
{
	return emulator_cb_read_from_save_file((uint8_t*) &timer, sizeof(timer_t));
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
