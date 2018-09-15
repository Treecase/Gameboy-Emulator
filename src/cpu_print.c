/*
 * gameboy opcode printing
 *
 */

#include "cpu_print.h"
#include "cpu.h"

#include "common.h"
#include "registers.h"
#include "mem.h"
#include "logging.h"



static WORD print_pc;



static BYTE next(void)
{ return memgval (print_pc++); }

static WORD next16(void) {
    WORD val = (memgval (print_pc + 1) << 8) | memgval (print_pc);
    print_pc += 2;
    return val;
}



/* PUBLIC API */
/* print_op: print an opcode */
WORD print_op (BYTE opcode, WORD _pc) {
    LOG_DOLOG = cpu_logging_enabled();

    print_pc = _pc;
    debugl ("%.2hhX  ", opcode);

    static const char *r[8] = { "B", "C", "D", "E", "H", "L", "(HL)", "A" };

    switch (opcode) {
    /* $10 Prefix */
    case 0x10:
        opcode = next();
        debugl ("%.2hhX  ", opcode);
        switch (opcode) {
        /* STOP */
        case 0x00: debug ("STOP"); break;

        default:
            error ("unknown opcode $10 $%.2hhX", opcode);
            break;
        }
        break;
    /* END $10 Prefix */

    /* $CB Prefix */
    case 0xCB:
        opcode = next();
        debugl ("%.2hhX  ", opcode);
        switch (opcode) {
        /* Miscellaneous */
        /* SWAP r */
        case 0x37:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
            debug ("SWAP %s", r[opcode & 7]);
            break;
        /* Rotates and Shifts (registers) */
        /* RLC r */
        case 0x07:
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
            debug ("RLC %s", r[opcode & 7]);
            break;

        /* RL r */
        case 0x17:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
            debug ("RL %s", r[opcode & 7]);
            break;

        /* RRC r */
        case 0x0F:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
            debug ("RRC %s", r[opcode & 7]);
            break;

        /* RR r */
        case 0x1F:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
            debug ("RR %s", r[opcode & 7]);
            break;

        /* SLA r */
        case 0x27:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
            debug ("SLA %s", r[opcode & 7]);
            break;

        /* SRA r */
        case 0x2F:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
            debug ("SRA %s", r[opcode & 7]);
            break;

        /* SRL r */
        case 0x3F:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
            debug ("SRL %s", r[opcode & 7]);
            break;


        /* NOTE: this makes use of the nonstandard `Case Ranges' GNU C extension */
        /* Bit Opcodes */
        /* BIT b, r */
        case 0x40 ... 0x7F:
            debug ("BIT %i, %s", (opcode >> 3) & 7, r[opcode & 7]);
            break;

        /* RES b, r */
        case 0x80 ... 0xBF:
            debug ("RES %i, %s", (opcode >> 3) & 7, r[opcode & 7]);
            break;

        /* SET b, r */
        case 0xC0 ... 0xFF:
            debug ("SET %i, %s", (opcode >> 3) & 7, r[opcode & 7]);
            break;


        default:
            /* should never happen */
            error ("unknown opcode $CB $%.2hhX", opcode);
            break;
        }
        break;
    /* END $CB Prefix */


    /* 8-Bit Loads */
    /* LD r, n */
    case 0x3E:
    case 0x06:
    case 0x0E:
    case 0x16:
    case 0x1E:
    case 0x26:
    case 0x2E:
        debug ("LD %s $%.2hhX", r[(opcode >> 3) & 7], next());
        break;


    /* LD r1, r2 */
    /* LD A, r */
    case 0x7F:
    case 0x78:
    case 0x79:
    case 0x7A:
    case 0x7B:
    case 0x7C:
    case 0x7D:
    case 0x7E:
        debug ("LD A, %s", r[opcode & 7]);
        break;

    /* LD B, r */
    case 0x47:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
        debug ("LD B, %s", r[opcode & 7]);
        break;

    /* LD C, r */
    case 0x4F:
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
        debug ("LD C, %s", r[opcode & 7]);
        break;

    /* LD D, r */
    case 0x57:
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
    case 0x56:
        debug ("LD D, %s", r[opcode & 7]);
        break;

    /* LD E, r */
    case 0x5F:
    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B:
    case 0x5C:
    case 0x5D:
    case 0x5E:
        debug ("LD E, %s", r[opcode & 7]);
        break;

    /* LD H, r */
    case 0x67:
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
        debug ("LD H, %s", r[opcode & 7]);
        break;

    /* LD L, r */
    case 0x6F:
    case 0x68:
    case 0x69:
    case 0x6A:
    case 0x6B:
    case 0x6C:
    case 0x6D:
    case 0x6E:
        debug ("LD L, %s", r[opcode & 7]);
        break;

    /* LD (HL), r */
    case 0x77:
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
        debug ("LD (HL), %s", r[opcode & 7]);
        break;
    /* LD (HL), n */
    case 0x36:
        debug ("LD (HL), $%.2hhX", next());
        break;

    /* LD A, (rr) */
    case 0x0A: debug ("LD A, (BC)"); break;
    case 0x1A: debug ("LD A, (DE)"); break;
    /* LD A, (nn) */
    case 0xFA: debug ("LD A, ($%.4hX)", next16()); break;

    /* LD (rr), A */
    case 0x02: debug ("LD (BC), A"); break;
    case 0x12: debug ("LD (DE), A"); break;
    /* LD (nn), A */
    case 0xEA: debug ("LD ($%.4hX), A", next16()); break;

    /* LD A, (C) aka LD A, ($FF00+C) */
    case 0xF2: debug ("LD A, ($FF00+C)"); break;
    /* LD (C), A */
    case 0xE2: debug ("LD ($FF00+C), A"); break;

    /* LD A,(HL-) aka LD A,(HLD) aka LDD A,(HL) */
    case 0x3A: debug ("LD A, (HL)"); break;
    /* LD (HL-),A aka LD (HLD),A aka LDD (HL),A */
    case 0x32: debug ("LD (HL-), A"); break;

    /* LD A,(HL+) aka LD A,(HLI) aka LDI A,(HL) */
    case 0x2A: debug ("LD A, (HL+)"); break;
    /* LD (HL+),A aka LD (HLI),A aka LDI (HL),A */
    case 0x22: debug ("LD (HL+), A"); break;

    /* LDH (n), A aka LD ($FF00+n), A */
    case 0xE0:
  { BYTE n = next();
    debug ("LD ($FF00+$%.2hhX)(%s), A", n, register_name (n));
  } break;
    /* LDH A, (n) aka LD A, ($FF00+n) */
    case 0xF0:
  { BYTE n = next();
    debug ("LD A, ($FF00+$%.2hhX)(%s)", n, register_name (n));
  } break;


    /* 16-Bit Loads */
    /* LD rr, nn */
    case 0x01: debug ("LD BC, $%.4hX", next16()); break;
    case 0x11: debug ("LD DE, $%.4hX", next16()); break;
    case 0x21: debug ("LD HL, $%.4hX", next16()); break;
    case 0x31: debug ("LD SP, $%.4hX", next16()); break;

    /* LD SP, HL */
    case 0xF9: debug ("LD SP, HL"); break;

    /* LD HL, SP+n aka LDHL SP,n*/
    case 0xF8: debug ("LD HL, SP+$%.2hhX", next()); break;

    /* LD (nn), SP */
    case 0x08: debug ("LD ($%.4hX), SP", next16()); break;

    /* PUSH rr */
    case 0xF5: debug ("PUSH AF"); break;
    case 0xC5: debug ("PUSH BC"); break;
    case 0xD5: debug ("PUSH DE"); break;
    case 0xE5: debug ("PUSH HL"); break;

    /* POP rr */
    case 0xF1: debug ("POP AF"); break;
    case 0xC1: debug ("POP BC"); break;
    case 0xD1: debug ("POP DE"); break;
    case 0xE1: debug ("POP HL"); break;


    /* 8-Bit ALU */
    /* ADD A, r */
    case 0x87:
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x85:
    case 0x86:
        debug ("ADD A, %s", r[opcode & 7]);
        break;
    /* ADD A, n */
    case 0xC6:
        debug ("ADD A, $%.2hhX", next());
        break;

    /* ADC A, r */
    case 0x8F:
    case 0x88:
    case 0x89:
    case 0x8A:
    case 0x8B:
    case 0x8C:
    case 0x8D:
    case 0x8E:
        debug ("ADC A, %s", r[opcode & 7]);
        break;
    /* ADC A, n */
    case 0xCE:
        debug ("ADC A, $%.2hhX", next());
        break;

    /* SUB A, r */
    case 0x97:
    case 0x90:
    case 0x91:
    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x96:
        debug ("SUB A, %s", r[opcode & 7]);
        break;
    /* SUB A, n */
    case 0xD6:
        debug ("SUB A, $%.2hhX", next());
        break;

    /* SBC A, r */
    case 0x9F:
    case 0x98:
    case 0x99:
    case 0x9A:
    case 0x9B:
    case 0x9C:
    case 0x9D:
    case 0x9E:
        debug ("SBC A, %s", r[opcode & 7]);
        break;
    /* SBC A, n   NOTE: undocumented */
    //case 0xDE?: *A = cpu_add (*A, next() + FLAGC); break;

    /* AND A, r */
    case 0xA7:
    case 0xA0:
    case 0xA1:
    case 0xA2:
    case 0xA3:
    case 0xA4:
    case 0xA5:
    case 0xA6:
        debug ("AND A, %s", r[opcode & 7]);
        break;
    /* AND A, n */
    case 0xE6:
        debug ("AND A, $%.2hhX", next());
        break;

    /* OR A, r */
    case 0xB7:
    case 0xB0:
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
        debug ("OR A, %s", r[opcode & 7]);
        break;
    /* OR A, n */
    case 0xF6:
        debug ("OR A, $%.2hhX", next());
        break;

    /* XOR A, r */
    case 0xAF:
    case 0xA8:
    case 0xA9:
    case 0xAA:
    case 0xAB:
    case 0xAC:
    case 0xAD:
    case 0xAE:
        debug ("XOR A, %s", r[opcode & 7]);
        break;
    /* XOR A, n */
    case 0xEE:
        debug ("XOR A, $%.2hhX", next());
        break;

    /* CP A, r */
    case 0xBF:
    case 0xB8:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
    case 0xBD:
    case 0xBE:
        debug ("CP A, %s", r[opcode & 7]);
        break;
    /* CP A, n */
    case 0xFE:
        debug ("CP A, $%.2hhX", next());
        break;

    /* INC r */
    case 0x3C: debug ("INC A"); break;
    case 0x04: debug ("INC B"); break;
    case 0x0C: debug ("INC C"); break;
    case 0x14: debug ("INC D"); break;
    case 0x1C: debug ("INC E"); break;
    case 0x24: debug ("INC H"); break;
    case 0x2C: debug ("INC L"); break;
    case 0x34: debug ("INC (HL)"); break;

    /* DEC r */
    case 0x3D: debug ("DEC A"); break;
    case 0x05: debug ("DEC B"); break;
    case 0x0D: debug ("DEC C"); break;
    case 0x15: debug ("DEC D"); break;
    case 0x1D: debug ("DEC E"); break;
    case 0x25: debug ("DEC H"); break;
    case 0x2D: debug ("DEC L"); break;
    case 0x35: debug ("DEC (HL)"); break;


    /* 16-Bit Arithmetic */
    /* ADD HL, rr */
    case 0x09: debug ("ADD HL, BC"); break;
    case 0x19: debug ("ADD HL, DE"); break;
    case 0x29: debug ("ADD HL, HL"); break;
    case 0x39: debug ("ADD HL, SP"); break;

    /* ADD SP, n */
    case 0xE8: debug ("ADD SP, $%.2hhX", next()); break;

    /* INC rr */
    case 0x03: debug ("INC BC"); break;
    case 0x13: debug ("INC DE"); break;
    case 0x23: debug ("INC HL"); break;
    case 0x33: debug ("INC SP"); break;

    /* DEC rr */
    case 0x0B: debug ("DEC BC"); break;
    case 0x1B: debug ("DEC DE"); break;
    case 0x2B: debug ("DEC HL"); break;
    case 0x3B: debug ("DEC SP"); break;


    /* Miscellaneous */
    /* DAA */
    case 0x27: debug ("DAA"); break;

    /* CPL */
    case 0x2F: debug ("CPL"); break;

    /* CCF */
    case 0x3F: debug ("CCF"); break;

    /* SCF */
    case 0x37: debug ("SCF"); break;

    /* NOP */
    case 0x00: debug ("NOP"); break;

    /* HALT */
    case 0x76: debug ("HALT"); break;

    /* DI */
    case 0xF3: debug ("DI"); break;
    /* EI */
    case 0xFB: debug ("EI"); break;


    /* Rotates and Shifts */
    /* RLCA */
    case 0x07: debug ("RLCA"); break;

    /* RLA */
    case 0x17: debug ("RLA"); break;

    /* RRCA */
    case 0x0F: debug ("RRCA"); break;

    /* RRA */
    case 0x1F: debug ("RRA"); break;


    /* Jumps */
    /* JP nn */
    case 0xC3: debug ("JP $%.4hX", next16()); break;

    /* JP cc, nn */
    case 0xC2: debug ("JP NZ, $%.4hX", next16()); break;
    case 0xCA: debug ("JP Z, $%.4hX", next16()); break;
    case 0xD2: debug ("JP NC, $%.4hX", next16()); break;
    case 0xDA: debug ("JP C, $%.4hX", next16()); break;

    /* JP (HL) */
    case 0xE9: debug ("JP (HL)($%.4hX)", *HL); break;

    /* JR n */
    case 0x18: debug ("JR Addr_%.4hX", (WORD)(print_pc + (SIGNED_BYTE)next())); break;

    /* JR cc, n */
    case 0x20: debug ("JR NZ, Addr_%.4hX", (WORD)(print_pc + (SIGNED_BYTE)next())); break;
    case 0x28: debug ("JR Z, Addr_%.4hX",  (WORD)(print_pc + (SIGNED_BYTE)next())); break;
    case 0x30: debug ("JR NC, Addr_%.4hX", (WORD)(print_pc + (SIGNED_BYTE)next())); break;
    case 0x38: debug ("JR C, Addr_%.4hX",  (WORD)(print_pc + (SIGNED_BYTE)next())); break;


    /* Calls */
    /* CALL nn */
    case 0xCD: debug ("CALL $%.4hX", next16()); break;

    /* CALL cc, nn */
    case 0xC4: debug ("CALL NZ, $%.4hX", next16()); break;
    case 0xCC: debug ("CALL Z, $%.4hX", next16()); break;
    case 0xD4: debug ("CALL NC, $%.4hX", next16()); break;
    case 0xDC: debug ("CALL C, $%.4hX", next16()); break;


    /* Restarts */
    /* RST n */
    case 0xC7: debug ("RST $00"); break;
    case 0xCF: debug ("RST $08"); break;
    case 0xD7: debug ("RST $10"); break;
    case 0xDF: debug ("RST $18"); break;
    case 0xE7: debug ("RST $20"); break;
    case 0xEF: debug ("RST $28"); break;
    case 0xF7: debug ("RST $30"); break;
    case 0xFF: debug ("RST $38"); break;


    /* Returns */
    /* RET */
    case 0xC9: debug ("RET"); break;

    /* RET cc */
    case 0xC0: debug ("RET NZ"); break;
    case 0xC8: debug ("RET Z"); break;
    case 0xD0: debug ("RET NC"); break;
    case 0xD8: debug ("RET C"); break;

    /* RETI */
    case 0xD9: debug ("RETI"); break;



    /* unknown opcode */
    default:
        error ("unknown opcode $%.2hhX", opcode);
        break;
    }
    return print_pc;
}

