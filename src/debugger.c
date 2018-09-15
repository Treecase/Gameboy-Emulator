/*
 * interactive debugger
 *
 */

#include "debugger.h"
#include "mem.h"
#include "cpu.h"
#include "common.h"
#include "logging.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>  /* errno */
#include <ctype.h>  /* isspace */
#include <stdlib.h> /* free */
#include <string.h> /* strerror */

#include <readline/history.h>
#include <readline/readline.h>



void debug_printreg (char *name);
void debug_printmem (uint16_t loc);
void debug_dumpregs(void);
void debug_dumpmem (uint16_t start, uint16_t end);
uint16_t strtoword (char *str);

char **debug_command_complete (const char *text, int start, int end);


static char *previouscommand = NULL;



/* debug_init:  */
void debug_init(void) {

    using_history();
    rl_attempted_completion_function = debug_command_complete;
    atexit (debug_cleanup);
}

/* debug_cleanup:  */
void debug_cleanup(void) {

    free (previouscommand);
    previouscommand = NULL;

    clear_history();
}

/* debug_prompt: interactive debugger */
void debug_prompt(void) {

    G_state.state = (G_state.state | EMUSTATE_DEBUG) & ~EMUSTATE_NORMAL;

    char *command = NULL;
    size_t cmdlen = 0;

    bool newcommand = true;

    command = readline ("> ");
    if (command != NULL)
        cmdlen = strlen (command);

    /* no input means just use the previous input */
    if (cmdlen == 0 && command != NULL && previouscommand != NULL) {
        free (command);
        command = strdup (previouscommand);
        cmdlen = strlen (command);
        newcommand = false;
    }


    /* error/EOI occurred */
    if (command == NULL) {
        G_state.running = false;
        return;
    }
    else {

        if (newcommand)
            add_history (command);

        unsigned start = 0;
        /* skip leading whitespace */
        for (start = 0; isspace (command[start]); ++start)
            ;

        unsigned l = 0,
                 i = start;
        for (; i < cmdlen; ++i)
            if (!isspace (command[i])) {
                while (!isspace (command[i]) && i < cmdlen)
                    ++i;
                l++;
            }

        char *tokn[l];
        for (i = 0; i < l; ++i)
            tokn[i] = NULL;
        i = 0;

        /* parse words out into token list */
        for (; start < cmdlen; ++start) {
            if (!isspace (command[start])) {
                unsigned end = start;
                while (!isspace (command[end]) && end < cmdlen)
                    end++;
                tokn[i++] = strndup (command + start, end - start);
                start = end;
            }
        }


        /* print: print value in register/memory location */
        if (!strcasecmp (tokn[0], "print")) {
            if (l == 2) {
                /* $NNNN -> print memory */
                if (tokn[1][0] == '$')
                    debug_printmem (strtoword (tokn[1]));
                /* print register */
                else if (isalpha (tokn[1][0]) && (strlen (tokn[1]) == 1 || strlen (tokn[1]) == 2))
                    debug_printreg (tokn[1]);
                else
                    error ("invalid argument to print: `%s'", tokn[1]);
            }
            else
                error ("print takes 1 argument");
        }
        /* dump: print registers/memory range */
        else if (!strcasecmp (tokn[0], "dump")) {
            /* dump registers */
            if (l == 1)
                debug_dumpregs();
            /* dump memory range */
            else if (l == 3)
                debug_dumpmem (strtoword (tokn[1]), strtoword (tokn[2]));
            else
                error ("dump takes 0 or 2 arguments");
        }
        /* break: set a new breakpoint */
        else if (!strcasecmp (tokn[0], "break")) {
            if (l == 2) {
                G_state.debug.enabled = true;
                printf ("set breakpoint at %.4hX\n",
                    G_state.debug.breakpoint = strtoword (tokn[1]));
            }
            else if (l == 1) {
                printf ("enabled breakpoints\n");
                G_state.debug.enabled = true;
            }
            else
                error ("break takes 0 or 1 arguments");
        }
        /* nobreak: clear breakpoint */
        else if (!strcasecmp (tokn[0], "nobreak")) {
            printf ("disabled breakpoints\n");
            G_state.debug.enabled = false;
        }
        /* step: step one instruction forward */
        else if (!strcasecmp (tokn[0], "step"))
            G_state.state = (EMUSTATE_NORMAL | EMUSTATE_DEBUG);
        /* run: continue running the program */
        else if (!strcasecmp (tokn[0], "run"))
            G_state.state = (G_state.state | EMUSTATE_NORMAL) & ~EMUSTATE_DEBUG;
        /* quit: exit program */
        else if (!strcasecmp (tokn[0], "quit"))
            G_state.running = false;
        else {
            /* TODO: calculator, etc. */
            error ("unknown command");
        }

        for (i = 0; i < l; ++i)
            free (tokn[i]);
    }


    if (previouscommand)
        free (previouscommand);

    if (command) {
        previouscommand = strdup (command);
        free (command);
    }
}


