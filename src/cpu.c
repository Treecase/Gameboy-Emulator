/*
 * Gameboy CPU module
 *
 */
/* TODO: figure out what's wrong with DAA */

#include "cpu.h"
#include "cpu_print.h"
#include "cpu_timing.h"
#include "cpu_print_arg.h"

#include "mem.h"
#include "common.h"
#include "logging.h"
#include "registers.h"



#define SETFLAGS(Z, N, H, C)    (SETFLAGZ(Z), SETFLAGN(N), SETFLAGH(H), SETFLAGC(C))

#define SETFLAGZ(state)         ({ if (state) SETBIT(*F, 7); else RESBIT(*F, 7); })
#define SETFLAGN(state)         ({ if (state) SETBIT(*F, 6); else RESBIT(*F, 6); })
#define SETFLAGH(state)         ({ if (state) SETBIT(*F, 5); else RESBIT(*F, 5); })
#define SETFLAGC(state)         ({ if (state) SETBIT(*F, 4); else RESBIT(*F, 4); })

#define HALF_CARRY(a,b)     (((((a) & 0x0F) + ((b) & 0x0F)) & 0x010) != 0)
#define FULL_CARRY(a,b)     (((((a) & 0xFF) + ((b) & 0xFF)) & 0x100) != 0)
#define HALF_BORROW(a,b)    (((a) & 0x0F) < ((b) & 0x0F))
#define FULL_BORROW(a,b)    (((a) & 0xFF) < ((b) & 0xFF))



static bool cpu_halted = false;



static void memset16 (WORD addr, WORD val) {
    memsval (addr    , val & 255);
    memsval (addr + 1, val >> 8 );
}


static BYTE next(void)
{ return memgval (pc++); }

static WORD next16(void) {
    WORD val = memgval (pc) | (memgval (pc+1) << 8);
    pc += 2;
    return val;
}


static WORD pop(void) {
    WORD val = memgval (sp) | (memgval (sp+1) << 8);
    sp += 2;
    return val;
}
static void push (WORD val) {
    sp -= 2;
    memset16 (sp, val);
}



/* easy addition, sets appropriate flags */
static BYTE cpu_add (BYTE a, BYTE b) {

    BYTE result = a + b;

    SETFLAGC(FULL_CARRY(a, b));
    SETFLAGH(HALF_CARRY(a, b));

    SETFLAGZ(result == 0);

    SETFLAGN(0);

    return result;
}
static WORD cpu_add16 (WORD a, WORD b) {
    WORD result = a + b;

    SETFLAGC( ((a + b) & 0x10000) != 0 );
    SETFLAGH( ((a ^ b ^ result) & 0x01000) != 0);

    SETFLAGZ(result == 0);
    SETFLAGN(0);
    return result;
}

/* easy subtraction, sets appropriate flags */
static BYTE cpu_sub (BYTE a, BYTE b) {

    BYTE result = a - b;

    SETFLAGC(FULL_BORROW(a, b));
    SETFLAGH(HALF_BORROW(a, b));

    SETFLAGZ(result == 0);

    SETFLAGN(1);

    return result;
}


static void cpu_restart (BYTE offset) {
    push (pc);
    pc = 0x0000 + offset;
};



/* PUBLIC API */
void cpu_cleanup(void);
/* cpu_init:  */
void cpu_init(void) {

    pc = 0x0100;
    sp = 0xFFFE;

    AF = calloc (2, 1);
    BC = calloc (2, 1);
    DE = calloc (2, 1);
    HL = calloc (2, 1);

    A = ((BYTE *)AF) + 1;
    F = ((BYTE *)AF);

    B = ((BYTE *)BC) + 1;
    C = ((BYTE *)BC);

    D = ((BYTE *)DE) + 1;
    E = ((BYTE *)DE);

    H = ((BYTE *)HL) + 1;
    L = ((BYTE *)HL);

    IME = true;

    atexit (cpu_cleanup);
}

static bool exec_op (BYTE opcode);
static void cpu_ack_interrupts(void);
/* cpu_cycle:  */
unsigned cpu_cycle(void) {

    unsigned cyclecount = 4;

    /* when we hit a breakpoint, halt */
    if (G_state.debug.enabled && pc == G_state.debug.breakpoint)
        G_state.state = (G_state.state | EMUSTATE_DEBUG) & ~EMUSTATE_NORMAL;

    /* if we are disassembling, don't go past the end of memory */
    if (G_state.state & EMUSTATE_DISASSEMBLE && pc+1 == 0x10000)
        G_state.running = false;

    /* acknowledge interrupts */
    if ((IME || cpu_halted) && !(G_state.state & EMUSTATE_DISASSEMBLE))
        cpu_ack_interrupts();


    if (!cpu_halted) {

        debugl ("%.4hX  ", pc);

        BYTE op = next();
        WORD old_pc = pc;

        bool extra_cycles = false;
        if (!(G_state.state & EMUSTATE_DISASSEMBLE)) {
            extra_cycles = exec_op (op);
            print_op_arg (op, old_pc);
        }
        else
            pc = print_op (op, old_pc);

        BYTE prefix = (op == 0xCB)? op : 0x00;
        cyclecount = op_cycles (prefix, op, extra_cycles);
    }
    return cyclecount;
}

