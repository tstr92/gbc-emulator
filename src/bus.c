
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
	if (((addr < 0xFEA0 ) || (addr > 0xFEFF)) &&
	    ((addr < 0xE000 ) || (addr > 0xFDFF)))
	{
		ret = ((uint8_t *) &bus.memory[0])[addr];
	}
	else
	{
		debug_printf ("illegal memory access at 0x%04x.\n", addr);
	}
#endif

	return ret;
}

void bus_set_memory(uint16_t addr, uint8_t val)
{
#if (0 < BUILD_TEST_DLL)
	((uint8_t *) &bus.memory[0])[addr] = val;
#else
	if (((addr < 0xFEA0 ) || (addr > 0xFEFF)) &&
	    ((addr < 0xE000 ) || (addr > 0xFDFF)))
	{
		((uint8_t *) &bus.memory[0])[addr] = val;
	}
#if (USE_0xE000_AS_PUTC_DEVICE)
	else if (addr == 0xE000)
	{
		putc(val, stdout);
		fflush(stdout);
	}
#endif
	else
	{
		debug_printf ("illegal memory access at 0x%04x.\n", addr);
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
/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
