
/*---------------------------------------------------------------------*
 *                                                                     *
 *                          GBC APU                                    *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: apu.c                                                *
 *        author: tstr92                                               *
 *          date: 2025-04-27                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include <stdbool.h>

#include "apu.h"
#include "bus.h"
#include "timer.h"
#include "debug.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define APU_ADDR_CH1_SWEEP            0xFF10
#define APU_ADDR_CH1_LENGTH_TIM_DC    0xFF11
#define APU_ADDR_CH1_VOL_ENVELOPE     0xFF12
#define APU_ADDR_CH1_PERIOD_LOW       0xFF13
#define APU_ADDR_CH1_PERIOD_HIGH_CTRL 0xFF14
#define APU_ADDR_CH2_LENGTH_TIM_DC    0xFF15
#define APU_ADDR_CH2_VOL_ENVELOPE     0xFF17
#define APU_ADDR_CH2_PERIOD_LOW       0xFF18
#define APU_ADDR_CH2_PERIOD_HIGH_CTRL 0xFF19
#define APU_ADDR_CH3_DAC_EN           0xFF1A
#define APU_ADDR_CH3_LENGTH_TIM       0xFF1B
#define APU_ADDR_CH3_OUT_LVL          0xFF1C
#define APU_ADDR_CH3_PERIOD_LOW       0xFF1D
#define APU_ADDR_CH3_PERIOD_HIGH_CTRL 0xFF1E
#define APU_ADDR_CH4_LENGTH_TIM       0xFF20
#define APU_ADDR_CH4_VOL_ENVELOPE     0xFF21
#define APU_ADDR_CH4_FREQ_RAND        0xFF22
#define APU_ADDR_CH4_CTRL             0xFF23
#define APU_ADDR_MASTER_VOL_VIN_PAN   0xFF24
#define APU_ADDR_SOUND_PANNING        0xFF25
#define APU_ADDR_AUDIO_MASTER_CONTROL 0xFF26

#define AUDIO_MASTER_CONTROL_AUDIO_ON (1<<7)
#define AUDIO_MASTER_CONTROL_CH1_ON   (1<<0)
#define AUDIO_MASTER_CONTROL_CH2_ON   (1<<0)
#define AUDIO_MASTER_CONTROL_CH3_ON   (1<<0)
#define AUDIO_MASTER_CONTROL_CH4_ON   (1<<0)

#define PANNING_CH1_RIGHT             (1<<0)
#define PANNING_CH2_RIGHT             (1<<1)
#define PANNING_CH3_RIGHT             (1<<2)
#define PANNING_CH4_RIGHT             (1<<3)
#define PANNING_CH1_LEFT              (1<<4)
#define PANNING_CH2_LEFT              (1<<5)
#define PANNING_CH3_LEFT              (1<<6)
#define PANNING_CH4_LEFT              (1<<7)

#define CH12_DC_MSK                    0xC0
#define CH12_DC_POS                    6
#define CH_LENGTH_TIMER_MSK            0x3F
#define CH12_DC_12_5_PERCENT           0x0
#define CH12_DC_25_PERCENT             0x1
#define CH12_DC_50_PERCENT             0x2
#define CH12_DC_75_PERCENT             0x3
#define CH_LENGTH_EN                   0x40
#define CH_TRIGGER                     0x80
#define CH123_PERIOD_HIGH_MSK          0x07
#define CH12_SWEEP_PACE_MSK            0x70
#define CH12_SWEEP_PACE_POS            4
#define CH12_SWEEP_DIR_SUBTRACT_MSK    0x08
#define CH12_SWEEP_STEP_MSK            0x07
#define CH124_INITIAL_VOLUME_MSK       0xF0
#define CH124_INITIAL_VOLUME_POS       4
#define CH124_ENV_DIR_INC_MSK          0x08
#define CH124_ENV_SWEEP_PACE_MSK       0x07
#define CH3_DAC_EN_MSK                 0x80
#define CH3_OUTPUT_LEVEL_MSK           0x60
#define CH3_OUTPUT_LEVEL_POS           5
#define CH3_OUTPUT_LEVEL_SHIFT_MSK     0x03
#define CH4_LFSR_CLOCK_SHIFT_MSK       0xF0
#define CH4_LFSR_CLOCK_SHIFT_POS       4
#define CH4_LFSR_WIDTH_7               0x08
#define CH4_LFSR_CLK_DIV_MSK           0x07


#define TIMER_ADDR_DIV   0xFF04
#define TIMER_GET_DIV() gbc_timer_get_memory(TIMER_ADDR_DIV);

#define DIV_APU_BIT (1<<5)

#define CH12_PERIOD_PRESCALER          4  /*  4 MHz /  4 =   1.000 MHz */
#define CH3_PERIOD_PRESCALER           2  /*  4 MHz /  2 =   2.000 MHz */
#define CH4_LFSR_PRESCALER             16 /*  4 MHz / 16 = 262.144 kHz */
#define CH12_PERIOD_SWEEP_PRESCALER    4  /* 512 Hz /  4 = 128.000  Hz */
#define CH_LENGTH_TIMER_PRESCALER      2  /* 512 Hz /  2 = 256.000  Hz */
#define CH124_ENVELOPE_SWEEP_PRESCALER 8  /* 512 Hz /  8 =  64.000  Hz */
#define CH123_PERIOD_OVERFLOW          0x800
#define CH12_LENGTH_TIMER_OVERFLOW     64

