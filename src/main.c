/*
 * A Gameboy emulator
 *
 */

#include "io.h"
#include "Z80.h"
#include "alarm.h"
#include "common.h"
#include "display.h"



/* Gameboy emulator */
int main (int argc, char *argv[]) {

    char *ROM_name = Z80_args (argc, argv);
    if (!ROM_name)
        die();

    Z80_load (ROM_name);
    Z80_init();

    mkalarm_cycle (456, display_scanline);


    /* TODO: fix ^D not exiting debugger properly */
    while (G_state.running) {
        Z80_frame();
        /* FIXME: stopgap to fix ^D bug */
        if (G_state.running == false)
            break;
        IO_update();
    }
    Z80_cleanup();

    return 0;
}

