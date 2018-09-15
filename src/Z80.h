/*
 * Gameboy emu main stuff
 *
 */

#ifndef __Z80_H
#define __Z80_H


#include <stdbool.h>



char *Z80_args (int argc, char *argv[]);

void Z80_init(void);
void Z80_cleanup(void);

void Z80_load (char *fname);
void Z80_frame(void);

void Z80_update_timer_frequency (bool hard);

void Z80_logging (bool enable);


#endif

