
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

#include "debug.h"
#include "bus.h"
#include "timer.h"
#include "emulator.h"
#include "trace.h"

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

#define INTERRUPT_ENABLE_ADDRESS 0xFFFF
#define INTERRUPT_FLAGS_ADDRESS  0xFF0F

#define INTERRUPT_VBLANK         (1<<0)
#define INTERRUPT_LCD            (1<<1)
#define INTERRUPT_TIMER          (1<<2)
#define INTERRUPT_SERIAL         (1<<3)
#define INTERRUPT_JOYPAD         (1<<4)

#define ISR_VECTOR_VBLANK        0x0040
#define ISR_VECTOR_LCD           0x0048
#define ISR_VECTOR_TIMER         0x0050
#define ISR_VECTOR_SERIAL        0x0058
#define ISR_VECTOR_JOYPAD        0x0060

/*---------------------------------------------------------------------*
 *  local data types                                                   *
 *---------------------------------------------------------------------*/
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

	uint32_t stall_cnt;
	uint64_t cycle_cnt;
	uint64_t next_instruction;

	bool interrupts_enabled;
	bool stopped;
	bool halted;

	uint16_t last_addr;
	uint8_t last_opcode;
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
static void eval_Z_flag(uint8_t reg);
static void set_Z_flag(bool zero);
static void set_N_flag(bool subtract);
static void eval_H_flag_c(uint8_t target, uint8_t operand, bool sub, uint8_t carry);
static void eval_H_flag(uint8_t target, uint8_t operand, bool sub);
static void eval_H_flag_16(uint16_t target, uint16_t operand);
static void set_H_flag(bool h);
static void eval_C_flag_c(uint8_t target, uint8_t operand, bool sub, uint8_t carry);
static void eval_C_flag(uint8_t target, uint8_t operand, bool sub);
static void eval_C_flag_16(uint16_t target, uint16_t operand);
static void set_C_flag(bool c);
static uint8_t cpu_handle_opcode(void);
static void cpu_print_state(void);
static uint8_t cpu_handle_interrupt(void);

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
static void eval_Z_flag(uint8_t reg)
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

static void set_Z_flag(bool zero)
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

static void set_N_flag(bool subtract)
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

static void eval_H_flag_c(uint8_t target, uint8_t operand, bool sub, uint8_t carry)
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

static void eval_H_flag(uint8_t target, uint8_t operand, bool sub)
{
	eval_H_flag_c(target, operand, sub, 0);
}

