/*
 * Display for Gameboy
 *
 */
/* TODO: maybe pull the colour palette out of low.c? */
/* FIXME: at startup, there are 3 frames drawn with no scanlines, then 1 frame drawn at scanline 138, then 1 frame drawn at scanline 154 */

#include "display.h"
#include "low.h"
#include "mem.h"
#include "cpu.h"
#include "logging.h"
#include "registers.h"
#include "alarm.h"

#include <string.h>     /* memset */



#define TILESIZE    16
#define SPRITESIZE  16

static AlarmID scanline_alarm;
static WORD sprites_to_draw[10];


static void searchOAM(void);
static void write_scanline(void);
static void hblank_start(void);

static void drawtile   (WORD tiledata_startaddr, int xpos, int ypos, int line);
static void drawsprite (WORD OAMaddr, int line, bool doublehigh);



/* display_init:  */
void display_init(void) {

    low_initdisplay();

    scanline_alarm = mkalarm_cycle (-1, NULL);
}

/* display_drawline: draw one scanline */
void display_scanline(void) {

    BYTE scanline = memgval (R_CURLINE);

    BYTE LCDSTAT  = memgval (R_LCDSTAT);
    BYTE LCDC = memgval (R_LCDCONT);

    /* check for scanline coincidence */
    if (memgval (R_CMPLINE) == scanline) {

        SETBIT(LCDSTAT, 2);
        memsval (R_LCDSTAT, LCDSTAT);

        if (GETBIT(LCDSTAT, 6)) {
            cpu_interrupt (INT_LCDC);
            debug ("SCANLINE COINC INTERRUPT (%u)", scanline);
        }
    }

    /* LCDC bit 7 = LCD enable */
    if (GETBIT(LCDC, 7)) {
        if (scanline <= 144) {
            searchOAM();
        }
        else {
            debug ("scanline %u", scanline);
            memsval (R_CURLINE, scanline + 1);
        }
    }
}

/* searchOAM: search for sprites that are on the current scanline */
static void searchOAM(void) {

    /* searching the OAM takes 80 cycles... */
    set_alarm_cycles (scanline_alarm, 80);
    set_alarm_func (scanline_alarm, write_scanline);


    BYTE scroll_y = memgval (R_SCROLLY);
    BYTE scanline = memgval (R_CURLINE);

    BYTE LCDC     = memgval (R_LCDCONT);
    BYTE LCDSTAT  = memgval (R_LCDSTAT);

    /* LCDSTAT mode 10 = searching OAM */
    if (GETBIT(LCDSTAT, 5)) {
        cpu_interrupt (INT_LCDC);
        debug ("LCDSTAT MODE 10 (oam search) INTERRUPT");
    }
    LCDSTAT = (LCDSTAT & ~3) | 2;
    memsval (R_LCDSTAT, LCDSTAT);


    bool doublehigh = GETBIT(LCDC, 2);
    int spr_height = doublehigh? 16 : 8;

    memset (sprites_to_draw, 0, sizeof(sprites_to_draw));

    /*  NOTE: there can only be 10 sprites per scanline */
    unsigned sprites_this_line = 0;
    for (unsigned i = 0; i < 40 && sprites_this_line < 10; ++i) {
        BYTE spr_y = memgval (0xFE00 + (i * 4));

        if (between (scanline+scroll_y, spr_y, spr_y+spr_height))
            sprites_to_draw[sprites_this_line++] = 0xFE00 + (i * 4);
    }
}

/* write_scanline: write the scanline to the screen */
static void write_scanline(void) {

    /* ...and drawing the scanline takes 172 cycles */
    set_alarm_cycles (scanline_alarm, 172);
    set_alarm_func  (scanline_alarm, hblank_start);


    BYTE scroll_y = memgval (R_SCROLLY);
    BYTE scanline = memgval (R_CURLINE);

    BYTE LCDSTAT  = memgval (R_LCDSTAT);
    BYTE LCDC     = memgval (R_LCDCONT);


    /* LCDSTAT mode 11 = writing to screen */
    LCDSTAT = (LCDSTAT & ~3) | 3;
    memsval (R_LCDSTAT, LCDSTAT);


    /* LCDC bit 3 = which Background Tile Table to use */
    /* 0 : $9800-$9BFF, 1 : $9C00-$9FFF */
    WORD BTT_addr = GETBIT(LCDC, 3)? 0x9C00 : 0x9800;
    /* LCDC bit 4 = which Tile Pattern Table to use */
    /* 0 : $8800-$97FF, 1 : $8000-$8FFF */
    WORD TPT_addr = GETBIT(LCDC, 4)? 0x8000 : 0x8800;

    /* draw the background */
    if (GETBIT(LCDC, 0)) {

        int BTT_row = (scanline + scroll_y) / 8;
        for (int tile = 0; tile < 32; ++tile) {

            /* each row in the BTT is 32 bytes */
            BYTE tilenumber = memgval (BTT_addr + (BTT_row * 32) + tile);

            WORD tiledata_addr;

            /* if we use the TPT at $8800, the BTT numbers are signed:
             * so the center of the table is at $9000 */
            if (GETBIT(LCDC, 4))
                tiledata_addr = TPT_addr + ((BYTE)tilenumber * TILESIZE);
            else {
                TPT_addr = 0x9000;
                tiledata_addr = TPT_addr + ((SIGNED_BYTE)tilenumber * TILESIZE);
            }

            drawtile (tiledata_addr,
                      tile * 8, scanline + scroll_y,
                      (scanline + scroll_y) % 8);
        }
    }

    /* draw the window */
    /* TODO: window drawing */
#if 0
    /* 1 = $9C00-$9FFF, 0 = $9800-$9BFF */
    WORD WTT_addr = GETBIT(LCDC, 6)? 0x9C00 : 0x9800;

    /* draw the window */
    if (GETBIT(LCDC, 5)) {

        int WTT_row = scanline / 8;
        for (int tile = 0; tile < 32; ++tile) {

            /* each row in the WTT is 32 bytes */
            BYTE tilenumber = memgval (WTT_addr + (WTT_row * 32) + tile);

            WORD tiledata_addr = TPT_addr + (tilenumber * TILESIZE);

            drawtile (tiledata_addr,
                      tile * 8, scanline,
                      scanline % 8);
        }
    }
#endif

    /* draw the sprites */
    if (GETBIT(LCDC, 1)) {

        /* LCDC bit 2 -- 0 : 8x8, 1 : 8x16 */
        bool doublehigh = GETBIT(LCDC, 2);
        int spr_height = doublehigh? 16 : 8;

        for (unsigned i = 0; i < 10; ++i)
            if (sprites_to_draw[i] != 0x0000)
                drawsprite (sprites_to_draw[i],
                            (scanline + scroll_y) % spr_height,
                            doublehigh);
    }
}

