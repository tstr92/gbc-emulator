#include <stdint.h>
#include "xprintf.h"

#define UART_TDR       *((uint8_t *) 0xE000)
#define TIMER_DIV      *((uint8_t *) 0xFF04)
#define TIMER_TIMA     *((uint8_t *) 0xFF05)
#define TIMER_TMA      *((uint8_t *) 0xFF06)
#define TIMER_TAC      *((uint8_t *) 0xFF07)

uint32_t timer;

void putc(int c)
{
    UART_TDR = (unsigned char) c;
}

void isr_vblank(void) __interrupt(0) /*__using(1)*/
{
    // xprintf("isr_vblank\n");
}

void isr_lcd(void) __interrupt(1) /*__using(1)*/
{
    // xprintf("isr_lcd\n");
}

void isr_timer(void) __interrupt(2) /*__using(1)*/
{
    timer++;
    // xprintf("isr_timer\n");
}

void isr_serial(void) __interrupt(3) /*__using(1)*/
{
    // xprintf("isr_serial\n");
}

void isr_joypad(void) __interrupt(4) /*__using(1)*/
{
    // xprintf("isr_joypad\n");
}

void main(void)
{
    // uint32_t test_cnt;

    /* enabe all interrupts */
    __asm__("ei");
    *((unsigned char*)0xFFFF) = 0xff;

    xdev_out(putc);

    // xprintf("Hello World!\n\n");

    // *((unsigned char*)0xFFFF) = 0xff;
    // *((unsigned char*)0xFF0F) = (1<<0);
    // *((unsigned char*)0xFF0F) = (1<<1);
    // *((unsigned char*)0xFF0F) = (1<<2);
    // *((unsigned char*)0xFF0F) = (1<<3);
    // *((unsigned char*)0xFF0F) = (1<<4);

    // int num_m_cycles[] = {256, 4, 16, 64};
    // for (int i = 0; i < 4; i++)
    // {
    //     timer = 0;
    //     test_cnt = 0;
    //     xprintf("%d M-Cycles\n", num_m_cycles[i]);
    //     TIMER_TAC = (1<<2) | i; // enable, 256 M-cycles
    //     while (timer < 100)
    //     {
    //         test_cnt++;
    //     }
    //     TIMER_TAC = 0;
    //     xprintf("test_cnt=%lu\n\n", test_cnt);
    // }

    timer = 0;
    TIMER_TAC = (1<<2) | 0; // enable, 256 M-cycles
    while (timer < 1000)
    {
    }

    __asm__("stop");
    return;
}
