
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


/*---------------------------------------------------------------------*
 *  include files                                                      *
 *---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "trace.h"

/*---------------------------------------------------------------------*
 *  local definitions                                                  *
 *---------------------------------------------------------------------*/
/* we use a uint8_t to index into trace_data. overflow handling is free
 * but keep this in mind when changing buffer size
 */
#define TRACE_BUFFER_LEN 256

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

static uint8_t index = 0;
static uint8_t trace_data[256];

/*---------------------------------------------------------------------*
 *  private function declarations                                      *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  private functions                                                  *
 *---------------------------------------------------------------------*/
static void save_trace(void)
{
    FILE *f = fopen("trace.txt", "w");
    if (f)
    {
        for (int i = 0; i < TRACE_BUFFER_LEN; i++)
        {
            uint8_t opcode = trace_data[(uint8_t)(index + i)];
            if (0xCB == opcode)
            {
                uint8_t opcode2 = trace_data[(uint8_t)(index++ + i)];
                fputs(mnemonics2[opcode2], f);
            }
            else
            {
                fputs(mnemonics[opcode], f);
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

void trace(uint8_t opcode, uint8_t opcode2)
{
    trace_data[index++] = opcode;
    if (opcode == 0xCB)
    {
        trace_data[index++] = opcode2;
    }
}
/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
