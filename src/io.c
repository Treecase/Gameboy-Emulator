/*
 * Gameboy input
 *
 */

#include "io.h"
#include "Z80.h"
#include "common.h"
#include "low.h"
#include "logging.h"


static bool old_btn_states[_NUM_BTNS];

/* IO_keydown: returns key pressed state */
bool IO_btndown (Keyname key, bool *oldstate) {

    if (key > _NUM_BTNS)
        fatal ("IO_keydown - invalid key %u\n", key);

    bool state = controller[key];

    if (oldstate != NULL)
        *oldstate = old_btn_states[key];

    return state;
}

/* IO_update:  */
void IO_update(void) {
#define PRESSED_THIS_FRAME(key) (!old_btn_states[key] && controller[key])
    /* update old states */
    for (unsigned i = 0; i < _NUM_BTNS; ++i)
        old_btn_states[i] = controller[i];

    low_wholeboard();

    /* exit when quit button is pressed */
    G_state.running = !controller[_QUIT];

    /* zoom in/out */
    /* FIXME: zooms in real fast */
    if (PRESSED_THIS_FRAME(_ZOOMIN) && G_scale < 10)
        G_scale++;
    if (PRESSED_THIS_FRAME(_ZOOMOUT) && G_scale > 1)
        G_scale--;

#undef PRESSED_THIS_FRAME
}

/* IO_print_help: print help */
void IO_print_help (char *name, bool help) {
    keybinds[0] = keybinds[0];  /* to shut the compiler up */

    printf ("Usage: %s [OPTIONS] ROMNAME\n", name);
    printf ("Try '%s --help' for more information.\n", name);

    if (help) {
        puts ("FILENAME is the path to a Gameboy ROM");
        printf ("Example: %s Tetris.gb\n\n", name);

        puts (" -s, --scale N\t\tscale display by N times");
        puts ("     --nolog=[czmd]\tdisable logging for some module:");
        puts ("\t\t\tc=cpu, z=Z80, m=memory, d=display");
        puts ("     --break N\t\tset a breakpoint at address");
        puts ("     --disassemble\tprint a disassembly of ROM");
        puts ("     --bios\t\trun the BIOS (scrolling Nintendo logo)");
        puts (" -h, --help\t\tdisplay this help and exit\n\n");
    }
}