typedef struct
{
    uint8_t ch1_sweep;              // 0xFF10
    uint8_t ch1_length_tim_dc;      // 0xFF11
    uint8_t ch1_vol_envelope;       // 0xFF12
    uint8_t ch1_period_low;         // 0xFF13
    uint8_t ch1_period_high_ctrl;   // 0xFF14
    uint8_t ch2_length_tim_dc;      // 0xFF15
    uint8_t ch2_vol_envelope;       // 0xFF17
    uint8_t ch2_period_low;         // 0xFF18
    uint8_t ch2_period_high_ctrl;   // 0xFF19
    uint8_t ch3_dac_en;             // 0xFF1A
    uint8_t ch3_length_tim;         // 0xFF1B
    uint8_t ch3_out_lvl;            // 0xFF1C
    uint8_t ch3_period_low;         // 0xFF1D
    uint8_t ch3_period_high_ctrl;   // 0xFF1E
    uint8_t ch4_length_tim;         // 0xFF20
    uint8_t ch4_vol_envelope;       // 0xFF21
    uint8_t ch4_freq_rand;          // 0xFF22
    uint8_t ch4_ctrl;               // 0xFF23
    uint8_t master_vol_vin_pan;     // 0xFF24
    uint8_t sound_panning;          // 0xFF25
    uint8_t audio_master_control;   // 0xFF26
    uint8_t wave_ram[0x10];         // 0xFF30 - 0xFF3F

    bool wave_ram_busy;
    uint8_t wave_ram_current_byte;
} apu_t;

typedef struct
{
    bool running;
    uint8_t id;
    bool wave_level_high;
    
    /* period, DC*/
    uint16_t duty_cycle;         // in 0.1%
    uint16_t period;             // period from registers
    uint16_t period_counter;     // period-clock: 1MHz
    uint8_t  period_prescaler;   // @CPU-CLK: prescaler to generate period-clock

    /* period sweep */
    uint8_t period_sweep_step;             // sets size of sweep step
    bool    period_sweep_dir_subtract;     // decrease period on sweep?
    uint8_t period_sweep_pace;             // sweep frequency (in 128Hz steps)
    uint8_t period_sweep_pace_counter;     // counts up to pace at 128Hz
    uint8_t period_sweep_pace_prescaler;   // divides div_apu(512Hz) down to 128Hz

    /* length timer */
    uint8_t  length_timer;             // counts up to 64 and turns off ch1/ch2
    uint8_t  length_timer_prescaler;   // divides div_apu(512Hz) down to 256Hz
    bool     length_enable;            // use length timer?

    /* volume and envelope */
    uint8_t volume;                          // ch1/ch2 volume level
    bool    envelope_dir_increase;           // increase volume on sweep?
    uint8_t envelope_sweep_pace;             // sweep frequency (in 128Hz steps)
    uint8_t envelope_sweep_pace_counter;     // counts up to pace at 64Hz
    uint8_t envelope_sweep_pace_prescaler;   // divides div_apu(512Hz) down to 64Hz
} ch12_t;

typedef struct
{
    bool running;
    bool dac_en;
    
    /* period*/
    uint16_t period;             // period from registers
    uint16_t period_counter;     // period-clock: 2MHz
    uint8_t  period_prescaler;   // @CPU-CLK: prescaler to generate period-clock

    /* length timer */
    uint8_t  length_timer;             // counts up to 256 and turns off ch3
    uint8_t  length_timer_prescaler;   // divides div_apu(512Hz) down to 256Hz
    bool     length_enable;            // use length timer?

    /* output level */
    uint8_t output_level_shift;   // sets volume: shift 4bit-samples by 0-4 bits (100%, 50%, 25%, 0%)

    /* wave-ram sampling */
    uint8_t output_sample;   // the current output sample taken from wave ram
    union
    {
        uint8_t wave_sample_select;               // counter that indexes into wave ram
        struct
        {
            uint8_t wave_ram_nibble_select : 1;   // select nibble: 0=high, 0 = low
            uint8_t wave_ram_byte_select   : 4;   // byte offset into wave-ram
        };
    };
} ch3_t;

