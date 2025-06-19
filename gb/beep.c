#include <stdint.h>

// Sound control registers
#define NR10 (*(volatile uint8_t *)0xFF10)
#define NR11 (*(volatile uint8_t *)0xFF11)
#define NR12 (*(volatile uint8_t *)0xFF12)
#define NR13 (*(volatile uint8_t *)0xFF13)
#define NR14 (*(volatile uint8_t *)0xFF14)

#define NR41 (*(volatile uint8_t *)0xFF20)
#define NR42 (*(volatile uint8_t *)0xFF21)
#define NR43 (*(volatile uint8_t *)0xFF22)
#define NR44 (*(volatile uint8_t *)0xFF23)

#define NR50 (*(volatile uint8_t *)0xFF24)
#define NR51 (*(volatile uint8_t *)0xFF25)
#define NR52 (*(volatile uint8_t *)0xFF26)

#define NR30 (*(volatile unsigned char *)0xFF1A) // Sound on/off
#define NR31 (*(volatile unsigned char *)0xFF1B) // Sound length
#define NR32 (*(volatile unsigned char *)0xFF1C) // Output level
#define NR33 (*(volatile unsigned char *)0xFF1D) // Frequency low
#define NR34 (*(volatile unsigned char *)0xFF1E) // Frequency high + trigger

#define WAVE_RAM ((volatile unsigned char *)0xFF30) // Wave pattern RAM

#define PANNING_CH1_RIGHT             (1<<0)
#define PANNING_CH2_RIGHT             (1<<1)
#define PANNING_CH3_RIGHT             (1<<2)
#define PANNING_CH4_RIGHT             (1<<3)
#define PANNING_CH1_LEFT              (1<<4)
#define PANNING_CH2_LEFT              (1<<5)
#define PANNING_CH3_LEFT              (1<<6)
#define PANNING_CH4_LEFT              (1<<7)

#define TIMER_TAC *((uint8_t *) 0xFF07)

uint32_t timer;
uint8_t timer2 = 0;

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
    if (10 < ++timer2)
    {
        timer2 = 0;
        // if (NR51 == (uint8_t)(PANNING_CH1_RIGHT | PANNING_CH4_LEFT))
        // {
        //     NR51 = (uint8_t)(PANNING_CH1_LEFT | PANNING_CH4_RIGHT);
        // }
        // else
        // {
        //     NR51 = (uint8_t)(PANNING_CH1_RIGHT | PANNING_CH4_LEFT);
        // }
    }
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

void play_beep(void)
{
    NR10 = 0x00;  // Sweep off
    NR11 = 0x81;  // Duty cycle 50%, length = 1 (arbitrary)
    NR12 = 0xF0;  // Max volume, no envelope
    NR13 = 0x6B;  // Frequency LSB (sets pitch)
    NR14 = 0x83;  // Frequency MSB + trigger
}

void play_noise(void)
{
    // --- Channel 4: Noise ---
    NR41 = 0x00;  // Sound length (not used unless length enable is set)
    NR42 = 0x30;  // Initial volume: max (F), no envelope
    NR43 = 0x14;  // Clock shift=1, width mode=0 (15 bits), divisor=4
    NR44 = 0x80;  // Trigger + length enable (bit 6)
}

void setup_wave_channel(void)
{
    // Define a simple sawtooth waveform (4-bit samples)
    for (int i = 0; i < 16; i++)
    {
        WAVE_RAM[i] = (i << 4) | i; // Each byte holds two 4-bit samples
    }

    NR30 = 0x80; // DAC on
    NR31 = 0x00; // Length (0 = play full)
    NR32 = 0x20; // Output level: 100% volume
    NR33 = 0xAA; // Freq low
    NR34 = 0x87; // Freq high (trigger + no length stop)
}

void main(void)
{
    /* enabe all interrupts */
    __asm__("ei");
    *((unsigned char*)0xFFFF) = 0xff;

    // Enable sound system
    NR52 = 0x80;  // 1000 0000 - APU on
    NR50 = 0x77;  // Max volume left & right
    NR51 = 0xFF; // Enable all channels on both outputs
    // NR51 = (uint8_t)(PANNING_CH1_RIGHT | PANNING_CH4_LEFT);

    play_beep();
    play_noise();
    setup_wave_channel();
    

    timer = 0;
    TIMER_TAC = (1<<2) | 0; // enable, 256 M-cycles
    while (timer < 300)
    {
    }
    
    NR52 = 0x00;     // Turn off sound (optional)

    __asm__("stop");
    return;
}
