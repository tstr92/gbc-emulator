
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

	bus_init();
	gbc_apu_init();

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
		if (gbc_cpu_stopped())
		{
			end = platform_getSysTick_ms();
			uint32_t duration = (uint32_t) (end - start);
			uint32_t ingame_frames = gbc_cpu_get_cycle_cnt() / 140448;
			uint32_t realworld_frames = (duration * 60) / 1000;
			uint64_t cyccnt_8mhz = 8000 * duration;
			printf("\n\n");
			printf("Cycle-Count: %llu, elapsed time: %lums, Cycle-Count(8MHz): %llu, emulation_ccnt/real_ccnt=%llu\n", gbc_cpu_get_cycle_cnt(), duration, cyccnt_8mhz, gbc_cpu_get_cycle_cnt()/cyccnt_8mhz);
			printf("emulation_frames: %lu, real_frames: %lu, emulation_frames/real_frames=%lu\n", ingame_frames, realworld_frames, ingame_frames/realworld_frames);
			printf("\nCPU Stopped!\n");
			break;
		}
	}
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