static void eval_H_flag_16(uint16_t target, uint16_t operand)
{
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

static void set_H_flag(bool h)
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

static void eval_C_flag_c(uint8_t target, uint8_t operand, bool sub, uint8_t carry)
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

static void eval_C_flag(uint8_t target, uint8_t operand, bool sub)
{
	eval_C_flag_c(target, operand, sub, 0);
}

static void eval_C_flag_16(uint16_t target, uint16_t operand)
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

static void set_C_flag(bool c)
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

static uint8_t cpu_handle_opcode(void)
{
	uint8_t cycle_cnt = 0;
	uint8_t opcode, opcode2 = 0;
	opcode_t opcode_type;

	opcode = bus_get_memory(cpu.pc);
	opcode_type = opcode_types[opcode];

	cpu.last_addr = cpu.pc;
	cpu.last_opcode = opcode;

	switch (opcode_type)
	{
	case OPC_NONE:
	{
		DBG_ERROR();
	}
	break;

	case OPC_NOP:
	{
		cycle_cnt = 4;
		cpu.pc++;
	}
	break;
	
	case OPC_STOP:
	{
		cpu.stopped = true;
		cycle_cnt = 4;
		cpu.pc += 2;
		gbc_timer_diva_reset();
		bus_stop_instr_cb();
	}
	break;
	
	case OPC_HALT:
	{
		cycle_cnt = 4;
		cpu.pc++;
		cpu.halted = true;
	}
	break;
	
	case OPC_EI:
	{
		cpu.interrupts_enabled = true;
		cycle_cnt = 4;
		cpu.pc++;
	}
	break;
	
	case OPC_DI:
	{
		cpu.interrupts_enabled = false;
		cycle_cnt = 4;
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
		cycle_cnt = 4;
		cpu.pc++;
	}
	break;
	
	case OPC_CPL:
	{
		set_N_flag(true);
		set_H_flag(true);
		cpu.af.a = ~cpu.af.a;
		cycle_cnt = 4;
		cpu.pc++;
	}
	break;
	
	case OPC_SCF:
	{
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(true);
		cycle_cnt = 4;
		cpu.pc++;
	}
	break;
	
	case OPC_CCF:
	{
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag((0 == (cpu.af.f & FLAG_C)));
		cycle_cnt = 4;
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
		cycle_cnt = 4;
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
		cycle_cnt = 4;
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
		cycle_cnt = 4;
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
		cycle_cnt = 4;
		cpu.pc += 1;
	}
	break;

	case OPC_CB:
	{
		opcode2 = bus_get_memory(cpu.pc + 1);;
		opcode2_t opcode_type2 = opcode_types2[opcode2];
		uint8_t *target_lut[8] = {
			&cpu.bc.b, &cpu.bc.c, &cpu.de.d, &cpu.de.e,
			&cpu.hl.h, &cpu.hl.l, NULL     , &cpu.af.a
		};
		uint8_t *target_p = target_lut[opcode2 & 0x07];
		uint8_t duration = (0x46 == (opcode2 & 0xc7)) ? 12 :
						   (0x06 == (opcode2 & 0x07)) ? 16 :
						   8;

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
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			bool bit7 = (0 != (operand & (1<<7)));
			uint8_t result = (operand << 1) | (bit7 ? 1 : 0);
			bus_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit7);
		}
		break;
		
		case OPC_RRC2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			bool bit0 = (0 != (operand & (1<<0)));
			uint8_t result = (operand >> 1)| (bit0 ? 0x80 : 0);
			bus_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_RL2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			bool bit7 = (0 != (operand & (1<<7)));
			bool c = (0 != (cpu.af.f & FLAG_C));
			uint8_t result = (operand << 1) | (c ? 0x1 : 0);
			bus_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit7);
		}
		break;
		
		case OPC_RR2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			bool bit0 = (0 != (operand & (1<<0)));
			bool c = (0 != (cpu.af.f & FLAG_C));
			uint8_t result = (operand >> 1) | (c ? 0x80 : 0);
			bus_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_SLA2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			bool bit7 = (0 != (operand & (1<<7)));
			uint8_t result = (operand << 1);
			bus_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit7);
		}
		break;
		
		case OPC_SRA2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			bool bit0 = (0 != (operand & (1<<0)));
			bool bit7 = (0 != (operand & (1<<7)));
			uint8_t result = (operand >> 1) | (bit7 ? 0x80 : 0);
			bus_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_SWAP2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			uint8_t result = (LOW_NIBBLE(operand) << 4) | HIGH_NIBBLE(operand);
			bus_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(false);
		}
		break;
		
		case OPC_SRL2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			bool bit0 = (0 != (operand & (1<<0)));
			uint8_t result = (operand >> 1);
			bus_set_memory(cpu.hl.hl, result);
			eval_Z_flag(result);
			set_N_flag(false);
			set_H_flag(false);
			set_C_flag(bit0);
		}
		break;
		
		case OPC_BIT2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			uint8_t bit = (opcode2 & 0x38) >> 3;
			eval_Z_flag((operand & (1 << bit)));
			set_N_flag(false);
			set_H_flag(true);
		}
		break;
		
		case OPC_RES2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			uint8_t bit = (opcode2 & 0x38) >> 3;
			uint8_t result = (operand & ~(1 << bit));
			bus_set_memory(cpu.hl.hl, result);
			// no flags affected
		}
		break;
		
		case OPC_SET2:
		{
			uint8_t operand = bus_get_memory(cpu.hl.hl);
			uint8_t bit = (opcode2 & 0x38) >> 3;
			uint8_t result = (operand | (1 << bit));
			bus_set_memory(cpu.hl.hl, result);
			// no flags affected
		}
		break;
		default: DBG_ERROR(); break;
		}

		cycle_cnt = duration;
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
			lo = bus_get_memory(cpu.pc + 1);
			hi = bus_get_memory(cpu.pc + 2);
			bus_set_memory(--cpu.sp, HIGH_BYTE(next_pc));
			bus_set_memory(--cpu.sp, LOW_BYTE(next_pc));
			cpu.pc = ((uint16_t)hi << 8) | lo;
			cycle_cnt = 24;
		}
		else
		{
			cycle_cnt = 12;
			cpu.pc += 3;
		}
	}
	break;

	case OPC_CAL2:
	{
		uint8_t hi, lo;
		uint16_t next_pc = cpu.pc + 3;
		lo = bus_get_memory(cpu.pc + 1);
		hi = bus_get_memory(cpu.pc + 2);
		bus_set_memory(--cpu.sp, HIGH_BYTE(next_pc));
		bus_set_memory(--cpu.sp, LOW_BYTE(next_pc));
		cpu.pc = ((uint16_t)hi << 8) | lo;
		cycle_cnt = 24;
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
			int8_t offset = (int8_t) bus_get_memory(cpu.pc + 1);
			cpu.pc += (offset + 2);
			cycle_cnt = 12;
		}
		else
		{
			cycle_cnt = 8;
			cpu.pc += 2;
		}
	}
	break;

	case OPC_JR:
	{
		int8_t offset = (int8_t) bus_get_memory(cpu.pc + 1);
		cpu.pc += (offset + 2);
		cycle_cnt = 12;
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
			lo = bus_get_memory(cpu.pc + 1);
			hi = bus_get_memory(cpu.pc + 2);
			cpu.pc = ((uint16_t)hi << 8) | lo;
			cycle_cnt = 16;
		}
		else
		{
			cycle_cnt = 12;
			cpu.pc += 3;
		}
	}
	break;

	case OPC_JP:
	{
		uint8_t hi, lo;
		lo = bus_get_memory(cpu.pc + 1);
		hi = bus_get_memory(cpu.pc + 2);
		cpu.pc = ((uint16_t)hi << 8) | lo;
		cycle_cnt = 16;
	}
	break;

	case OPC_JPHL:
	{
		cpu.pc = cpu.hl.hl;
		cycle_cnt = 4;
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
			lo = bus_get_memory(cpu.sp++);
			hi = bus_get_memory(cpu.sp++);
			cpu.pc = ((uint16_t)hi << 8) | lo;
			cycle_cnt = 20;
		}
		else
		{
			cycle_cnt = 8;
			cpu.pc += 1;
		}
	}
	break;

	case OPC_RETI:
		cpu.interrupts_enabled = true;
		/* fall through */
	case OPC_RET:
	{
		uint8_t lo, hi;
		lo = bus_get_memory(cpu.sp++);
		hi = bus_get_memory(cpu.sp++);
		cpu.pc = ((uint16_t)hi << 8) | lo;
		cycle_cnt = 16;
	}
	break;

	case OPC_RST:
	{
		uint8_t offset_lut[8] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};
		uint8_t t = (opcode & 0x38) >> 3;
		uint8_t offset = offset_lut[t];

		bus_set_memory(--cpu.sp, (((cpu.pc + 1) & 0xFF00) >> 8));
		bus_set_memory(--cpu.sp, (((cpu.pc + 1) & 0x00FF) >> 0));

		cpu.pc = offset;

		cycle_cnt = 16;
	}
	break;

	case OPC_ADD:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l,        0, cpu.af.a,
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = (6 == i) ? bus_get_memory(cpu.hl.hl) : operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t result = cpu.af.a + operand;
		eval_Z_flag(result);
		set_N_flag(false);
		eval_H_flag(cpu.af.a, operand, false);
		eval_C_flag(cpu.af.a, operand, false);
		cpu.af.a = result;
		cycle_cnt = duration;
		cpu.pc += 1;
	}
	break;

	case OPC_SUB:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l,        0, cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = (6 == i) ? bus_get_memory(cpu.hl.hl) : operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t result = cpu.af.a - operand;
		eval_Z_flag(result);
		set_N_flag(true);
		eval_H_flag(cpu.af.a, operand, true);
		eval_C_flag(cpu.af.a, operand, true);
		cpu.af.a = result;
		cycle_cnt = duration;
		cpu.pc += 1;
	}
	break;

	case OPC_AND:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l,        0, cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = (6 == i) ? bus_get_memory(cpu.hl.hl) : operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		cpu.af.a = cpu.af.a & operand;
		eval_Z_flag(cpu.af.a);
		set_N_flag(false);
		set_H_flag(true);
		set_C_flag(false);
		cycle_cnt = duration;
		cpu.pc += 1;
	}
	break;

	case OPC_OR:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l,        0, cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = (6 == i) ? bus_get_memory(cpu.hl.hl) : operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		cpu.af.a = cpu.af.a | operand;
		eval_Z_flag(cpu.af.a);
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(false);
		cycle_cnt = duration;
		cpu.pc += 1;
	}
	break;

	case OPC_ADD2:
	{
		uint8_t operand = bus_get_memory(cpu.pc + 1);
		eval_C_flag(cpu.af.a, operand, false);
		eval_H_flag(cpu.af.a, operand, false);
		set_N_flag(false);
		cpu.af.a += operand;
		eval_Z_flag(cpu.af.a);
		cycle_cnt = 8;
		cpu.pc += 2;
	}
	break;

	case OPC_SUB2:
	{
		uint8_t operand = bus_get_memory(cpu.pc + 1);
		eval_C_flag(cpu.af.a, operand, true);
		eval_H_flag(cpu.af.a, operand, true);
		set_N_flag(true);
		cpu.af.a -= operand;
		eval_Z_flag(cpu.af.a);
		cycle_cnt = 8;
		cpu.pc += 2;
	}
	break;

	case OPC_AND2:
	{
		uint8_t operand = bus_get_memory(cpu.pc + 1);
		set_C_flag(false);
		set_H_flag(true);
		set_N_flag(false);
		cpu.af.a = cpu.af.a & operand;
		eval_Z_flag(cpu.af.a);
		cycle_cnt = 8;
		cpu.pc += 2;
	}
	break;

	case OPC_OR2:
	{
		uint8_t operand = bus_get_memory(cpu.pc + 1);
		set_C_flag(false);
		set_H_flag(false);
		set_N_flag(false);
		cpu.af.a = cpu.af.a | operand;
		eval_Z_flag(cpu.af.a);
		cycle_cnt = 8;
		cpu.pc += 2;
	}
	break;

	case OPC_ADC:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l,        0, cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = (6 == i) ? bus_get_memory(cpu.hl.hl) : operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t c = (0 != (cpu.af.f & FLAG_C)) ? 1 : 0;
		uint8_t result = cpu.af.a + operand + c;
		eval_Z_flag(result);
		set_N_flag(false);
		eval_H_flag_c(cpu.af.a, operand, false, c);
		eval_C_flag_c(cpu.af.a, operand, false, c);
		cpu.af.a = result;
		cycle_cnt = duration;
		cpu.pc += 1;
	}
	break;

	case OPC_SBC:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l,        0, cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = (6 == i) ? bus_get_memory(cpu.hl.hl) : operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t c = (0 != (cpu.af.f & FLAG_C)) ? 1 : 0;
		uint8_t result = cpu.af.a - operand - c;
		eval_Z_flag(result);
		set_N_flag(true);
		eval_H_flag_c(cpu.af.a, operand, true, c);
		eval_C_flag_c(cpu.af.a, operand, true, c);
		cpu.af.a = result;
		cycle_cnt = duration;
		cpu.pc += 1;
	}
	break;

	case OPC_XOR:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l,        0, cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = (6 == i) ? bus_get_memory(cpu.hl.hl) : operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		cpu.af.a = cpu.af.a ^ operand;
		eval_Z_flag(cpu.af.a);
		set_N_flag(false);
		set_H_flag(false);
		set_C_flag(false);
		cycle_cnt = duration;
		cpu.pc += 1;
	}
	break;

	case OPC_CP:
	{
		uint8_t operand_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e, 
			cpu.hl.h, cpu.hl.l,        0, cpu.af.a, 
		};
		uint8_t i = (opcode & 0x07);
		uint8_t operand = (6 == i) ? bus_get_memory(cpu.hl.hl) : operand_lut[i];
		uint8_t duration = (i == 6) ? 8 : 4;
		uint8_t result = cpu.af.a - operand;
		eval_Z_flag(result);
		set_N_flag(true);
		eval_H_flag(cpu.af.a, operand, true);
		eval_C_flag(cpu.af.a, operand, true);
		eval_H_flag(cpu.af.a, operand, true);
		eval_C_flag(cpu.af.a, operand, true);
		cycle_cnt = duration;
		cpu.pc += 1;
	}
	break;

	case OPC_ADC2:
	{
		uint8_t operand = bus_get_memory(cpu.pc + 1);
		uint8_t c = (cpu.af.f & FLAG_C) ? 1: 0;
		eval_C_flag_c(cpu.af.a, operand, false, c);
		eval_H_flag_c(cpu.af.a, operand, false, c);
		set_N_flag(false);
		cpu.af.a += operand + c;
		eval_Z_flag(cpu.af.a);
		cycle_cnt = 8;
		cpu.pc += 2;
	}
	break;

	case OPC_SBC2:
	{
		uint8_t operand = bus_get_memory(cpu.pc + 1);
		uint8_t c = (cpu.af.f & FLAG_C) ? 1: 0;
		eval_C_flag_c(cpu.af.a, operand, true, c);
		eval_H_flag_c(cpu.af.a, operand, true, c);
		set_N_flag(true);
		cpu.af.a -= (operand + c);
		eval_Z_flag(cpu.af.a);
		cycle_cnt = 8;
		cpu.pc += 2;
	}
	break;

	case OPC_XOR2:
	{
		uint8_t operand = bus_get_memory(cpu.pc + 1);
		uint8_t c = (cpu.af.f & FLAG_C) ? 1: 0;
		set_C_flag(false);
		set_H_flag(false);
		set_N_flag(false);
		cpu.af.a = cpu.af.a ^ operand;
		eval_Z_flag(cpu.af.a);
		cycle_cnt = 8;
		cpu.pc += 2;
	}
	break;

	case OPC_CP2:
	{
		uint8_t operand = bus_get_memory(cpu.pc + 1);
		uint8_t c = (cpu.af.f & FLAG_C) ? 1: 0;
		eval_C_flag(cpu.af.a, operand, true);
		eval_H_flag(cpu.af.a, operand, true);
		set_N_flag(true);
		eval_Z_flag(cpu.af.a - operand);
		cycle_cnt = 8;
		cpu.pc += 2;
	}
	break;

	case OPC_LD:
	{
		uint8_t src_lut[8] = {
			cpu.bc.b, cpu.bc.c, cpu.de.d, cpu.de.e,
			cpu.hl.h, cpu.hl.l,        0, cpu.af.a
		};
		uint8_t *dst_lut[8] = {
			&cpu.bc.b, &cpu.bc.c, &cpu.de.d, &cpu.de.e,
			&cpu.hl.h, &cpu.hl.l, NULL     , &cpu.af.a
		};
		uint8_t r0 = (opcode & 0x38) >> 3;
		uint8_t r1 = (opcode & 0x07) >> 0;
		uint8_t src = (6 == r1) ? bus_get_memory(cpu.hl.hl) : src_lut[r1];
		uint8_t *dst = dst_lut[r0];
		uint8_t duration = ((6 == r0) || (6 == r1)) ? 8 : 4;
		*dst = src;
		cycle_cnt = duration;
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
		bus_set_memory(cpu.hl.hl, src);
		cycle_cnt = 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LDd8:
	{
		uint8_t data = bus_get_memory(cpu.pc + 1);
		uint8_t *dst_lut[8] = {
			&cpu.bc.b, &cpu.bc.c, &cpu.de.d, &cpu.de.e,
			&cpu.hl.h, &cpu.hl.l, NULL     , &cpu.af.a
		};
		uint8_t r = (opcode & 0x38) >> 3;
		uint8_t *dst = dst_lut[r];
		*dst = data;
		cycle_cnt = 8;
		cpu.pc += 2;
	}
	break;

	case OPC_LDd82:
	{
		uint8_t data = bus_get_memory(cpu.pc + 1);
		uint8_t r = (opcode & 0x38) >> 3;
		bus_set_memory(cpu.hl.hl, data);
		cycle_cnt = 12;
		cpu.pc += 2;
	}
	break;

	case OPC_LDa2r:
	{
		uint8_t mux = (opcode & 0x30) >> 4;
		switch (mux)
		{
		case 0: bus_set_memory(cpu.bc.bc, cpu.af.a); break;
		case 1: bus_set_memory(cpu.de.de, cpu.af.a); break;
		case 2: bus_set_memory(cpu.hl.hl++, cpu.af.a); break;
		case 3: bus_set_memory(cpu.hl.hl--, cpu.af.a); break;
		default: DBG_ERROR(); break;
		}
		cycle_cnt = 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LDr2a:
	{
		uint8_t src;
		uint8_t mux = (opcode & 0x30) >> 4;
		switch (mux)
		{
		case 0: src = bus_get_memory(cpu.bc.bc); break;
		case 1: src = bus_get_memory(cpu.de.de); break;
		case 2: src = bus_get_memory(cpu.hl.hl++); break;
		case 3: src = bus_get_memory(cpu.hl.hl--); break;
		default: DBG_ERROR(); break;
		}
		cpu.af.a = src;
		cycle_cnt = 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LDd16:
	{
		uint8_t hi, lo;
		uint16_t d16;
		uint8_t mux = (opcode & 0x30) >> 4;
		lo = bus_get_memory(cpu.pc + 1);
		hi = bus_get_memory(cpu.pc + 2);
		d16 = ((uint16_t)(hi << 8)) | lo;
		switch (mux)
		{
		case 0: cpu.bc.bc = d16; break;
		case 1: cpu.de.de = d16; break;
		case 2: cpu.hl.hl = d16; break;
		case 3: cpu.sp = d16; break;
		default: DBG_ERROR(); break;
		}
		cycle_cnt = 12;
		cpu.pc += 3;
	}
	break;

	case OPC_LD16s:
	{
		uint8_t hi, lo;
		uint16_t addr;
		lo = bus_get_memory(cpu.pc + 1);
		hi = bus_get_memory(cpu.pc + 2);
		addr = ((uint16_t)(hi << 8)) | lo;
		bus_set_memory(addr + 0, LOW_BYTE(cpu.sp));
		bus_set_memory(addr + 1, HIGH_BYTE(cpu.sp));
		cycle_cnt = 20;
		cpu.pc += 3;
	}
	break;

	case OPC_LDHa8:
	{
		uint8_t a8 = bus_get_memory(cpu.pc + 1);
		uint16_t addr = 0xff00 + a8;
		bus_set_memory(addr, cpu.af.a);
		cycle_cnt = 12;
		cpu.pc += 2;
	}
	break;

	case OPC_LDHA:
	{
		uint8_t a8 = bus_get_memory(cpu.pc + 1);
		uint16_t addr = 0xff00 + a8;
		cpu.af.a = bus_get_memory(addr);
		cycle_cnt = 12;
		cpu.pc += 2;
	}
	break;

	case OPC_LDCA:
	{
		uint16_t addr = 0xff00 + cpu.bc.c;
		bus_set_memory(addr, cpu.af.a);
		cycle_cnt = 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LDAC:
	{
		uint16_t addr = 0xff00 + cpu.bc.c;
		cpu.af.a = bus_get_memory(addr);
		cycle_cnt = 8;
		cpu.pc += 1;
	}
	break;

	case OPC_PUSH:
	{
		uint8_t r = (opcode & 0x30) >> 4;
		uint16_t src_lut[4] = { cpu.bc.bc, cpu.de.de, cpu.hl.hl, cpu.af.af };
		uint16_t src = src_lut[r];
		bus_set_memory(--cpu.sp, HIGH_BYTE(src));
		bus_set_memory(--cpu.sp, LOW_BYTE(src));
		cycle_cnt = 16;
		cpu.pc += 1;
	}
	break;

	case OPC_POP:
	{
		uint8_t hi, lo;
		uint8_t r = (opcode & 0x30) >> 4;
		uint16_t *dst_lut[4] = { &cpu.bc.bc, &cpu.de.de, &cpu.hl.hl, &cpu.af.af };
		uint16_t *dst = dst_lut[r];
		lo = bus_get_memory(cpu.sp++) & ((dst == &cpu.af.af) ? 0xf0 : 0xff);
		hi = bus_get_memory(cpu.sp++);
		*dst = ((uint16_t)(hi << 8)) | lo;
		cycle_cnt = 12;
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
		cycle_cnt = 4;
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
		cycle_cnt = 4;
		cpu.pc += 1;
	}
	break;

	case OPC_INC3:
	{
		uint8_t val = bus_get_memory(cpu.hl.hl);
		eval_H_flag(val, 1, false);
		set_N_flag(false);
		bus_set_memory(cpu.hl.hl, val + 1);
		eval_Z_flag(val + 1);
		cycle_cnt = 12;
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
		cycle_cnt = 8;
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
		cycle_cnt = 4;
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
		cycle_cnt = 4;
		cpu.pc += 1;
	}
	break;

	case OPC_DEC3:
	{
		uint8_t val = bus_get_memory(cpu.hl.hl);
		eval_H_flag(val, 1, true);
		set_N_flag(true);
		bus_set_memory(cpu.hl.hl, val - 1);
		eval_Z_flag(val - 1);
		cycle_cnt = 12;
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
		cycle_cnt = 8;
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
		cycle_cnt = 8;
		cpu.pc += 1;
	}
	break;

	case OPC_ADDSP:
	{
		int8_t r8 = bus_get_memory(cpu.pc + 1);
		set_Z_flag(false);
		set_N_flag(false);
		eval_H_flag(cpu.sp, r8, false);
		eval_C_flag(cpu.sp, r8, false);
		cpu.sp += r8;
		cycle_cnt = 16;
		cpu.pc += 2;
	}
	break;

	case OPC_LDHLS:
	{
		int8_t r8 = bus_get_memory(cpu.pc + 1);
		set_Z_flag(false);
		set_N_flag(false);
		eval_H_flag(cpu.sp, r8, false);
		eval_C_flag(cpu.sp, r8, false);
		cpu.hl.hl = cpu.sp + r8;
		cycle_cnt = 12;
		cpu.pc += 2;
	}
	break;

	case OPC_LDSHL:
	{
		cpu.sp = cpu.hl.hl;
		cycle_cnt = 8;
		cpu.pc += 1;
	}
	break;

	case OPC_LD16A:
	{
		uint8_t hi, lo;
		uint16_t a16;
		lo = bus_get_memory(cpu.pc + 1);
		hi = bus_get_memory(cpu.pc + 2);
		a16 = ((uint16_t)(hi << 8)) | lo;
		bus_set_memory(a16, cpu.af.a);
		cycle_cnt = 16;
		cpu.pc += 3;
	}
	break;

	case OPC_LDA16:
	{
		uint8_t hi, lo;
		uint16_t a16;
		lo = bus_get_memory(cpu.pc + 1);
		hi = bus_get_memory(cpu.pc + 2);
		a16 = ((uint16_t)(hi << 8)) | lo;
		cpu.af.a = bus_get_memory(a16);
		cycle_cnt = 16;
		cpu.pc += 3;
	}
	break;

	default: DBG_ERROR(); break;
	}

	trace(opcode, opcode2);

	return cycle_cnt;
}

static void cpu_print_state(void)
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
	printf("PC: %04x (Next Opcode = %02x), SP: %04x\n", cpu.pc, bus_get_memory(cpu.pc), cpu.sp);
	printf("Z: %d, N: %d, H: %d, C: %d\n", zf, nf, hf, cf);
	printf("A: %02x, B: %02x, C: %02x, D: %02x, E: %02x, H: %02x, L: %02x\n", a, b, c, d, e, h, l);
	printf("BC: %04x, DE: %04x, HL: %04x\n", cpu.bc.bc, cpu.de.de, cpu.hl.hl);
}

static uint8_t cpu_handle_interrupt(void)
{
	static enum
	{
		isr_state_idle_e,
		isr_state_setup_pc_e,
	} isr_state = isr_state_idle_e;
	static uint8_t isr = 0;
	uint8_t cycle_cnt = 0;

	switch (isr_state)
	{
		case isr_state_setup_pc_e:
		{
			uint8_t IF = bus_get_memory(INTERRUPT_FLAGS_ADDRESS);

			/* push current pc to stack */
			bus_set_memory(--cpu.sp, HIGH_BYTE(cpu.pc));
			bus_set_memory(--cpu.sp, LOW_BYTE(cpu.pc));

			/* setup pc for interrupt and clear respective interrupt flag */
			if (isr & INTERRUPT_VBLANK)
			{
				cpu.pc = ISR_VECTOR_VBLANK;
				bus_set_memory(INTERRUPT_FLAGS_ADDRESS, (IF & ~INTERRUPT_VBLANK));
			}
			else if (isr & INTERRUPT_LCD)
			{
				cpu.pc = ISR_VECTOR_LCD;
				bus_set_memory(INTERRUPT_FLAGS_ADDRESS, (IF & ~INTERRUPT_LCD));
			}
			else if (isr & INTERRUPT_TIMER)
			{
				cpu.pc = ISR_VECTOR_TIMER;
				bus_set_memory(INTERRUPT_FLAGS_ADDRESS, (IF & ~INTERRUPT_TIMER));
			}
			else if (isr & INTERRUPT_SERIAL)
			{
				cpu.pc = ISR_VECTOR_SERIAL;
				bus_set_memory(INTERRUPT_FLAGS_ADDRESS, (IF & ~INTERRUPT_SERIAL));
			}
			else /*if (isr & INTERRUPT_JOYPAD)*/
			{
				cpu.pc = ISR_VECTOR_JOYPAD;
				bus_set_memory(INTERRUPT_FLAGS_ADDRESS, (IF & ~INTERRUPT_JOYPAD));
			}

			/* disable all interrupts */
			cpu.interrupts_enabled = false;
			
			cycle_cnt = 3;

			isr_state = isr_state_idle_e;
		}
		break;

		case isr_state_idle_e:
		default:
		{
			uint8_t IE = bus_get_memory(INTERRUPT_ENABLE_ADDRESS);
			uint8_t IF = bus_get_memory(INTERRUPT_FLAGS_ADDRESS);
			isr = (IE & IF);
			if (0 != isr)
			{
				cpu.halted = false;
				if (cpu.interrupts_enabled)
				{
					isr_state = isr_state_setup_pc_e;
					cycle_cnt = 2;
				}
			}
		}
		break;
	}

	return cycle_cnt;
}

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
void gbc_cpu_init(void)
{
	memset(&cpu, 0, sizeof(cpu));
	
	/* CGB hardware can be detected by examining the CPU accumulator (A-register) directly after startup.
	   A value of $11 indicates CGB (or GBA) hardware, if so, CGB functions can be used (if unlocked, see above).
	   When A=$11, you may also examine Bit 0 of the CPUs B-Register to separate between CGB (bit cleared) and
	   GBA (bit set), by that detection it is possible to use “repaired” color palette data matching for GBA displays.
	*/
	cpu.af.a = 0x11;

	cpu.pc = 0x0100;
	cpu.sp = 0xFFFE;

	return;
}

uint32_t gbc_cpu_tick(void)
{
	uint32_t cycle_cnt;

	cycle_cnt = cpu.stall_cnt;
	cpu.stall_cnt = 0;

	if (0 == cycle_cnt)
	{
		cycle_cnt = cpu_handle_interrupt();
	}

	if (0 == cycle_cnt)
	{
		if (!cpu.halted)
		{
			cycle_cnt = cpu_handle_opcode();
		}
		else
		{
			cycle_cnt = 1;
		}
	}

	cpu.cycle_cnt += cycle_cnt;

	return cycle_cnt;
}

bool gbc_cpu_stopped(void)
{
	return cpu.stopped;
}

uint64_t gbc_cpu_get_cycle_cnt(void)
{
	return cpu.cycle_cnt;
}

void gbc_cpu_stall(uint32_t num_ticks)
{
	cpu.stall_cnt = num_ticks;
}

void gbc_cpu_write_internal_state(void)
{
	emulator_cb_write_to_save_file((uint8_t*) &cpu, sizeof(sm83_t), "cpu");
	return;
}

int gbc_cpu_set_internal_state(void)
{
	return emulator_cb_read_from_save_file((uint8_t*) &cpu, sizeof(sm83_t));
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

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
