/*
 * Gameboy memory module
 *
 */

#ifndef __MEM_H
#define __MEM_H


#include "common.h"

#include <stdbool.h>



void mem_init (bool use_bootROM);
void mem_loadcart (char *fname);

BYTE  memgval (WORD byte);
void  memsval (WORD byte, BYTE value);

void mem_set_register (WORD location, BYTE byte);

void mem_logging (bool enable);
bool mem_log_enabled(void);


#endif

