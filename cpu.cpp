//
// Created by edi on 4/22/26.
//

#include <iostream>
#include "cpu.h"

void Cpu::set_flag(uint8_t flag, bool value) {
    if (value) {
        F |= flag;
    } else {
        F &= ~flag;
    }
}

uint16_t Cpu::combine(uint8_t high, uint8_t low) const {
    return static_cast<uint16_t>(high) << 8 | low;
}

void Cpu::split(uint8_t& high, uint8_t& low, uint16_t value) {
    high = value >> 8;
    low = value & 0xFF;
}

uint16_t Cpu::af() const {
    return combine(A, F);
}

void Cpu::set_af(uint16_t value) {
    A = value >> 8;
    F = value & 0xF0;
}

uint16_t Cpu::bc() const {
    return combine(B, C);
}

void Cpu::set_bc(uint16_t value) {
    split(B, C, value);
}

uint16_t Cpu::de() const {
    return combine(D, E);
}

void Cpu::set_de(uint16_t value) {
    split(D, E, value);
}

uint16_t Cpu::hl() const {
    return combine(H, L);
}

void Cpu::set_hl(uint16_t value) {
    split(H, L, value);
}

uint8_t Cpu::rr(uint8_t value, bool set_z) { // helper for the 'rr' operations
    uint8_t old_carry = (F & FLAG_C) ? 1 : 0;
    uint8_t saved_bit = value & 1;
    value = value >> 1;
    value |= (old_carry << 7);
    if (set_z) {
        set_flag(FLAG_Z, value == 0);
    } else {
        set_flag(FLAG_Z, false);
    }
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, saved_bit != 0x00);
    return value;
}

uint8_t Cpu::rl(uint8_t value, bool set_z) {
    uint8_t old_carry = (F & FLAG_C) ? 1 : 0;
    uint8_t saved_bit = (value >> 7) & 1;
    value = value << 1;
    value |= old_carry;
    if (set_z) {
        set_flag(FLAG_Z, value == 0);
    } else {
        set_flag(FLAG_Z, false);
    }
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, saved_bit != 0x00);
    return value;
}

uint8_t Cpu::rrc(uint8_t value, bool set_z) { // helper for the 'rrc' operations
    uint8_t saved_bit = value & 1;
    value = value >> 1;
    value |= (saved_bit << 7);
    if (set_z) {
        set_flag(FLAG_Z, value == 0);
    } else {
        set_flag(FLAG_Z, false);
    }
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, saved_bit != 0x00);
    return value;
}

uint8_t Cpu::rlc(uint8_t value, bool set_z) {
    uint8_t saved_bit = (value >> 7) & 1;
    value = value << 1;
    value |= saved_bit;
    if (set_z) {
        set_flag(FLAG_Z, value == 0);
    } else {
        set_flag(FLAG_Z, false);
    }
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, saved_bit != 0x00);
    return value;
}

uint8_t Cpu::srl(uint8_t value) { // helper for the 'srl' operations
    uint8_t saved_bit = value & 1;
    value = value >> 1;
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, saved_bit != 0x00);
    return value;
}

uint8_t Cpu::or_x(uint8_t value) { // helper for the 'or' operation
    A |= value;
    set_flag(FLAG_Z, A == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, false);
    return A;
}

uint8_t Cpu::xor_x(uint8_t value) { // helper for the 'xor' operation
    A ^= value;
    set_flag(FLAG_Z, A == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, false);
    return A;
}

uint8_t Cpu::and_x(uint8_t value) { // helper for the 'and' operation
    A &= value;
    set_flag(FLAG_Z, A == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, true);
    set_flag(FLAG_C, false);
    return A;
}

uint8_t Cpu::add(uint8_t value, bool with_carry) {   // helper for the 'add' function
    uint8_t carry_in = (F & FLAG_C) ? 1 : 0;         // and also works for the 'adc'
    uint16_t sum = static_cast<uint16_t>(A) + value; // (add with carry)
    bool half_carry, carry;
    if (with_carry) {
        half_carry = ((A & 0x0F) + (value & 0x0F)
                        + carry_in) > 0x0F;
        carry = (sum + carry_in) > 0xFF;
        A = A + value + carry_in;
    }
    else {
        half_carry = ((A & 0x0F)
                    + (value & 0x0F)) > 0x0F;
        carry = sum > 0xFF;
        A = A + value;
    }
    set_flag(FLAG_Z, A == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, half_carry);
    set_flag(FLAG_C, carry);
    return A;
}

