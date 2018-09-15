/*
 * Gameboy memory module
 *
 */
/* FIXME: There seems to be some problems with banking */

#include "mem.h"
#include "logging.h"

#include "io.h"         /* IO_btndown, IO_update */
#include "cpu.h"        /* cpu_interrupt */
#include "Z80.h"        /* Z80_update_timer_frequency */

#include <fcntl.h>      /* open */
#include <stdlib.h>     /* malloc, etc. */
#include <string.h>     /* memcmp */
#include <unistd.h>     /* lseek, close */
#include <sys/mman.h>   /* mmap */
#include <assert.h>     /* assert */


/* banking stuff */
static enum {
    MMU_UNSUP = -2,
    MMU_UNKN  = -1,
    MMU_NONE  =  0,

    MMU_MBC1  =  1,
    MMU_MBC2  =  2,
    MMU_MBC3  =  3,
    MMU_MBC5  =  5,

    MMU_RMBL,   /* rumble carts are basically MBC5s */
    MMU_HUC1,   /* HUC-1s are basically MBC1s       */
} MBC_type;

static bool MBC_mode,
            RAM_enabled = true;

/* memory areas */
static BYTE **RAMbank;
static BYTE **ROMbank;
static BYTE *RAM;

static unsigned ROMbankcount = 0,
                RAMbankcount = 0,
                ROMbank_i = 1,
                RAMbank_i = 0;

static bool MODE_STARTUP = false;
static BYTE bios[256];


static void alloc_memory_regions (BYTE *cart);
static void print_ROM_info (BYTE *cart);
static void trap_register_write (WORD location, BYTE byte);
static void trap_ROM_write (WORD location, BYTE byte);
static BYTE readinput (BYTE button_set);
static BYTE get_serial_byte(void);



/* mem_cleanup:  */
static void mem_cleanup(void) {

    free (RAM);
    RAM = NULL;

    for (unsigned i = 0; i < ROMbankcount; ++i)
        free (ROMbank[i]);
    free (ROMbank);

    for (unsigned i = 0; i < RAMbankcount; ++i)
        free (RAMbank[i]);
    free (RAMbank);
}

/* mem_init:  */
void mem_init (bool use_bootROM) {

    /*          MEMORY MAP
     *          ==========
     *
     * Interrupt Enable Register
     * --------------------------- FFFF
     * Internal RAM
     * --------------------------- FF80
     * Empty but unusable for I/O
     * --------------------------- FF4C
     * I/O ports
     * --------------------------- FF00
     * Empty but unusable for I/O
     * --------------------------- FEA0
     * Sprite Attrib Memory (OAM)
     * --------------------------- FE00
     * Echo of 8kB Internal RAM
     * --------------------------- E000
     * 8kB Internal RAM
     * --------------------------- C000
     * 8kB switchable RAM bank       
     * --------------------------- A000
     * 8kB Video RAM                 
     * --------------------------- 8000 --
     * 16kB switchable ROM bank          |
     * --------------------------- 4000  |= 32kB Cartridge
     * 16kB ROM bank #0                  |
     * --------------------------- 0000 --
     */

    if (use_bootROM) {

        FILE *biosfile = fopen ("errata/DMG_ROM.bin", "r");
        if (!biosfile)
            fatal ("failed to open BIOS");

        fread (bios, 1, 0x100, biosfile);
        fclose (biosfile);

        MODE_STARTUP = true;
    }
    else
        MODE_STARTUP = false;

    atexit (mem_cleanup);
}

/* mem_loadcart: load the cartridge into memory */
void mem_loadcart (char *fname) {

    FILE *cart_file = fopen (fname, "r");
    if (cart_file == NULL)
        fatal ("failed to load '%s'", fname);

    /* alloc + read ROM information area */
    BYTE *buf = calloc (1, 0x150);
    fseek (cart_file, 0L, SEEK_SET);
    fread (buf, sizeof(BYTE), 0x150, cart_file);
    fseek (cart_file, 0L, SEEK_SET);

    /* allocate the RAM/ROM banks + print ROM info*/
    alloc_memory_regions (buf);
    print_ROM_info (buf);

    free (buf);

    /* copy ROM data into ROM banks */
    for (unsigned i = 0; i < ROMbankcount; ++i)
        fread (ROMbank[i], sizeof(BYTE), 0x4000, cart_file);

    /* allocate unbanked RAM area ($8000-$FFFF) */
    RAM = calloc (1, 0x8000);

    fclose (cart_file);
}

