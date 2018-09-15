/*
 * Low level interfacing b/t emu and OS
 * (ie for drawing, IO, etc.)
 *
 */

#ifndef __LOW_H
#define __LOW_H


#include <X11/keysym.h>
#include <X11/X.h>
#include <stdbool.h>



typedef KeySym KeyID;


/* defined in low.c, used in Z80.c */
extern unsigned G_scale;

void low_initdisplay(void);

void low_wholeboard(void);

void low_drawpixel (unsigned x, unsigned y, unsigned ind);
void low_update(void);


#endif

