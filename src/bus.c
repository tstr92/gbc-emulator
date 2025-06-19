
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
#include "serial.h"
#include "apu.h"
#include "ppu.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define DMA            0xFF46   // OAM DMA source address & start (Copies 0xXX00-XX9F (with XX=0x00-0xDF) to 0xFE00-0xFE9f (Sprite Attribute Table)) in 160 M-Cycles
#define KEY1           0xFF4D   // Prepare speed switch
#define RP             0xFF56   // Infrared communications port
#define SVBK           0xFF70   // WRAM bank
#define SVBK_WRAM_BANK (0x07)   // select bank 1...7

#define DMA_DST_BASE        0xFE00
#define DMA_CP_LEN          0xA0
#define VRAM_DMA_MAX_CP_LEN 0x800
#define VRAM_DMA_LEN_MSK    0x7F
#define VRAM_DMA_HBLANK_MSK 0x80
#define VRAM_DMA_CP_CYCLES  (8 * 4)

#define KEY1_DOUBLE_SPEED  0x80
#define KEY1_SWITCH_ARMED  0x01

typedef struct
{
	uint8_t wram_banksel;
	uint8_t wram[8][4096];
	uint8_t hram[127];
	uint8_t IF;
	uint8_t IE;
	uint8_t key1;

	struct
	{
		uint8_t addr;
		uint8_t offset;
		uint8_t prescaler;
		bool active;
	} oam_dma;

	struct
	{
		uint16_t src;
		uint16_t dst;
		uint16_t len;
		enum
		{
			general_purpose_dma,
			HBlank_dma
		} mode;
		bool active;
	} vram_dma;

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
static inline void bus_dma_cpy(uint16_t dst, uint16_t src, uint16_t len);
static void bus_oam_dma_tick(void);

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
static inline void bus_dma_cpy(uint16_t dst, uint16_t src, uint16_t len)
{
	for (int i = 0; i < len; i++)
	{
		bus_set_memory(dst + i, bus_get_memory(src + i));
	}
}

static void bus_oam_dma_tick(void)
{
	if (bus.oam_dma.active)
	{
		if (4 <= ++bus.oam_dma.prescaler)
		{
			bus.oam_dma.prescaler = 0;
			bus_dma_cpy(DMA_DST_BASE + bus.oam_dma.offset, (((uint16_t) bus.oam_dma.addr) << 8) + bus.oam_dma.offset, 1);
			if (DMA_CP_LEN <= ++bus.oam_dma.offset)
			{
				bus.oam_dma.active = false;
			}
		}
	}
}


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
		case 0xA000 ... 0xBFFF:   // Area for switchable external RAM banks
			ret = ((uint8_t *) &bus.memory[0])[addr];
		break;

		case 0xC000 ... 0xCFFF:   // Game Boy’s working RAM bank 0
		{
			ret = bus.wram[0][addr & 0xFFF];
		}
		break;

		case 0xD000 ... 0xDFFF:   // Game Boy’s working RAM bank 1
		{
			ret = bus.wram[bus.wram_banksel][addr & 0xFFF];
		}
		break;

		case DMA:
		{
			ret = bus.oam_dma.addr;
		}
		break;

		case KEY1:
		{
			ret = bus.key1;
		}
		break;

		/* write-only vram-dma-registers */
		case 0xFF51: case 0xFF52: case 0xFF53: case 0xFF54:
		{
			ret = 0xff;
		}
		break;
		case 0xFF55:
		{
			ret = (bus.vram_dma.len / 16) - 1;
			if (!bus.vram_dma.active)
			{
				ret |= 0x80;
			}
		}

		case 0x8000 ... 0x9FFF:   // Video RAM
		case 0xFF40 ... 0xFF45:
		case 0xFF47 ... 0xFF4B:
		case 0xFF4F:
		case 0xFF68 ... 0xFF6C:
		case 0xFE00 ... 0xFE9F:   // Sprite Attribute Table
		{
			ret = gbc_ppu_get_memory(addr);
		}
		break;

		case 0xFF80 ... 0xFFFE:   // High RAM Area
		{
			ret = bus.hram[addr & 0x7F];
		}
		break;

		case            0xFF0F:   // Interrupt Flags Register
		{
			ret = bus.IF;
		}
		break;
	
		case            0xFFFF:   // Interrupt Enable Register
		{
			ret = bus.IE;
		}
		break;

		// 0xFF00 - 0xFF7F   Devices Mappings. Used to access I/O devices

		case            0xFF00:   // Joypad
		{
			ret = gbc_joypad_get_memory(addr);
		}
		break;

		case 0xFF01 ... 0xFF02:   // Serial
		{
			ret = gbc_serial_get_memory(addr);
		}
		break;

		case 0xFF04 ... 0xFF07:   // Timer
		{
			ret = gbc_timer_get_memory(addr);
		}
		break;

		case 0xFF10 ... 0xFF3F:   // Audio + Audio Wave RAM
		{
			ret = gbc_apu_get_memory(addr);
		}
		break;

		case RP:
		{
			debug_printf ("Memory access 'Infrared' at 0x%04x.\n", addr);
		}
		break;

		case SVBK:
		{
			ret = bus.wram_banksel;
		}
		break;

		case 0xE000 ... 0xFDFF:   // reserved
		case 0xFEA0 ... 0xFEFF:   // reserved
		default:
			debug_printf ("illegal memory read at 0x%04x.\n", addr);
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
		{
			printf("%02x -> %04x\n", val, addr);
		}
		break;

		case 0x4000 ... 0x7FFF:   // Area for switch ROM Bank
		{
			printf("%02x -> %04x\n", val, addr);
		}
		break;

		case 0xA000 ... 0xBFFF:   // Area for switchable external RAM banks
			((uint8_t *) &bus.memory[0])[addr] = val;
		break;

		case 0xC000 ... 0xCFFF:   // Game Boy’s working RAM bank 0
		{
			bus.wram[0][addr & 0xFFF] = val;
		}
		break;

		case 0xD000 ... 0xDFFF:   // Game Boy’s working RAM bank 1
		{
			bus.wram[bus.wram_banksel][addr & 0xFFF] = val;
		}
		break;

		case DMA:
		{
			if (0xDF >= val)
			{
				bus.oam_dma.active = true;
				bus.oam_dma.addr = val;
				bus.oam_dma.offset = 0;
				bus.oam_dma.prescaler = 0;
			}
		}
		break;

		case KEY1:
		{
			bus.key1 &= ~KEY1_SWITCH_ARMED;
			bus.key1 |= (val & KEY1_SWITCH_ARMED);
		}
		break;

		case 0xFF51: // VRAM DMA SRC Low
		{
			/* "The four lower bits of this address will be ignored and treated as 0." */
			bus.vram_dma.src &= 0xFF00;
			bus.vram_dma.src |= val & 0xF0;
		}
		break;
		case 0xFF52: // VRAM DMA SRC High
		{
			bus.vram_dma.src &= 0x00FF;
			bus.vram_dma.src |= ((uint16_t) val) << 8;
		}
		break;
		case 0xFF53: // VRAM DMA DST Low
		{
			/* "The four lower bits of this address will be ignored and treated as 0." */
			bus.vram_dma.dst &= 0xFF00;
			bus.vram_dma.dst |= val & 0xF0;
		}
		break;
		case 0xFF54: // VRAM DMA DST High
		{
			/* "Only bits 12-4 are respected; others are ignored." */
			bus.vram_dma.dst &= 0x00FF;
			bus.vram_dma.dst |= (((uint16_t) val & 0x1F) << 8) | 0x8000;
		}
		break;
		case 0xFF55: // VRAM DMA Length/Mode/Start
		{
			if ((bus.vram_dma.active) && (HBlank_dma == bus.vram_dma.mode) && (!(val & 0x80)))
			{
				bus.vram_dma.active = false;
			}
			else if ((((0x0000 <= bus.vram_dma.src) && (0x7FF0 >= bus.vram_dma.src))  ||
			          ((0xA000 <= bus.vram_dma.src) && (0xDFF0 >= bus.vram_dma.src))) &&
			         ( (0x8000 <= bus.vram_dma.dst) && (0x9FF0 >= bus.vram_dma.dst)))
			{
				uint16_t num_blocks;
				num_blocks = ((val & VRAM_DMA_LEN_MSK) + 1);
				bus.vram_dma.active = true;
				bus.vram_dma.len = num_blocks * 16;
				if (val & VRAM_DMA_HBLANK_MSK)
				{
					bus.vram_dma.mode = HBlank_dma;
				}
				else
				{
					uint32_t num_stall_cycles;
					num_stall_cycles = (VRAM_DMA_CP_CYCLES * num_blocks) << ((bus.key1 & KEY1_DOUBLE_SPEED) ? 1 : 0);
					bus.vram_dma.mode = general_purpose_dma;
					bus_dma_cpy(bus.vram_dma.dst, bus.vram_dma.src, bus.vram_dma.len);
					gbc_cpu_stall(num_stall_cycles);
					printf("DMA Go! %d Bytes %04x -> %04X\n", bus.vram_dma.len, bus.vram_dma.src, bus.vram_dma.dst);
				}
			}
		}
		break;

		case 0x8000 ... 0x9FFF:   // Video RAM
		case 0xFF40 ... 0xFF45:
		case 0xFF47 ... 0xFF4B:
		case 0xFF4F:
		case 0xFF68 ... 0xFF6C:
		case 0xFE00 ... 0xFE9F:   // Sprite Attribute Table
		{
			gbc_ppu_set_memory(addr, val);
		}
		break;

		case 0xFF80 ... 0xFFFE:   // High RAM Area
		{
			bus.hram[addr & 0x7F] = val;
		}
		break;

		case            0xFF00:   // Joypad
			gbc_joypad_set_memory(addr, val);
		break;

		case 0xFF01 ... 0xFF02:   // Serial
			gbc_serial_set_memory(addr, val);
		break;

		case 0xFF04 ... 0xFF07:   // Timer
			gbc_timer_set_memory(addr, val);
		break;

		case 0xFF10 ... 0xFF26:   // Audio
		case 0xFF30 ... 0xFF3F:   // Audio Wave RAM
			gbc_apu_set_memory(addr, val);
		break;

		case RP:
			debug_printf ("Memory access 'Infrared' at 0x%04x.\n", addr);
		break;

		case SVBK:
		{
			bus.wram_banksel = (0 == (val & SVBK_WRAM_BANK)) ? 1 : (val & SVBK_WRAM_BANK);
		}
		break;

		case            0xFF0F:   // Interrupt Flags Register
		{
			bus.IF = val;
		}
		break;

		case            0xFFFF:   // Interrupt Enable Register
		{
			bus.IE = val;
		}
		break;

#if (USE_0xE000_AS_PUTC_DEVICE)
		case            0xE000:   // UART
		{
			putc(val, stdout);
			fflush(stdout);
			// #define UART_BUFFER_SIZE 1024
			// static char uart_buffer[UART_BUFFER_SIZE];
			// static int uart_buffer_index = 0;
			// uart_buffer[uart_buffer_index++] = val;
			// if (('\n' == val) || (UART_BUFFER_SIZE <= uart_buffer_index))
			// {
			// 	uart_buffer_index = 0;
			// 	printf(uart_buffer);
			// 	fflush(stdout);
			// }
			// #undef UART_BUFFER_SIZE
		}
		break;
		case 0xE001 ... 0xFDFF:   // reserved
#else
		case 0xE000 ... 0xFDFF:   // reserved
		case 0xFEA0 ... 0xFEFF:   // reserved
#endif
		default:
			debug_printf ("illegal memory write at 0x%04x.\n", addr);
		break;
	}
