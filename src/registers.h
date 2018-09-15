/*
 * Gameboy special registers
 *
 */

#ifndef __SPECIAL_REGISTERS_H
#define __SPECIAL_REGISTERS_H


enum {
    R_JOYPAD      = 0xFF00,
    R_SIODATA     = 0xFF01,
    R_SIOCONT     = 0xFF02,

    R_DIVIDER     = 0xFF04,
    R_TIMECNT     = 0xFF05,
    R_TIMEMOD     = 0xFF06,
    R_TIMCONT     = 0xFF07,

    R_IFLAGS      = 0xFF0F,
    R_SNDREG10    = 0xFF10,
    R_SNDREG11    = 0xFF11,
    R_SNDREG12    = 0xFF12,
    R_SNDREG13    = 0xFF13,
    R_SNDREG14    = 0xFF14,
    R_SNDREG21    = 0xFF16,
    R_SNDREG22    = 0xFF17,
    R_SNDREG23    = 0xFF18,
    R_SNDREG24    = 0xFF19,
    R_SNDREG30    = 0xFF1A,
    R_SNDREG31    = 0xFF1B,
    R_SNDREG32    = 0xFF1C,
    R_SNDREG33    = 0xFF1D,
    R_SNDREG34    = 0xFF1E,

    R_SNDREG41    = 0xFF20,
    R_SNDREG42    = 0xFF21,
    R_SNDREG43    = 0xFF22,
    R_SNDREG44    = 0xFF23,
    R_SNDREG50    = 0xFF24,
    R_SNDREG51    = 0xFF25,
    R_SNDREG52    = 0xFF26,

    R_LCDCONT     = 0xFF40,
    R_LCDSTAT     = 0xFF41,
    R_SCROLLY     = 0xFF42,
    R_SCROLLX     = 0xFF43,
    R_CURLINE     = 0xFF44,
    R_CMPLINE     = 0xFF45,
    R_DMACONT     = 0xFF46,
    R_BGRDPAL     = 0xFF47,
    R_OBJ0PAL     = 0xFF48,
    R_OBJ1PAL     = 0xFF49,
    R_WNDPOSY     = 0xFF4A,
    R_WNDPOSX     = 0xFF4B,

    R_ISWITCH     = 0xFFFF
};


/* NOTE: this is just used by cpu_print*.c,
 *       so the compiler warns about unused
 *       functions a lot -- these don't matter,
 *       so I disabled them :) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static char *register_name (unsigned offset) {
    switch (0xFF00 + offset) {
    case R_JOYPAD  : return "JOYPAD"  ;
    case R_SIODATA : return "SIODATA" ;
    case R_SIOCONT : return "SIOCONT" ;

    case R_DIVIDER : return "DIVIDER" ;
    case R_TIMECNT : return "TIMECNT" ;
    case R_TIMEMOD : return "TIMEMOD" ;
    case R_TIMCONT : return "TIMCONT" ;

    case R_IFLAGS  : return "IFLAGS"  ;
    case R_SNDREG10: return "SNDREG10";
    case R_SNDREG11: return "SNDREG11";
    case R_SNDREG12: return "SNDREG12";
    case R_SNDREG13: return "SNDREG13";
    case R_SNDREG14: return "SNDREG14";
    case R_SNDREG21: return "SNDREG21";
    case R_SNDREG22: return "SNDREG22";
    case R_SNDREG23: return "SNDREG23";
    case R_SNDREG24: return "SNDREG24";
    case R_SNDREG30: return "SNDREG30";
    case R_SNDREG31: return "SNDREG31";
    case R_SNDREG32: return "SNDREG32";
    case R_SNDREG33: return "SNDREG33";
    case R_SNDREG34: return "SNDREG34";

    case R_SNDREG41: return "SNDREG41";
    case R_SNDREG42: return "SNDREG42";
    case R_SNDREG43: return "SNDREG43";
    case R_SNDREG44: return "SNDREG44";
    case R_SNDREG50: return "SNDREG50";
    case R_SNDREG51: return "SNDREG51";
    case R_SNDREG52: return "SNDREG52";

    case R_LCDCONT : return "LCDCONT" ;
    case R_LCDSTAT : return "LCDSTAT" ;
    case R_SCROLLY : return "SCROLLY" ;
    case R_SCROLLX : return "SCROLLX" ;
    case R_CURLINE : return "CURLINE" ;
    case R_CMPLINE : return "CMPLINE" ;
    case R_DMACONT : return "DMACONT" ;
    case R_BGRDPAL : return "BGRDPAL" ;
    case R_OBJ0PAL : return "OBJ0PAL" ;
    case R_OBJ1PAL : return "OBJ1PAL" ;
    case R_WNDPOSY : return "WNDPOSY" ;
    case R_WNDPOSX : return "WNDPOSX" ;

    case R_ISWITCH : return "ISWITCH" ;


    default        : return "RAM"     ;
    }
}
#pragma GCC diagnostic pop

#endif

