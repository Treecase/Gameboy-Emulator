#!/usr/bin/python3
#
# This script is used to automatically generate cpu_timing.c,
# using the tables in opcode_timings.txt. This makes it easy
# to change incorrect opcode times.
#



print ("""/*
 * Gameboy CPU opcode timings
 *
 */

#include "common.h"

""")


with open ('opcode_timings.txt', 'r') as f:

    print ('static const unsigned noprefix_cycles_true[256]\n    = {')

    linecounter = 0x00
    while linecounter < 0x100:
        line = f.readline()

        print ('        /* 0x' + hex(linecounter)[2:].rjust (2, '0') + ' */')
        print ('        ', end='')
        for time in line.split():
            print (time + ',', end=' ')
        print()
        linecounter += 0x10
    print ('};\n')


    line = f.readline()
    while line == '\n':
        line = f.readline()

    print ('static const unsigned noprefix_cycles_false[256]\n  = {')

    linecounter = 0x00
    while linecounter < 0x100:

        print ('        /* 0x' + hex(linecounter)[2:].rjust (2, '0') + ' */')
        print ('        ', end='')
        for time in line.split():
            print (time + ',', end=' ')

        print()
        linecounter += 0x10
        line = f.readline()
    print ('};\n')


print ("""
/* op_cycles: how many cycles an instruction takes */
unsigned op_cycles (BYTE prefix, BYTE opcode, bool yes) {

    /* most instructions take 4 cycles */
    unsigned cycles = 4;


    if (prefix == 0xCB) {
        cycles = 8;

        BYTE tmp = opcode & 0x0F;

        if (tmp == 0x06 || tmp == 0x0E)
            cycles = 16;
    }
    else {
        if (yes)
            cycles = noprefix_cycles_true[opcode];
        else
            cycles = noprefix_cycles_false[opcode];
    }

    return cycles;
}
""")

