
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         GBC Timer                                   *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: trace.c                                              *
 *        author: tstr92                                               *
 *          date: 2025-10-03                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

#if (0 != DEBUG)
/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "trace.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
#define TRACE_BUFFER_LEN 1024

typedef enum
{
    tt_opcode_e = 0,
    tt_event_e = 1,
} trace_type_t;

typedef struct
{
    trace_type_t type;
    union
    {
        trace_data_t trace_data;
        char event[TRACE_EVENT_STR_SIZE];
    };
} trace_container_t;

/*---------------------------------------------------------------------*
 *  external declarations                                              *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  public data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private data                                                       *
 *---------------------------------------------------------------------*/
/* -> static const char *mnemonics[] = {...};
 * -> static const char *mnemonics2[] = {...};
 */
#include "mnemonics.inc"

static size_t tb_idx = 0;
static trace_container_t trace_data[TRACE_BUFFER_LEN];

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/
static inline void inc_tb_idx(void);

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
static inline void inc_tb_idx(void)
{
    tb_idx++;
    if (TRACE_BUFFER_LEN <= tb_idx)
    {
        tb_idx = 0;
    }
}

static void save_trace(void)
{
    FILE *f = fopen("trace.txt", "w");
    if (f)
    {
        size_t idx = (0 == tb_idx) ? (TRACE_BUFFER_LEN - 1) : (tb_idx - 1);
        size_t num_cycle_cnt_digits;
        {
            size_t local_idx = idx;
            for (int i = 0; i < TRACE_BUFFER_LEN; i++)
            {
                if (tt_opcode_e == trace_data[idx].type)
                {
                    num_cycle_cnt_digits  = (size_t) log10f((float)trace_data[local_idx].trace_data.cycle_cnt);
                    break;
                }
                else
                {
                    local_idx = (0 == local_idx) ? (TRACE_BUFFER_LEN - 1) : (local_idx - 1);
                }
            }
        }
        for (int i = 0; i < TRACE_BUFFER_LEN; i++)
        {
            size_t idx = (size_t)(tb_idx + i) % TRACE_BUFFER_LEN;
            if (tt_event_e == trace_data[idx].type)
            {
                fprintf(f, "%s\n", trace_data[idx].event);
            }
            else
            {
                char const * mnemonic;
                trace_data_t *td;
                uint8_t opcode;
                char opcode_data[9]; // "00 00 00" + \0 = 9 Bytes
                
                td = &trace_data[idx].trace_data;
                opcode = td->code_mem[0];

                if (0xCB == opcode)
                {
                    uint8_t opcode2 = td->code_mem[1];
                    mnemonic = mnemonics2[opcode2];
                }
                else
                {
                    mnemonic = mnemonics[opcode];
                }
                
                for (int j = 0, offset = 0; j < td->opcode_size; j++)
                {
                    offset += sprintf(&opcode_data[offset], "%02x ", td->code_mem[j]);
                }
                opcode_data[8] = '\0';

                fprintf(f, "%.*llu, PC=%04x: %-8.8s, %-16s, SP=%04x, [ A F B C D E H L ] = [ %02x %02x %02x %02x %02x %02x %02x %02x ], stack = [ %02x %02x %02x %02x (...)]\n",
                    num_cycle_cnt_digits, td->cycle_cnt, td->pc, opcode_data, mnemonic, td->sp, td->a, td->f, td->b, td->c, td->d, td->e, td->h, td->l,
                    td->stack[0], td->stack[1], td->stack[2], td->stack[3]
                );
            }
        }
        fclose(f);
    }
}

/*---------------------------------------------------------------------*
 *  public functions                                                   *
 *---------------------------------------------------------------------*/
void trace_init(void)
{
    atexit(save_trace);
}

void trace_opcode(trace_data_t *p_trace_data)
{
    trace_data[tb_idx].type = tt_opcode_e;
    trace_data[tb_idx].trace_data = *p_trace_data;
    inc_tb_idx();
}

/* evt should have max TRACE_EVENT_STR_SIZE chars, there */
void trace_event(char *evt)
{
    trace_data[tb_idx].type = tt_event_e;
    snprintf(trace_data[tb_idx].event, sizeof(trace_data[tb_idx].event), "%s", evt);
    inc_tb_idx();
}
#endif

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
