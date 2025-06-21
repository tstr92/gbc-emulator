
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

#include "emulator.h"
#include "bus.h"
#include "cpu.h"
#include "joypad.h"
#include "timer.h"
#include "serial.h"
#include "apu.h"
#include "ppu.h"
#include "debug.h"

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
	uint8_t ext_ram_banksel;
	uint16_t rom_banksel;
	bool ext_ram_enabled;

	uint8_t IF;
	uint8_t IE;
	uint8_t key1;

	bool dmg_mode;
	uint8_t cartridge_type;

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

#if (0 == BUILD_TEST_DLL)
	uint8_t wram[8][4*1024];
	uint8_t ext_ram[16][8*1024];
	uint8_t hram[127];
#else
	uint8_t memory[64 * 1024];
#endif
} bus_t;


#define CARTRIDGE_HEADER_ADDR 0x100
__attribute__((packed))
typedef struct
{
	uint32_t entry_point;                        // 0x100 - 0x103
	uint8_t nintendo_logo[48];                   // 0x104 - 0x133
	union
	{
		char title_0[16];                        // 0x134 - 0x143
		struct
		{
			union
			{
				char title_1[15];                // 0x134 - 0x142
				struct
				{
					char title_2[11];            // 0x134 - 0x13E
					char manufacturer_code[4];   // 0x13F - 0x142
				};
			};
			uint8_t GBC_flag;                    // 0x143
		};
	};
	char new_licensee_code[2];                   // 0x144 - 0x145 (only valid if old_licensee == 0x33)
	uint8_t SGB_flag;                            // 0x146
	uint8_t cartridge_type;                      // 0x147
	uint8_t rom_size;                            // 0x148 (ROM size is given by 32 KiB × (1 << <value>))
	uint8_t ram_size;                            // 0x149 (0: No RAM, 1: unused, 2: 8k, 3: 32k, 4: 128k, 5: 64k)
	uint8_t destination_code;                    // 0x14A (0x00: Japan, 0x01 Overseas only)
	uint8_t old_licensee_code;                   // 0x14B
	uint8_t mask_rom_version;                    // 0x14C
	uint8_t header_chksum;                       // 0x14D Checksum of 0x0134–0x014C (chksum -= (val + 1))
	uint16_t global_chksum;                      // 0x14E-0x14F Checksum of 0x0134–0x014C (sum of all rom-bytes except this chksum)
} cartridge_header_t;
#define CARTRIDGE_HEADER_SIZE sizeof(cartridge_header_t)

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
static uint8_t rom[512][16*1024];

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/
static int bus_check_cartridge_header(cartridge_header_t *pHeader);
static inline void bus_dma_cpy(uint16_t dst, uint16_t src, uint16_t len);
static void bus_oam_dma_tick(void);
static void bus_write_mbc(uint16_t addr, uint8_t val);
static void bus_write_mbc5(uint16_t addr, uint8_t val);

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
int bus_check_cartridge_header(cartridge_header_t *pHeader)
{
	int ret;
	uint8_t chksum;
	size_t rom_size;
	size_t ram_size;
	const size_t ram_sizes[] =
	{
		0, 0, 8192, 32768, 13172, 65536
	};

	chksum = 0;
	for (int i = 0x34; i <= 0x4C; i++)
	{
		chksum -= (((uint8_t *) pHeader)[i] + 1);
	}

	if (chksum != pHeader->header_chksum)
	{
		return 0;
	}

	if (5 < pHeader->ram_size)
	{
		return 0;
	}

	rom_size = (32768 *  (1 << pHeader->rom_size)) / 1024;
	ram_size = ram_sizes[pHeader->ram_size];

	debug_printf("        entry_point: %08x\n", pHeader->entry_point      );
	debug_printf("              title: '%s'\n", pHeader->title_0          );
	debug_printf("  new_licensee_code: '%s'\n", pHeader->new_licensee_code);
	debug_printf("           GBC_flag: %02x\n", pHeader->GBC_flag         );
	debug_printf("           SGB_flag: %02x\n", pHeader->SGB_flag         );
	debug_printf("     cartridge_type: %02x\n", pHeader->cartridge_type   );
	debug_printf("           rom_size: %dk\n" , rom_size                  );
	debug_printf("           ram_size: %d\n",   ram_size                  );
	debug_printf("   destination_code: %02x\n", pHeader->destination_code );
	debug_printf("  old_licensee_code: %02x\n", pHeader->old_licensee_code);
	debug_printf("   mask_rom_version: %02x\n", pHeader->mask_rom_version );
	debug_printf("      header_chksum: %02x\n", pHeader->header_chksum    );
	debug_printf("calc. header_chksum: %02x\n", chksum                    );
	debug_printf("      global_chksum: %04x\n", pHeader->global_chksum    );

	return 1;
}

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