typedef struct
{
    bool running;

    /* length timer */
    uint8_t  length_timer;             // counts up to 64 and turns off ch4
    uint8_t  length_timer_prescaler;   // divides div_apu(512Hz) down to 256Hz
    bool     length_enable;            // use length timer?
    
    /* frequency and randomness */
    uint16_t lfsr;             // 16bit shift register
    bool     lfsr_length_7Bit; // using only 7bit of 17 
    uint32_t lfsr_prescaler;   // ((16 * divider) << shift)
    uint32_t lfsr_counter;     // counts to ((16 * divider) << shift)

    /* volume and envelope */
    uint8_t volume;                          // ch1/ch2 volume level
    bool    envelope_dir_increase;           // increase volume on sweep?
    uint8_t envelope_sweep_pace;             // sweep frequency (in 128Hz steps)
    uint8_t envelope_sweep_pace_counter;     // counts up to pace at 64Hz
    uint8_t envelope_sweep_pace_prescaler;   // divides div_apu(512Hz) down to 64Hz
} ch4_t;

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static apu_t  apu = {0};
static ch12_t ch1 = {0};
static ch12_t ch2 = {0};
static ch3_t  ch3 = {0};
static ch4_t  ch4 = {0};

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/
static uint8_t apu_ch12_tick(ch12_t *chx, bool div_apu_512Hz);
static uint8_t apu_ch3_tick(bool div_apu_512Hz);
static uint8_t apu_ch4_tick(bool div_apu_512Hz);

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
static uint8_t apu_ch12_tick(ch12_t *chx, bool div_apu_512Hz)
{
    uint8_t ret;

    ret = 0;

    if (chx->running)
    {
        apu.audio_master_control |= chx->id;

        if (chx->wave_level_high)
        {
            ret = chx->volume;
        }

        /* Period / Duty-Cycle: Generates Output */
        if (CH12_PERIOD_PRESCALER <= ++chx->period_prescaler)
        {
            uint32_t total_time;
            uint32_t dc_threshold;

            chx->period_prescaler = 0;
            total_time           = CH123_PERIOD_OVERFLOW - chx->period;
            dc_threshold         = chx->period + (total_time * (uint32_t) chx->duty_cycle) / 1000;
            chx->wave_level_high = (chx->period_counter >= dc_threshold);

            if (CH123_PERIOD_OVERFLOW <= ++chx->period_counter)
            {
                chx->period_counter = chx->period;
            }
        }

        /* Period Sweep (only CH1 !!!)*/
        if ((div_apu_512Hz) && (0 != chx->period_sweep_pace))
        {
            if (CH12_PERIOD_SWEEP_PRESCALER <= ++chx->period_sweep_pace_prescaler)
            {
                chx->period_sweep_pace_prescaler = 0;
                if (chx->period_sweep_pace <= ++chx->period_sweep_pace_counter)
                {
                    uint16_t new_period;

                    chx->period_sweep_pace_counter = 0;

                    if (chx->period_sweep_dir_subtract)
                    {
                        new_period = chx->period - (chx->period >> chx->period_sweep_step);
                    }
                    else
                    {
                        new_period = chx->period + (chx->period >> chx->period_sweep_step);
                    }
                    
                    if (CH123_PERIOD_OVERFLOW <= new_period)
                    {
                        chx->running = false;
                    }

                    /* write back period */
                    chx->period = (new_period & (CH123_PERIOD_OVERFLOW - 1));
                    apu.ch1_period_low = (chx->period >> 0) & 0xFF;
                    apu.ch1_period_high_ctrl &= ~CH123_PERIOD_HIGH_MSK;
                    apu.ch1_period_high_ctrl |= (chx->period >> 8) & CH123_PERIOD_HIGH_MSK;

                    /* latch sweep parameters */
                    chx->period_sweep_pace = (apu.ch1_sweep & CH12_SWEEP_PACE_MSK) >> 4;
                    chx->period_sweep_dir_subtract = (0 != (apu.ch1_sweep & CH12_SWEEP_DIR_SUBTRACT_MSK));
                    chx->period_sweep_step = (apu.ch1_sweep & CH12_SWEEP_STEP_MSK);
                }
            }
        }

        /* length timer */
        if (div_apu_512Hz && chx->length_enable)
        {
            if (CH_LENGTH_TIMER_PRESCALER <= ++chx->length_timer_prescaler)
            {
                chx->length_timer_prescaler = 0;
                if (CH12_LENGTH_TIMER_OVERFLOW <= ++chx->length_timer)
                {
                    chx->length_timer = 0;
                    chx->running = false;
                }
            }
        }

        /* envelope (volume control) */
        if (div_apu_512Hz  && (0 != chx->envelope_sweep_pace))
        {
            if (CH124_ENVELOPE_SWEEP_PRESCALER <= ++chx->envelope_sweep_pace_prescaler)
            {
                chx->envelope_sweep_pace_prescaler = 0;
                if (chx->envelope_sweep_pace <= ++chx->envelope_sweep_pace_counter)
                {
                    chx->envelope_sweep_pace_counter = 0;
                    if (chx->envelope_dir_increase)
                    {
                        if (15 > chx->volume)
                        {
                            chx->volume++;
                        }
                    }
                    else
                    {
                        if (0 < chx->volume)
                        {
                            chx->volume--;
                        }
                    }
                }
            }
        }

        /* turn off ch1 if the volume is 0 and decreasing */
        if ((0 == chx->volume) && (false == chx->envelope_dir_increase))
        {
            chx->running = false;
        }
    }
    else
    {
        apu.audio_master_control &= ~chx->id;
    }

    return (ret & 0x0f);
}

