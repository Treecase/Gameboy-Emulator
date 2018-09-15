/*
 * global state, etc.
 *
 */

#ifndef __COMMON_H
#define __COMMON_H


#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>


#define LEN(arr)    (sizeof(arr)/sizeof(*arr))
#define die()       exit (EXIT_FAILURE)

#define GETBIT(n, bit)  (((n) >> (bit)) & 1)
#define SETBIT(n, bit)  ((n) |=  (1 << (bit)))
#define RESBIT(n, bit)  ((n) &= ~(1 << (bit)))


typedef uint8_t  BYTE;
typedef  int8_t  SIGNED_BYTE;
typedef uint16_t WORD;


struct debugger {
    bool     enabled;
    uint16_t breakpoint;
};

enum emustates {
    EMUSTATE_NORMAL      = 1 << 0,
    EMUSTATE_DEBUG       = 1 << 1,
    EMUSTATE_DISASSEMBLE = 1 << 2,
};

struct emustate {
    bool            running;
    enum emustates  state;
    struct debugger debug;
};

/* defined in Z80.c */
extern struct emustate G_state;



static inline bool between (int n, int min, int max) {
    return min <= n && n <= max;
}



#endif