/* cpu_interrupt: raise an interrupt */
void cpu_interrupt (enum interrupt int_type) {
    BYTE IFLAGS = memgval (R_IFLAGS);
    IFLAGS |= int_type;
    debug ("interrupt %i requested", int_type);
    memsval (R_IFLAGS, IFLAGS);
}

/* cpu_logging: turn on/off CPU logging */
void cpu_logging (bool enable) {
    LOG_DOLOG = enable;
}

/* cpu_logging_enabled:  */
bool cpu_logging_enabled(void) {
    return LOG_DOLOG;
}



/* INTERNAL FUNCs */
/* exec_op: decode + execute an opcode */
bool exec_op (BYTE opcode) {

    /* for conditional instructions timing */
    bool condition_true = false;

    switch (opcode) {

    /* $CB Prefix */
    case 0xCB:
        opcode = next();
        switch (opcode) {
        /* Miscellaneous */
        /* SWAP r */
        case 0x37: *A = ((*A) << 4) | ((*A) >> 4); SETFLAGS(*A == 0, 0,0,0); break;
        case 0x30: *B = ((*B) << 4) | ((*B) >> 4); SETFLAGS(*B == 0, 0,0,0); break;
        case 0x31: *C = ((*C) << 4) | ((*C) >> 4); SETFLAGS(*C == 0, 0,0,0); break;
        case 0x32: *D = ((*D) << 4) | ((*D) >> 4); SETFLAGS(*D == 0, 0,0,0); break;
        case 0x33: *E = ((*E) << 4) | ((*E) >> 4); SETFLAGS(*E == 0, 0,0,0); break;
        case 0x34: *H = ((*H) << 4) | ((*H) >> 4); SETFLAGS(*H == 0, 0,0,0); break;
        case 0x35: *L = ((*L) << 4) | ((*L) >> 4); SETFLAGS(*L == 0, 0,0,0); break;
        case 0x36:
         {  BYTE m = memgval (*HL);
            m = (m << 4) | (m >> 4);
            memsval (*HL, m);
            SETFLAGS(m == 0, 0,0,0);
         }  break;
        /* Rotates and Shifts (registers) */
        /* RLC r */
        case 0x07: *A = ((*A) << 1) | ((*A) >> 7); SETFLAGS(*A == 0, 0,0, (*A) & 1); break;
        case 0x00: *B = ((*B) << 1) | ((*B) >> 7); SETFLAGS(*B == 0, 0,0, (*B) & 1); break;
        case 0x01: *C = ((*C) << 1) | ((*C) >> 7); SETFLAGS(*C == 0, 0,0, (*C) & 1); break;
        case 0x02: *D = ((*D) << 1) | ((*D) >> 7); SETFLAGS(*D == 0, 0,0, (*D) & 1); break;
        case 0x03: *E = ((*E) << 1) | ((*E) >> 7); SETFLAGS(*E == 0, 0,0, (*E) & 1); break;
        case 0x04: *H = ((*H) << 1) | ((*H) >> 7); SETFLAGS(*H == 0, 0,0, (*H) & 1); break;
        case 0x05: *L = ((*L) << 1) | ((*L) >> 7); SETFLAGS(*L == 0, 0,0, (*L) & 1); break;
        case 0x06:
         {  BYTE m = memgval (*HL);
            m = (m << 1) | (m >> 7);
            memsval (*HL, m);
            SETFLAGS(m == 0, 0,0, m & 1);
         }  break;

        /* RL r */
#define RL(r)  ({ BYTE tmp = ((r) << 1) | FLAGC;\
                  SETFLAGS(tmp == 0, 0,0, (r) >> 7);\
                  tmp; })
        case 0x17: *A = RL(*A); break;
        case 0x10: *B = RL(*B); break;
        case 0x11: *C = RL(*C); break;
        case 0x12: *D = RL(*D); break;
        case 0x13: *E = RL(*E); break;
        case 0x14: *H = RL(*H); break;
        case 0x15: *L = RL(*L); break;
        case 0x16: memsval (*HL, RL(memgval (*HL))); break;
#undef RL

        /* RRC r */
        case 0x0F: *A = ((*A) >> 1) | ((*A) << 7); SETFLAGS(*A == 0, 0,0, (*A) >> 7); break;
        case 0x08: *B = ((*B) >> 1) | ((*B) << 7); SETFLAGS(*B == 0, 0,0, (*B) >> 7); break;
        case 0x09: *C = ((*C) >> 1) | ((*C) << 7); SETFLAGS(*C == 0, 0,0, (*C) >> 7); break;
        case 0x0A: *D = ((*D) >> 1) | ((*D) << 7); SETFLAGS(*D == 0, 0,0, (*D) >> 7); break;
        case 0x0B: *E = ((*E) >> 1) | ((*E) << 7); SETFLAGS(*E == 0, 0,0, (*E) >> 7); break;
        case 0x0C: *H = ((*H) >> 1) | ((*H) << 7); SETFLAGS(*H == 0, 0,0, (*H) >> 7); break;
        case 0x0D: *L = ((*L) >> 1) | ((*L) << 7); SETFLAGS(*L == 0, 0,0, (*L) >> 7); break;
        case 0x0E:
         {  BYTE m = memgval (*HL);
            m = (m >> 1) | (m << 7);
            memsval (*HL, m);
            SETFLAGS(m == 0, 0,0, m >> 7);
         }  break;

        /* RR r */
#define RR(r)  ({ BYTE tmp = ((r) >> 1) | (FLAGC << 7);\
                  SETFLAGS(tmp == 0, 0,0, (r) & 1);\
                  tmp; })
        case 0x1F: *A = RR(*A); break;
        case 0x18: *B = RR(*B); break;
        case 0x19: *C = RR(*C); break;
        case 0x1A: *D = RR(*D); break;
        case 0x1B: *E = RR(*E); break;
        case 0x1C: *H = RR(*H); break;
        case 0x1D: *L = RR(*L); break;
        case 0x1E: memsval (*HL, RR(memgval (*HL))); break;