static uint8_t apu_ch3_tick(bool div_apu_512Hz)
{
    uint8_t ret;

    ret = 0;

    if (ch3.running)
    {
        apu.audio_master_control |= AUDIO_MASTER_CONTROL_CH3_ON;

        ret = ch3.output_sample >> ch3.output_level_shift;

        /* Period: Select Output Sample */
        if (CH3_PERIOD_PRESCALER <= ++ch3.period_prescaler)
        {
            ch3.period_prescaler = 0;
            if (CH123_PERIOD_OVERFLOW <= ++ch3.period_counter)
            {
                ch3.period_counter = ch3.period;
                if (0 == ch3.wave_ram_nibble_select)
                {
                    ch3.output_sample = apu.wave_ram[ch3.wave_ram_byte_select] >> 4;
                }
                else
                {
                    ch3.output_sample = apu.wave_ram[ch3.wave_ram_byte_select] >> 0;
                }
                ch3.wave_sample_select++;
            }
        }

        /* length timer */
        if (div_apu_512Hz && ch3.length_enable)
        {
            if (CH_LENGTH_TIMER_PRESCALER <= ++ch3.length_timer_prescaler)
            {
                ch3.length_timer_prescaler = 0;
                if (0 == ++ch3.length_timer)
                {
                    ch3.running = false;
                }
            }
        }
    }
    else
    {
        apu.audio_master_control &= ~AUDIO_MASTER_CONTROL_CH3_ON;
    }

    return ret & 0x0f;
}

static uint8_t apu_ch4_tick(bool div_apu_512Hz)
{
    uint8_t ret;

    ret = 0;

    if (ch4.running)
    {
        apu.audio_master_control |= AUDIO_MASTER_CONTROL_CH4_ON;

        ret = (0 == (ch4.lfsr & 0x01)) ? ch4.volume : 0;

        /* LFSR Frequency */
        if (ch4.lfsr_prescaler <= ++ch4.lfsr_counter)
        {
            bool a, b, xnor_result;
            uint16_t msk;

            ch4.lfsr_counter = 0;

            /* do xnor */
            a = (0 != (ch4.lfsr & (1<<0)));
            b = (0 != (ch4.lfsr & (1<<1)));
            xnor_result = !(a ^ b);
            
            /* copy  */
            msk = (1 << 15) | ((!!ch4.lfsr_length_7Bit) << 7);
            ch4.lfsr = xnor_result ? (ch4.lfsr | msk) : (ch4.lfsr & ~msk);
            ch4.lfsr >>= 1;
        }

        /* length timer */
        if (div_apu_512Hz && ch4.length_enable)
        {
            if (CH_LENGTH_TIMER_PRESCALER <= ++ch4.length_timer_prescaler)
            {
                ch4.length_timer_prescaler = 0;
                if (0 == ++ch4.length_timer)
                {
                    ch4.running = false;
                }
            }
        }

        /* envelope (volume control) */
        if (div_apu_512Hz  && (0 != ch4.envelope_sweep_pace))
        {
            if (CH124_ENVELOPE_SWEEP_PRESCALER <= ++ch4.envelope_sweep_pace_prescaler)
            {
                ch4.envelope_sweep_pace_prescaler = 0;
                if (ch4.envelope_sweep_pace <= ++ch4.envelope_sweep_pace_counter)
                {
                    ch4.envelope_sweep_pace_counter = 0;
                    if (ch4.envelope_dir_increase)
                    {
                        if (15 > ch4.volume)
                        {
                            ch4.volume++;
                        }
                    }
                    else
                    {
                        if (0 < ch4.volume)
                        {
                            ch4.volume--;
                        }
                    }
                }
            }
        }

        /* turn off ch1 if the volume is 0 and decreasing */
        if ((0 == ch4.volume) && (false == ch4.envelope_dir_increase))
        {
            ch4.running = false;
        }
    }
    else
    {
        apu.audio_master_control &= ~AUDIO_MASTER_CONTROL_CH4_ON;
    }


    return ret & 0x0f;
}

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
void gbc_apu_init(void)
{
    /* ch1 */
    ch1.id = AUDIO_MASTER_CONTROL_CH1_ON;
    ch1.duty_cycle = 125;

    /* ch2 (has no period-sweep !) */
    ch2.id = AUDIO_MASTER_CONTROL_CH2_ON;
    ch2.duty_cycle = 125;
    ch2.period_sweep_dir_subtract = true;   // no overflow
    ch2.period_sweep_pace = 0;              // sweep disabled
}