/* hblank_start:  */
static void hblank_start(void) {

    set_alarm_func (scanline_alarm, NULL);

    BYTE scanline = memgval (R_CURLINE);
    BYTE LCDSTAT  = memgval (R_LCDSTAT);

    /* LCDSTAT mode 00 = hblank */
    if (GETBIT(LCDSTAT, 3)) {
        cpu_interrupt (INT_LCDC);
        debug ("LCDSTAT MODE 00 (hblank) INTERRUPT");
    }
    LCDSTAT = LCDSTAT & ~3;
    memsval (R_LCDSTAT, LCDSTAT);

    debug ("scanline %u drawn", scanline);
    memsval (R_CURLINE, scanline + 1);
}


/* display_update: update the screen */
void display_update(void) {

    BYTE LCDSTAT = memgval (R_LCDSTAT);

    /* LCDSTAT mode 01 = vblank */
    if (GETBIT(LCDSTAT, 4)) {
        cpu_interrupt (INT_LCDC);
        debug ("LCDSTAT MODE 01 (vblank) INTERRUPT");
    }
    LCDSTAT = (LCDSTAT & ~3) | 1;
    memsval (R_LCDSTAT, LCDSTAT);
    memsval (R_CURLINE, 0);

    cpu_interrupt (INT_VBLANK);

    debug ("frame drawn");

    low_update();
}

/* dis_logging:  */
void dis_logging (bool enable) {
    LOG_DOLOG = enable;
}

/* INTERNAL FUNCTIONS */
/* drawsprite:  */
static void drawsprite (WORD spriteOAMaddr, int line, bool doublehigh) {
#define PALETTE_GETCOLOUR(pal, index)   (((pal) >> (2 * (index))) & 3)

    BYTE yoff  = memgval (spriteOAMaddr + 0),
         xoff  = memgval (spriteOAMaddr + 1),
         index = memgval (spriteOAMaddr + 2),
         flags = memgval (spriteOAMaddr + 3);

    bool above_window    = GETBIT(flags, 7),    /* TODO: figure out how to handle this */
         y_flip          = GETBIT(flags, 6),
         x_flip          = GETBIT(flags, 5),
         using_palette_1 = GETBIT(flags, 4);

    /* in 8x16 mode, the least significant bit is ignored (=0) */
    if (doublehigh)
        index &= ~1;

    BYTE palette = using_palette_1? memgval (R_OBJ1PAL) : memgval (R_OBJ0PAL);
    WORD sprdata_addr = 0x8000 + (index * SPRITESIZE);

    BYTE byte1,
         byte2;
    if (y_flip) {
        byte1 = memgval (sprdata_addr + ((7 - line) * 2) + 0),
        byte2 = memgval (sprdata_addr + ((7 - line) * 2) + 1);
    }
    else {
        byte1 = memgval (sprdata_addr + (line * 2) + 0),
        byte2 = memgval (sprdata_addr + (line * 2) + 1);
    }


    /* draw the line */
    for (int x = 0; x < 8; ++x) {

        int bit = x_flip? x : 7-x;
        BYTE palette_index = GETBIT(byte1, bit) | (GETBIT(byte2, bit) << 1);

        BYTE pixel = PALETTE_GETCOLOUR(palette, palette_index);

        if (pixel == 0)
            continue;

        low_drawpixel ((xoff -  8) + x,
                       (yoff - 16) + line,
                       pixel);
    }
}

/* drawtile: draw a line of a tile */
static void drawtile (WORD tiledata_startaddr, int xpos, int ypos, int line) {

    BYTE palette = memgval (R_BGRDPAL);

    int xoff = xpos - memgval (R_SCROLLX),
        yoff = ypos - memgval (R_SCROLLY);

    BYTE byte1 = memgval (tiledata_startaddr + (line*2) + 0),
         byte2 = memgval (tiledata_startaddr + (line*2) + 1);

    for (int x = 0; x < 8; ++x) {

        BYTE palette_index = GETBIT(byte1, 7-x) | (GETBIT(byte2, 7-x) << 1);

        BYTE pixel = PALETTE_GETCOLOUR(palette, palette_index);

        low_drawpixel ((xoff + x) % 256,
                        yoff      % 256,
                       pixel);
    }
#undef PALETTE_GETCOLOUR
}