/* memgval: get value of some byte */
BYTE memgval (WORD location) {

    assert (ROMbank_i != 0);
    assert (ROMbank_i < ROMbankcount);
    assert (RAMbank_i < RAMbankcount);

    /* unused memory reads return $FF */
    BYTE byte = 0xFF;

    /* RAM at $8000-$FFFF */
    if (location >= 0x8000) {
        /* switchable RAM bank at $A000-$BFFF */
        if (between (location, 0xA000, 0xBFFF)) {
            if (RAM_enabled) {
                byte = RAMbank[RAMbank_i][location - 0xA000];
            }
            else
                error ("READ: RAM not enabled");
        }
        /* unbanked RAM at $8000-$9FFF, $C000-$FFFF */
        else
            byte = RAM[location - 0x8000];
    }
    /* bios ROM at $0000-$00FF while running */
    else if (MODE_STARTUP && location < 0x0100) {
        byte = bios[location];
    }
    /* ROM at $0000-$7FFF */
    else {
        /* ROM bank 0 at $0000-$3FFF */
        if (location < 0x4000) {
            byte = ROMbank[0][location];
        }
        /* switchable ROM banks at $4000-$7FFF */
        else
            byte = ROMbank[ROMbank_i][location - 0x4000];
    }

    /* because memory accesses can happen multiple times per
     * instruction, these debug calls kill the framerate */
//    debug ("read %.2hhX from %.4hX", byte, location);
    return byte;
}

/* mem_set_register: set a register without triggering any side-effects (used by Z80.c) */
void mem_set_register (WORD location, BYTE byte) {

    if (between (location, 0xFF00, 0xFFFF)) {
        RAM[0x8000 - location] = byte;
    }
    else
        error ("%.4hX is not an IO register!", location);

}

/* memsval: set a byte in memory */
void memsval (WORD location, BYTE byte) {

    assert (ROMbank_i != 0);
    assert (ROMbank_i < ROMbankcount);
    assert (RAMbank_i < RAMbankcount);

    /* because memory accesses can happen multiple times per
     * instruction, these debug calls kill the framerate */
//    debug ("writing %.2hhX to %.4hX", byte, location);
    /* RAM at $8000-$FFFF */
    if (location >= 0x8000) {
        /* banked RAM at $A000-$BFFF */
        if (between (location, 0xA000, 0xBFFF)) {
            if (RAM_enabled) {
                RAMbank[RAMbank_i][location - 0xA000] = byte;
            }
            else {
                error ("WRITE: RAM not enabled");
            }
        }
        /* unbanked RAM at $8000-$9FFF, $C000-$FFFF */
        else {
            RAM[location - 0x8000] = byte;

            /* IO registers at $FF00-$FFFF */
            if (between (location, 0xFF00, 0xFFFF)) {
                trap_register_write (location, byte);
            }
            /* generic RAM */
            else {
                /* memory mirroring between $E000-$FE00 and $C000-$DE00 */
                if (between (location, 0xE000, 0xFE00-1)) {
                    RAM[0xC000 + (location - 0x8000 - 0xE000)] = byte;
                }
                else if (between (location, 0xC000, 0xDE00-1)) {
                    RAM[0xE000 + (location - 0x8000 - 0xC000)] = byte;
                }
            }
        }
    }
    /* ROM goes from $0000-$7FFF */
    else {
        trap_ROM_write (location, byte);
    }
}

/* mem_logging:  */
void mem_logging (bool enable) {
    LOG_DOLOG = enable;
}
/* mem_log_enabled:  */
bool mem_log_enabled(void) {
    return LOG_DOLOG;
}


