
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         SM83 Microcontroller                        *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: cpu.c                                                *
 *        author: tstr92                                               *
 *          date: 2024-04-09                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define FLAG_Z	(0x80)	// Zero
#define FLAG_N	(0x40)	// Negative
#define FLAG_H	(0x20)	// Half Carry
#define FLAG_C	(0x10)	// Carry

#define HIGH_NIBBLE(_uint8) ((_uint8 & 0xf0) >> 4)
#define LOW_NIBBLE(_uint8) ((_uint8 & 0x0f) >> 0)
#define HIGH_BYTE(_uint16) ((_uint16 & 0xff00) >> 8)
#define LOW_BYTE(_uint16) ((_uint16 & 0x00ff) >> 0)
#define IS_IN_RANGE(_val, _min, _max) ((_val >= _min) && (_val <= _max))

#define DBG_ERROR() printf("Error: %s:%d\n", __FUNCTION__, __LINE__)

#if (0 < DEBUG)
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...) do {} while (0)
#endif

/*---------------------------------------------------------------------*
 *  local data types                                                   *
 *---------------------------------------------------------------------*/
/* Memory Address    Purpose
 * 0x0000 - 0x3FFF   Permanently mapped ROM Bank
 * 0x4000 - 0x7FFF   Area for switch ROM Bank
 * 0x8000 - 0x9FFF   Video RAM
 * 0xA000 - 0xBFFF   Area for switchable external RAM banks
 * 0xC000 - 0xCFFF   Game Boy’s working RAM bank 0
 * 0xD000 - 0xDFFF   Game Boy’s working RAM bank 1
 * 0xE000 - 0xFDFF   reserved
 * 0xFE00 - 0xFE9F   Sprite Attribute Table
 * 0xFEA0 - 0xFEFF   reserved
 * 0xFF00 - 0xFF7F   Devices Mappings. Used to access I/O devices
 * 0xFF80 - 0xFFFE   High RAM Area
 * 0xFFFF            Interrupt Enable Register
 */

typedef struct
{
	union
	{
		struct
		{
			uint8_t f;
			uint8_t a;
		};
		uint16_t af;
	} af;
	union
	{
		struct
		{
			uint8_t c;
			uint8_t b;
		};
		uint16_t bc;
	} bc;
	union
	{
		struct
		{
			uint8_t e;
			uint8_t d;
		};
		uint16_t de;
	} de;
	union
	{
		struct
		{
			uint8_t l;
			uint8_t h;
		};
		uint16_t hl;
	} hl;
	uint16_t sp;	// stack pointer
	uint16_t pc;	// program counter
	
	// complete memory map
#if (0 < BUILD_TEST_DLL)
	uint8_t rom         [0x10000];
#else
	uint8_t rom         [0x4000];
	uint8_t switch_rom  [0x4000];
	uint8_t video_ram   [0x2000];
	uint8_t switch_ram  [0x2000];
	uint8_t int_ram0    [0x1000];
	uint8_t int_ram1    [0x1000];
	uint8_t reserved0   [0x1E00];
	uint8_t sprite_attr [0x00A0];
	uint8_t reserved1   [0x0060];
	uint8_t dev_map     [0x0080];
	uint8_t int_en      [0x0080];
#endif

	uint64_t cycle_cnt;
	uint64_t next_instruction;

	bool interrupts_enabled;
	bool stopped;
} sm83_t;

typedef enum
{
	OPC_NONE, OPC_NOP, OPC_STOP, OPC_HALT, OPC_EI, OPC_DI, OPC_DAA,
	OPC_CPL, OPC_SCF, OPC_CCF, OPC_RLCA, OPC_RLA, OPC_RRCA, OPC_RRA,
	OPC_CB, OPC_CALL, OPC_CAL2, OPC_JRc, OPC_JR, OPC_JPc, OPC_JP,
	OPC_JPHL, OPC_RETc, OPC_RET, OPC_RETI, OPC_RST, OPC_ADD, OPC_SUB,
	OPC_AND, OPC_OR, OPC_ADD2, OPC_SUB2, OPC_AND2, OPC_OR2, OPC_ADC,
	OPC_SBC, OPC_XOR, OPC_CP, OPC_ADC2, OPC_SBC2, OPC_XOR2, OPC_CP2,
	OPC_LD, OPC_LD2, OPC_LDd8, OPC_LDd82, OPC_LDa2r, OPC_LDr2a,
	OPC_LDd16, OPC_LD16s, OPC_LDHa8, OPC_LDHA, OPC_LDCA, OPC_LDAC,
	OPC_LDHLS, OPC_LDSHL, OPC_LD16A, OPC_LDA16, OPC_INC1, OPC_INC2,
	OPC_INC3, OPC_INC16, OPC_DEC1, OPC_DEC2, OPC_DEC3, OPC_DEC16,
	OPC_ADD16, OPC_ADDSP, OPC_POP, OPC_PUSH,
} opcode_t;