#undef RR

        /* SLA r */
        case 0x27: SETFLAGC((*A) >> 7); *A = (*A) << 1; SETFLAGS(*A == 0, 0,0, FLAGC); break;
        case 0x20: SETFLAGC((*B) >> 7); *B = (*B) << 1; SETFLAGS(*B == 0, 0,0, FLAGC); break;
        case 0x21: SETFLAGC((*C) >> 7); *C = (*C) << 1; SETFLAGS(*C == 0, 0,0, FLAGC); break;
        case 0x22: SETFLAGC((*D) >> 7); *D = (*D) << 1; SETFLAGS(*D == 0, 0,0, FLAGC); break;
        case 0x23: SETFLAGC((*E) >> 7); *E = (*E) << 1; SETFLAGS(*E == 0, 0,0, FLAGC); break;
        case 0x24: SETFLAGC((*H) >> 7); *H = (*H) << 1; SETFLAGS(*H == 0, 0,0, FLAGC); break;
        case 0x25: SETFLAGC((*L) >> 7); *L = (*L) << 1; SETFLAGS(*L == 0, 0,0, FLAGC); break;
        case 0x26:
         {  BYTE m = memgval (*HL);
            SETFLAGC(m >> 7);
            m <<= 1;
            memsval (*HL, m);
            SETFLAGS(m == 0, 0,0, FLAGC);
         }  break;

        /* SRA r */
        case 0x2F: SETFLAGC((*A) & 1); *A = ((*A) >> 1) | ((*A) & 128); SETFLAGS(*A == 0, 0,0, FLAGC); break;
        case 0x28: SETFLAGC((*B) & 1); *B = ((*B) >> 1) | ((*B) & 128); SETFLAGS(*B == 0, 0,0, FLAGC); break;
        case 0x29: SETFLAGC((*C) & 1); *C = ((*C) >> 1) | ((*C) & 128); SETFLAGS(*C == 0, 0,0, FLAGC); break;
        case 0x2A: SETFLAGC((*D) & 1); *D = ((*D) >> 1) | ((*D) & 128); SETFLAGS(*D == 0, 0,0, FLAGC); break;
        case 0x2B: SETFLAGC((*E) & 1); *E = ((*E) >> 1) | ((*E) & 128); SETFLAGS(*E == 0, 0,0, FLAGC); break;
        case 0x2C: SETFLAGC((*H) & 1); *H = ((*H) >> 1) | ((*H) & 128); SETFLAGS(*H == 0, 0,0, FLAGC); break;
        case 0x2D: SETFLAGC((*L) & 1); *L = ((*L) >> 1) | ((*L) & 128); SETFLAGS(*L == 0, 0,0, FLAGC); break;
        case 0x2E:
         {  BYTE m = memgval (*HL);
            SETFLAGC(m & 1);
            m = (m >> 1) | (m & 128);
            memsval (*HL, m);
            SETFLAGS(m == 0, 0,0, FLAGC);
         }  break;

        /* SRL r */
        case 0x3F: SETFLAGC((*A) & 1); *A = ((*A) >> 1); SETFLAGS(*A == 0, 0,0, FLAGC); break;
        case 0x38: SETFLAGC((*B) & 1); *B = ((*B) >> 1); SETFLAGS(*B == 0, 0,0, FLAGC); break;
        case 0x39: SETFLAGC((*C) & 1); *C = ((*C) >> 1); SETFLAGS(*C == 0, 0,0, FLAGC); break;
        case 0x3A: SETFLAGC((*D) & 1); *D = ((*D) >> 1); SETFLAGS(*D == 0, 0,0, FLAGC); break;
        case 0x3B: SETFLAGC((*E) & 1); *E = ((*E) >> 1); SETFLAGS(*E == 0, 0,0, FLAGC); break;
        case 0x3C: SETFLAGC((*H) & 1); *H = ((*H) >> 1); SETFLAGS(*H == 0, 0,0, FLAGC); break;
        case 0x3D: SETFLAGC((*L) & 1); *L = ((*L) >> 1); SETFLAGS(*L == 0, 0,0, FLAGC); break;
        case 0x3E:
         {  BYTE m = memgval (*HL);
            SETFLAGC(m & 1);
            m = (m >> 1);
            memsval (*HL, m);
            SETFLAGS(m == 0, 0,0, FLAGC);
         }  break;


        /* NOTE: BIT, RES, and SET cases use `Case Ranges' GNU C extension */
        /* Bit Opcodes */
        /* BIT b, r */
        case 0x40 ... 0x7F:
         {  BYTE register_values[8] = { *B, *C, *D, *E, *H, *L, memgval (*HL), *A };

            SETFLAGS(!GETBIT(register_values[opcode & 7], (opcode >> 3) & 7), 0,1, FLAGC);
         }  break;