/* INTERNAL FNs */
/* alloc_memory_regions: allocate ROM/RAM banks + set MBC type */
static void alloc_memory_regions (BYTE *cart) {

    /* MBC type is found at $0147 */
    switch (cart[0x0147]) {
    case 0x00:                      break; // ROM ONLY
    case 0x08:                      break; // ROM+RAM
    case 0x09: MBC_type = MMU_NONE; break; // ROM+RAM+BATTERY

    case 0x01:                             // ROM+MBC1
    case 0x02:                             // ROM+MBC1+RAM
    case 0x03: MBC_type = MMU_MBC1; break; // ROM+MBC1+RAM+BATT

    case 0x05:                      break; // ROM+MBC2
    case 0x06: MBC_type = MMU_MBC2; break; // ROM+MBC2+BATTERY

    case 0x0B: MBC_type = MMU_UNSUP;break; // ROM+MMM01
    case 0x0C: MBC_type = MMU_UNSUP;break; // ROM+MMM01+SRAM
    case 0x0D: MBC_type = MMU_UNSUP;break; // ROM+MMM01+SRAM+BATT

    case 0x0F:                      break; // ROM+MBC3+TIMER+BATT
    case 0x10:                      break; // ROM+MBC3+TIMER+RAM+BATT
    case 0x11:                      break; // ROM+MBC3
    case 0x12:                      break; // ROM+MBC3+RAM
    case 0x13: MBC_type = MMU_MBC3; break; // ROM+MBC3+RAM+BATT

    case 0x19:                      break; // ROM+MBC5
    case 0x1A:                      break; // ROM+MBC5+RAM
    case 0x1B: MBC_type = MMU_MBC5; break; // ROM+MBC5+RAM+BATT

    case 0x1C:                      break; // ROM+MBC5+RUMBLE
    case 0x1D:                      break; // ROM+MBC5+RUMBLE+SRAM
    case 0x1E: MBC_type = MMU_RMBL; break; // ROM+MBC5+RUMBLE+SRAM+BATT

    case 0xFF: MBC_type = MMU_HUC1; break; // Hudson HuC-1

    case 0x1F: MBC_type = MMU_UNSUP;break; // Pocket Camera
    case 0xFD: MBC_type = MMU_UNSUP;break; // Bandai TAMA5
    case 0xFE: MBC_type = MMU_UNSUP;break; // Hudson HuC-3

    default:   MBC_type = MMU_UNKN; break;
    }
    if (MBC_type == MMU_UNKN)   fatal ("unknown memory controller ($%.2hhX)", cart[0x0147]);
    if (MBC_type == MMU_UNSUP)  fatal ("unsupported memory controller ($%.2hhX)", cart[0x0147]);

    /* ROM bank count is found at $0148 */
    switch (cart[0x0148]) {
    case 0x00: ROMbankcount =   2; break;
    case 0x01: ROMbankcount =   4; break;
    case 0x02: ROMbankcount =   8; break;
    case 0x03: ROMbankcount =  16; break;
    case 0x04: ROMbankcount =  32; break;
    case 0x05: ROMbankcount =  64; break;
    case 0x06: ROMbankcount = 128; break;
    case 0x52: ROMbankcount =  72; break;
    case 0x53: ROMbankcount =  80; break;
    case 0x54: ROMbankcount =  96; break;
    default:
        fatal ("unknown ROM bank count ($%.2hhX)", cart[0x0148]);
        break;
    }

    /* RAM bank count is found at $0149 */
    switch (cart[0x0149]) {
    case 0: RAMbankcount =  0; break;
    case 1: RAMbankcount =  1; break;
    case 2: RAMbankcount =  1; break;
    case 3: RAMbankcount =  4; break;
    case 4: RAMbankcount = 16; break;
    default:
        fatal ("Unknown RAM bank count ($%.2hhX)", cart[0x0149]);
        break;
    }


    /* allocate ROM banks */
    ROMbank = malloc (sizeof(BYTE *) * ROMbankcount);
    for (unsigned i = 0; i < ROMbankcount; ++i)
        ROMbank[i] = calloc (0x4000, sizeof(BYTE));
    ROMbank_i = 1;

    /* allocate RAM banks */
    RAMbankcount += 1;
    RAM_enabled = false;

    RAMbank = malloc (sizeof(BYTE *) * RAMbankcount);
    for (unsigned i = 0; i < RAMbankcount; ++i)
        RAMbank[i] = calloc (0x4000, sizeof(BYTE));
}