/* internal: call this with every clock tick */
void gbc_apu_tick(void)
{
    static uint8_t last_div = 0;
    uint8_t div;
    bool div_apu_512Hz;
    uint8_t samples[4];
    uint8_t left, right;

    apu.wave_ram_busy = false;
    div = TIMER_GET_DIV();
    div_apu_512Hz = ((last_div & DIV_APU_BIT) && !(div & DIV_APU_BIT));
    
    samples[0] = apu_ch12_tick(&ch1, div_apu_512Hz);
    samples[1] = apu_ch12_tick(&ch2, div_apu_512Hz);
    samples[2] = apu_ch3_tick(div_apu_512Hz);
    samples[3] = apu_ch4_tick(div_apu_512Hz);

    left = 0;
    right = 0;

    right += (0 != (apu.sound_panning & PANNING_CH1_RIGHT)) ? samples[0] : 0;
    right += (0 != (apu.sound_panning & PANNING_CH2_RIGHT)) ? samples[1] : 0;
    right += (0 != (apu.sound_panning & PANNING_CH3_RIGHT)) ? samples[2] : 0;
    right += (0 != (apu.sound_panning & PANNING_CH4_RIGHT)) ? samples[3] : 0;
    left  += (0 != (apu.sound_panning &  PANNING_CH1_LEFT)) ? samples[0] : 0;
    left  += (0 != (apu.sound_panning &  PANNING_CH2_LEFT)) ? samples[1] : 0;
    left  += (0 != (apu.sound_panning &  PANNING_CH3_LEFT)) ? samples[2] : 0;
    left  += (0 != (apu.sound_panning &  PANNING_CH4_LEFT)) ? samples[3] : 0;

    last_div = div;

    return;
}

/* internal: only call this for address 0xFF10 - 0xFF3F */
uint8_t gbc_apu_get_memory(uint16_t addr)
{
    uint8_t ret;

    ret = 0;

    switch (addr)
    {
        case APU_ADDR_CH1_SWEEP:
        {
            ret = apu.ch1_sweep;
        }
        break;

        case APU_ADDR_CH1_VOL_ENVELOPE:
        {
            ret = apu.ch1_vol_envelope;
        }
        break;

        case APU_ADDR_CH2_VOL_ENVELOPE:
        {
            ret = apu.ch2_vol_envelope;
        }
        break;

        case APU_ADDR_CH3_DAC_EN:
        {
            ret = apu.ch3_dac_en;
        }
        break;

        case APU_ADDR_CH3_OUT_LVL:
        {
            ret = apu.ch3_out_lvl;
        }
        break;

        case APU_ADDR_CH4_VOL_ENVELOPE:
        {
            ret = apu.ch4_vol_envelope;
        }
        break;

        case APU_ADDR_CH4_FREQ_RAND:
        {
            ret = apu.ch4_freq_rand;
        }
        break;

        case APU_ADDR_MASTER_VOL_VIN_PAN:
        {
            ret = apu.master_vol_vin_pan;
        }
        break;

        case APU_ADDR_SOUND_PANNING:
        {
            ret = apu.sound_panning;
        }
        break;

        case APU_ADDR_AUDIO_MASTER_CONTROL:
        {
            ret = apu.audio_master_control;
        }
        break;

        case 0xFF30 ... 0xFF3F:   // Audio Wave RAM
        {
            ret = apu.wave_ram_busy ? apu.wave_ram_current_byte : apu.wave_ram[addr & 0xF];
        }
        break;

        case APU_ADDR_CH1_LENGTH_TIM_DC:
        case APU_ADDR_CH1_PERIOD_LOW:
        case APU_ADDR_CH1_PERIOD_HIGH_CTRL:
        case APU_ADDR_CH2_LENGTH_TIM_DC:
        case APU_ADDR_CH2_PERIOD_LOW:
        case APU_ADDR_CH2_PERIOD_HIGH_CTRL:
        case APU_ADDR_CH3_LENGTH_TIM:
        case APU_ADDR_CH3_PERIOD_LOW:
        case APU_ADDR_CH3_PERIOD_HIGH_CTRL:
        case APU_ADDR_CH4_LENGTH_TIM:
        case APU_ADDR_CH4_CTRL:
        case 0xFF16: case 0xFF1F: case 0xFF27 ... 0xFF2F:
        {
            /* write-only or reserved */
            ret = 0xFF;
        }
        break;

        default:
        {
            DBG_ERROR();
        }
        break;
    }

    return ret;
}

