
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         GBC Timer                                   *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: trace.h                                              *
 *        author: tstr92                                               *
 *          date: 2025-10-03                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

#ifndef _TRACE_H_
#define _TRACE_H_

#if (0 != DEBUG)

/*---------------------------------------------------------------------*
 *  additional includes                                                *
 *---------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------*
 *  global definitions                                                 *
 *---------------------------------------------------------------------*/
#define TRACE_EVENT_STR_SIZE 32

#define TRACE_EVENT(fmt, ...)                                   \
do                                                              \
{                                                               \
    char _evt[TRACE_EVENT_STR_SIZE];                            \
    snprintf(_evt, TRACE_EVENT_STR_SIZE-1, fmt, ##__VA_ARGS__); \
    _evt[TRACE_EVENT_STR_SIZE-1] = '\0';                        \
    trace_event(_evt);                                          \
} while (0)

/*---------------------------------------------------------------------*
 *  type declarations                                                  *
 *---------------------------------------------------------------------*/
typedef struct
{
    uint8_t a;
    uint8_t f;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t pc;
    uint16_t sp;
    uint8_t opcode_size;
    uint8_t code_mem[3];
    uint64_t cycle_cnt;
    uint8_t stack[4];
} trace_data_t;

/*---------------------------------------------------------------------*
 *  function prototypes                                                *
 *---------------------------------------------------------------------*/
void trace_init(void);
void trace_opcode(trace_data_t *p_trace_data);
void trace_event(char *evt); /* evt should have max TRACE_EVENT_STR_SIZE chars, no \n required as it is added by the tracer */

/*---------------------------------------------------------------------*
 *  global data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  inline functions and function-like macros                          *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/
#else
#define trace_init(...)   do {} while (0)
#define trace_opcode(...) do {} while (0)
#define trace_event(...)  do {} while (0)
#define TRACE_EVENT(...)  do {} while (0)
#endif
#endif /* #ifndef _TRACE_H_ */