/* print_ROM_info:  */
/* NOTE: exits when an unsupported thing is detected */
static void print_ROM_info (BYTE *cart) {

    printf ("TITLE: %.16s\n", (char *)(cart + 0x0134));

    switch (cart[0x0143]) {
    case 0x80:
        puts ("Gameboy Color compatible ROM");
        break;
    case 0xC0:
        puts ("Gameboy Color ROM");
	fatal ("\nGameboy Color is not supported");
        break;
    default:
        puts ("Gameboy ROM");
        break;
    }

    fputs ("Instruction set: ", stdout);
    switch (cart[0x0146]) {
    case 0x00: puts ("Gameboy");        break;
    case 0x03: puts ("Super Gameboy");
        fatal ("\nSGB intruction set is not supported");
        break;

    default:
        printf ("Unknown - %d\n", cart[0x0146]);
        break;
    }

    fputs ("Cartridge type: ", stdout);
    switch (cart[0x0147]) {
    case 0x00: puts ("ROM ONLY");                  break;
    case 0x01: puts ("ROM+MBC1");                  break;
    case 0x02: puts ("ROM+MBC1+RAM");              break;
    case 0x03: puts ("ROM+MBC1+RAM+BATT");         break;
    case 0x05: puts ("ROM+MBC2");                  break;
    case 0x06: puts ("ROM+MBC2+BATTERY");          break;
    case 0x08: puts ("ROM+RAM");                   break;
    case 0x09: puts ("ROM+RAM+BATTERY");           break;
    case 0x0B: puts ("ROM+MMM01");                 break;
    case 0x0C: puts ("ROM+MMM01+SRAM");            break;
    case 0x0D: puts ("ROM+MMM01+SRAM+BATT");       break;
    case 0x0F: puts ("ROM+MBC3+TIMER+BATT");       break;
    case 0x10: puts ("ROM+MBC3+TIMER+RAM+BATT");   break;
    case 0x11: puts ("ROM+MBC3");                  break;
    case 0x12: puts ("ROM+MBC3+RAM");              break;
    case 0x13: puts ("ROM+MBC3+RAM+BATT");         break;
    case 0x19: puts ("ROM+MBC5");                  break;
    case 0x1A: puts ("ROM+MBC5+RAM");              break;
    case 0x1C: puts ("ROM+MBC5+RUMBLE");           break;
    case 0x1B: puts ("ROM+MBC5+RAM+BATT");         break;
    case 0x1D: puts ("ROM+MBC5+RUMBLE+SRAM");      break;
    case 0x1E: puts ("ROM+MBC5+RUMBLE+SRAM+BATT"); break;
    case 0x1F: puts ("Pocket Camera");             break;
    case 0xFD: puts ("Bandai TAMA5");              break;
    case 0xFE: puts ("Hudson HuC-3");              break;
    case 0xFF: puts ("Hudson HuC-1");              break;
    default:
        printf ("Unknown - %d\n", cart[0x0147]);
        break;
    }

    fputs ("ROM size: ", stdout);
    switch (cart[0x0148]) {
    case 0x00: puts ("256Kbit = 32KByte = 2 banks");  break;
    case 0x01: puts ("512Kbit = 64KByte = 4 banks");  break;
    case 0x02: puts ("1Mbit = 128KByte = 8 banks");   break;
    case 0x03: puts ("2Mbit = 256KByte = 16 banks");  break;
    case 0x04: puts ("4Mbit = 512KByte = 32 banks");  break;
    case 0x05: puts ("8Mbit = 1MByte = 64 banks");    break;
    case 0x06: puts ("16Mbit = 2MByte = 128 banks");  break;
    case 0x52: puts ("9Mbit = 1.1MByte = 72 banks");  break;
    case 0x53: puts ("10Mbit = 1.2MByte = 80 banks"); break;
    case 0x54: puts ("12Mbit = 1.5MByte = 96 banks"); break;
    default:
        printf ("Unknown - %d\n", cart[0x0148]);
        break;
    }

    fputs ("RAM size: ", stdout);
    switch (cart[0x0149]) {
    case 0: puts ("None");                       break;
    case 1: puts ( "16Kbit = 2kB = 1 bank");     break;
    case 2: puts ( "64Kbit = 8kB = 1 bank");     break;
    case 3: puts ("256Kbit = 32kB = 4 banks");   break;
    case 4: puts (  "1Mbit = 128kB = 16 banks"); break;
    default:
        printf ("Unknown - %d\n", cart[0x0149]);
        break;
    }

    fputs ("Destination code: ", stdout);
    switch (cart[0x014A]) {
    case 0: puts ("Japanese"    );  break;
    case 1: puts ("Non-Japanese");  break;
    default:
        printf ("Unknown - %d\n", cart[0x014A]);
        break;
    }

    fputs ("Licensee code: ", stdout);
    switch (cart[0x014B]) {
    case 0x79: puts ("Accolade"); break;
    case 0xA4: puts ("Konami");   break;

    case 0x33:
        printf ("%1$c %2$c(%1$.2hhX%2$.2hhX)\n", cart[0x0144], cart[0x0145]);
        break;

    default:
        printf ("Unknown - %d\n", cart[0x014B]);
        break;
    }

    printf ("Mask ROM version: %d\n", cart[0x014C]);

    putchar ('\n');
}

