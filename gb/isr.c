#include "xprintf.h"

void isr_vblank(void) __interrupt(0) /*__using(1)*/
{
    xprintf("isr_vblank\n");
}

void isr_lcd(void) __interrupt(1) /*__using(1)*/
{
    xprintf("isr_lcd\n");
}

void isr_timer(void) __interrupt(2) /*__using(1)*/
{
    xprintf("isr_timer\n");
}

void isr_serial(void) __interrupt(3) /*__using(1)*/
{
    xprintf("isr_serial\n");
}

void isr_joypad(void) __interrupt(4) /*__using(1)*/
{
    xprintf("isr_joypad\n");
}