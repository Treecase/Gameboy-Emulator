/*
 * Gameboy CPU stuff
 *
 */

#ifndef __CPU_H
#define __CPU_H


#include "common.h"

#include <stdbool.h>



#define FLAGZ   (((*F) >> 7) & 1)
#define FLAGN   (((*F) >> 6) & 1)
#define FLAGH   (((*F) >> 5) & 1)
#define FLAGC   (((*F) >> 4) & 1)



/* REGISTERS */
/* program counter (NEXT byte in the program) */
WORD pc;

/* stack pointer (decrements BEFORE pushing) */
WORD sp;


/* registers 4 16-bit/8 8-bit */
WORD *AF,
     *BC,
     *DE,
     *HL;

/* Flag register F:
 *
 *  7 Z - zero flag       : math op returns 0
 *  6 N - subtract flag   : last math op was a sub
 *  5 H - half-carry flag : carry from lower nibble in last math op
 *  4 C - carry flag      : carry in last math op
 * 3-0  - unused
 */
BYTE  *A, *F,
      *B, *C,
      *D, *E,
      *H, *L;

/* interrupt master enable */
bool IME;


/* interrupt:
 *  enum for interrupt types
 */
enum interrupt {
    INT_VBLANK  = 1 << 0,
    INT_LCDC    = 1 << 1,
    INT_TIMER   = 1 << 2,
    INT_SERIAL  = 1 << 3,
    INT_KPINPUT = 1 << 4,
};



void cpu_init(void);
unsigned cpu_cycle(void);
void cpu_interrupt (enum interrupt int_type);

void cpu_logging (bool enable);
bool cpu_logging_enabled(void);


#endif