/* readinput:  */
/* TODO: move this somewhere else? */
static BYTE readinput (BYTE button_set) {

    BYTE val = 0x00;
    bool old[8];
    bool new[8];

    for (unsigned i = 0; i < 8; ++i)
        new[i] = IO_btndown (i, &old[i]);

    /* update virtual controller */
    IO_update();

    /* down, up, left, right */
    if (GETBIT(button_set,  0) == 0) {
        val |= new[BTN_DOWN]  << 3;
        val |= new[BTN_UP]    << 2;
        val |= new[BTN_LEFT]  << 1;
        val |= new[BTN_RIGHT] << 0;
    }

    /* start, select, b, a */
    if (GETBIT(button_set, 1) == 0) {
        val |= new[BTN_START]  << 3;
        val |= new[BTN_SELECT] << 2;
        val |= new[BTN_B]      << 1;
        val |= new[BTN_A]      << 0;
    }

    //fprintf (stderr, "  %c                \n", new[BTN_UP]? '*' : '^');
    //fprintf (stderr, " %c %c  %c %c  %c %c\n", new[BTN_LEFT]? '*' : '<', new[BTN_RIGHT]? '*' : '>',
    //                                           new[BTN_START]? '*' : '-', new[BTN_SELECT]? '*' : '-',
    //                                           new[BTN_B]? '*' : 'B', new[BTN_A]? '*' : 'A');
    //fprintf (stderr, "  %c                \n", new[BTN_DOWN]? '*' : 'v');


    /* interrupt on HI to LO */
    for (unsigned btn = 0; btn < 8; ++btn)
        if (old[btn] == 1 && new[btn] == 0) {
            cpu_interrupt (INT_KPINPUT);
            break;
        }

    val = ~val;
    val &= 0x0F;
    return val;
}

/* get_serial_byte: read a byte from the serial port */
/* TODO:  */
static BYTE get_serial_byte(void) {
    return 0x00;
}

/* trap_register_write: writing to certain registers has
 *                      side-effects -- we handle them here */
static void trap_register_write (WORD location, BYTE byte) {

    /* read input */
    if (location == 0xFF00) {
        RAM[0xFF00 - 0x8000] = readinput ((byte >> 4) & 3);
    }
    /* serial I/O */
    else if (location == 0xFF02) {
        if (GETBIT(byte, 7)) {
            fprintf (stderr, "%c", memgval (0xFF01));
            RAM[0xFF02 - 0x8000] = get_serial_byte();
        }
    }
    /* writing to DIVIDER resets it and TIMECNT */
#define HARD    true
#define SOFT    false
    else if (location == 0xFF04) {
        RAM[0xFF04 - 0x8000] = 0x00;
        RAM[0xFF05 - 0x8000] = 0x00;
        Z80_update_timer_frequency (HARD);
    }
    /* TIMEMOD immediately sets TIMECNT */
    else if (location == 0xFF06) {
        RAM[0xFF05 - 0x8000] = byte;
        RAM[0xFF06 - 0x8000] = byte;
        Z80_update_timer_frequency (SOFT);
    }
    /* TIMCONT immediately sets timer frequency */
    else if (location == 0xFF07) {
        RAM[0xFF07 - 0x8000] = byte;
        Z80_update_timer_frequency (HARD);
    }
#undef HARD
#undef SOFT
    /* disable BIOS ROM */
    else if (location == 0xFF50 && byte == 0x01) {
        MODE_STARTUP = false;
    }
    /* DMA transfer */
    else if (location == 0xFF46) {
        /* TODO: this takes 162 cycles */
        WORD addr = byte * 0x100;
        for (unsigned i = 0; i < 0xA0; ++i)
            memsval (0xFE00 + i, memgval (addr + i));
    }
}

