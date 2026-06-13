
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         GBC Emulator                                *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: emulator.c                                           *
 *        author: tstr92                                               *
 *          date: 2025-05-10                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#include "emulator.h"
#include "bus.h"
#include "cpu.h"
#include "timer.h"
#include "apu.h"
#include "ppu.h"
#include "debug.h"
#include "trace.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static bool run = true;

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
int emulator_load_game(uint8_t *rom, size_t rom_size, uint8_t *sram, size_t sram_size, rtc_t *p_rtc)
{
	int error = bus_init_memory(rom, rom_size, sram, sram_size, p_rtc);

	if (0 == error)
	{
		gbc_cpu_init();
		bus_init();
		gbc_apu_init();
		gbc_ppu_init();
		trace_init();
	}

	debug_printf("emulator_load_game returned %d\n", error);

	return error;
}

void emulator_run(void)
{
	uint64_t start, end;
	int i;

	start = platform_getSysTick_ms();

	while(run)
	{
		bus_tick();
		emulator_tick_cb();
		if (gbc_cpu_stopped())
		{
			end = platform_getSysTick_ms();
			uint32_t duration = (uint32_t) (end - start);
			uint32_t ingame_frames = gbc_cpu_get_cycle_cnt() / 140448;
			uint32_t realworld_frames = (duration * 60) / 1000;
			uint64_t cyccnt_8mhz = 8000 * duration;
			debug_printf("\n\n");
			debug_printf("Cycle-Count: %"PRIu64", elapsed time: %"PRIu32"ms, Cycle-Count(8MHz): %"PRIu64", emulation_ccnt/real_ccnt=%"PRIu64"\n", gbc_cpu_get_cycle_cnt(), duration, cyccnt_8mhz, 0==cyccnt_8mhz?0:gbc_cpu_get_cycle_cnt()/cyccnt_8mhz);
			debug_printf("emulation_frames: %"PRIu32", real_frames: %"PRIu32", emulation_frames/real_frames=%"PRIu32"\n", ingame_frames, realworld_frames, 0==realworld_frames?0:ingame_frames/realworld_frames);
			debug_printf("\nCPU Stopped!\n");
			break;
		}
	}
}

__attribute__((weak)) uint8_t emulator_get_speed(void)
{
    return 10;
}

void emulator_write_save_file(void)
{
	gbc_cpu_write_internal_state();
	gbc_bus_write_internal_state();
	gbc_ppu_write_internal_state();
	gbc_apu_write_internal_state();
	gbc_tim_write_internal_state();
}

size_t emulator_get_save_file_size(void)
{
    return (size_t) (
        gbc_cpu_get_internal_state_size() +
        gbc_bus_get_internal_state_size() +
        gbc_ppu_get_internal_state_size() +
        gbc_apu_get_internal_state_size() +
        gbc_tim_get_internal_state_size()
    );
}

int emulator_load_save_file(void)
{
	int ret = 0;

	if (0 == ret)
	{
		ret = gbc_cpu_set_internal_state();
	}
	if (0 == ret)
	{
		ret = gbc_bus_set_internal_state();
	}
	if (0 == ret)
	{
		ret = gbc_ppu_set_internal_state();
	}
	if (0 == ret)
	{
		ret = gbc_apu_set_internal_state();
	}
	if (0 == ret)
	{
		ret = gbc_tim_set_internal_state();
	}

	return ret;
}

void emulator_stop(void)
{
	run = false;
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