uint8_t Cpu::sub(uint8_t value, bool with_carry) {  // helper for the 'sub' function
    uint8_t carry_in = (F & FLAG_C) ? 1 : 0;        // and also works for the 'sbc'
    bool half_carry, carry;                         // (subtract with carry)
    if (with_carry) {
        half_carry = (A & 0x0F) <
                    ((value & 0x0F) + carry_in);
        carry = A < (value + carry_in);
        A = A - value - carry_in;
    }
    else {
        half_carry = (A & 0x0F) < (value & 0x0F);
        carry = A < value;
        A = A - value;
    }
    set_flag(FLAG_Z, A == 0);
    set_flag(FLAG_N, true);
    set_flag(FLAG_H, half_carry);
    set_flag(FLAG_C, carry);
    return A;
}

void Cpu::cp(uint8_t value) {      // helper for the 'cp' (compare) operation
    set_flag(FLAG_Z, A == value);
    set_flag(FLAG_N, true);
    set_flag(FLAG_H, (A & 0x0F) < (value & 0x0F));
    set_flag(FLAG_C, A < value);
}

void Cpu::ret() {           // helper for 'ret' operation
    uint8_t low = mem.read(SP);
    SP++;
    uint8_t high = mem.read(SP);
    SP++;
    PC = combine(high, low);
}