static void bus_write_mbc(uint16_t addr, uint8_t val)
{
	switch (bus.cartridge_type)
	{
		case 0x19 ... 0x1E:
		{
			bus_write_mbc5(addr, val);
		}
		break;

		default: break;
	}

	return;
}

static void bus_write_mbc5(uint16_t addr, uint8_t val)
{
	switch (addr)
	{
		case 0x0000 ... 0x1FFF:
		{
			if (0x0A == (val & 0x0F))
			{
				bus.ext_ram_enabled = true;
			}
			else if (0x00 == (val & 0x0F))
			{
				bus.ext_ram_enabled = false;
			}
		}
		break;
		
		case 0x2000 ... 0x2FFF:
		{
			bus.rom_banksel &= 0xFF00;
			bus.rom_banksel |= val;
		}
		break;
		
		case 0x3000 ... 0x3FFF:
		{
			bus.rom_banksel &= 0x00FF;
			bus.rom_banksel |= (((uint16_t)val) & 0x01) << 8;
		}
		break;
		
		case 0x4000 ... 0x5FFF:
		{
			bus.ext_ram_banksel = (val & 0x0f);
		}
		break;

		default: break;
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
		{
			ret = rom[0][addr & 0x3FFF];
		}
		break;

		case 0x4000 ... 0x7FFF:   // Area for switch ROM Bank
		{
			ret = rom[bus.rom_banksel][addr & 0x3FFF];
		}
		break;

		case 0xA000 ... 0xBFFF:   // Area for switchable external RAM banks
		{
			if (bus.ext_ram_enabled)
			{
				ret = bus.ext_ram[bus.ext_ram_banksel][addr & 0x1FFF];
			}
			else
			{
				ret = 0;
			}
		}
		break;

		case 0xC000 ... 0xCFFF:   // Game Boy’s working RAM bank 0
		case 0xE000 ... 0xEFFF:   // Echo RAM
		{
			ret = bus.wram[0][addr & 0xFFF];
		}
		break;

		case 0xD000 ... 0xDFFF:   // Game Boy’s working RAM bank 1
		case 0xF000 ... 0xFDFF:   // Echo RAM
		{
			ret = bus.wram[bus.wram_banksel][addr & 0xFFF];
		}
		break;

		case DMA:
		{
			ret = bus.oam_dma.addr;
		}
		break;

		case 0xff4c:
		{
			printf("todo k0\n");
			ret = 0;
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
			ret = 0x02; // currently not receiving
		}
		break;

		case SVBK:
		{
			ret = bus.wram_banksel;
		}
		break;

		// case 0xE000 ... 0xFDFF:   // reserved
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
		case 0x4000 ... 0x7FFF:   // Area for switch ROM Bank
		{
			bus_write_mbc(addr, val);
		}
		break;

		case 0xA000 ... 0xBFFF:   // Area for switchable external RAM banks
		{
			if (bus.ext_ram_enabled)
			{
				bus.ext_ram[bus.ext_ram_banksel][addr & 0x1FFF] = val;
			}
		}
		break;

		case 0xC000 ... 0xCFFF:   // Game Boy’s working RAM bank 0
		case 0xE000 ... 0xEFFF:   // Echo RAM
		{
			bus.wram[0][addr & 0xFFF] = val;
		}
		break;

		case 0xD000 ... 0xDFFF:   // Game Boy’s working RAM bank 1
		case 0xF000 ... 0xFDFF:   // Echo RAM
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
			// printf("%04x\n", addr);
			/* "The four lower bits of this address will be ignored and treated as 0." */
			bus.vram_dma.src &= 0xFF00;
			bus.vram_dma.src |= val & 0xF0;
		}
		break;
		case 0xFF52: // VRAM DMA SRC High
		{
			// printf("%04x\n", addr);
			bus.vram_dma.src &= 0x00FF;
			bus.vram_dma.src |= ((uint16_t) val) << 8;
		}
		break;
		case 0xFF53: // VRAM DMA DST Low
		{
			// printf("%04x\n", addr);
			/* "The four lower bits of this address will be ignored and treated as 0." */
			bus.vram_dma.dst &= 0xFF00;
			bus.vram_dma.dst |= val & 0xF0;
		}
		break;
		case 0xFF54: // VRAM DMA DST High
		{
			// printf("%04x\n", addr);
			/* "Only bits 12-4 are respected; others are ignored." */
			bus.vram_dma.dst &= 0x00FF;
			bus.vram_dma.dst |= (((uint16_t) val & 0x1F) << 8) | 0x8000;
		}
		break;
		case 0xFF55: // VRAM DMA Length/Mode/Start
		{
			if (!bus.dmg_mode)
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
					bus.vram_dma.len = num_blocks * 16;
					if (val & VRAM_DMA_HBLANK_MSK)
					{
						bus.vram_dma.mode = HBlank_dma;
						bus.vram_dma.active = true;
					}
					else
					{
						uint32_t num_stall_cycles;
						num_stall_cycles = (VRAM_DMA_CP_CYCLES * num_blocks) << ((bus.key1 & KEY1_DOUBLE_SPEED) ? 1 : 0);
						bus.vram_dma.mode = general_purpose_dma;
						bus_dma_cpy(bus.vram_dma.dst, bus.vram_dma.src, bus.vram_dma.len);
						gbc_cpu_stall(num_stall_cycles);
						printf("DMA Go! %d Bytes %04x -> %04X\n", bus.vram_dma.len, bus.vram_dma.src, bus.vram_dma.dst);
						bus.vram_dma.active = false;
					}
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
			printf("todo Infrared\n");
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
		// case 0xE001 ... 0xFDFF:   // reserved
#else
		// case 0xE000 ... 0xFDFF:   // reserved
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
	cartridge_header_t header;

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
		if ((CARTRIDGE_HEADER_ADDR + CARTRIDGE_HEADER_SIZE) > fileSize)
		{
			printf("Error: File too small.\n");
			ret = false;
		}
	}

	if (false != ret)
	{
		fseek(gbFile, CARTRIDGE_HEADER_ADDR, SEEK_SET);
		fread(&header, 1, CARTRIDGE_HEADER_SIZE, gbFile);
		fseek(gbFile, 0, SEEK_SET);
		if (false == bus_check_cartridge_header(&header))
		{
			printf("Error: Header Checksum mismatch.\n");
			ret = false;
		}
		else
		{
			bus.cartridge_type = header.cartridge_type;
			bus.dmg_mode = (0 == (header.GBC_flag & 0x80));
		}
	}

	if (false != ret)
	{
		fread(rom, 1, fileSize, gbFile);
		fclose(gbFile);
	}

	return ret;
}


