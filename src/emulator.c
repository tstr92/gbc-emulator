
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

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
int emulator_load_game(char *fileName)
{
	if (!bus_init_memory(fileName))
	{
		printf("Error: Could not open file '%s'.\n", fileName);
		return 1;
	}

	gbc_cpu_init();
	bus_init();
	gbc_apu_init();
	trace_init();

	return 0;
}

void emulator_run(void)
{
	uint64_t start, end;
	int i;

	start = platform_getSysTick_ms();

	for (;;)
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
			printf("\n\n");
			printf("Cycle-Count: %llu, elapsed time: %lums, Cycle-Count(8MHz): %llu, emulation_ccnt/real_ccnt=%llu\n", gbc_cpu_get_cycle_cnt(), duration, cyccnt_8mhz, 0==cyccnt_8mhz?0:gbc_cpu_get_cycle_cnt()/cyccnt_8mhz);
			printf("emulation_frames: %lu, real_frames: %lu, emulation_frames/real_frames=%lu\n", ingame_frames, realworld_frames, 0==realworld_frames?0:ingame_frames/realworld_frames);
			printf("\nCPU Stopped!\n");
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

int emulator_load_save_file(void)
{
	int ret = 0;

	if (0 == ret)
	{
		gbc_cpu_set_internal_state();
	}
	if (0 == ret)
	{
		gbc_bus_set_internal_state();
	}
	if (0 == ret)
	{
		gbc_ppu_set_internal_state();
	}
	if (0 == ret)
	{
		gbc_apu_set_internal_state();
	}
	if (0 == ret)
	{
		gbc_tim_set_internal_state();
	}

	return ret;
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