Cpu::Cpu(Memory& memory) : mem(memory) {
    A = 0x01;
    F = 0xB0;
    B = 0x00;
    C = 0x13;
    D = 0x00;
    E = 0xD8;
    H = 0x01;
    L = 0x4D;
    SP = 0xFFFE;
    PC = 0x0100;
}
void Cpu::step() {
    uint8_t opcode = mem.read(PC);
    PC++;

    switch (opcode)
    {
    case 0x00: // NOP
        break;

    case 0x01: { // LD BC, d16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            set_bc(combine(high, low));
            PC += 2;
            break;
        }

    case 0x02: { // LD (BC), A
            mem.write(bc(), A);
            break;
        }

    case 0x03: { // INC BC
            set_bc(bc() + 1);
            break;
        }

    case 0x04: { // INC B
            B++;
            set_flag(FLAG_Z, B == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, (B & 0x0F) == 0x00);
            break;
        }

    case 0x05: { // DEC B
            B--;
            set_flag(FLAG_Z, B == 0);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, (B & 0x0F) == 0x0F);
            break;
        }

    case 0x06: { // LD B, d8
            B = mem.read(PC);
            PC++;
            break;
        }

    case 0x07: { // RLCA
            A = rlc(A, false);
            break;
        }

    case 0x08: { // LD (a16), SP
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            uint16_t address = combine(high, low);
            mem.write(address, SP);
            mem.write(address + 1, SP >> 8);
            PC += 2;
            break;
        }

    case 0x09: { // ADD HL, BC
            uint32_t sum = static_cast<uint32_t>(hl()) + bc();
            bool carry = sum > 0xFFFF;
            bool half_carry = ((hl() & 0x0FFF) + (bc() & 0x0FFF)) > 0x0FFF;
            set_hl(bc() + hl());
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, half_carry);
            set_flag(FLAG_C, carry);
            break;
        }

    case 0x0A: { // LD A, (BC)
            A = mem.read(bc());
            break;
        }

    case 0x0B: { // DEC BC
            set_bc(bc() - 1);
            break;
        }

    case 0x0C: { // INC C
            C++;
            set_flag(FLAG_Z, C == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, (C & 0x0F) == 0x00);
            break;
        }

    case 0x0D: { // DEC C
            C--;
            set_flag(FLAG_Z, C == 0);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, (C & 0x0F) == 0x0F);
            break;
        }

    case 0x0E: { // LD C, d8
            C = mem.read(PC);
            PC++;
            break;
        }

    case 0x0F: { // RRCA
            A = rrc(A, false);
            break;
        }

    case 0x10: { /* STOP (kinda useless as an instruction,
                    therefore not yet implemented properly) */
            PC++;
            break;
        }

    case 0x11: { // LD DE, d16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            set_de(combine(high, low));
            PC += 2;
            break;
        }

    case 0x12: { // LD (DE), A
            mem.write(de(), A);
            break;
        }

    case 0x13: { // INC DE
            set_de(de() + 1);
            break;
        }

    case 0x14: { // INC D
            D++;
            set_flag(FLAG_Z, D == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, (D & 0x0F) == 0x00);
            break;
        }

    case 0x15: { // DEC D
            D--;
            set_flag(FLAG_Z, D == 0);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, (D & 0x0F) == 0x0F);
            break;
        }

    case 0x16: { // LD D, d8
            D = mem.read(PC);
            PC++;
            break;
        }

    case 0x17: { // RLA
            A = rl(A, false);
            break;
        }

    case 0x18: { // JR s8
            int8_t offset = static_cast<int8_t>(mem.read(PC));
            PC += offset + 1;
            break;
        }

    case 0x19: { // ADD HL, DE
            uint32_t sum = static_cast<uint32_t>(hl()) + de();
            bool carry = sum > 0xFFFF;
            bool half_carry = ((hl() & 0x0FFF) + (de() & 0x0FFF)) > 0x0FFF;
            set_hl(de() + hl());
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, half_carry);
            set_flag(FLAG_C, carry);
            break;
        }

    case 0x1A: { // LD A, (DE)
            A = mem.read(de());
            break;
        }

    case 0x1B: { // DEC DE
            set_de(de() - 1);
            break;
        }

    case 0x1C: { // INC E
            E++;
            set_flag(FLAG_Z, E == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, (E & 0x0F) == 0x00);
            break;
        }

    case 0x1D: { // DEC E
            E--;
            set_flag(FLAG_Z, E == 0);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, (E & 0x0F) == 0x0F);
            break;
        }

    case 0x1E: { // LD E, d8
            E = mem.read(PC);
            PC++;
            break;
        }

    case 0x1F: { // RRA
            A = rr(A, false);
            break;
        }

    case 0x20: { // JR NZ, s8
            int8_t offset = static_cast<int8_t>(mem.read(PC));
            PC++;
            if (!(F & FLAG_Z)) {
                PC += offset;
            }
            break;
        }

    case 0x21: { // LD HL, d16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            set_hl(combine(high, low));
            PC += 2;
            break;
        }

    case 0x22: { // LD (HL+), A
            mem.write(hl(), A);
            set_hl(hl() + 1);
            break;
        }

    case 0x23: { // INC HL
            set_hl(hl() + 1);
            break;
        }

    case 0x24: { // INC H
            H++;
            set_flag(FLAG_Z, H == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, (H & 0x0F) == 0x00);
            break;
        }

    case 0x25: { // DEC H
            H--;
            set_flag(FLAG_Z, H == 0);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, (H & 0x0F) == 0x0F);
            break;
        }

    case 0x26: { // LD H, d8
            H = mem.read(PC);
            PC++;
            break;
        }

    case 0x27: { // DAA
            // TODO: <-
            break;
        }

    case 0x28: { // JR Z, s8
            int8_t offset = static_cast<int8_t>(mem.read(PC));
            PC++;
            if (F & FLAG_Z) {
                PC += offset;
            }
            break;
        }

    case 0x29: { // ADD HL, HL
            uint32_t sum = static_cast<uint32_t>(hl()) + hl();
            bool carry = sum > 0xFFFF;
            bool half_carry = ((hl() & 0x0FFF) + (hl() & 0x0FFF)) > 0x0FFF;
            set_hl(hl() + hl());
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, half_carry);
            set_flag(FLAG_C, carry);
            break;
        }

    case 0x2A: { // LD A, (HL+)
            A = mem.read(hl());
            set_hl(hl() + 1);
            break;
        }

    case 0x2B: { // DEC HL
            set_hl(hl() - 1);
            break;
        }

    case 0x2C: { // INC L
            L++;
            set_flag(FLAG_Z, L == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, (L & 0x0F) == 0x00);
            break;
        }

    case 0x2D: { // DEC L
            L--;
            set_flag(FLAG_Z, L == 0);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, (L & 0x0F) == 0x0F);
            break;
        }

    case 0x2E: { // LD L, d8
            L = mem.read(PC);
            PC++;
            break;
        }

    case 0x2F: { // CPL
            // TODO: <-
            break;
        }

    case 0x30: { // JR NC, s8
            int8_t offset = static_cast<int8_t>(mem.read(PC));
            PC++;
            if (!(F & FLAG_C)) {
                PC += offset;
            }
            break;
        }

    case 0x31: { // LD SP, d16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            SP = combine(high, low);
            PC += 2;
            break;
        }

    case 0x32: { // LD (HL-), A
            mem.write(hl(), A);
            set_hl(hl() - 1);
            break;
        }

    case 0x33: { // INC SP
            SP++;
            break;
        }

    case 0x34: { // INC (HL)
            uint8_t HL = mem.read(hl());
            HL++;
            mem.write(hl(), HL);
            set_flag(FLAG_Z, HL == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, (HL & 0x0F) == 0x00);
            break;
        }

    case 0x35: { // DEC (HL)
            uint8_t HL = mem.read(hl());
            HL--;
            mem.write(hl(), HL);
            set_flag(FLAG_Z, HL == 0);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, (HL & 0x0F) == 0x0F);
            break;
        }

    case 0x36: { // LD (HL), d8
            mem.write(hl(), mem.read(PC));
            PC++;
            break;
        }

    case 0x37: { // SCF
            // TODO: <-
            break;
        }

    case 0x38: { // JR C, s8
            int8_t offset = static_cast<int8_t>(mem.read(PC));
            PC++;
            if (F & FLAG_C) {
                PC += offset;
            }
            break;
        }

    case 0x39: { // ADD HL, SP
            uint32_t sum = static_cast<uint32_t>(hl()) + SP;
            bool carry = sum > 0xFFFF;
            bool half_carry = ((hl() & 0x0FFF) + (SP & 0x0FFF)) > 0x0FFF;
            set_hl(hl() + SP);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, half_carry);
            set_flag(FLAG_C, carry);
            break;
        }

    case 0x3A: { // LD A, (HL-)
            A = mem.read(hl());
            set_hl(hl() - 1);
            break;
        }

    case 0x3B: { // DEC SP
            SP--;
            break;
        }

    case 0x3C: { // INC A
            A++;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, (A & 0x0F) == 0x00);
            break;
        }

    case 0x3D: { // DEC A
            A--;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, (A & 0x0F) == 0x0F);
            break;
        }

    case 0x3E: { // LD A, d8
            A = mem.read(PC);
            PC++;
            break;
        }

    case 0x3F: {
            // TODO: <-
            break;
        }

    case 0x40: // LD B, B
        break;

    case 0x41: { // LD B, C
            B = C;
            break;
        }

    case 0x42: { // LD B, D
            B = D;
            break;
        }

    case 0x43: { // LD B, E
            B = E;
            break;
        }

    case 0x44: { // LD B, H
            B = H;
            break;
        }

    case 0x45: { // LD B, L
            B = L;
            break;
        }

    case 0x46: { // LD B, (HL)
            B = mem.read(hl());
            break;
        }

    case 0x47: { // LD B, A
            B = A;
            break;
        }

    case 0x48: { // LD C, B
            C = B;
            break;
        }

    case 0x49: // LD C, C
        break;

    case 0x4A: { // LD C, D
            C = D;
            break;
        }

    case 0x4B: { // LD C, E
            C = E;
            break;
        }

    case 0x4C: { // LD C, H
            C = H;
            break;
        }

    case 0x4D: { // LD C, L
            C = L;
            break;
        }

    case 0x4E: { // LD C, (HL)
            C = mem.read(hl());
            break;
        }

    case 0x4F: { // LD C, A
            C = A;
            break;
        }

    case 0x50: { // LD D, B
            D = B;
            break;
        }

    case 0x51: { // LD D, C
            D = C;
            break;
        }

    case 0x52: // LD D, D
        break;

    case 0x53: { // LD D, E
            D = E;
            break;
        }

    case 0x54: { // LD D, H
            D = H;
            break;
        }

    case 0x55: { // LD D, L
            D = L;
            break;
        }

    case 0x56: { // LD D, (HL)
            D = mem.read(hl());
            break;
        }

    case 0x57: { // LD D, A
            D = A;
            break;
        }

    case 0x58: { // LD E, B
            E = B;
            break;
        }

    case 0x59: { // LD E, C
            E = C;
            break;
        }

    case 0x5A: { // LD E, D
            E = D;
            break;
        }

    case 0x5B:// LD E, E
        break;

    case 0x5C: { // LD E, H
            E = H;
            break;
        }

    case 0x5D: { // LD E, L
            E = L;
            break;
        }

    case 0x5E: { // LD E, (HL)
            E = mem.read(hl());
            break;
        }

    case 0x5F: { // LD E, A
            E = A;
            break;
        }

    case 0x60: { // LD H, B
            H = B;
            break;
        }

    case 0x61: { // LD H, C
            H = C;
            break;
        }

    case 0x62: { // LD H, D
            H = D;
            break;
        }

    case 0x63: { // LD H, E
            H = E;
            break;
        }

    case 0x64:
        break;

    case 0x65: { // LD H, L
            H = L;
            break;
        }

    case 0x66: { // LD H, (HL)
            H = mem.read(hl());
            break;
        }

    case 0x67: { // LD H, A
            H = A;
            break;
        }

    case 0x68: { // LD L, B
            L = B;
            break;
        }

    case 0x69: { // LD L, C
            L = C;
            break;
        }

    case 0x6A: { // LD L, D
            L = D;
            break;
        }

    case 0x6B: { // LD L, E
            L = E;
            break;
        }

    case 0x6C: { // LD L, H
            L = H;
            break;
        }

    case 0x6D: // LD L, L
        break;

    case 0x6E: { // LD L, (HL)
            L = mem.read(hl());
            break;
        }

    case 0x6F: { // LD L, A
            L = A;
            break;
        }

    case 0x70: { // LD (HL), B
            mem.write(hl(), B);
            break;
        }

    case 0x71: { // LD (HL), C
            mem.write(hl(), C);
            break;
        }

    case 0x72: { // LD (HL), D
            mem.write(hl(), D);
            break;
        }

    case 0x73: { // LD (HL), E
            mem.write(hl(), E);
            break;
        }

    case 0x74: { // LD (HL), H
            mem.write(hl(), H);
            break;
        }

    case 0x75: { // LD (HL), L
            mem.write(hl(), L);
            break;
        }

    case 0x76: { // HALT
            // treated as NOP now
            // TODO: proper halt and interrupt handling
            break;
        }

    case 0x77: { // LD (HL), A
            mem.write(hl(), A);
            break;
        }

    case 0x78: { // LD A, B
            A = B;
            break;
        }

    case 0x79: { // LD A, C
            A = C;
            break;
        }

    case 0x7A: { // LD A, D
            A = D;
            break;
        }

    case 0x7B: { // LD A, E
            A = E;
            break;
        }

    case 0x7C: {
            A = H;
            break;
        }

    case 0x7D: {
            A = L;
            break;
        }

    case 0x7E: { // LD A, (HL)
            A = mem.read(hl());
            break;
        }

    case 0x7F: // LD A, A
        break;

    case 0x80: { // ADD A, B
            A = add(B, false);
            break;
        }

    case 0x81: { // ADD A, C
            A = add(C, false);
            break;
        }

    case 0x82: { // ADD A, D
            A = add(D, false);
            break;
        }

    case 0x83: { // ADD A, E
            A = add(E, false);
            break;
        }

    case 0x84: { // ADD A, H
            A = add(H, false);
            break;
        }

    case 0x85: { // ADD A, L
            A = add(L, false);
            break;
        }

    case 0x86: { // ADD A, (HL)
            A = add(mem.read(hl()), false);
            break;
        }

    case 0x87: { // ADD A, A
            A = add(A, false);
            break;
    }

    case 0x88: { // ADC A, B
            A = add(B, true);
            break;
    }

    case 0x89: { // ADC A, C
            A = add(C, true);
            break;
    }

    case 0x8A: { // ADC A, D
            A = add(D, true);
            break;
    }

    case 0x8B: { // ADC A, E
            A = add(E, true);
            break;
    }

    case 0x8C: { // ADC A, H
            A = add(H, true);
            break;
    }

    case 0x8D: { // ADC A, L
            A = add(L, true);
            break;
    }

    case 0x8E: { // ADC A, (HL)
            A = add(mem.read(hl()), true);
            break;
    }

    case 0x8F: { // ADC A, A
            A = add(A, true);
            break;
    }

    case 0x90: { // SUB A, B
            A = sub(B, false);
            break;
    }

    case 0x91: { // SUB A, C
            A = sub(C, false);
            break;
    }

    case 0x92: { // SUB A, D
            A = sub(D, false);
            break;
    }

    case 0x93: { // SUB A, E
            A = sub(E, false);
            break;
    }

    case 0x94: { // SUB A, H
            A = sub(H, false);
            break;
    }

    case 0x95: { // SUB A, L
            A = sub(L, false);
            break;
    }

    case 0x96: { // SUB A, (HL)
            A = sub(mem.read(hl()), false);
            break;
    }

    case 0x97: { // SUB A, A
            A = sub(A, false);
            break;
    }

    case 0x98: { // SBC A, B
            A = sub(B, true);
            break;
    }

    case 0x99: { // SBC A, C
            A = sub(C, true);
            break;
    }

    case 0x9A: { // SBC A, D
            A = sub(D, true);
            break;
    }

    case 0x9B: { // SBC A, E
            A = sub(E, true);
            break;
    }

    case 0x9C: { // SBC A, H
            A = sub(H, true);
            break;
    }

    case 0x9D: { // SBC A, L
            A = sub(L, true);
            break;
    }

    case 0x9E: { // SBC A, (HL)
            A = sub(mem.read(hl()), true);
            break;
    }

    case 0x9F: { // SBC A, A
            A = sub(A, true);
            break;
    }

    case 0xA0: { // AND A, B
            A = and_x(B);
            break;
        }

    case 0xA1: { // AND A, C
            A = and_x(C);
            break;
        }

    case 0xA2: { // AND A, D
            A = and_x(D);
            break;
        }

    case 0xA3: { // AND A, E
            A = and_x(E);
            break;
        }

    case 0xA4: { // AND A, H
            A = and_x(H);
            break;
        }

    case 0xA5: { // AND A, L
            A = and_x(L);
            break;
        }

    case 0xA6: { // AND A, (HL)
            A = and_x(mem.read(hl()));
            break;
        }

    case 0xA7: { // AND A, A
            A = and_x(A);
            break;
        }

    case 0xA8: { // XOR A, B
            A = xor_x(B);
            break;
        }

    case 0xA9: { // XOR A, C
            A = xor_x(C);
            break;
        }

    case 0xAA: { // XOR A, D
            A = xor_x(D);
            break;
        }

    case 0xAB: { // XOR A, E
            A = xor_x(E);
            break;
        }

    case 0xAC: { // XOR A, H
            A = xor_x(H);
            break;
        }

    case 0xAD: { // XOR A, L
            A = xor_x(L);
            break;
        }

    case 0xAE: { // XOR A, (HL)
            A = xor_x(mem.read(hl()));
            break;
        }

    case 0xAF: { // XOR A, A
            A = xor_x(A);
            break;
        }

    case 0xB0: { // OR A, B
            A = or_x(B);
            break;
        }

    case 0xB1: { // OR A, C
            A = or_x(C);
            break;
        }

    case 0xB2: { // OR A, D
            A = or_x(D);
            break;
        }

    case 0xB3: { // OR A, E
            A = or_x(E);
            break;
        }

    case 0xB4: { // OR A, H
            A = or_x(H);
            break;
        }

    case 0xB5: { // OR A, L
            A = or_x(L);
            break;
        }

    case 0xB6: { // OR A, (HL)
            A = or_x(mem.read(hl()));
            break;
        }

    case 0xB7: { // OR A, A
            A = or_x(A);
            break;
        }

    case 0xB8: { // CP A, B
            cp(B);
            break;
        }

    case 0xB9: { // CP A, C
            cp(C);
            break;
        }

    case 0xBA: { // CP A, D
            cp(D);
            break;
        }

    case 0xBB: { // CP A, E
            cp(E);
            break;
        }

    case 0xBC: { // CP A, H
            cp(H);
            break;
        }

    case 0xBD: { // CP A, L
            cp(L);
            break;
        }

    case 0xBE: { // CP A, (HL)
            cp(mem.read(hl()));
            break;
        }

    case 0xBF: { // CP A, A
            cp(A);
            break;
        }

    case 0xC0: { // RET NZ
            if (!(F & FLAG_Z))
                ret();
            break;
        }

    case 0xC1: { // POP BC
            uint8_t low = mem.read(SP);
            SP++;
            uint8_t high = mem.read(SP);
            SP++;
            set_bc(combine(high, low));
            break;
        }

    case 0xC3: { // JP a16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            PC = combine(high, low);
            break;
        }

    case 0xC4: { // CALL NZ, a16
            if (!(F & FLAG_Z))
            {
                uint8_t low = mem.read(PC);
                uint8_t high = mem.read(PC + 1);
                PC += 2;
                SP--;
                mem.write(SP, (PC >> 8) & 0xFF);
                SP--;
                mem.write(SP, PC & 0xFF);
                PC = combine(high, low);
            }
            else
                PC += 2;
            break;
        }

    case 0xC5: { // PUSH BC
            uint8_t low = bc();
            uint8_t high = bc() >> 8;
            mem.write(SP - 1, high);
            mem.write(SP - 2, low);
            SP -= 2;
            break;
        }

    case 0xC6: { // ADD A, d8
            uint8_t operand = mem.read(PC);
            A = add(operand, false);
            PC++;
            break;
        }

    case 0xC8: { // RET Z
            if (F & FLAG_Z)
                ret();
            break;
        }

    case 0xC9: { // RET
            ret();
            break;
        }

    case 0xCB: { // redirect to CB table
            uint8_t cb_opcode = mem.read(PC);
            PC++;
            switch (cb_opcode) {
            case 0x08: { // RRC B
                    B = rrc(B, true);
                    break;
            }

            case 0x09: { // RRC C
                    C = rrc(C, true);
                    break;
            }

            case 0x0A: { // RRC D
                    D = rrc(D, true);
                    break;
            }

            case 0x0B: { // RRC E
                    E = rrc(E, true);
                    break;
            }

            case 0x0C: { // RRC H
                    H = rrc(H, true);
                    break;
            }

            case 0x0D: { // RRC L
                     L = rrc(L, true);
                    break;
            }

            case 0x0E: { // RRC (HL)
                    mem.write(hl(), rrc(mem.read(hl()), true));
                    break;
            }

            case 0x0F: { // RRC A
                    A = rrc(A, true);
                    break;
            }

            case 0x18: { // RR B
                    B = rr(B, true);
                    break;
            }

            case 0x19: { // RR C
                    C = rr(C, true);
                    break;
                }

            case 0x1A: { // RR D
                    D = rr(D, true);
                    break;
                }

            case 0x1B: { // RR E
                    E = rr(E, true);
                    break;
            }

            case 0x1C: { // RR H
                    H = rr(H, true);
                    break;
            }

            case 0x1D: { // RR L
                    L = rr(L, true);
                    break;
            }

            case 0x1E: { // RR (HL)
                    mem.write(hl(), rr(mem.read(hl()), true));
                    break;
            }

            case 0x1F: { // RR A
                    A = rr(A, true);
                    break;
            }

            case 0x38: { // SRL B
                    B = srl(B);
                    break;
                }

            case 0x39: { // SRL C
                    C = srl(C);
                    break;
                }

            case 0x3A: { // SRL D
                    D = srl(D);
                    break;
                }

            case 0x3B: { // SRL E
                    E = srl(E);
                    break;
                }

            case 0x3C: { // SRL H
                    H = srl(H);
                    break;
                }

            case 0x3D: { // SRL L
                    L = srl(L);
                    break;
                }

            case 0x3E: { // SRL (HL)
                    mem.write(hl(), srl(mem.read(hl())));
                    break;
                }

            case 0x3F: { // SRL A
                    A = srl(A);
                    break;
            }

            default: {
                    std::cerr << "Unknown CB opcode 0x" << std::hex
                    << static_cast<int>(cb_opcode) << " at PC = 0x" << PC - 2 << "\n";
                    std::exit(1);
                }
            }
            break;
        }

    case 0xCD: { // CALL a16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            PC += 2;
            SP--;
            mem.write(SP, (PC >> 8) & 0xFF);
            SP--;
            mem.write(SP, PC & 0xFF);
            PC = combine(high, low);
            break;
        }

    case 0xCE: { // ADC A, d8
            uint8_t operand = mem.read(PC);
            A = add(operand, true);
            PC++;
            break;
        }

    case 0xD0: { // RET NC
            if (!(F & FLAG_C))
                ret();
            break;
        }

    case 0xD1: { // POP DE
            uint8_t low = mem.read(SP);
            SP++;
            uint8_t high = mem.read(SP);
            SP++;
            set_de(combine(high, low));
            break;
        }

    case 0xD5: { // PUSH DE
            uint8_t low = de();
            uint8_t high = de() >> 8;
            mem.write(SP - 1, high);
            mem.write(SP - 2, low);
            SP -= 2;
            break;
        }

    case 0xD6: { // SUB A, d8
            uint8_t operand  = mem.read(PC);
            A = sub(operand, false);
            PC++;
            break;
        }

    case 0xD8: { // RET C
            if (F & FLAG_C)
                ret();
            break;
        }

    case 0xDE: { // SBC A, d8
            uint8_t operand  = mem.read(PC);
            A = sub(operand, true);
            PC++;
            break;
        }

    case 0xE0: { // LD (a8), A
            mem.write(0xFF00 + mem.read(PC), A);
            PC++;
            break;
        }

    case 0xE1: { // POP HL
            uint8_t low = mem.read(SP);
            SP++;
            uint8_t high = mem.read(SP);
            SP++;
            set_hl(combine(high, low));
            break;
        }

    case 0xE5: { // PUSH HL
            uint8_t low = hl();
            uint8_t high = hl() >> 8;
            mem.write(SP - 1, high);
            mem.write(SP - 2, low);
            SP -= 2;
            break;
        }

    case 0xE6: { // AND A, d8
            uint8_t operand = mem.read(PC);
            A = and_x(operand);
            PC++;
            break;
        }

    case 0xEA: { // LD (a16), A
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            mem.write(combine(high, low), A);
            PC += 2;
            break;
        }

    case 0xEE: { // XOR A, d8
            uint8_t operand = mem.read(PC);
            A = xor_x(operand);
            PC++;
            break;
        }

    case 0xF0: { // LD A, (a8)
            A = mem.read(0xFF00 + mem.read(PC));
            PC++;
            break;
        }

    case 0xF1: { // POP AF
            uint8_t low = mem.read(SP);
            SP++;
            uint8_t high = mem.read(SP);
            SP++;
            set_af(combine(high, low));
            break;
        }

    case 0xF3: { // DI
            IME = false;
            break;
        }

    case 0xF5: { // PUSH AF
            uint8_t low = af();
            uint8_t high = af() >> 8;
            mem.write(SP - 1, high);
            mem.write(SP - 2, low);
            SP -= 2;
            break;
        }

    case 0xF6: { // OR A, d8
            uint8_t operand = mem.read(PC);
            A = or_x(operand);
            PC++;
            break;
        }

    case 0xFA: { // LD A, (a16)
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            A = mem.read(combine(high, low));
            PC += 2;
            break;
        }

    case 0xFE: { // CP d8
            uint8_t operand = mem.read(PC);
            cp(operand);
            PC++;
            break;
        }

    default:
        std::cerr << "Unknown opcode 0x" << std::hex
        << static_cast<int>(opcode) << " at PC = 0x" << PC - 1 << "\n";
        std::exit(1);
    }
}