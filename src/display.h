/*
 * Display for Gameboy
 *
 */

#ifndef __DISPLAY_H
#define __DISPLAY_H


#include <stdbool.h>



void display_init(void);

void display_scanline(void);

void display_update(void);
void display_clear(void);

void dis_logging (bool enable);


#endif

