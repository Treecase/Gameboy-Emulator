/*
 * Gameboy input
 *
 */

#ifndef __IO_H
#define __IO_H


#include "low.h"
#include <stdbool.h>



enum _keynums {
    BTN_RIGHT   = 0,
    BTN_LEFT    = 1,
    BTN_UP      = 2,
    BTN_DOWN    = 3,

    BTN_A       = 4,
    BTN_B       = 5,
    BTN_SELECT  = 6,
    BTN_START   = 7,

    _ZOOMIN        ,
    _ZOOMOUT       ,
    _QUIT          ,
    _NUM_BTNS
};

typedef enum _keynums Keyname;

KeyID keybinds[_NUM_BTNS];
bool  controller[_NUM_BTNS];


bool IO_btndown (Keyname key, bool *oldstate);
void IO_update(void);

void IO_log (int log_lvl, char *format, ...);

void IO_print_help (char *name, bool help);


#endif