/* internal: only call this for address 0xFF10 - 0xFF3F */
void gbc_apu_set_memory(uint16_t addr, uint8_t val)
{
    switch (addr)
    {
        case APU_ADDR_CH1_SWEEP:
        {
            apu.ch1_sweep = val;
        }
        break;

        case APU_ADDR_CH1_LENGTH_TIM_DC:
        {
            apu.ch1_length_tim_dc = val;
            switch ((val & CH12_DC_MSK) >> CH12_DC_POS)
            {
                case CH12_DC_12_5_PERCENT: ch1.duty_cycle = 125; break;
                case CH12_DC_25_PERCENT  : ch1.duty_cycle = 250; break;
                case CH12_DC_50_PERCENT  : ch1.duty_cycle = 500; break;
                case CH12_DC_75_PERCENT  : ch1.duty_cycle = 750; break;
                default: break;
            }
            ch1.length_timer = (val & CH_LENGTH_TIMER_MSK);
        }
        break;

        case APU_ADDR_CH1_VOL_ENVELOPE:
        {
            apu.ch1_vol_envelope = val;
            if ((0 == ((val & CH124_INITIAL_VOLUME_MSK) >> CH124_INITIAL_VOLUME_POS)) &&
                (0 ==  (val & CH124_ENV_DIR_INC_MSK)))
            {
                ch1.running = false;
            }
        }
        break;

        case APU_ADDR_CH1_PERIOD_LOW:
        {
            apu.ch1_period_low = val;
        }
        break;

        case APU_ADDR_CH1_PERIOD_HIGH_CTRL:
        {
            apu.ch1_period_high_ctrl = val;
            ch1.length_enable = (0 != (val & CH_LENGTH_EN));
            if (val & CH_TRIGGER)
            {
                ch1.running = true;

                /* latch parameters */
                ch1.period_sweep_pace = (apu.ch1_sweep & CH12_SWEEP_PACE_MSK) >> CH12_SWEEP_PACE_POS;
                ch1.period_sweep_dir_subtract = (0 != (apu.ch1_sweep & CH12_SWEEP_DIR_SUBTRACT_MSK));
                ch1.period_sweep_step = (apu.ch1_sweep & CH12_SWEEP_STEP_MSK);
                ch1.period = (uint16_t) apu.ch1_period_low + (((uint16_t) apu.ch1_period_high_ctrl & CH123_PERIOD_HIGH_MSK) << 8);
                ch1.volume = (apu.ch1_vol_envelope & CH124_INITIAL_VOLUME_MSK) >> CH124_INITIAL_VOLUME_POS;
                ch1.envelope_dir_increase = (0 != (apu.ch1_vol_envelope & CH124_ENV_DIR_INC_MSK));
                ch1.envelope_sweep_pace = (apu.ch1_vol_envelope & CH124_ENV_SWEEP_PACE_MSK);
                
                /* reset length-timer if it is expired */
                if (0 == ch1.length_timer)
                {
                    ch1.length_timer = (apu.ch1_length_tim_dc & CH_LENGTH_TIMER_MSK);
                    ch1.length_timer_prescaler = 0;
                }

                /* reset internal states */
                ch1.period_counter = 0;
                ch1.period_prescaler = 0;
                ch1.period_sweep_pace_counter = 0;
                ch1.period_sweep_pace_prescaler = 0;
                ch1.envelope_sweep_pace_counter = 0;
                ch1.envelope_sweep_pace_prescaler = 0;
                ch1.wave_level_high = false;

                /* turn off ch1 if there would be a sweep overflow */
                if (!ch1.period_sweep_dir_subtract)
                {
                    uint16_t new_period = ch1.period + (ch1.period >> ch1.period_sweep_step);
                    if (CH123_PERIOD_OVERFLOW <= new_period)
                    {
                        ch1.running = false;
                    }
                }
                
                /* turn off ch1 if the volume is 0 and decreasing */
                if ((0 == ch1.volume) && (false == ch1.envelope_dir_increase))
                {
                    ch1.running = false;
                }
            }
        }
        break;

        case APU_ADDR_CH2_LENGTH_TIM_DC:
        {
            apu.ch2_length_tim_dc = val;
            switch ((val & CH12_DC_MSK) >> CH12_DC_POS)
            {
                case CH12_DC_12_5_PERCENT: ch2.duty_cycle = 125; break;
                case CH12_DC_25_PERCENT  : ch2.duty_cycle = 250; break;
                case CH12_DC_50_PERCENT  : ch2.duty_cycle = 500; break;
                case CH12_DC_75_PERCENT  : ch2.duty_cycle = 750; break;
                default: break;
            }
            ch2.length_timer = (val & CH_LENGTH_TIMER_MSK);
        }
        break;

        case APU_ADDR_CH2_VOL_ENVELOPE:
        {
            apu.ch2_vol_envelope = val;
            if ((0 == ((val & CH124_INITIAL_VOLUME_MSK) >> CH124_INITIAL_VOLUME_POS)) &&
                (0 ==  (val & CH124_ENV_DIR_INC_MSK)))
            {
                ch2.running = false;
            }
        }
        break;

        case APU_ADDR_CH2_PERIOD_LOW:
        {
            apu.ch2_period_low = val;
        }
        break;

        case APU_ADDR_CH2_PERIOD_HIGH_CTRL:
        {
            apu.ch2_period_high_ctrl = val;
            ch2.length_enable = (0 != (val & CH_LENGTH_EN));
            if (val & CH_TRIGGER)
            {
                ch2.running = true;
    
                /* latch parameters */
                ch2.period = (uint16_t) apu.ch2_period_low + (((uint16_t) apu.ch2_period_high_ctrl & CH123_PERIOD_HIGH_MSK) << 8);
                ch2.volume = (apu.ch2_vol_envelope & CH124_INITIAL_VOLUME_MSK) >> CH124_INITIAL_VOLUME_POS;
                ch2.envelope_dir_increase = (0 != (apu.ch2_vol_envelope & CH124_ENV_DIR_INC_MSK));
                ch2.envelope_sweep_pace = (apu.ch2_vol_envelope & CH124_ENV_SWEEP_PACE_MSK);
                
                /* reset length-timer if it is expired */
                if (0 == ch2.length_timer)
                {
                    ch2.length_timer = (apu.ch2_length_tim_dc & CH_LENGTH_TIMER_MSK);
                    ch2.length_timer_prescaler = 0;
                }
    
                /* reset internal states */
                ch2.period_counter = 0;
                ch2.period_prescaler = 0;
                ch2.envelope_sweep_pace_counter = 0;
                ch2.envelope_sweep_pace_prescaler = 0;
                ch2.wave_level_high = false;
    
                /* paranoia: make sure ch2 uses no period-sweep */
                ch2.period_sweep_dir_subtract = true;   // no overflow
                ch2.period_sweep_pace = 0;              // sweep disabled
                
                /* turn off ch2 if the volume is 0 and decreasing */
                if ((0 == ch2.volume) && (false == ch2.envelope_dir_increase))
                {
                    ch2.running = false;
                }
            }
        }
        break;

        case APU_ADDR_CH3_DAC_EN:
        {
            apu.ch3_dac_en = val;
            ch3.dac_en = (0 != (val & CH3_DAC_EN_MSK));
            if (!ch3.dac_en)
            {
                ch3.running = false;
            }
        }
        break;

        case APU_ADDR_CH3_LENGTH_TIM:
        {
            apu.ch3_length_tim = val;
            ch3.length_timer = val;
        }
        break;

        case APU_ADDR_CH3_OUT_LVL:
        {
            // Samples are 4Bits long
            // this table determines by how much each sample
            // is shifted right to decrease its volume:
            // { mute, 100%, 50%, 25% }
            const uint8_t shift_lut[] = { 4, 0, 1, 2};
            apu.ch3_out_lvl = val;
            ch3.output_level_shift = shift_lut[((val & CH3_OUTPUT_LEVEL_MSK) >> CH3_OUTPUT_LEVEL_POS)];
        }
        break;

        case APU_ADDR_CH3_PERIOD_LOW:
        {
            apu.ch3_period_low = val;
        }
        break;

        case APU_ADDR_CH3_PERIOD_HIGH_CTRL:
        {
            apu.ch3_period_high_ctrl = val;
            ch3.length_enable = (0 != (val & CH_LENGTH_EN));
            if (val & CH_TRIGGER)
            {
                ch3.running = true;
    
                /* latch parameters */
                ch3.period = (uint16_t) apu.ch3_period_low + (((uint16_t) apu.ch3_period_high_ctrl & CH123_PERIOD_HIGH_MSK) << 8);
                ch3.length_timer = apu.ch3_length_tim;
                
                /* reset length-timer if it is expired */
                if (0 == ch3.length_timer)
                {
                    ch3.length_timer = apu.ch3_length_tim;
                    ch3.length_timer_prescaler = 0;
                }
    
                /* reset internal states */
                ch3.period_counter = 0;
                ch3.period_prescaler = 0;
                ch3.wave_sample_select = 1; /* hardware quirk */

                if (!ch3.dac_en)
                {
                    ch3.running = false;
                }
            }
        }
        break;

        case APU_ADDR_CH4_LENGTH_TIM:
        {
            apu.ch4_length_tim = val;
            ch4.length_timer = (val & CH_LENGTH_TIMER_MSK);
        }
        break;

        case APU_ADDR_CH4_VOL_ENVELOPE:
        {
            apu.ch4_vol_envelope = val;
            if ((0 == ((val & CH124_INITIAL_VOLUME_MSK) >> CH124_INITIAL_VOLUME_POS)) &&
                (0 ==  (val & CH124_ENV_DIR_INC_MSK)))
            {
                ch4.running = false;
            }
        }
        break;

        case APU_ADDR_CH4_FREQ_RAND:
        {
            apu.ch4_freq_rand = val;
            uint8_t shift = ((val & CH4_LFSR_CLOCK_SHIFT_MSK) >> CH4_LFSR_CLOCK_SHIFT_POS);
            uint8_t div = (val & CH4_LFSR_CLK_DIV_MSK);
            if (0 == div)
            {
                // Divider = 0 is treated as 0.5, so we multiply by 0.5 = divide by 2 (-> shift + 1)
                ch4.lfsr_prescaler = CH4_LFSR_PRESCALER << (shift + 1);
            }
            else
            {
                ch4.lfsr_prescaler = (CH4_LFSR_PRESCALER * div) << shift;
            }
            ch4.lfsr_length_7Bit = (0 != (val & CH4_LFSR_WIDTH_7));
        }
        break;

        case APU_ADDR_CH4_CTRL:
        {
            ch4.length_enable = (0 != (val & CH_LENGTH_EN));
            if (val & CH_TRIGGER)
            {
                ch4.running = true;
    
                /* latch parameters */
                ch4.volume = (apu.ch4_vol_envelope & CH124_INITIAL_VOLUME_MSK) >> CH124_INITIAL_VOLUME_POS;
                ch4.envelope_dir_increase = (0 != (apu.ch4_vol_envelope & CH124_ENV_DIR_INC_MSK));
                ch4.envelope_sweep_pace = (apu.ch4_vol_envelope & CH124_ENV_SWEEP_PACE_MSK);
                
                /* reset length-timer if it is expired */
                if (0 == ch4.length_timer)
                {
                    ch4.length_timer = (apu.ch4_length_tim & CH_LENGTH_TIMER_MSK);
                    ch4.length_timer_prescaler = 0;
                }
    
                /* reset internal states */
                ch4.envelope_sweep_pace_counter = 0;
                ch4.envelope_sweep_pace_prescaler = 0;
                ch4.lfsr = 0;
                
                /* turn off ch4 if the volume is 0 and decreasing */
                if ((0 == ch4.volume) && (false == ch4.envelope_dir_increase))
                {
                    ch4.running = false;
                }
            }
        }
        break;

        case APU_ADDR_MASTER_VOL_VIN_PAN:
        {
            /* todo: not implemented */
        }
        break;

        case APU_ADDR_SOUND_PANNING:
        {
            apu.sound_panning = val;
        }
        break;

        case APU_ADDR_AUDIO_MASTER_CONTROL:
        {
            apu.audio_master_control = (apu.audio_master_control & ~AUDIO_MASTER_CONTROL_AUDIO_ON) |
                                       (val & AUDIO_MASTER_CONTROL_AUDIO_ON);
        }
        break;

        case 0xFF30 ... 0xFF3F:   // Audio Wave RAM
        {
            apu.wave_ram[addr & 0xF] = val;
        }
        break;

        case 0xFF16:
        case 0xFF1F:
        case 0xFF27 ... 0xFF2F:
        {
            /* reserved -> nothing to do */
        }
        break;

        default:
        {
            DBG_ERROR();
        }
        break;
    }
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