/* trap_ROM_write: writing to ROM has different effects depening on the MMU being used */
static void trap_ROM_write (WORD location, BYTE byte) {

    /* writing to ROM causes the processor to interpret the byte depending on where it is being written to;
     * for example, on a cartridge with an MBC1, writing a byte to $6000-$7FFF area selects the memory mode
     * to use
     */

    /* RAM enable/disable */
    if (location < 0x2000) {
        /* with MBC2, bit 0 of the high bit of location must be 0 to toggle RAM */
        if (MBC_type != MMU_MBC2 || (MBC_type == MMU_MBC2 && ((location >> 8) & 1) == 0))
            RAM_enabled = ((byte & 0xF) == 0xA);
        debug ("%sabled RAM", RAM_enabled? "en" : "dis");
    }
    /* ROM banking */
    else if (between (location, 0x2000, 0x3FFF)) {
        switch (MBC_type) {
        case MMU_MBC1:
        case MMU_HUC1:
            /* change bits 0-4 of ROM bank number */
            ROMbank_i &= ~31;
            ROMbank_i |= byte & 31;
            ROMbank_i %= ROMbankcount;
            break;

        case MMU_MBC2:
            /* with MBC2, bit 0 of the high bit of location must be 1 to select ROM */
            if (((location >> 8) & 1) == 0) {
                /* change bits 0-3 of ROM bank number */
                ROMbank_i &= ~15;
                ROMbank_i |= byte & 15;
            }
            break;

        case MMU_MBC3:
            /* change bits 0-6 of ROM bank number */
            ROMbank_i &= ~127;
            ROMbank_i |= byte & 127;
            break;

        case MMU_MBC5:
        case MMU_RMBL:
            /* MBC5 splits its 9 bits up between 2000-2FFF (bits 0-7) and 3000-3FFF (bit 9) area */
            if (between (location, 0x2000, 0x2FFF)) {
                /* change bits 0-7 of ROM bank number */
                ROMbank_i &= ~255;
                ROMbank_i |= byte;
            }
            else {
                /* change bit 8 of ROM bank number */
                ROMbank_i &= ~256;
                ROMbank_i |= (byte & 1) << 8;
            }
            break;

        default:
            ROMbank_i = 0;
            break;
        }
        ROMbank_i %= ROMbankcount;
        debug ("SWITCHED TO ROM BANK %i", ROMbank_i);

        /* 0 and 1 both select bank 1 in all but MBC5 */
        if ((MBC_type != MMU_MBC5 && MBC_type != MMU_RMBL) && ROMbank_i == 0)
            ROMbank_i = 1;
    }
    /* RAM/ROM banking */
    else if (between (location, 0x4000, 0x5FFF)) {
        switch (MBC_type) {
        case MMU_MBC1:
        case MMU_HUC1:
            /* RAM banking */
            if (MBC_mode == 1) {
                RAMbank_i = byte & 3;
                RAMbank_i %= RAMbankcount;
                debug ("SWITCHED TO RAM BANK %i", RAMbank_i);
            }
            /* ROM banking */
            else {
                /* change bits 5-7 of ROM bank number */
                ROMbank_i &= 31;
                ROMbank_i |= byte & ~31;
                ROMbank_i %= ROMbankcount;
                if (ROMbank_i == 0)
                    ROMbank_i = 1;
                debug ("SWITCHED TO ROM BANK %i", ROMbank_i);
            }
            break;

        case MMU_MBC5:
            RAMbank_i = byte & 15;
            RAMbank_i %= RAMbankcount;
            debug ("SWITCHED TO RAM BANK %i", RAMbank_i);
            break;

        case MMU_RMBL:
            RAMbank_i = byte & 7;
            RAMbank_i %= RAMbankcount;
            if (byte & (1 << 3))
                puts ("--RUMBLE--");
            debug ("SWITCHED TO RAM BANK %i", RAMbank_i);
            break;

        case MMU_UNKN:
            debug ("UNKNOWN MMU!");
            break;

        case MMU_NONE:
            debug ("ROM ONLY CART!");
            break;


        default:
            break;
        }
    }
    /* MBC mode select
     * 0 = 2MB/8KB, 1 = 512KB/32KB */
    else if (between (location, 0x6000, 0x7FFF)) {
        MBC_mode = byte & 1;
        RAMbank_i = 0;
        debug ("SET MBC MODE TO %i", MBC_mode);
    }
    /* other locations cause an error */
    /* TODO: better error messages */
    else
        fatal ("SEGFAULT: Write to read-only memory!");
}

