
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         SM83 Memory Bus                             *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: bus.c                                                *
 *        author: tstr92                                               *
 *          date: 2024-04-21                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include <stdio.h>
#include <io.h>

#include "debug.h"
#include "bus.h"
#include "cpu.h"
#include "joypad.h"
#include "timer.h"


/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
typedef struct
{
	uint8_t memory[64 * 1024];
} bus_t;

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static bus_t bus;

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
uint8_t bus_get_memory(uint16_t addr)
{
	uint8_t ret = 0;

#if (0 < BUILD_TEST_DLL)
	ret = ((uint8_t *) &bus.memory[0])[addr];
#else
	switch (addr)
	{
		case 0x0000 ... 0x3FFF:   // Permanently mapped ROM Bank
		case 0x4000 ... 0x7FFF:   // Area for switch ROM Bank
		case 0x8000 ... 0x9FFF:   // Video RAM
		case 0xA000 ... 0xBFFF:   // Area for switchable external RAM banks
		case 0xC000 ... 0xCFFF:   // Game Boy’s working RAM bank 0
		case 0xD000 ... 0xDFFF:   // Game Boy’s working RAM bank 1
		case 0xFE00 ... 0xFE9F:   // Sprite Attribute Table
		case 0xFF80 ... 0xFFFE:   // High RAM Area
		case            0xFF0F:   // Interrupt Flags Register
		case            0xFFFF:   // Interrupt Enable Register
			ret = ((uint8_t *) &bus.memory[0])[addr];
		break;

		// 0xFF00 - 0xFF7F   Devices Mappings. Used to access I/O devices

		case            0xFF00:   // Joypad
			ret = gbc_joypad_get_memory(addr);
		break;

		case 0xFF01 ... 0xFF02:   // Serial
			debug_printf ("Memory access 'Serial' at 0x%04x.\n", addr);
		break;

		case 0xFF04 ... 0xFF07:   // Timer
			ret = gbc_timer_get_memory(addr);
		break;

		case 0xFF10 ... 0xFF3F:   // Audio + Audio Wave RAM
		break;

		case 0xE000 ... 0xFDFF:   // reserved
		case 0xFEA0 ... 0xFEFF:   // reserved
		default:
			debug_printf ("illegal memory access at 0x%04x.\n", addr);
		break;
	}
#endif

	return ret;
}

void bus_set_memory(uint16_t addr, uint8_t val)
{
#if (0 < BUILD_TEST_DLL)
	((uint8_t *) &bus.memory[0])[addr] = val;
#else
switch (addr)
{
	case 0x0000 ... 0x3FFF:   // Permanently mapped ROM Bank
	case 0x4000 ... 0x7FFF:   // Area for switch ROM Bank
	case 0x8000 ... 0x9FFF:   // Video RAM
	case 0xA000 ... 0xBFFF:   // Area for switchable external RAM banks
	case 0xC000 ... 0xCFFF:   // Game Boy’s working RAM bank 0
	case 0xD000 ... 0xDFFF:   // Game Boy’s working RAM bank 1
	case 0xFE00 ... 0xFE9F:   // Sprite Attribute Table
	case 0xFF80 ... 0xFFFE:   // High RAM Area
	case            0xFF0F:   // Interrupt Flags Register
	case            0xFFFF:   // Interrupt Enable Register
		((uint8_t *) &bus.memory[0])[addr] = val;
	break;

	case            0xFF00:   // Joypad
		gbc_joypad_set_memory(addr, val);
	break;

	case 0xFF01 ... 0xFF02:   // Serial
		debug_printf ("Memory access 'Serial' at 0x%04x.\n", addr);
	break;

	case 0xFF04 ... 0xFF07:   // Timer
		gbc_timer_set_memory(addr, val);
	break;

	case 0xFF10 ... 0xFF26:   // Audio
	case 0xFF30 ... 0xFF3F:   // Audio Wave RAM
	break;

#if (USE_0xE000_AS_PUTC_DEVICE)
	case            0xE000:   // UART
		putc(val, stdout);
		fflush(stdout);
	break;
	case 0xE001 ... 0xFDFF:   // reserved
#else
	case 0xE000 ... 0xFDFF:   // reserved
	case 0xFEA0 ... 0xFEFF:   // reserved
#endif
	default:
		debug_printf ("illegal memory access at 0x%04x.\n", addr);
	break;
}
#endif
	debug_printf("\nwrote %02x to %04x\n", val, addr);
}

bool bus_init_memory(const char *filename)
{
	bool ret;
	FILE *gbFile;
	size_t fileSize;

	ret = true;

	if (false != ret)
	{
		if (NULL == filename)
		{
			ret = false;
		}
	}

	if (false != ret)
	{
		gbFile = fopen(filename, "rb");
		if (NULL == gbFile)
		{
			ret = false;
		}
	}

	if (false != ret)
	{
		int fd = _fileno(gbFile);
		fileSize = _filelength(fd);
		if ((32 * 1024) < fileSize)
		{
			ret = false;
		}
	}

	if (false != ret)
	{
		fread(bus.memory, 1, fileSize, gbFile);
		fclose(gbFile);
	}

	return ret;
}

void bus_tick(void)
{
	gbc_cpu_tick();
	gbc_timer_tick();
}

int main(int argc, char *argv[])
{
	if (2 == argc)
	{
		char *FileName  = argv[1];
		if (!bus_init_memory(FileName))
		{
			printf("Error: Could not open file '%s'.\n", FileName);
			return 1;
		}
	}
	else
	{
		printf("Error: Expecting FileName as argument.\nInvocation:\n\t'%s <file>'.\n", argv[0]);
		return 1;
	}

	for (;;)
	{
		bus_tick();
		if (gbc_cpu_stopped())
		{
			printf("\nCPU Stopped!\n");
			break;
		}
	}
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