typedef enum
{
	OPC_RLC, OPC_RRC, OPC_RL, OPC_RR, OPC_SLA, OPC_SRA, OPC_SWAP,
	OPC_SRL, OPC_BIT, OPC_RES, OPC_SET, OPC_RLC2, OPC_RRC2,
	OPC_RL2, OPC_RR2, OPC_SLA2, OPC_SRA2, OPC_SWAP2, OPC_SRL2,
	OPC_BIT2, OPC_RES2, OPC_SET2,
} opcode2_t;

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
static const opcode_t opcode_types[256] = 
{
	/*        x0         x1         x2         x3         x4         x5         x6         x7         x8         x9         xA         xB         xC         xD         xE         xF   */
	/*0x*/ OPC_NOP  , OPC_LDd16, OPC_LDa2r, OPC_INC16, OPC_INC1 , OPC_DEC1 , OPC_LDd8 , OPC_RLCA , OPC_LD16s, OPC_ADD16, OPC_LDr2a, OPC_DEC16, OPC_INC2 , OPC_DEC2 , OPC_LDd8 , OPC_RRCA ,
	/*1x*/ OPC_STOP , OPC_LDd16, OPC_LDa2r, OPC_INC16, OPC_INC1 , OPC_DEC1 , OPC_LDd8 , OPC_RLA  , OPC_JR   , OPC_ADD16, OPC_LDr2a, OPC_DEC16, OPC_INC2 , OPC_DEC2 , OPC_LDd8 , OPC_RRA  ,
	/*2x*/ OPC_JRc  , OPC_LDd16, OPC_LDa2r, OPC_INC16, OPC_INC1 , OPC_DEC1 , OPC_LDd8 , OPC_DAA  , OPC_JRc  , OPC_ADD16, OPC_LDr2a, OPC_DEC16, OPC_INC2 , OPC_DEC2 , OPC_LDd8 , OPC_CPL  ,
	/*3x*/ OPC_JRc  , OPC_LDd16, OPC_LDa2r, OPC_INC16, OPC_INC3 , OPC_DEC3 , OPC_LDd82, OPC_SCF  , OPC_JRc  , OPC_ADD16, OPC_LDr2a, OPC_DEC16, OPC_INC2 , OPC_DEC2 , OPC_LDd8 , OPC_CCF  ,
	/*4x*/ OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   ,
	/*5x*/ OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   ,
	/*6x*/ OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   ,
	/*7x*/ OPC_LD2  , OPC_LD2  , OPC_LD2  , OPC_LD2  , OPC_LD2  , OPC_LD2  , OPC_HALT , OPC_LD2  , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   , OPC_LD   ,
	/*8x*/ OPC_ADD  , OPC_ADD  , OPC_ADD  , OPC_ADD  , OPC_ADD  , OPC_ADD  , OPC_ADD  , OPC_ADD  , OPC_ADC  , OPC_ADC  , OPC_ADC  , OPC_ADC  , OPC_ADC  , OPC_ADC  , OPC_ADC  , OPC_ADC  ,
	/*9x*/ OPC_SUB  , OPC_SUB  , OPC_SUB  , OPC_SUB  , OPC_SUB  , OPC_SUB  , OPC_SUB  , OPC_SUB  , OPC_SBC  , OPC_SBC  , OPC_SBC  , OPC_SBC  , OPC_SBC  , OPC_SBC  , OPC_SBC  , OPC_SBC  ,
	/*Ax*/ OPC_AND  , OPC_AND  , OPC_AND  , OPC_AND  , OPC_AND  , OPC_AND  , OPC_AND  , OPC_AND  , OPC_XOR  , OPC_XOR  , OPC_XOR  , OPC_XOR  , OPC_XOR  , OPC_XOR  , OPC_XOR  , OPC_XOR  ,
	/*Bx*/ OPC_OR   , OPC_OR   , OPC_OR   , OPC_OR   , OPC_OR   , OPC_OR   , OPC_OR   , OPC_OR   , OPC_CP   , OPC_CP   , OPC_CP   , OPC_CP   , OPC_CP   , OPC_CP   , OPC_CP   , OPC_CP   ,
	/*Cx*/ OPC_RETc , OPC_POP  , OPC_JPc  , OPC_JP   , OPC_CALL , OPC_PUSH , OPC_ADD2 , OPC_RST  , OPC_RETc , OPC_RET  , OPC_JPc  , OPC_CB   , OPC_CALL , OPC_CAL2 , OPC_ADC2 , OPC_RST  ,
	/*Dx*/ OPC_RETc , OPC_POP  , OPC_JPc  , OPC_NONE , OPC_CALL , OPC_PUSH , OPC_SUB2 , OPC_RST  , OPC_RETc , OPC_RETI , OPC_JPc  , OPC_NONE , OPC_CALL , OPC_NONE , OPC_SBC2 , OPC_RST  ,
	/*Ex*/ OPC_LDHa8, OPC_POP  , OPC_LDCA , OPC_NONE , OPC_NONE , OPC_PUSH , OPC_AND2 , OPC_RST  , OPC_ADDSP, OPC_JPHL , OPC_LD16A, OPC_NONE , OPC_NONE , OPC_NONE , OPC_XOR2 , OPC_RST  ,
	/*Fx*/ OPC_LDHA , OPC_POP  , OPC_LDAC , OPC_DI   , OPC_NONE , OPC_PUSH , OPC_OR2  , OPC_RST  , OPC_LDHLS, OPC_LDSHL, OPC_LDA16, OPC_EI   , OPC_NONE , OPC_NONE , OPC_CP2  , OPC_RST  ,
};

static const opcode2_t opcode_types2[256] = 
{
	/*        x0         x1         x2         x3         x4         x5         x6         x7         x8         x9         xA         xB         xC         xD         xE         xF    */
	/*0x*/ OPC_RLC  , OPC_RLC  , OPC_RLC  , OPC_RLC  , OPC_RLC  , OPC_RLC  , OPC_RLC2 , OPC_RLC  , OPC_RRC  , OPC_RRC  , OPC_RRC  , OPC_RRC  , OPC_RRC  , OPC_RRC  , OPC_RRC2 , OPC_RRC  ,
	/*1x*/ OPC_RL   , OPC_RL   , OPC_RL   , OPC_RL   , OPC_RL   , OPC_RL   , OPC_RL2  , OPC_RL   , OPC_RR   , OPC_RR   , OPC_RR   , OPC_RR   , OPC_RR   , OPC_RR   , OPC_RR2  , OPC_RR   ,
	/*2x*/ OPC_SLA  , OPC_SLA  , OPC_SLA  , OPC_SLA  , OPC_SLA  , OPC_SLA  , OPC_SLA2 , OPC_SLA  , OPC_SRA  , OPC_SRA  , OPC_SRA  , OPC_SRA  , OPC_SRA  , OPC_SRA  , OPC_SRA2 , OPC_SRA  ,
	/*3x*/ OPC_SWAP , OPC_SWAP , OPC_SWAP , OPC_SWAP , OPC_SWAP , OPC_SWAP , OPC_SWAP2, OPC_SWAP , OPC_SRL  , OPC_SRL  , OPC_SRL  , OPC_SRL  , OPC_SRL  , OPC_SRL  , OPC_SRL2 , OPC_SRL  ,
	/*4x*/ OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT2 , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT2 , OPC_BIT  ,
	/*5x*/ OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT2 , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT2 , OPC_BIT  ,
	/*6x*/ OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT2 , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT2 , OPC_BIT  ,
	/*7x*/ OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT2 , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT  , OPC_BIT2 , OPC_BIT  ,
	/*8x*/ OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES2 , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES2 , OPC_RES  ,
	/*9x*/ OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES2 , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES2 , OPC_RES  ,
	/*Ax*/ OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES2 , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES2 , OPC_RES  ,
	/*Bx*/ OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES2 , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES  , OPC_RES2 , OPC_RES  ,
	/*Cx*/ OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET2 , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET2 , OPC_SET  ,
	/*Dx*/ OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET2 , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET2 , OPC_SET  ,
	/*Ex*/ OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET2 , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET2 , OPC_SET  ,
	/*Fx*/ OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET2 , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET  , OPC_SET2 , OPC_SET  ,
};

static sm83_t cpu;

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
uint8_t cpu_get_memory(uint16_t addr)
{
	uint8_t ret = 0;

#if (0 < BUILD_TEST_DLL)
	ret = ((uint8_t *) &cpu.rom[0])[addr];
#else
	if (((addr < 0xFEA0 ) || (addr > 0xFEFF)) &&
	    ((addr < 0xE000 ) || (addr > 0xFDFF)))
	{
		ret = ((uint8_t *) &cpu.rom[0])[addr];
	}
	else
	{
		debug_printf ("illegal memory access at 0x%04x.\n", addr);
	}
#endif

	return ret;
}

