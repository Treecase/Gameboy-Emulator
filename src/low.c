/*
 * Low level interfacing b/t emu and OS
 * (ie for drawing, IO, etc.)
 *
 */ 

#include "io.h"
#include "low.h"
#include "Z80.h"
#include "mem.h"
#include "common.h"
#include "logging.h"
#include "registers.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>


#define REAL_W      (SCR_W * G_scale)
#define REAL_H      (SCR_H * G_scale)



/* TEMP */
static bool _DEBUG_draw_full_screen = false;

void low_clearscreen(void);
void low_cleanup(void);



static Display         *conn = NULL;
static Window          window;
static char            *colour_names[4] = { "rgb:ff/ff/ff","rgb:aa/aa/aa","rgb:55/55/55","rgb:00/00/00" };
static GC              palette[4];
static GC              white;
static Pixmap          buffer;

static bool initialized = false;

static unsigned SCR_W = 160;
static unsigned SCR_H = 144;

unsigned int G_scale = 1;   /* TODO: rename G_scale */



/* low_initdisplay: initialize the display */
void low_initdisplay(void) {

    /* setup keybinds */
    /* TODO: move this? */
    keybinds[BTN_UP]     = XK_W;
    keybinds[BTN_DOWN]   = XK_S;
    keybinds[BTN_LEFT]   = XK_A;
    keybinds[BTN_RIGHT]  = XK_D;

    keybinds[BTN_A]      = XK_M;
    keybinds[BTN_B]      = XK_N;
    keybinds[BTN_START]  = XK_space;
    keybinds[BTN_SELECT] = XK_Alt_R;

    keybinds[_ZOOMIN]    = XK_plus;
    keybinds[_ZOOMOUT]   = XK_minus;
    keybinds[_QUIT]      = XK_Escape;


    if (!initialized) {
        conn = XOpenDisplay (NULL);
        if (!conn)
            fatal ("failed to open X connection!\n");

        /* setup colours */
        XColor colours[4];
        for (int i = 0; i < 4; ++i) {
            XParseColor (conn,
                         XDefaultColormap (conn, XDefaultScreen (conn)),
                         colour_names[i],
                         &colours[i]);
            XAllocColor (conn,
                         XDefaultColormap (conn, XDefaultScreen (conn)),
                         &colours[i]);
        }


        /* create window */
        window = XCreateSimpleWindow (conn,
                                      XDefaultRootWindow (conn),
                                      0,0,
                                      REAL_W,REAL_H,
                                      0,0,
                                      0);

        /* create backbuffer */
        buffer = XCreatePixmap (conn,
                                window,
                                256 * G_scale,256 * G_scale,
                                DefaultDepth (conn, DefaultScreen (conn)));

        white = XCreateGC (conn, window, 0, NULL);
        XSetForeground (conn, white, BlackPixel (conn, XDefaultScreen (conn)));

        /* set palette */
        for (int i = 0; i < 4; ++i) {
            palette[i] = XCreateGC (conn, window, 0, NULL);
            XSetForeground (conn, palette[i], colours[i].pixel);
        }

        /* map + set up window */
        XMapWindow (conn, window);

        XSelectInput (conn,
                      window,
                      StructureNotifyMask | KeyPressMask | KeyReleaseMask);
        XStoreName (conn, window, "GB");

        /* wait for window to be mapped */
        XEvent e;
        do {
            XNextEvent (conn, &e);
        } while (e.type != MapNotify);

        initialized = true;


        low_clearscreen();
        atexit (low_cleanup);
    }
    else
        error ("attempt to call low_initdisplay after already init!");
}

/* low_wholeboard: read the state of the controller */
void low_wholeboard(void) {
#define KEYSTATE(map, keycode)  (((map)[(keycode)/8] & (1 << ((keycode) % 8)))? true : false)
    if (!initialized)
        return;

    char map[32];
    KeyCode keycode;

    /* get keyboard state */
    XQueryKeymap (conn, map);

    /* get the state of mapped keys */
    for (unsigned i = 0; i < _NUM_BTNS; ++i) {
        keycode = XKeysymToKeycode (conn, keybinds[i]);
        controller[i] = KEYSTATE(map, keycode);
    }

    /* TEMP */
    static bool _DEBUG_old_toggle_fullscreen = false;
    bool _DEBUG_toggle_fullscreen = KEYSTATE(map, XKeysymToKeycode (conn, XK_F));

    if (_DEBUG_toggle_fullscreen && !_DEBUG_old_toggle_fullscreen)
        _DEBUG_draw_full_screen = !_DEBUG_draw_full_screen;
    _DEBUG_old_toggle_fullscreen = _DEBUG_toggle_fullscreen;

#undef KEYSTATE
}

/* low_drawpixel: draw a pixel */
void low_drawpixel (unsigned x, unsigned y, unsigned ind) {
    if (!initialized)
        return;

    x %= 256;
    y %= 256;
    x *= G_scale;
    y *= G_scale;

    if (ind > 3)
        fatal ("invalid color: %d", ind);

    XFillRectangle (conn,
                    buffer,
                    palette[ind],
                    x,y,
                    G_scale,G_scale);
}

/* low_update: update the screen */
void low_update(void) {
    if (!initialized)
        return;

    unsigned  width = _DEBUG_draw_full_screen? 256*G_scale : REAL_W,
             height = _DEBUG_draw_full_screen? 256*G_scale : REAL_H;

    XCopyArea (conn,
               buffer, window,
               palette[0],
               0,0,
               width,height,
               0,0);

    XFlush (conn);

    low_clearscreen();
}

/* INTERNAL FNs */
/* low_clearscreen:  */
void low_clearscreen(void) {
    if (!initialized)
        return;

    XFillRectangle (conn,
                    buffer,
                    white,
                    0,0,
                    REAL_W,REAL_H);
}

/* low_cleanup: clean up */
void low_cleanup(void) {

    if (initialized) {

        XFreePixmap (conn, buffer);
        buffer = -1;

        for (int i = 0; i < 4; ++i) {
            XFreeGC (conn, palette[i]);
            palette[i] = NULL;
        }

        XFreeGC (conn, white);
        white = NULL;

        XCloseDisplay (conn);
        conn = NULL;

        initialized = false;
    }
    else
        error ("attempt to call low_cleanup after already cleaned!");
}

