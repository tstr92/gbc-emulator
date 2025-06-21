
/*---------------------------------------------------------------------*
 *                                                                     *
 *                         SM83 Memory Bus                             *
 *                                                                     *
 *                                                                     *
 *       project: Gameboy Color Emulator                               *
 *   module name: bus.h                                                *
 *        author: tstr92                                               *
 *          date: 2024-04-21                                           *
 *                                                                     *
 *---------------------------------------------------------------------*/

#ifndef _BUS_H_
#define _BUS_H_

/*---------------------------------------------------------------------*
 *  Memory Map                                                         *
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

/*---------------------------------------------------------------------*
 *  additional includes                                                *
 *---------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------*
 *  global definitions                                                 *
 *---------------------------------------------------------------------*/
#define IRQ_FLAGS_ADDRESS 0xFF0F
#define IRQ_VBLANK        (1<<0)
#define IRQ_LCD           (1<<1)
#define IRQ_TIMER         (1<<2)
#define IRQ_SERIAL        (1<<3)
#define IRQ_JOYPAD        (1<<4)
#define BUS_SET_IRQ(_irq) bus_set_memory(IRQ_FLAGS_ADDRESS, bus_get_memory(IRQ_FLAGS_ADDRESS) | _irq)

/*---------------------------------------------------------------------*
 *  type declarations                                                  *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  function prototypes                                                *
 *---------------------------------------------------------------------*/
void bus_init(void);
void bus_tick(void);
bool bus_DMG_mode(void);
uint8_t bus_get_memory(uint16_t addr);
void bus_set_memory(uint16_t addr, uint8_t val);
bool bus_init_memory(const char *filename);
int bus_load_game(char *fileName);

void bus_stop_instr_cb(void);
void bus_HBlank_cb(void);

void gbc_bus_write_internal_state(void);
int gbc_bus_set_internal_state(void);

/*---------------------------------------------------------------------*
 *  global data                                                        *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  inline functions and function-like macros                          *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  eof                                                                *
 *---------------------------------------------------------------------*/

 #endif /* #ifndef _BUS_H_ */