void cpu_set_memory(uint16_t addr, uint8_t val)
{
#if (0 < BUILD_TEST_DLL)
	((uint8_t *) &cpu.rom[0])[addr] = val;
#else
	if (((addr < 0xFEA0 ) || (addr > 0xFEFF)) &&
	    ((addr < 0xE000 ) || (addr > 0xFDFF)))
	{
		((uint8_t *) &cpu.rom[0])[addr] = val;
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

void cpu_init(void)
{
	memset(&cpu, 0, sizeof(cpu));
}

void cpu_isr_handled(void)
{
	#warning what to do here?
}

void eval_Z_flag(uint8_t reg)
{
	if (0 == reg)
	{
		cpu.af.f |= FLAG_Z;
	}
	else
	{
		cpu.af.f &= ~FLAG_Z;
	}
}

void set_Z_flag(bool zero)
{
	if (zero)
	{
		cpu.af.f |= FLAG_Z;
	}
	else
	{
		cpu.af.f &= ~FLAG_Z;
	}
}

void set_N_flag(bool subtract)
{
	if (subtract)
	{
		cpu.af.f |= FLAG_N;
	}
	else
	{
		cpu.af.f &= ~FLAG_N;
	}
}

void eval_H_flag_c(uint8_t target, uint8_t operand, bool sub, uint8_t carry)
{
	bool h;
	uint8_t temp = target + operand;
	if (!sub)
	{
		h = ((target & 0xF) + (operand & 0xF) + carry) > 0xF;
	}
	else
	{
		h = ((int)((target & 0xf) - (operand & 0xf) - carry) < 0);
	}

	if (h)
	{
		cpu.af.f |= FLAG_H;
	}
	else
	{
		cpu.af.f &= ~FLAG_H;
	}

	return;
}

void eval_H_flag(uint8_t target, uint8_t operand, bool sub)
{
	eval_H_flag_c(target, operand, sub, 0);
}

void eval_H_flag_16(uint16_t target, uint16_t operand)
{
	// bool h;
	// uint16_t temp = target + operand;
	// if (operand >= 0)
	// {
	// 	h = ((target & 0xFFF) + (operand & 0xFFF)) > 0xFFF;
	// }
	// else
	// {
	// 	h = (temp & 0xFFF) > (target & 0xFFF);
	// }

	if (((target & 0xFFF) + (operand & 0xFFF)) > 0xFFF)
	{
		cpu.af.f |= FLAG_H;
	}
	else
	{
		cpu.af.f &= ~FLAG_H;
	}

	return;
}

void set_H_flag(bool h)
{
	if (h)
	{
		cpu.af.f |= FLAG_H;
	}
	else
	{
		cpu.af.f &= ~FLAG_H;
	}
}

void eval_C_flag_c(uint8_t target, uint8_t operand, bool sub, uint8_t carry)
{
	bool c;
	if (!sub)
	{
		c = ((uint16_t)(target + operand + carry)) > 0xFF;
	}
	else
	{
		c = ((int) (target - operand - carry) < 0);
	}

	if (c)
	{
		cpu.af.f |= FLAG_C;
	}
	else
	{
		cpu.af.f &= ~FLAG_C;
	}

	return;
}

void eval_C_flag(uint8_t target, uint8_t operand, bool sub)
{
	eval_C_flag_c(target, operand, sub, 0);
}

void eval_C_flag_16(uint16_t target, uint16_t operand)
{
	if ((((uint32_t) target) + ((uint32_t) operand)) > 0xFFFF)
	{
		cpu.af.f |= FLAG_C;
	}
	else
	{
		cpu.af.f &= ~FLAG_C;
	}

	return;
}

void set_C_flag(bool c)
{
	if (c)
	{
		cpu.af.f |= FLAG_C;
	}
	else
	{
		cpu.af.f &= ~FLAG_C;
	}
}

void cpu_handle_opcode(void)
{
	uint8_t opcode;
	opcode_t opcode_type;

	opcode = cpu_get_memory(cpu.pc);
	opcode_type = opcode_types[opcode];

	switch (opcode_type)
	{
	case OPC_NONE:
	{
		DBG_ERROR();
	}
	break;

	case OPC_NOP:
	{
		cpu.next_instruction += 4;
		cpu.pc++;
	}
	break;
	
	case OPC_STOP:
	{
		cpu.stopped = true;
		cpu.next_instruction += 4;
		cpu.pc += 2;
	}
	break;
	
	case OPC_HALT:
	{
		cpu.next_instruction += 4;
		cpu.pc++;
	}
	break;
	
	case OPC_EI:
	{
		cpu.interrupts_enabled = true;
		cpu.next_instruction += 4;
		cpu.pc++;
	}
	break;
	
	case OPC_DI:
	{
		cpu.interrupts_enabled = false;
		cpu.next_instruction += 4;
		cpu.pc++;
	}
	break;
	
	case OPC_DAA:
	{
		uint8_t a_reg = cpu.af.a;
		bool N = (0 != (cpu.af.f & FLAG_N));
		bool C = (0 != (cpu.af.f & FLAG_C));
		bool H = (0 != (cpu.af.f & FLAG_H));
		bool new_C = C;
		if (!N)
		{  // after an addition, adjust if (half-)carry occurred or if result is out of bounds
			if (C || (a_reg > 0x99))
			{
				a_reg += 0x60;
				new_C = true;
			}
			if (H || ((a_reg & 0x0f) > 0x09))
			{
				a_reg += 0x6;
			}
		} 
		else
		{  // after a subtraction, only adjust if (half-)carry occurred
			if (C)
			{
				a_reg -= 0x60;
			}
			if (H)
			{
				a_reg -= 0x6;
			}
		}
		set_C_flag(new_C);
		set_H_flag(false);
		eval_Z_flag(a_reg);
		cpu.af.a = a_reg;
		cpu.next_instruction += 4;
		cpu.pc++;
	}
	break;
	
	case OPC_CPL:
	{
		set_N_flag(true);
		set_H_flag(true);
		cpu.af.a = ~cpu.af.a;
		cpu.next_instruction += 4;
		cpu.pc++;
	}
	break;
	
	case OPC_SCF:
	{
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(true);
		cpu.next_instruction += 4;
		cpu.pc++;
	}
	break;
	
	case OPC_CCF:
	{
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag((0 == (cpu.af.f & FLAG_C)));
		cpu.next_instruction += 4;
		cpu.pc++;
	}
	break;

	case OPC_RLCA:
	{
		bool bit7 = (0 != (cpu.af.a & (1<<7)));
		cpu.af.a = (cpu.af.a << 1) | (bit7 ? 0x1 : 0);
		set_Z_flag(false);
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(bit7);
		cpu.next_instruction += 4;
		cpu.pc += 1;
	}
	break;

	case OPC_RLA:
	{
		bool bit7 = (0 != (cpu.af.a & (1<<7)));
		bool c = (0 != (cpu.af.f & FLAG_C));
		cpu.af.a = (cpu.af.a << 1) | (c ? 0x1 : 0);
		set_Z_flag(false);
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(bit7);
		cpu.next_instruction += 4;
		cpu.pc += 1;
	}
	break;

	case OPC_RRCA:
	{
		bool bit0 = (0 != (cpu.af.a & (1<<0)));
		cpu.af.a = (cpu.af.a >> 1) | (bit0 ? 0x80 : 0);
		set_Z_flag(false);
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(bit0);
		cpu.next_instruction += 4;
		cpu.pc += 1;
	}
	break;

	case OPC_RRA:
	{
		bool bit0 = (0 != (cpu.af.a & (1<<0)));
		bool c = (0 != (cpu.af.f & FLAG_C));
		cpu.af.a = (cpu.af.a >> 1) | (c ? 0x80 : 0);
		set_Z_flag(false);
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(bit0);
		cpu.next_instruction += 4;
		cpu.pc += 1;
	}
	break;

	case OPC_CB:
	{
		uint8_t opcode2 = cpu_get_memory(cpu.pc + 1);;
		opcode2_t opcode_type2 = opcode_types2[opcode2];
		uint8_t *target_lut[8] = {
			&cpu.bc.b, &cpu.bc.c, &cpu.de.d, &cpu.de.e,
			&cpu.hl.h, &cpu.hl.l, NULL     , &cpu.af.a
		};
		uint8_t i = opcode2 & 0x07;
		uint8_t *target_p = target_lut[i];
		uint8_t duration = (i == 6) ? 16 : 8;

		switch (opcode_type2)
		{
		case OPC_RLC:
		{
			bool bit7 = (0 != (*target_p & (1<<7)));
			*target_p = (*target_p << 1) | (bit7 ? 1 : 0);
			eval_Z_flag(*target_p);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit7);
		}
		break;
		
		case OPC_RRC:
		{
			bool bit0 = (0 != (*target_p & (1<<0)));
			*target_p = (*target_p >> 1)| (bit0 ? 0x80 : 0);
			eval_Z_flag(*target_p);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_RL:
		{
			bool bit7 = (0 != (*target_p & (1<<7)));
			bool c = (0 != (cpu.af.f & FLAG_C));
			*target_p = (*target_p << 1) | (c ? 0x1 : 0);
			eval_Z_flag(*target_p);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit7);
		}
		break;
		
		case OPC_RR:
		{
			bool bit0 = (0 != (*target_p & (1<<0)));
			bool c = (0 != (cpu.af.f & FLAG_C));
			*target_p = (*target_p >> 1) | (c ? 0x80 : 0);
			eval_Z_flag(*target_p);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_SLA:
		{
			bool bit7 = (0 != (*target_p & (1<<7)));
			*target_p = (*target_p << 1);
			eval_Z_flag(*target_p);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit7);
		}
		break;
		
		case OPC_SRA:
		{
			bool bit0 = (0 != (*target_p & (1<<0)));
			bool bit7 = (0 != (*target_p & (1<<7)));
			*target_p = (*target_p >> 1) | (bit7 ? 0x80 : 0);
			eval_Z_flag(*target_p);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_SWAP:
		{
			*target_p = (LOW_NIBBLE(*target_p) << 4) | HIGH_NIBBLE(*target_p);
			eval_Z_flag(*target_p);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(false);
		}
		break;
		
		case OPC_SRL:
		{
			bool bit0 = (0 != (*target_p & (1<<0)));
			*target_p = (*target_p >> 1);
			eval_Z_flag(*target_p);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_BIT:
		{
			uint8_t bit = (opcode2 & 0x38) >> 3;
			eval_Z_flag((*target_p & (1 << bit)));
			set_N_flag(false);
			set_H_flag(true);
		}
		break;
		
		case OPC_RES:
		{
			uint8_t bit = (opcode2 & 0x38) >> 3;
			*target_p &= ~(1 << bit);
			// no flags affected
		}
		break;
		
		case OPC_SET:
		{
			uint8_t bit = (opcode2 & 0x38) >> 3;
			*target_p |= (1 << bit);
			// no flags affected
		}
		break;
		
		case OPC_RLC2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			bool bit7 = (0 != (operand & (1<<7)));
			uint8_t result = (operand << 1) | (bit7 ? 1 : 0);
			cpu_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit7);
		}
		break;
		
		case OPC_RRC2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			bool bit0 = (0 != (operand & (1<<0)));
			uint8_t result = (operand >> 1)| (bit0 ? 0x80 : 0);
			cpu_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_RL2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			bool bit7 = (0 != (operand & (1<<7)));
			bool c = (0 != (cpu.af.f & FLAG_C));
			uint8_t result = (operand << 1) | (c ? 0x1 : 0);
			cpu_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit7);
		}
		break;
		
		case OPC_RR2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			bool bit0 = (0 != (operand & (1<<0)));
			bool c = (0 != (cpu.af.f & FLAG_C));
			uint8_t result = (operand >> 1) | (c ? 0x80 : 0);
			cpu_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_SLA2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			bool bit7 = (0 != (operand & (1<<7)));
			uint8_t result = (operand << 1);
			cpu_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit7);
		}
		break;
		
		case OPC_SRA2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			bool bit0 = (0 != (operand & (1<<0)));
			bool bit7 = (0 != (operand & (1<<7)));
			uint8_t result = (operand >> 1) | (bit7 ? 0x80 : 0);
			cpu_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_SWAP2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			uint8_t result = (LOW_NIBBLE(operand) << 4) | HIGH_NIBBLE(operand);
			cpu_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(false);
		}
		break;
		
		case OPC_SRL2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			bool bit0 = (0 != (operand & (1<<0)));
			uint8_t result = (operand >> 1);
			cpu_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_BIT2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			uint8_t bit = (opcode2 & 0x38) >> 3;
			eval_Z_flag((operand & (1 << bit)));
			set_N_flag(false);
			set_H_flag(true);
		}
		break;
		
		case OPC_RES2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			uint8_t bit = (opcode2 & 0x38) >> 3;
			uint8_t result = (operand & ~(1 << bit));
			cpu_set_memory(cpu.hl.hl, result);
			// no flags affected
		}
		break;
		
		case OPC_SET2:
		{
			uint8_t operand = cpu_get_memory(cpu.hl.hl);
			uint8_t bit = (opcode2 & 0x38) >> 3;
			uint8_t result = (operand | (1 << bit));
			cpu_set_memory(cpu.hl.hl, result);
			// no flags affected
		}
		break;
		default: DBG_ERROR(); break;
		}

		cpu.next_instruction += duration;
		cpu.pc += 2;
	}
	break;

	case OPC_CALL:
	{
		bool cond_lut[4] = {
			(0 == (cpu.af.f & FLAG_Z)), (0 != (cpu.af.f & FLAG_Z)),
			(0 == (cpu.af.f & FLAG_C)), (0 != (cpu.af.f & FLAG_C)),
		};
		uint8_t cond = ((opcode & 0x38) >> 3);
		bool call_taken = cond_lut[cond];

		if (call_taken)
		{
			uint8_t hi, lo;
			uint16_t next_pc = cpu.pc + 3;
			lo = cpu_get_memory(cpu.pc + 1);
			hi = cpu_get_memory(cpu.pc + 2);
			cpu_set_memory(--cpu.sp, HIGH_BYTE(next_pc));
			cpu_set_memory(--cpu.sp, LOW_BYTE(next_pc));
			cpu.pc = ((uint16_t)hi << 8) | lo;
			cpu.next_instruction += 24;
		}
		else
		{
			cpu.next_instruction += 12;
			cpu.pc += 3;
		}
	}
	break;

	case OPC_CAL2:
	{
		uint8_t hi, lo;
		uint16_t next_pc = cpu.pc + 3;
		lo = cpu_get_memory(cpu.pc + 1);
		hi = cpu_get_memory(cpu.pc + 2);
		cpu_set_memory(--cpu.sp, HIGH_BYTE(next_pc));
		cpu_set_memory(--cpu.sp, LOW_BYTE(next_pc));
		cpu.pc = ((uint16_t)hi << 8) | lo;
		cpu.next_instruction += 24;
	}
	break;

	case OPC_JRc:
	{
		bool cond_lut[4] = {
			(0 == (cpu.af.f & FLAG_Z)), (0 != (cpu.af.f & FLAG_Z)),
			(0 == (cpu.af.f & FLAG_C)), (0 != (cpu.af.f & FLAG_C)),
		};
		uint8_t cond = ((opcode & 0x38) >> 3) - 4;
		bool jump_taken = cond_lut[cond];

		if (jump_taken)
		{
			int8_t offset = (int8_t) cpu_get_memory(cpu.pc + 1);
			cpu.pc += (offset + 2);
			cpu.next_instruction += 12;
		}
		else
		{
			cpu.next_instruction += 8;
			cpu.pc += 2;
		}
	}
	break;

	case OPC_JR:
	{
		int8_t offset = (int8_t) cpu_get_memory(cpu.pc + 1);
		cpu.pc += (offset + 2);
		cpu.next_instruction += 12;
	}
	break;

	case OPC_JPc:
	{
		bool cond_lut[4] = {
			(0 == (cpu.af.f & FLAG_Z)), (0 != (cpu.af.f & FLAG_Z)),
			(0 == (cpu.af.f & FLAG_C)), (0 != (cpu.af.f & FLAG_C)),
		};
		uint8_t cond = ((opcode & 0x38) >> 3);
		bool jump_taken = cond_lut[cond];

		if (jump_taken)
		{
			uint8_t hi, lo;
			lo = cpu_get_memory(cpu.pc + 1);
			hi = cpu_get_memory(cpu.pc + 2);
			cpu.pc = ((uint16_t)hi << 8) | lo;
			cpu.next_instruction += 16;
		}
		else
		{
			cpu.next_instruction += 12;
			cpu.pc += 3;
		}
	}
	break;

	case OPC_JP:
	{
		uint8_t hi, lo;
		lo = cpu_get_memory(cpu.pc + 1);
		hi = cpu_get_memory(cpu.pc + 2);
		cpu.pc = ((uint16_t)hi << 8) | lo;
		cpu.next_instruction += 16;
	}
	break;

	case OPC_JPHL:
	{
		cpu.pc = cpu.hl.hl;
		cpu.next_instruction += 4;
	}
	break;

	case OPC_RETc:
	{
		bool cond_lut[4] = {
			(0 == (cpu.af.f & FLAG_Z)), (0 != (cpu.af.f & FLAG_Z)),
			(0 == (cpu.af.f & FLAG_C)), (0 != (cpu.af.f & FLAG_C)),
		};
		uint8_t cond = (opcode & 0x38) >> 3;
		bool return_taken = cond_lut[cond];
		
		if (return_taken)
		{
			uint8_t lo, hi;
			lo = cpu_get_memory(cpu.sp++);
			hi = cpu_get_memory(cpu.sp++);
			cpu.pc = ((uint16_t)hi << 8) | lo;
			cpu.next_instruction += 20;
		}
		else
		{
			cpu.next_instruction += 8;
			cpu.pc += 1;
		}
	}
	break;

	case OPC_RETI:
		cpu_isr_handled();
		/* fall through */
	case OPC_RET:
	{
		uint8_t lo, hi;
		lo = cpu_get_memory(cpu.sp++);
		hi = cpu_get_memory(cpu.sp++);
		cpu.pc = ((uint16_t)hi << 8) | lo;
		cpu.next_instruction += 16;
	}
	break;

	case OPC_RST:
	{
		uint8_t offset_lut[8] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};
		uint8_t t = (opcode & 0x38) >> 3;
		uint8_t offset = offset_lut[t];

		cpu_set_memory(--cpu.sp, (((cpu.pc + 1) & 0xFF00) >> 8));
		cpu_set_memory(--cpu.sp, (((cpu.pc + 1) & 0x00FF) >> 0));

		cpu.pc = offset;

		cpu.next_instruction += 16;
	}
	break;

	case OPC_ADD:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l, cpu_get_memory(cpu.hl.hl), cpu.af.a,
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t result = cpu.af.a + operand;
		eval_Z_flag(result);
		set_N_flag(false);
		eval_H_flag(cpu.af.a, operand, false);
		eval_C_flag(cpu.af.a, operand, false);
		cpu.af.a = result;
		cpu.next_instruction += duration;
		cpu.pc += 1;
	}
	break;

	case OPC_SUB:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l, cpu_get_memory(cpu.hl.hl), cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t result = cpu.af.a - operand;
		eval_Z_flag(result);
		set_N_flag(true);
		eval_H_flag(cpu.af.a, operand, true);
		eval_C_flag(cpu.af.a, operand, true);
		cpu.af.a = result;
		cpu.next_instruction += duration;
		cpu.pc += 1;
	}
	break;

	case OPC_AND:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l, cpu_get_memory(cpu.hl.hl), cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		cpu.af.a = cpu.af.a & operand;
		eval_Z_flag(cpu.af.a);
		set_N_flag(false);
		set_H_flag(true);
		set_C_flag(false);
		cpu.next_instruction += duration;
		cpu.pc += 1;
	}
	break;

	case OPC_OR:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l, cpu_get_memory(cpu.hl.hl), cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		cpu.af.a = cpu.af.a | operand;
		eval_Z_flag(cpu.af.a);
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(false);
		cpu.next_instruction += duration;
		cpu.pc += 1;
	}
	break;

	case OPC_ADD2:
	{
		uint8_t operand = cpu_get_memory(cpu.pc + 1);
		eval_C_flag(cpu.af.a, operand, false);
		eval_H_flag(cpu.af.a, operand, false);
		set_N_flag(false);
		cpu.af.a += operand;
		eval_Z_flag(cpu.af.a);
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_SUB2:
	{
		uint8_t operand = cpu_get_memory(cpu.pc + 1);
		eval_C_flag(cpu.af.a, operand, true);
		eval_H_flag(cpu.af.a, operand, true);
		set_N_flag(true);
		cpu.af.a -= operand;
		eval_Z_flag(cpu.af.a);
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_AND2:
	{
		uint8_t operand = cpu_get_memory(cpu.pc + 1);
		set_C_flag(false);
		set_H_flag(true);
		set_N_flag(false);
		cpu.af.a = cpu.af.a & operand;
		eval_Z_flag(cpu.af.a);
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_OR2:
	{
		uint8_t operand = cpu_get_memory(cpu.pc + 1);
		set_C_flag(false);
		set_H_flag(false);
		set_N_flag(false);
		cpu.af.a = cpu.af.a | operand;
		eval_Z_flag(cpu.af.a);
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_ADC:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l, cpu_get_memory(cpu.hl.hl), cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t c = (0 != (cpu.af.f & FLAG_C)) ? 1 : 0;
		uint8_t result = cpu.af.a + operand + c;
		eval_Z_flag(result);
		set_N_flag(false);
		eval_H_flag_c(cpu.af.a, operand, false, c);
		eval_C_flag_c(cpu.af.a, operand, false, c);
		cpu.af.a = result;
		cpu.next_instruction += duration;
		cpu.pc += 1;
	}
	break;

	case OPC_SBC:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l, cpu_get_memory(cpu.hl.hl), cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t c = (0 != (cpu.af.f & FLAG_C)) ? 1 : 0;
		uint8_t result = cpu.af.a - operand - c;
		eval_Z_flag(result);
		set_N_flag(true);
		eval_H_flag_c(cpu.af.a, operand, true, c);
		eval_C_flag_c(cpu.af.a, operand, true, c);
		cpu.af.a = result;
		cpu.next_instruction += duration;
		cpu.pc += 1;
	}
	break;

	case OPC_XOR:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l, cpu_get_memory(cpu.hl.hl), cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		cpu.af.a = cpu.af.a ^ operand;
		eval_Z_flag(cpu.af.a);
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(false);
		cpu.next_instruction += duration;
		cpu.pc += 1;
	}
	break;

	case OPC_CP:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l, cpu_get_memory(cpu.hl.hl), cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t result = cpu.af.a - operand;
		eval_Z_flag(result);
		set_N_flag(true);
		eval_H_flag(cpu.af.a, operand, true);
		eval_C_flag(cpu.af.a, operand, true);
		eval_H_flag(cpu.af.a, operand, true);
		eval_C_flag(cpu.af.a, operand, true);
		cpu.next_instruction += duration;
		cpu.pc += 1;
	}
	break;

	case OPC_ADC2:
	{
		uint8_t operand = cpu_get_memory(cpu.pc + 1);
		uint8_t c = (cpu.af.f & FLAG_C) ? 1: 0;
		eval_C_flag_c(cpu.af.a, operand, false, c);
		eval_H_flag_c(cpu.af.a, operand, false, c);
		set_N_flag(false);
		cpu.af.a += operand + c;
		eval_Z_flag(cpu.af.a);
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_SBC2:
	{
		uint8_t operand = cpu_get_memory(cpu.pc + 1);
		uint8_t c = (cpu.af.f & FLAG_C) ? 1: 0;
		eval_C_flag_c(cpu.af.a, operand, true, c);
		eval_H_flag_c(cpu.af.a, operand, true, c);
		set_N_flag(true);
		cpu.af.a -= (operand + c);
		eval_Z_flag(cpu.af.a);
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_XOR2:
	{
		uint8_t operand = cpu_get_memory(cpu.pc + 1);
		uint8_t c = (cpu.af.f & FLAG_C) ? 1: 0;
		set_C_flag(false);
		set_H_flag(false);
		set_N_flag(false);
		cpu.af.a = cpu.af.a ^ operand;
		eval_Z_flag(cpu.af.a);
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_CP2:
	{
		uint8_t operand = cpu_get_memory(cpu.pc + 1);
		uint8_t c = (cpu.af.f & FLAG_C) ? 1: 0;
		eval_C_flag(cpu.af.a, operand, true);
		eval_H_flag(cpu.af.a, operand, true);
		set_N_flag(true);
		eval_Z_flag(cpu.af.a - operand);
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_LD:
	{
		uint8_t src_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e,
			cpu.hl.h, cpu.hl.l, cpu_get_memory(cpu.hl.hl), cpu.af.a
		};
		uint8_t *dst_lut[8] = {
			&cpu.bc.b, &cpu.bc.c, &cpu.de.d, &cpu.de.e,
			&cpu.hl.h, &cpu.hl.l, NULL     , &cpu.af.a
		};
		uint8_t r0 = (opcode & 0x38) >> 3;
		uint8_t r1 = (opcode & 0x07) >> 0;
		uint8_t src = src_lut[r1];
		uint8_t *dst = dst_lut[r0];
		uint8_t duration = ((6 == r0) || (6 == r1)) ? 8 : 4;
		*dst = src;
		cpu.next_instruction += duration;
		cpu.pc += 1;
	}
	break;

	case OPC_LD2:
	{
		uint8_t src_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e,
			cpu.hl.h, cpu.hl.l, 0       , cpu.af.a
		};
		uint8_t r1 = (opcode & 0x07) >> 0;
		uint8_t src = src_lut[r1];
		cpu_set_memory(cpu.hl.hl, src);
		cpu.next_instruction += 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LDd8:
	{
		uint8_t data = cpu_get_memory(cpu.pc + 1);
		uint8_t *dst_lut[8] = {
			&cpu.bc.b, &cpu.bc.c, &cpu.de.d, &cpu.de.e,
			&cpu.hl.h, &cpu.hl.l, NULL     , &cpu.af.a
		};
		uint8_t r = (opcode & 0x38) >> 3;
		uint8_t *dst = dst_lut[r];
		*dst = data;
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_LDd82:
	{
		uint8_t data = cpu_get_memory(cpu.pc + 1);
		uint8_t r = (opcode & 0x38) >> 3;
		cpu_set_memory(cpu.hl.hl, data);
		cpu.next_instruction += 8;
		cpu.pc += 2;
	}
	break;

	case OPC_LDa2r:
	{
		uint8_t mux = (opcode & 0x30) >> 4;
		switch (mux)
		{
		case 0: cpu_set_memory(cpu.bc.bc, cpu.af.a); break;
		case 1: cpu_set_memory(cpu.de.de, cpu.af.a); break;
		case 2: cpu_set_memory(cpu.hl.hl++, cpu.af.a); break;
		case 3: cpu_set_memory(cpu.hl.hl--, cpu.af.a); break;
		default: DBG_ERROR(); break;
		}
		cpu.next_instruction += 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LDr2a:
	{
		uint8_t src;
		uint8_t mux = (opcode & 0x30) >> 4;
		switch (mux)
		{
		case 0: src = cpu_get_memory(cpu.bc.bc); break;
		case 1: src = cpu_get_memory(cpu.de.de); break;
		case 2: src = cpu_get_memory(cpu.hl.hl++); break;
		case 3: src = cpu_get_memory(cpu.hl.hl--); break;
		default: DBG_ERROR(); break;
		}
		cpu.af.a = src;
		cpu.next_instruction += 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LDd16:
	{
		uint8_t hi, lo;
		uint16_t d16;
		uint8_t mux = (opcode & 0x30) >> 4;
		lo = cpu_get_memory(cpu.pc + 1);
		hi = cpu_get_memory(cpu.pc + 2);
		d16 = ((uint16_t)(hi << 8)) | lo;
		switch (mux)
		{
		case 0: cpu.bc.bc = d16; break;
		case 1: cpu.de.de = d16; break;
		case 2: cpu.hl.hl = d16; break;
		case 3: cpu.sp = d16; break;
		default: DBG_ERROR(); break;
		}
		cpu.next_instruction += 12;
		cpu.pc += 3;
	}
	break;

	case OPC_LD16s:
	{
		uint8_t hi, lo;
		uint16_t addr;
		lo = cpu_get_memory(cpu.pc + 1);
		hi = cpu_get_memory(cpu.pc + 2);
		addr = ((uint16_t)(hi << 8)) | lo;
		cpu_set_memory(addr + 0, LOW_BYTE(cpu.sp));
		cpu_set_memory(addr + 1, HIGH_BYTE(cpu.sp));
		cpu.next_instruction += 20;
		cpu.pc += 3;
	}
	break;

	case OPC_LDHa8:
	{
		uint8_t a8 = cpu_get_memory(cpu.pc + 1);
		uint16_t addr = 0xff00 + a8;
		cpu_set_memory(addr, cpu.af.a);
		cpu.next_instruction += 12;
		cpu.pc += 2;
	}
	break;

	case OPC_LDHA:
	{
		uint8_t a8 = cpu_get_memory(cpu.pc + 1);
		uint16_t addr = 0xff00 + a8;
		cpu.af.a = cpu_get_memory(addr);
		cpu.next_instruction += 12;
		cpu.pc += 2;
	}
	break;

	case OPC_LDCA:
	{
		uint16_t addr = 0xff00 + cpu.bc.c;
		cpu_set_memory(addr, cpu.af.a);
		cpu.next_instruction += 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LDAC:
	{
		uint16_t addr = 0xff00 + cpu.bc.c;
		cpu.af.a = cpu_get_memory(addr);
		cpu.next_instruction += 8;
		cpu.pc += 1;
	}
	break;

	case OPC_PUSH:
	{
		uint8_t r = (opcode & 0x30) >> 4;
		uint16_t src_lut[4] = { cpu.bc.bc, cpu.de.de, cpu.hl.hl, cpu.af.af };
		uint16_t src = src_lut[r];
		cpu_set_memory(--cpu.sp, HIGH_BYTE(src));
		cpu_set_memory(--cpu.sp, LOW_BYTE(src));
		cpu.next_instruction += 16;
		cpu.pc += 1;
	}
	break;

	case OPC_POP:
	{
		uint8_t hi, lo;
		uint8_t r = (opcode & 0x30) >> 4;
		uint16_t *dst_lut[4] = { &cpu.bc.bc, &cpu.de.de, &cpu.hl.hl, &cpu.af.af };
		uint16_t *dst = dst_lut[r];
		lo = cpu_get_memory(cpu.sp++) & ((dst == &cpu.af.af) ? 0xf0 : 0xff);
		hi = cpu_get_memory(cpu.sp++);
		*dst = ((uint16_t)(hi << 8)) | lo;
		cpu.next_instruction += 12;
		cpu.pc += 1;
	}
	break;

	case OPC_INC1:
	{
		uint8_t mux = (opcode & 0x30) >> 4;
		uint8_t *val;
		switch (mux)
		{
		case 0: val = &cpu.bc.b; break;
		case 1: val = &cpu.de.d; break;
		case 2: val = &cpu.hl.h; break;
		default: DBG_ERROR(); break;
		}
		eval_H_flag(*val, 1, false);
		set_N_flag(false);
		(*val)++;
		eval_Z_flag(*val);
		cpu.next_instruction += 4;
		cpu.pc += 1;
	}
	break;

	case OPC_INC2:
	{
		uint8_t mux = (opcode & 0x30) >> 4;
		uint8_t *val;
		switch (mux)
		{
		case 0: val = &cpu.bc.c; break;
		case 1: val = &cpu.de.e; break;
		case 2: val = &cpu.hl.l; break;
		case 3: val = &cpu.af.a; break;
		default: DBG_ERROR(); break;
		}
		eval_H_flag(*val, 1, false);
		set_N_flag(false);
		(*val)++;
		eval_Z_flag(*val);
		cpu.next_instruction += 4;
		cpu.pc += 1;
	}
	break;

	case OPC_INC3:
	{
		uint8_t val = cpu_get_memory(cpu.hl.hl);
		eval_H_flag(val, 1, false);
		set_N_flag(false);
		cpu_set_memory(cpu.hl.hl, val + 1);
		eval_Z_flag(val + 1);
		cpu.next_instruction += 12;
		cpu.pc += 1;
	}
	break;

	case OPC_INC16:
	{
		uint8_t mux = (opcode & 0x30) >> 4;
		switch (mux)
		{
		case 0: cpu.bc.bc++; break;
		case 1: cpu.de.de++; break;
		case 2: cpu.hl.hl++; break;
		case 3: cpu.sp++; break;
		default: DBG_ERROR(); break;
		}
		cpu.next_instruction += 8;
		cpu.pc += 1;
	}
	break;

	case OPC_DEC1:
	{
		uint8_t mux = (opcode & 0x30) >> 4;
		uint8_t *val;
		switch (mux)
		{
		case 0: val = &cpu.bc.b; break;
		case 1: val = &cpu.de.d; break;
		case 2: val = &cpu.hl.h; break;
		default: DBG_ERROR(); break;
		}
		eval_H_flag((uint8_t)*val, 1, true);
		set_N_flag(true);
		(*val)--;
		eval_Z_flag(*val);
		cpu.next_instruction += 4;
		cpu.pc += 1;
	}
	break;

	case OPC_DEC2:
	{
		uint8_t mux = (opcode & 0x30) >> 4;
		uint8_t *val;
		switch (mux)
		{
		case 0: val = &cpu.bc.c; break;
		case 1: val = &cpu.de.e; break;
		case 2: val = &cpu.hl.l; break;
		case 3: val = &cpu.af.a; break;
		default: DBG_ERROR(); break;
		}
		eval_H_flag(*val, 1, true);
		set_N_flag(true);
		(*val)--;
		eval_Z_flag(*val);
		cpu.next_instruction += 4;
		cpu.pc += 1;
	}
	break;

	case OPC_DEC3:
	{
		uint8_t val = cpu_get_memory(cpu.hl.hl);
		eval_H_flag(val, 1, true);
		set_N_flag(true);
		cpu_set_memory(cpu.hl.hl, val - 1);
		eval_Z_flag(val - 1);
		cpu.next_instruction += 12;
		cpu.pc += 1;
	}
	break;

	case OPC_DEC16:
	{
		uint8_t mux = (opcode & 0x30) >> 4;
		switch (mux)
		{
		case 0: cpu.bc.bc--; break;
		case 1: cpu.de.de--; break;
		case 2: cpu.hl.hl--; break;
		case 3: cpu.sp--; break;
		default: DBG_ERROR(); break;
		}
		cpu.next_instruction += 8;
		cpu.pc += 1;
	}
	break;

	case OPC_ADD16:
	{
		uint16_t operand_lut[4] = { cpu.bc.bc, cpu.de.de, cpu.hl.hl, cpu.sp };
		uint8_t ss = (opcode & 0x30) >> 4;
		uint16_t operand = operand_lut[ss];
		set_N_flag(false);
		eval_H_flag_16(cpu.hl.hl, operand);
		eval_C_flag_16(cpu.hl.hl, operand);
		cpu.hl.hl += operand;
		cpu.next_instruction += 8;
		cpu.pc += 1;
	}
	break;

	case OPC_ADDSP:
	{
		int8_t r8 = cpu_get_memory(cpu.pc + 1);
		set_Z_flag(false);
		set_N_flag(false);
		eval_H_flag(cpu.sp, r8, false);
		eval_C_flag(cpu.sp, r8, false);
		cpu.sp += r8;
		cpu.next_instruction += 16;
		cpu.pc += 2;
	}
	break;

	case OPC_LDHLS:
	{
		int8_t r8 = cpu_get_memory(cpu.pc + 1);
		set_Z_flag(false);
		set_N_flag(false);
		eval_H_flag(cpu.sp, r8, false);
		eval_C_flag(cpu.sp, r8, false);
		cpu.hl.hl = cpu.sp + r8;
		cpu.next_instruction += 12;
		cpu.pc += 2;
	}
	break;

	case OPC_LDSHL:
	{
		cpu.sp = cpu.hl.hl;
		cpu.next_instruction += 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LD16A:
	{
		uint8_t hi, lo;
		uint16_t a16;
		lo = cpu_get_memory(cpu.pc + 1);
		hi = cpu_get_memory(cpu.pc + 2);
		a16 = ((uint16_t)(hi << 8)) | lo;
		cpu_set_memory(a16, cpu.af.a);
		cpu.next_instruction += 16;
		cpu.pc += 3;
	}
	break;

	case OPC_LDA16:
	{
		uint8_t hi, lo;
		uint16_t a16;
		lo = cpu_get_memory(cpu.pc + 1);
		hi = cpu_get_memory(cpu.pc + 2);
		a16 = ((uint16_t)(hi << 8)) | lo;
		cpu.af.a = cpu_get_memory(a16);
		cpu.next_instruction += 16;
		cpu.pc += 3;
	}
	break;

	default: DBG_ERROR(); break;
	}

}

void cpu_print_state(void)
{
	bool zf,nf,hf,cf;
	zf = (0 != (cpu.af.f & FLAG_Z));
	nf = (0 != (cpu.af.f & FLAG_N));
	hf = (0 != (cpu.af.f & FLAG_H));
	cf = (0 != (cpu.af.f & FLAG_C));
	uint8_t a,b,c,d,e,h,l;
	a = cpu.af.a;
	b = cpu.bc.b;
	c = cpu.bc.c;
	d = cpu.de.d;
	e = cpu.de.e;
	h = cpu.hl.h;
	l = cpu.hl.l;
	printf("\n");
	printf("PC: %04x (Next Opcode = %02x), SP: %04x\n", cpu.pc, cpu_get_memory(cpu.pc), cpu.sp);
	printf("Z: %d, N: %d, H: %d, C: %d\n", zf, nf, hf, cf);
	printf("A: %02x, B: %02x, C: %02x, D: %02x, E: %02x, H: %02x, L: %02x\n", a, b, c, d, e, h, l);
	printf("BC: %04x, DE: %04x, HL: %04x\n", cpu.bc.bc, cpu.de.de, cpu.hl.hl);
}

void cpu_tick(void)
{
	// currently ignoring cpu.next_instruction, which can be used for cycle-accuracy
	cpu_handle_opcode();
	cpu.cycle_cnt++;

	return;
}

#if (0 < BUILD_TEST_DLL)
void cpu_setup(uint8_t a, uint8_t f, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t h, uint8_t l, uint16_t pc, uint16_t sp)
{
	memset(&cpu, 0, sizeof(cpu));
	cpu.af.a = a;
	cpu.af.f = f;
	cpu.bc.b = b;
	cpu.bc.c = c;
	cpu.de.d = d;
	cpu.de.e = e;
	cpu.hl.h = h;
	cpu.hl.l = l;
	cpu.pc = pc;
	cpu.sp = sp;
}

void cpu_get_state(uint8_t *a, uint8_t *f, uint8_t *b, uint8_t *c, uint8_t *d, uint8_t *e, uint8_t *h, uint8_t *l, uint16_t *pc, uint16_t *sp)
{
	*a = cpu.af.a;
	*f = cpu.af.f;
	*b = cpu.bc.b;
	*c = cpu.bc.c;
	*d = cpu.de.d;
	*e = cpu.de.e;
	*h = cpu.hl.h;
	*l = cpu.hl.l;
	*pc = cpu.pc;
	*sp = cpu.sp;
}
#endif

int main(int argc, char *argv[])
{
	if (2 == argc)
	{
		char *FileName  = argv[1];
		FILE *gbFile = fopen(FileName, "rb");
		if (NULL == gbFile)
		{
			printf("Error: Could not open file '%s'.\n", FileName);
			return 1;
		}
		fread(cpu.rom, 1, 32*1024, gbFile);
		fclose(gbFile);
	}
	else
	{
		printf("Error: Expecting FileName as argument.\nInvocation:\n\t'%s <file>'.\n", argv[0]);
		return 1;
	}
	
	for (;;)
	{
		cpu_tick();
		if (cpu.stopped)
		{
			printf("\nCPU Stopped!\n");
			break;
		}
	}
}

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