/* INTERNAL FNs */
/* debug_printreg: print register value */
void debug_printreg (char *name) {

    uint16_t value = -1;

    bool fail     = false,
         wordreg  = false,
         comboreg = false;


    /* TODO: better way to do this? */
    if (strlen (name) == 2) {
        wordreg = true;
        if      (!strcasecmp (name, "AF")) {
            value = *AF;
            comboreg = true;
        }
        else if (!strcasecmp (name, "BC")) {
            value = *BC;
            comboreg = true;
        }
        else if (!strcasecmp (name, "DE")) {
            value = *DE;
            comboreg = true;
        }
        else if (!strcasecmp (name, "HL")) {
            value = *HL;
            comboreg = true;
        }
        else if (!strcasecmp (name, "PC"))
            value = pc;
        else if (!strcasecmp (name, "SP"))
            value = sp;

        else
            fail = true;
    }
    else if (strlen (name) == 1) {
        if      (!strcasecmp (name, "A"))
            value = *A;
        else if (!strcasecmp (name, "F"))
            value = *F;
        else if (!strcasecmp (name, "B"))
            value = *B;
        else if (!strcasecmp (name, "C"))
            value = *C;
        else if (!strcasecmp (name, "D"))
            value = *D;
        else if (!strcasecmp (name, "E"))
            value = *E;
        else if (!strcasecmp (name, "H"))
            value = *H;
        else if (!strcasecmp (name, "L"))
            value = *L;
        else
            fail = true;
    }
    else
        fail = true;

    if (fail)
        error ("invalid register name %s", name);
    else {
        if (wordreg) {
            printf ("%s = $%.4hX", name, value);
            if (comboreg)
                printf (" ($%c=%.2hhX %c=$%.2hhX)\n", name[0], value >> 8, name[1], value & 255);
            else
                putchar ('\n');
        }
        else
            printf ("%s = $%.2hhX\n", name, value & 255);
    }
}

/* debug_printmem: print a memory location */
void debug_printmem (uint16_t loc) {
    printf ("%.4hX  $%.2hhX\n", loc, memgval (loc));
}

/* debug_dumpregs: dump registers */
void debug_dumpregs(void) {

    debug_printreg ("AF");
    debug_printreg ("BC");
    debug_printreg ("DE");
    debug_printreg ("HL");

    debug_printreg ("PC");
    debug_printreg ("SP");
}

/* debug_dumpmem: dump memory region */
void debug_dumpmem (uint16_t start, uint16_t end) {
    for (int i = start; i <= end && i < 0x10000; ++i)
        debug_printmem (i);
}

/* strtoword: convert str to a 16-bit unsigned */
uint16_t strtoword (char *str) {

    uint16_t val = 0;
    long r;
    char *end;

    int base = 0;
    if (str[0] == '$') {
        base = 16;
        str++;
    }

    r = strtol (str, &end, base);
    if (end == str)
        error ("%s is not a number", str);
    else if (r > (1 << 16))
        error ("%s is out of range for 16-bit value!", str);
    else
        val = r;

    return val;
}

#if 0
/* i_strtol: wrapper for strtol */
long i_strtol (const char *str) {

    char *end;
    int   base = 0;
    long  r;

    if (str[0] == '$') {
        base = 16;
        str++;
    }

    errno = 0;

    r = strtol (str, &end, base);
    if (end == str)
        error ("strrol failed: %s is not a number", str);
    else if (errno != 0)
        error ("strtol failed: %s", strerror (errno));

    return r;
}
#endif




/* AUTOCOMPLETE STUFF */
char *command_names[]
 = {    "print",
        "dump",
        "break",
        "nobreak",
        "step",
        "run",
        "quit",
        NULL
   };

char **debug_command_complete  (const char *text, int start, int end);
char  *debug_command_generator (const char *text, int state);

/* debug_command_complete: readline completion function */
char **debug_command_complete (const char *text, int start, int end) {
    start = start;  /* to shut the compiler up */
    end   = end  ;  /* "                       */

    rl_attempted_completion_over = 1;
    return rl_completion_matches (text, debug_command_generator);
}

/* debug_command_generator: generator for autocompletion */
char *debug_command_generator (const char *text, int state) {

    static int list_index, len;
    char *command;

    if (!state) {
        list_index = 0;
        len = strlen (text);
    }

    while ((command = command_names[list_index++]) != NULL)
        if (!strncasecmp (command, text, len))
            return strdup (command);

    return NULL;
}

