/*
 * Gameboy emu main stuff
 *
 */

#include <time.h>   /* clock_gettime, timespec */
#include <getopt.h> /* getopt */
#include <string.h> /* strlen */
#include <unistd.h> /* usleep */

/* TODO: rejig for fewer interdependencies */
#include "Z80.h"
#include "cpu.h"
#include "low.h"
#include "io.h"
#include "mem.h"
#include "common.h"
#include "display.h"
#include "logging.h"
#include "debugger.h"
#include "registers.h"
#include "alarm.h"



static const unsigned CPU_FREQUENCY = 4194304;



/* timer */
static AlarmID timer_alarm;
static void timer_update(void);
static const unsigned timer_cycles[4] = { 1024, 16, 64, 256 };

static AlarmID divider_alarm;
static void divider_update(void);
static const unsigned divider_cycles = 256;

static bool use_bios = false;



struct emustate G_state
    = { .running=true,
        .state=EMUSTATE_NORMAL,
        .debug={ .enabled=false,
                 .breakpoint=-1
               },
      };



/* millis:  */
static long double millis(void) {
    struct timespec spec;
    clock_gettime (CLOCK_MONOTONIC, &spec);
    return spec.tv_sec + (spec.tv_nsec / 1000000000.0L);
}



/* Z80_args: handle argv */
char *Z80_args (int argc, char *argv[]) {

    bool forcecpulogging = false;

    const char *shortopts = "hdb";
    const struct option longopts[]
     = { { "help"   , no_argument      , NULL, 'h' },
         { "log"    , optional_argument, NULL, 'l' },
         { "break"  , required_argument, NULL, 'k' },
         { "disassemble", no_argument  , NULL, 'd' },
         { "bios"   , no_argument      , NULL, 'b' },
         { NULL     , no_argument      , NULL,  0  }
       };

    char *in_file = NULL;


    int opt;
    while ((opt = getopt_long (argc, argv, shortopts, longopts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            IO_print_help (argv[0], true);
            die();
            break;

        case 'l':
            /* argstring, select which things to enable */
            if (optarg) {
                bool on = true;
                for (unsigned i = 0; i < strlen (optarg); ++i) {
                    if (optarg[i] == '!' && i+1 < strlen (optarg)) {
                        i++;
                        on = false;
                    }
                    switch (optarg[i]) {
                    case 'z': Z80_logging (on); break;
                    case 'm': mem_logging (on); break;
                    case 'd': dis_logging (on); break;
                    case 'c':
                        if (on || forcecpulogging)
                            cpu_logging (true);
                        break;
                    default:
                        error ("unknown debug module %c", optarg[i]);
                        break;
                    }
                    on = false;
                }
            }
            /* no argstring means enable all logging */
            else {
                cpu_logging (true);
                Z80_logging (true);
                mem_logging (true);
                dis_logging (true);
            }
            break;

        case 'k':
          { char *end;
            long breakpoint;

            int base = 0;
            debug ("optarg: %s", optarg);
            if (optarg[0] == '$') {
                base = 16;
                optarg++;
            }

            debug ("base %i", base);
            breakpoint = strtol (optarg, &end, base);
            if (breakpoint < 0 || breakpoint > (2 << 15) || end == optarg)
                fatal ("invalid breakpoint %li", breakpoint);

            G_state.debug.enabled    = true;
            G_state.debug.breakpoint = breakpoint;

            cpu_logging (true);
            forcecpulogging = true;
          } break;

        case 'd':
            G_state.state |= EMUSTATE_DISASSEMBLE;
            cpu_logging (true);
            forcecpulogging = true;

            Z80_logging (false);
            mem_logging (false);
            dis_logging (false);
            break;

        case 'b':
            use_bios = true;
            break;
                            
        case '?':
            IO_print_help (argv[0], false);
            die();
            break;

        default:
            IO_print_help (argv[0], false);
            die();
            break;
        }
    }
    if (optind >= argc) {
        IO_print_help (argv[0], false);
        die();
    }
    else
        in_file = argv[optind];

    return in_file;
}

/* Z80_init:  */
void Z80_init(void) {

    srand (time (NULL));

    init_alarms (CPU_FREQUENCY);

    cpu_init();
    mem_init (use_bios);
    display_init();
    debug_init();

    sp = 0x0000;
    pc = 0x0000;


    /*  set registers to the values they would
     *  have had if we had run the BIOS */
    if (!use_bios) {

        pc = 0x0100;
       *AF = 0x0000;
       *BC = 0x0013;
       *DE = 0x00D8;
       *HL = 0x014D;
        sp = 0xFFFE;

        memsval (0xFF10, 0x80);
        memsval (0xFF11, 0xBF);
        memsval (0xFF12, 0xF3);
        memsval (0xFF14, 0xBF);
        memsval (0xFF16, 0x3F);
        memsval (0xFF19, 0xBF);
        memsval (0xFF1A, 0x7F);
        memsval (0xFF1B, 0xFF);
        memsval (0xFF1C, 0x9F);
        memsval (0xFF1E, 0xBF);
        memsval (0xFF20, 0xFF);
        memsval (0xFF23, 0xBF);
        memsval (0xFF24, 0x77);
        memsval (0xFF25, 0xF3);
        memsval (0xFF26, 0xF1);
        memsval (0xFF40, 0x91);
        memsval (0xFF47, 0xFC);
        memsval (0xFF48, 0xFF);
        memsval (0xFF49, 0xFF);
    }

    /* set up alarms */
    BYTE TIMECONT = memgval (R_TIMCONT);
    timer_alarm = mkalarm_cycle (timer_cycles[TIMECONT & 3], timer_update);

    divider_alarm = mkalarm_cycle (divider_cycles, divider_update);
}

/* Z80_update_timer_frequency:  */
void Z80_update_timer_frequency (bool hard) {
    void (*set_cycles)(AlarmID, long);

    if (hard)
        set_cycles = set_alarm_cycles;
    else
        set_cycles = set_alarm_cycles_clean;

    set_cycles (timer_alarm, timer_cycles[memgval (R_TIMCONT) & 3]);
}

/* timer_update:  */
static void timer_update(void) {

    BYTE TIMECONT = memgval (R_TIMCONT);

    if (GETBIT(TIMECONT, 2)) {

        BYTE timecount = memgval (R_TIMECNT);

        if (timecount + 1 > 255) {
            memsval (R_TIMECNT, memgval (R_TIMEMOD));
            cpu_interrupt (INT_TIMER);
        }
        else {
            timecount++;
            memsval (R_TIMECNT, timecount);
        }
    }
}

/* divider_update:  */
static void divider_update(void) {
    BYTE divider = memgval (R_DIVIDER);
    mem_set_register (R_DIVIDER, divider + 1);
}

/* Z80_cleanup: clean up */
void Z80_cleanup(void) {
    /* unused */
}

/* Z80_load: load the program */
void Z80_load (char *fname) {
    mem_loadcart (fname);
}


/* Z80_frame: emulate one frame */
void Z80_frame(void) {

    static const double vbl_freq = 59.73;

    long double start = millis(),
                waste = 0.0L;
    unsigned long cycles_this_frame = 0;

    while (cycles_this_frame < CPU_FREQUENCY / vbl_freq && G_state.running) {

        /* program is not paused */
        if (G_state.state & EMUSTATE_NORMAL) {

            unsigned num_cycles = cpu_cycle();
            cycles_this_frame += num_cycles;

            /* update/run alarms */
            update_alarms (num_cycles);
        }

        /* run the debugger if requested */
        if (G_state.state & EMUSTATE_DEBUG) {
            long double dbg_st = millis();
            debug_prompt();
            waste += millis() - dbg_st;
        }
    }

    display_update();

    long double frametime = (millis() - start) - waste;
    debug ("frame took %Lf seconds (%Lf FPS)", frametime, 1.0L / frametime);

/* NOTE: can be disabled for unlimited framerate */
#if 1
    /* sleep for any extra time */
    long sleeptime = (long)(((1.0/vbl_freq) - frametime) * 1000000.0);
    if (sleeptime > 0)
        usleep (sleeptime);
#endif
}

/* Z80_logging:  */
void Z80_logging (bool enable) {
    LOG_DOLOG = enable;
}