#endif
	// debug_printf("\nwrote %02x to %04x\n", val, addr);
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

void bus_init(void)
{
	bus.wram_banksel = 1;
}

void bus_tick(void)
{
	int ticks = ((bus.key1 & KEY1_DOUBLE_SPEED) ? 2 : 1);
	for (int i = 0; i < ticks; i++)
	{
		gbc_cpu_tick();
		gbc_timer_tick();
		bus_oam_dma_tick();
	}
	gbc_apu_tick();
	gbc_ppu_tick();
}

void bus_stop_instr_cb(void)
{
	if (bus.key1 & KEY1_SWITCH_ARMED)
	{
		bus.key1 ^= KEY1_DOUBLE_SPEED;
		bus.key1 &= ~KEY1_SWITCH_ARMED;
	}
}

void bus_HBlank_cb(void)
{
	if (bus.vram_dma.active)
	{
		uint32_t num_stall_cycles;
		bus_dma_cpy(bus.vram_dma.dst, bus.vram_dma.src, 16);
		bus.vram_dma.src += 16;
		bus.vram_dma.dst += 16;
		bus.vram_dma.len -= 16;
		if (0 == bus.vram_dma.len)
		{
			bus.vram_dma.active = false;
		}
		num_stall_cycles = VRAM_DMA_CP_CYCLES << ((bus.key1 & KEY1_DOUBLE_SPEED) ? 1 : 0);
		gbc_cpu_stall(num_stall_cycles);
	}
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