void gbc_bus_write_internal_state(void)
{
	emulator_cb_write_to_save_file((uint8_t*) &bus, sizeof(bus_t), "bus");
	return;
}


int gbc_bus_set_internal_state(void)
{
	return emulator_cb_read_from_save_file((uint8_t*) &bus, sizeof(bus_t));
}

void bus_init(void)
{
	bus.wram_banksel = 1;
	bus.rom_banksel = 1;
	bus.ext_ram_banksel = 0;
	bus.ext_ram_enabled = false;
}

void bus_tick(void)
{
	static uint8_t emulation_speed_cnt = 0;

	/* These parts might run at Gameboy Color Double Speed */
	int ticks = ((bus.key1 & KEY1_DOUBLE_SPEED) ? 2 : 1);
	for (int i = 0; i < ticks; i++)
	{
		gbc_cpu_tick();
		gbc_timer_tick();
		bus_oam_dma_tick();
	}

	/* PPU always runs at "normal speed" */
	gbc_ppu_tick();

	/* throttle APU to always run at 4MHz, even if emulator runs at higher speed
	 *  -> we stay at 60 fps
	 *  -> audio pitch stays correct
	 *  -> audio speeds up according to how quickly the game changes APU Registers
	 */
	if (emulation_speed_cnt < 10)
	{
		gbc_apu_tick();
	}

	emulation_speed_cnt++;
	if (emulation_speed_cnt >= emulator_get_speed())
	{
		emulation_speed_cnt = 0;
	}

	return;
}

bool bus_DMG_mode(void)
{
	return bus.dmg_mode;
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
		num_stall_cycles = (bus.key1 & KEY1_DOUBLE_SPEED) ? (2 * VRAM_DMA_CP_CYCLES) : VRAM_DMA_CP_CYCLES;
		gbc_cpu_stall(num_stall_cycles);
	}
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