#define DO_FOR_REGISTER(n, fn, ...)\
    switch(n) {\
    case 0: *B = fn(*B, ##__VA_ARGS__); break;\
    case 1: *C = fn(*C, ##__VA_ARGS__); break;\
    case 2: *D = fn(*D, ##__VA_ARGS__); break;\
    case 3: *E = fn(*E, ##__VA_ARGS__); break;\
    case 4: *H = fn(*H, ##__VA_ARGS__); break;\
    case 5: *L = fn(*L, ##__VA_ARGS__); break;\
    case 7: *A = fn(*A, ##__VA_ARGS__); break;\
    \
    case 6:\
     {  BYTE tmp = memgval (*HL);\
        memsval (*HL, fn(tmp, ##__VA_ARGS__));\
     }  break;\
    }

        /* RES n, r */
        case 0x80 ... 0xBF:
/* NOTE: DO_FOR_REGISTER macro spams warnings, so we just hide them! :) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsequence-point"
            DO_FOR_REGISTER(opcode & 7, RESBIT, (opcode >> 3) & 7);
            break;

        /* SET n, r */
        case 0xC0 ... 0xFF:
            DO_FOR_REGISTER(opcode & 7, SETBIT, (opcode >> 3) & 7); break;
#pragma GCC diagnostic pop

#undef DO_FOR_REGISTER


        default:
            /* this should never happen, but just in case... */
            fatal ("ILLEGAL INSTRUCTION: Invalid instruction `$CB $%.2hhX'", opcode);
            break;
        }
        break;
    /* END $CB Prefix */


    /* 8-Bit Loads */
    /* LD r, n */
    case 0x3E: *A = next(); break;
    case 0x06: *B = next(); break;
    case 0x0E: *C = next(); break;
    case 0x16: *D = next(); break;
    case 0x1E: *E = next(); break;
    case 0x26: *H = next(); break;
    case 0x2E: *L = next(); break;


    /* LD r, r */
    /* LD A, r */
    case 0x7F: *A = *A; break;
    case 0x78: *A = *B; break;
    case 0x79: *A = *C; break;
    case 0x7A: *A = *D; break;
    case 0x7B: *A = *E; break;
    case 0x7C: *A = *H; break;
    case 0x7D: *A = *L; break;
    case 0x7E: *A = memgval (*HL); break;

    /* LD B, r */
    case 0x47: *B = *A; break;
    case 0x40: *B = *B; break;
    case 0x41: *B = *C; break;
    case 0x42: *B = *D; break;
    case 0x43: *B = *E; break;
    case 0x44: *B = *H; break;
    case 0x45: *B = *L; break;
    case 0x46: *B = memgval (*HL); break;

    /* LD C, r */
    case 0x4F: *C = *A; break;
    case 0x48: *C = *B; break;
    case 0x49: *C = *C; break;
    case 0x4A: *C = *D; break;
    case 0x4B: *C = *E; break;
    case 0x4C: *C = *H; break;
    case 0x4D: *C = *L; break;
    case 0x4E: *C = memgval (*HL); break;

    /* LD D, r */
    case 0x57: *D = *A; break;
    case 0x50: *D = *B; break;
    case 0x51: *D = *C; break;
    case 0x52: *D = *D; break;
    case 0x53: *D = *E; break;
    case 0x54: *D = *H; break;
    case 0x55: *D = *L; break;
    case 0x56: *D = memgval (*HL); break;

    /* LD E, r */
    case 0x5F: *E = *A; break;
    case 0x58: *E = *B; break;
    case 0x59: *E = *C; break;
    case 0x5A: *E = *D; break;
    case 0x5B: *E = *E; break;
    case 0x5C: *E = *H; break;
    case 0x5D: *E = *L; break;
    case 0x5E: *E = memgval (*HL); break;

    /* LD H, r */
    case 0x67: *H = *A; break;
    case 0x60: *H = *B; break;
    case 0x61: *H = *C; break;
    case 0x62: *H = *D; break;
    case 0x63: *H = *E; break;
    case 0x64: *H = *H; break;
    case 0x65: *H = *L; break;
    case 0x66: *H = memgval (*HL); break;

    /* LD L, r */
    case 0x6F: *L = *A; break;
    case 0x68: *L = *B; break;
    case 0x69: *L = *C; break;
    case 0x6A: *L = *D; break;
    case 0x6B: *L = *E; break;
    case 0x6C: *L = *H; break;
    case 0x6D: *L = *L; break;
    case 0x6E: *L = memgval (*HL); break;

    /* LD (HL), r */
    case 0x77: memsval (*HL, *A); break;
    case 0x70: memsval (*HL, *B); break;
    case 0x71: memsval (*HL, *C); break;
    case 0x72: memsval (*HL, *D); break;
    case 0x73: memsval (*HL, *E); break;
    case 0x74: memsval (*HL, *H); break;
    case 0x75: memsval (*HL, *L); break;
    /* LD (HL), n */
    case 0x36: memsval (*HL,  next()); break;

    /* LD A, (rr) */
    case 0x0A: *A = memgval (*BC); break;
    case 0x1A: *A = memgval (*DE); break;
    /* LD A, (nn) */
    case 0xFA: *A = memgval (next16()); break;

    /* LD (rr), A */
    case 0x02: memsval (*BC, *A); break;
    case 0x12: memsval (*DE, *A); break;
    /* LD (nn), A */
    case 0xEA: memsval (next16(), *A); break;

    /* LD A, (C) aka LD A, ($FF00+C) */
    case 0xF2: *A = memgval (0xFF00 + (*C)); break;
    /* LD (C), A */
    case 0xE2: memsval (0xFF00 + (*C),  *A); break;

    /* LD A,(HL-) aka LD A,(HLD) aka LDD A,(HL) */
    case 0x3A: *A = memgval ((*HL)--); break;
    /* LD (HL-),A aka LD (HLD),A aka LDD (HL),A */
    case 0x32: memsval ((*HL)--,  *A); break;

    /* LD A,(HL+) aka LD A,(HLI) aka LDI A,(HL) */
    case 0x2A: *A = memgval ((*HL)++); break;
    /* LD (HL+),A aka LD (HLI),A aka LDI (HL),A */
    case 0x22: memsval ((*HL)++,  *A); break;

    /* LDH (n), A aka LD ($FF00+n), A */
    case 0xE0: memsval (0xFF00+next(),  *A); break;
    /* LDH A, (n) aka LD A, ($FF00+n) */
    case 0xF0: *A = memgval (0xFF00+next()); break;


    /* 16-Bit Loads */
    /* LD rr, nn */
    case 0x01: *BC = next16(); break;
    case 0x11: *DE = next16(); break;
    case 0x21: *HL = next16(); break;
    case 0x31:  sp = next16(); break;

    /* LD SP, HL */
    case 0xF9: sp = *HL; break;

    /* LD HL, SP+n aka LDHL SP,n*/
    case 0xF8:
      { SIGNED_BYTE n = next();
        *HL = (WORD)(sp + (SIGNED_BYTE)n);
        SETFLAGS(0, 0, HALF_CARRY(sp, n), FULL_CARRY(sp, n));
      } break;

    /* LD (nn), SP */
    case 0x08: memset16 (next16(), sp); break;

    /* PUSH rr */
    case 0xF5: push (*AF); break;
    case 0xC5: push (*BC); break;
    case 0xD5: push (*DE); break;
    case 0xE5: push (*HL); break;

    /* POP rr */
    /* NOTE: because only the 4 high bits of F are used (as the flags) we have to mask with $F0 */
    case 0xF1: *AF = pop(); *F &= 0xF0; break;
    case 0xC1: *BC = pop(); break;
    case 0xD1: *DE = pop(); break;
    case 0xE1: *HL = pop(); break;


    /* 8-Bit ALU */
    /* ADD A, r */
    case 0x87: *A = cpu_add (*A, *A); break;
    case 0x80: *A = cpu_add (*A, *B); break;
    case 0x81: *A = cpu_add (*A, *C); break;
    case 0x82: *A = cpu_add (*A, *D); break;
    case 0x83: *A = cpu_add (*A, *E); break;
    case 0x84: *A = cpu_add (*A, *H); break;
    case 0x85: *A = cpu_add (*A, *L); break;
    case 0x86: *A = cpu_add (*A, memgval (*HL)); break;
    /* ADD A, n */
    case 0xC6: *A = cpu_add (*A, next()); break;

    /* ADC A, r */
#define ADC(value)  \
        ({\
            bool old_c = FLAGC;\
            (*A) = cpu_add (*A, value);\
            if (HALF_CARRY(*A, old_c))   SETFLAGH(1);\
            if (FULL_CARRY(*A, old_c))   SETFLAGC(1);\
            (*A) += old_c;\
            SETFLAGZ((*A) == 0);\
        })
    case 0x8F: ADC(*A); break;
    case 0x88: ADC(*B); break;
    case 0x89: ADC(*C); break;
    case 0x8A: ADC(*D); break;
    case 0x8B: ADC(*E); break;
    case 0x8C: ADC(*H); break;
    case 0x8D: ADC(*L); break;
    case 0x8E: ADC(memgval (*HL)); break;
    /* ADC A, n */
    case 0xCE:
        ADC (next());
        break;
#undef ADC

    /* SUB A, r */
    case 0x97: *A = cpu_sub (*A, *A); break;
    case 0x90: *A = cpu_sub (*A, *B); break;
    case 0x91: *A = cpu_sub (*A, *C); break;
    case 0x92: *A = cpu_sub (*A, *D); break;
    case 0x93: *A = cpu_sub (*A, *E); break;
    case 0x94: *A = cpu_sub (*A, *H); break;
    case 0x95: *A = cpu_sub (*A, *L); break;
    case 0x96: *A = cpu_sub (*A, memgval (*HL)); break;
    /* SUB A, n */
    case 0xD6: *A = cpu_sub (*A, next()); break;

    /* SBC A, r */
#define SBC(value)  \
        ({\
            bool old_c = FLAGC;\
            (*A) = cpu_sub (*A, value);\
            if (HALF_BORROW(*A, old_c))    SETFLAGH(1);\
            if (FULL_BORROW(*A, old_c))    SETFLAGC(1);\
            (*A) -= old_c;\
            SETFLAGZ((*A) == 0);\
        })
    case 0x9F: SBC(*A); break;
    case 0x98: SBC(*B); break;
    case 0x99: SBC(*C); break;
    case 0x9A: SBC(*D); break;
    case 0x9B: SBC(*E); break;
    case 0x9C: SBC(*H); break;
    case 0x9D: SBC(*L); break;
    case 0x9E: SBC(memgval (*HL)); break;
    /* SBC A, n */
    case 0xDE: SBC(next()); break;
#undef SBC

    /* AND A, r */
    case 0xA7: *A &= *A; SETFLAGS(*A == 0, 0,1,0); break;
    case 0xA0: *A &= *B; SETFLAGS(*A == 0, 0,1,0); break;
    case 0xA1: *A &= *C; SETFLAGS(*A == 0, 0,1,0); break;
    case 0xA2: *A &= *D; SETFLAGS(*A == 0, 0,1,0); break;
    case 0xA3: *A &= *E; SETFLAGS(*A == 0, 0,1,0); break;
    case 0xA4: *A &= *H; SETFLAGS(*A == 0, 0,1,0); break;
    case 0xA5: *A &= *L; SETFLAGS(*A == 0, 0,1,0); break;
    case 0xA6: *A &= memgval (*HL); SETFLAGS(*A == 0, 0,1,0); break;
    /* AND A, n */
    case 0xE6: *A &= next(); SETFLAGS(*A == 0, 0,1,0); break;

    /* OR A, r */
    case 0xB7: *A |= *A; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xB0: *A |= *B; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xB1: *A |= *C; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xB2: *A |= *D; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xB3: *A |= *E; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xB4: *A |= *H; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xB5: *A |= *L; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xB6: *A |= memgval (*HL); SETFLAGS(*A == 0, 0,0,0); break;
    /* OR A, n */
    case 0xF6: *A |= next(); SETFLAGS(*A == 0, 0,0,0); break;

    /* XOR A, r */
    case 0xAF: *A ^= *A; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xA8: *A ^= *B; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xA9: *A ^= *C; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xAA: *A ^= *D; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xAB: *A ^= *E; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xAC: *A ^= *H; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xAD: *A ^= *L; SETFLAGS(*A == 0, 0,0,0); break;
    case 0xAE: *A ^= memgval (*HL); SETFLAGS(*A == 0, 0,0,0); break;
    /* XOR A, n */
    case 0xEE: *A ^= next(); SETFLAGS(*A == 0, 0,0,0); break;

    /* CP A, r */
    case 0xBF: cpu_sub (*A, *A); break;
    case 0xB8: cpu_sub (*A, *B); break;
    case 0xB9: cpu_sub (*A, *C); break;
    case 0xBA: cpu_sub (*A, *D); break;
    case 0xBB: cpu_sub (*A, *E); break;
    case 0xBC: cpu_sub (*A, *H); break;
    case 0xBD: cpu_sub (*A, *L); break;
    case 0xBE: cpu_sub (*A, memgval (*HL)); break;
    /* CP A, n */
    case 0xFE: cpu_sub (*A, next()); break;

    /* INC r */
    case 0x3C: { bool old_c = FLAGC; *A = cpu_add (*A, 1); SETFLAGC(old_c) ; } break;
    case 0x04: { bool old_c = FLAGC; *B = cpu_add (*B, 1); SETFLAGC(old_c) ; } break;
    case 0x0C: { bool old_c = FLAGC; *C = cpu_add (*C, 1); SETFLAGC(old_c) ; } break;
    case 0x14: { bool old_c = FLAGC; *D = cpu_add (*D, 1); SETFLAGC(old_c) ; } break;
    case 0x1C: { bool old_c = FLAGC; *E = cpu_add (*E, 1); SETFLAGC(old_c) ; } break;
    case 0x24: { bool old_c = FLAGC; *H = cpu_add (*H, 1); SETFLAGC(old_c) ; } break;
    case 0x2C: { bool old_c = FLAGC; *L = cpu_add (*L, 1); SETFLAGC(old_c) ; } break;
    case 0x34: { bool old_c = FLAGC; memsval (*HL, cpu_add (memgval (*HL), 1)); SETFLAGC(old_c) ; } break;

    /* DEC r */
    case 0x3D: { bool old_c = FLAGC; *A = cpu_sub (*A, 1); SETFLAGC(old_c); } break;
    case 0x05: { bool old_c = FLAGC; *B = cpu_sub (*B, 1); SETFLAGC(old_c); } break;
    case 0x0D: { bool old_c = FLAGC; *C = cpu_sub (*C, 1); SETFLAGC(old_c); } break;
    case 0x15: { bool old_c = FLAGC; *D = cpu_sub (*D, 1); SETFLAGC(old_c); } break;
    case 0x1D: { bool old_c = FLAGC; *E = cpu_sub (*E, 1); SETFLAGC(old_c); } break;
    case 0x25: { bool old_c = FLAGC; *H = cpu_sub (*H, 1); SETFLAGC(old_c); } break;
    case 0x2D: { bool old_c = FLAGC; *L = cpu_sub (*L, 1); SETFLAGC(old_c); } break;
    case 0x35: { bool old_c = FLAGC; memsval (*HL, cpu_sub (memgval (*HL), 1)); SETFLAGC(old_c); } break;


    /* 16-Bit Arithmetic */
    /* ADD HL, rr */
    case 0x09: { bool old = FLAGZ; *HL = cpu_add16 (*HL, *BC); SETFLAGZ(old); } break;
    case 0x19: { bool old = FLAGZ; *HL = cpu_add16 (*HL, *DE); SETFLAGZ(old); } break;
    case 0x29: { bool old = FLAGZ; *HL = cpu_add16 (*HL, *HL); SETFLAGZ(old); } break;
    case 0x39: { bool old = FLAGZ; *HL = cpu_add16 (*HL,  sp); SETFLAGZ(old); } break;

    /* ADD SP, n */
    case 0xE8:
     {  SIGNED_BYTE n = next();

        SETFLAGC(FULL_CARRY(sp, n));
        SETFLAGH(HALF_CARRY(sp, n));

        sp += (SIGNED_BYTE)n;

        SETFLAGZ(0);
        SETFLAGN(0);
     }  break;

    /* INC rr */
    case 0x03: (*BC) += 1; break;
    case 0x13: (*DE) += 1; break;
    case 0x23: (*HL) += 1; break;
    case 0x33:   sp  += 1; break;

    /* DEC rr */
    case 0x0B: (*BC) -= 1; break;
    case 0x1B: (*DE) -= 1; break;
    case 0x2B: (*HL) -= 1; break;
    case 0x3B:   sp  -= 1; break;


    /* Miscellaneous */
    /* DAA */
    /* FIXME: blargg says it's broken */
    case 0x27:
     {
        BYTE correction = 0x00;

        if (FLAGH)
            correction |= 0x06;
        if (FLAGC)
            correction |= 0x60;

        if (!FLAGN) {
            if (((*A) & 0x0F) > 0x09)
                correction |= 0x06;
            if ((*A) > 0x99)
                correction |= 0x60;
        }
        *A += FLAGN? -correction : correction;

        SETFLAGS(*A == 0, FLAGN, (correction & 0x60) != 0, 0);
     }  break;

    /* CPL */
    case 0x2F: *A = ~(*A); SETFLAGS(FLAGZ, 1,1, FLAGC); break;

    /* CCF */
    case 0x3F: SETFLAGS(FLAGZ, 0,0, !FLAGC); break;

    /* SCF */
    case 0x37: SETFLAGS(FLAGZ, 0,0, 1); break;

    /* NOP */
    case 0x00: break;

    /* HALT */
    /* TODO: HALT instruction repeating */
    case 0x76: cpu_halted = true; break;

    /* STOP */
    case 0x10:
        cpu_halted = true;
        memsval (R_LCDCONT, memgval (R_LCDCONT) | (1 << 7));
        break;


    /* DI */
    case 0xF3: IME = false; break;
    /* EI */
    case 0xFB: IME = true; break;


    /* Rotates and Shifts */
    /* RLCA */
    case 0x07:
        *A = ((*A) << 1) | ((*A) >> 7);
        SETFLAGS(0, 0,0, (*A) & 1);
        break;

    /* RLA */
    case 0x17:
      { BYTE tmp = ((*A) << 1) | FLAGC;
        SETFLAGC((*A) >> 7);
        *A = tmp;
        SETFLAGS(0, 0,0, FLAGC);
      } break;

    /* RRCA */
    case 0x0F:
        *A = ((*A) >> 1) | ((*A) << 7);
        SETFLAGS(0, 0,0, (*A) >> 7);
        break;

    /* RRA */
    case 0x1F:
      { BYTE tmp = ((*A) >> 1) | (FLAGC << 7);
        SETFLAGC((*A) & 1);
        *A = tmp;
        SETFLAGS(0, 0,0, FLAGC);
      } break;


    /* Jumps */
    /* JP nn */
    case 0xC3: pc = next16(); break;

    /* JP cc, nn */
    case 0xC2: { WORD nn = next16(); if (!FLAGZ) { pc = nn; condition_true = true; }} break;
    case 0xCA: { WORD nn = next16(); if ( FLAGZ) { pc = nn; condition_true = true; }} break;
    case 0xD2: { WORD nn = next16(); if (!FLAGC) { pc = nn; condition_true = true; }} break;
    case 0xDA: { WORD nn = next16(); if ( FLAGC) { pc = nn; condition_true = true; }} break;

    /* JP (HL) */
    case 0xE9: pc = *HL; break;

    /* JR n */
    case 0x18: pc += (SIGNED_BYTE)next(); break;

    /* JR cc, n */
    case 0x20: { SIGNED_BYTE offset = next(); if (!FLAGZ) { pc += offset; condition_true = true; }} break;
    case 0x28: { SIGNED_BYTE offset = next(); if ( FLAGZ) { pc += offset; condition_true = true; }} break;
    case 0x30: { SIGNED_BYTE offset = next(); if (!FLAGC) { pc += offset; condition_true = true; }} break;
    case 0x38: { SIGNED_BYTE offset = next(); if ( FLAGC) { pc += offset; condition_true = true; }} break;


    /* Calls */
    /* CALL nn */
    case 0xCD:
      { WORD addr = next16();
        push (pc);
        pc = addr;
      } break;

    /* CALL cc, nn */
    case 0xC4: { WORD addr = next16(); if (!FLAGZ) { push (pc); pc = addr; condition_true = true; }} break;
    case 0xCC: { WORD addr = next16(); if ( FLAGZ) { push (pc); pc = addr; condition_true = true; }} break;
    case 0xD4: { WORD addr = next16(); if (!FLAGC) { push (pc); pc = addr; condition_true = true; }} break;
    case 0xDC: { WORD addr = next16(); if ( FLAGC) { push (pc); pc = addr; condition_true = true; }} break;


    /* Restarts */
    /* RST n */
    case 0xC7: cpu_restart (0x00); break;
    case 0xCF: cpu_restart (0x08); break;
    case 0xD7: cpu_restart (0x10); break;
    case 0xDF: cpu_restart (0x18); break;
    case 0xE7: cpu_restart (0x20); break;
    case 0xEF: cpu_restart (0x28); break;
    case 0xF7: cpu_restart (0x30); break;
    case 0xFF: cpu_restart (0x38); break;


    /* Returns */
    /* RET */
    case 0xC9: pc = pop(); break;

    /* RET cc */
    case 0xC0: if (!FLAGZ) { pc = pop(); condition_true = true; } break;
    case 0xC8: if ( FLAGZ) { pc = pop(); condition_true = true; } break;
    case 0xD0: if (!FLAGC) { pc = pop(); condition_true = true; } break;
    case 0xD8: if ( FLAGC) { pc = pop(); condition_true = true; } break;

    /* RETI */
    case 0xD9: pc = pop(); IME = true; break;



    /* unknown opcode */
    default:
        fatal ("ILLEGAL INSTRUCTION: Invalid instruction `$%.2hhX'", opcode);
        break;
    }
    return condition_true;
}

/* cpu_ack_interrupts: acknowledge any interrupts */
static void cpu_ack_interrupts(void) {

    static const char *interrupt_names[] = { "VBLANK", "LCD CONTROLLER", "TIMER OVERFLOW", "SERIAL I/O ENDED", "BUTTON RELEASE" };

    BYTE IFLAGS  = memgval (R_IFLAGS);
    BYTE ISWITCH = memgval (R_ISWITCH);

    /* check if an interrupt has occurred and is enabled */
    for (unsigned i = 0; i < 5; ++i) {
        if (GETBIT(IFLAGS, i)) {
            cpu_halted = false;
            if (GETBIT(ISWITCH, i) && IME) {

                debug ("===%s INTERRUPT ACKNOWLEDGE===", interrupt_names[i]);

                push (pc);

                pc = 0x40 + (8 * i);

                IME = false;
                RESBIT(IFLAGS, i);

                memsval (R_IFLAGS, IFLAGS);
                break;
            }
            //else
            //    debug ("%s interrupt requested, but disabled", interrupt_names[i]);
        }
    }
}

/* cpu_cleanup:  */
void cpu_cleanup(void) {
    free (AF);

    /* FIXME: for some reason, GNU
     *  readline has a pointer that
     *  overlaps BC, causing an invalid
     *  free() error at exit */
//    if (!G_state.debug.enabled)
        free (BC);

    free (DE);
    free (HL);
}

