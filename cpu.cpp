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

uint8_t Cpu::sub(uint8_t value, bool with_carry) { // helper for the 'sub' function
    uint8_t carry_in = (F & FLAG_C) ? 1 : 0;       // and also works for the 'sbc'
    bool half_carry, carry;                        // (subtract with carry)
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

void Cpu::cp(uint8_t value) { // helper for the 'cp' (compare) operation
    set_flag(FLAG_Z, A == value);
    set_flag(FLAG_N, true);
    set_flag(FLAG_H, (A & 0x0F) < (value & 0x0F));
    set_flag(FLAG_C, A < value);
}

void Cpu::ret() { // helper for 'ret' operation
    uint8_t low = mem.read(SP);
    SP++;
    uint8_t high = mem.read(SP);
    SP++;
    PC = combine(high, low);
}

void Cpu::rst(uint16_t address) { // helper for the 'rst' (reset) operation
    SP--;
    mem.write(SP, (PC >> 8) & 0xFF);
    SP--;
    mem.write(SP, PC & 0xFF);
    PC = address;
}

void Cpu::call() { // helper for the 'call' operation
    uint8_t low = mem.read(PC);
    uint8_t high = mem.read(PC + 1);
    PC += 2;
    SP--;
    mem.write(SP, (PC >> 8) & 0xFF);
    SP--;
    mem.write(SP, PC & 0xFF);
    PC = combine(high, low);
}

uint8_t Cpu::swap (uint8_t value) {
    value = (value >> 4) | (value << 4);
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, false);
    return value;
}

uint8_t Cpu::rr(uint8_t value, bool set_z) { // helper for the 'rr' operations
    uint8_t old_carry = (F & FLAG_C) ? 1 : 0;
    uint8_t bit_0 = value & 1;
    value = value >> 1;
    value |= (old_carry << 7);
    if (set_z) {
        set_flag(FLAG_Z, value == 0);
    } else {
        set_flag(FLAG_Z, false);
    }
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, bit_0 != 0x00);
    return value;
}

uint8_t Cpu::rl(uint8_t value, bool set_z) {
    uint8_t old_carry = (F & FLAG_C) ? 1 : 0;
    uint8_t bit_7 = (value >> 7) & 1;
    value = value << 1;
    value |= old_carry;
    if (set_z) {
        set_flag(FLAG_Z, value == 0);
    } else {
        set_flag(FLAG_Z, false);
    }
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, bit_7 != 0x00);
    return value;
}

uint8_t Cpu::rrc(uint8_t value, bool set_z) { // helper for the 'rrc' operations
    uint8_t bit_0 = value & 1;
    value = value >> 1;
    value |= (bit_0 << 7);
    if (set_z) {
        set_flag(FLAG_Z, value == 0);
    } else {
        set_flag(FLAG_Z, false);
    }
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, bit_0 != 0x00);
    return value;
}

uint8_t Cpu::rlc(uint8_t value, bool set_z) {
    uint8_t bit_7 = (value >> 7) & 1;
    value = value << 1;
    value |= bit_7;
    if (set_z) {
        set_flag(FLAG_Z, value == 0);
    } else {
        set_flag(FLAG_Z, false);
    }
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, bit_7 != 0x00);
    return value;
}

uint8_t Cpu::srl(uint8_t value) { // helper for the 'srl' operations
    uint8_t bit_0 = value & 1;
    value = value >> 1;
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, bit_0 != 0x00);
    return value;
}

uint8_t Cpu::sla(uint8_t value) { // helper for the 'sla' operations
    uint8_t bit_7 = (value >> 7) & 1;
    value = value << 1;
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, bit_7 != 0x00);
    return value;
}

uint8_t Cpu::sra(uint8_t value) { // helper for the 'sra' operations
    uint8_t bit_0 = value & 1;
    uint8_t bit_7 = value & 0x80;
    value = value >> 1;
    value |= bit_7;
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, bit_0 != 0x00);
    return value;
}

void Cpu::bit(uint8_t bit_position, uint8_t value) {
    set_flag(FLAG_Z, (value & (1 << bit_position)) == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, true);
}

uint8_t Cpu::res(uint8_t bit_position, uint8_t value) {
    return value & ~(1 << bit_position);
}

uint8_t Cpu::set_bit(uint8_t bit_position, uint8_t value) {
    return value | (1 << bit_position);
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
            uint8_t adjustment = 0x00;
            if (F & FLAG_N) {
                if (F & FLAG_C)
                    adjustment += 0x60;
                if (F & FLAG_H)
                    adjustment += 0x06;
                A -= adjustment;
            } else {
                if (F & FLAG_C || A > 0x99) {
                    adjustment += 0x60;
                    set_flag(FLAG_C, true);
                }
                if (F & FLAG_H || (A & 0x0F) > 0x09)
                    adjustment += 0x06;
                A += adjustment;
            }
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_H, false);
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
            A = ~A;
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, true);
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
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, true);
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

    case 0x3F: { // CCF
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, !(F & FLAG_C));
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

    case 0xC2: { // JP NZ, a16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            PC += 2;
            if (!(F & FLAG_Z))
                PC = combine(high, low);
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
                call();
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

    case 0xC7: { // RST 0
            rst(0x00);
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

    case 0xCA: { // JP Z, a16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            PC += 2;
            if (F & FLAG_Z)
                PC = combine(high, low);
            break;
        }

    case 0xCB: { // redirect to CB table
            uint8_t cb_opcode = mem.read(PC);
            PC++;
            switch (cb_opcode) {
            case 0x00: { // RLC B
                    B = rlc(B, true);
                    break;
                }

            case 0x01: { // RLC C
                    C = rlc(C, true);
                    break;
                }

            case 0x02: { // RLC D
                    D = rlc(D, true);
                    break;
                }

            case 0x03: { // RLC E
                    E = rlc(E, true);
                    break;
                }

            case 0x04: { // RLC H
                    H = rlc(H, true);
                    break;
                }

            case 0x05: { // RLC L
                    L = rlc(L, true);
                    break;
                }

            case 0x06: { // RLC (HL)
                    mem.write(hl(), rlc(mem.read(hl()), true));
                    break;
                }

            case 0x07: { // RLC A
                    A = rlc(A, true);
                    break;
                }

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

            case 0x10: { // RL B
                    B = rl(B, true);
                    break;
                }

            case 0x11: { // RL C
                    C = rl(C, true);
                    break;
                }

            case 0x12: { // RL D
                    D = rl(D, true);
                    break;
                }

            case 0x13: { // RL E
                    E = rl(E, true);
                    break;
                }

            case 0x14: { // RL H
                    H = rl(H, true);
                    break;
                }

            case 0x15: { // RL L
                    L = rl(L, true);
                    break;
                }

            case 0x16: { // RL (HL)
                    mem.write(hl(), rl(mem.read(hl()), true));
                    break;
                }

            case 0x17: { // RL A
                    A = rl(A, true);
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

            case 0x20: { // SLA B
                    B = sla(B);
                    break;
                }

            case 0x21: { // SLA C
                    C = sla(C);
                    break;
                }

            case 0x22: { // SLA D
                    D = sla(D);
                    break;
                }

            case 0x23: { // SLA E
                    E = sla(E);
                    break;
                }

            case 0x24: { // SLA H
                    H = sla(H);
                    break;
                }

            case 0x25: { // SLA L
                    L = sla(L);
                    break;
                }

            case 0x26: { // SLA (HL)
                    mem.write(hl(), sla(mem.read(hl())));
                    break;
                }

            case 0x27: { // SLA A
                    A = sla(A);
                    break;
                }

            case 0x28: { // SRA B
                    B = sra(B);
                    break;
                }

            case 0x29: { // SRA C
                    C = sra(C);
                    break;
                }

            case 0x2A: { // SRA D
                    D = sra(D);
                    break;
                }

            case 0x2B: { // SRA E
                    E = sra(E);
                    break;
                }

            case 0x2C: { // SRA H
                    H = sra(H);
                    break;
                }

            case 0x2D: { // SRA L
                    L = sra(L);
                    break;
                }

            case 0x2E: { // SRA (HL)
                    mem.write(hl(), sra(mem.read(hl())));
                    break;
                }

            case 0x2F: { // SRA A
                    A = sra(A);
                    break;
                }

            case 0x30: { // SWAP B
                    B = swap(B);
                    break;
                }

            case 0x31: { // SWAP C
                    C = swap(C);
                    break;
                }

            case 0x32: { // SWAP D
                    D = swap(D);
                    break;
                }

            case 0x33: { // SWAP E
                    E = swap(E);
                    break;
                }

            case 0x34: { // SWAP H
                    H = swap(H);
                    break;
                }

            case 0x35: { // SWAP L
                    L = swap(L);
                    break;
                }

            case 0x36: { // SWAP (HL)
                    mem.write(hl(), swap(mem.read(hl())));
                    break;
                }

            case 0x37: { // SWAP A
                    A = swap(A);
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

            case 0x40: { // BIT 0, B
                    bit(0, B);
                    break;
                }

            case 0x41: { // BIT 0, C
                    bit(0, C);
                    break;
                }

            case 0x42: { // BIT 0, D
                    bit(0, D);
                    break;
                }

            case 0x43: { // BIT 0, E
                    bit(0, E);
                    break;
                }

            case 0x44: { // BIT 0, H
                    bit(0, H);
                    break;
                }

            case 0x45: { // BIT 0, L
                    bit(0, L);
                    break;
                }

            case 0x46: { // BIT 0, (HL)
                    bit(0, mem.read(hl()));
                    break;
                }

            case 0x47: { // BIT 0, A
                    bit(0, A);
                    break;
                }

            case 0x48: { // BIT 1, B
                    bit(1, B);
                    break;
                }

            case 0x49: { // BIT 1, C
                    bit(1, C);
                    break;
                }

            case 0x4A: { // BIT 1, D
                    bit(1, D);
                    break;
                }

            case 0x4B: { // BIT 1, E
                    bit(1, E);
                    break;
                }

            case 0x4C: { // BIT 1, H
                    bit(1, H);
                    break;
                }

            case 0x4D: { // BIT 1, L
                    bit(1, L);
                    break;
                }

            case 0x4E: { // BIT 1, (HL)
                    bit(1, mem.read(hl()));
                    break;
                }

            case 0x4F: { // BIT 1, A
                    bit(1, A);
                    break;
                }


            case 0x50: { // BIT 2, B
                    bit(2, B);
                    break;
                }

            case 0x51: { // BIT 2, C
                    bit(2, C);
                    break;
                }

            case 0x52: { // BIT 2, D
                    bit(2, D);
                    break;
                }

            case 0x53: { // BIT 2, E
                    bit(2, E);
                    break;
                }

            case 0x54: { // BIT 2, H
                    bit(2, H);
                    break;
                }

            case 0x55: { // BIT 2, L
                    bit(2, L);
                    break;
                }

            case 0x56: { // BIT 2, (HL)
                    bit(2, mem.read(hl()));
                    break;
                }

            case 0x57: { // BIT 2, A
                    bit(2, A);
                    break;
                }

            case 0x58: { // BIT 3, B
                    bit(3, B);
                    break;
                }

            case 0x59: { // BIT 3, C
                    bit(3, C);
                    break;
                }

            case 0x5A: { // BIT 3, D
                    bit(3, D);
                    break;
                }

            case 0x5B: { // BIT 3, E
                    bit(3, E);
                    break;
                }

            case 0x5C: { // BIT 3, H
                    bit(3, H);
                    break;
                }

            case 0x5D: { // BIT 3, L
                    bit(3, L);
                    break;
                }

            case 0x5E: { // BIT 3, (HL)
                    bit(3, mem.read(hl()));
                    break;
                }

            case 0x5F: { // BIT 3, A
                    bit(3, A);
                    break;
                }


            case 0x60: { // BIT 4, B
                    bit(4, B);
                    break;
                }

            case 0x61: { // BIT 4, C
                    bit(4, C);
                    break;
                }

            case 0x62: { // BIT 4, D
                    bit(4, D);
                    break;
                }

            case 0x63: { // BIT 4, E
                    bit(4, E);
                    break;
                }

            case 0x64: { // BIT 4, H
                    bit(4, H);
                    break;
                }

            case 0x65: { // BIT 4, L
                    bit(4, L);
                    break;
                }

            case 0x66: { // BIT 4, (HL)
                    bit(4, mem.read(hl()));
                    break;
                }

            case 0x67: { // BIT 4, A
                    bit(4, A);
                    break;
                }

            case 0x68: { // BIT 5, B
                    bit(5, B);
                    break;
                }

            case 0x69: { // BIT 5, C
                    bit(5, C);
                    break;
                }

            case 0x6A: { // BIT 5, D
                    bit(5, D);
                    break;
                }

            case 0x6B: { // BIT 5, E
                    bit(5, E);
                    break;
                }

            case 0x6C: { // BIT 5, H
                    bit(5, H);
                    break;
                }

            case 0x6D: { // BIT 5, L
                    bit(5, L);
                    break;
                }

            case 0x6E: { // BIT 5, (HL)
                    bit(5, mem.read(hl()));
                    break;
                }

            case 0x6F: { // BIT 5, A
                    bit(5, A);
                    break;
                }


            case 0x70: { // BIT 6, B
                    bit(6, B);
                    break;
                }

            case 0x71: { // BIT 6, C
                    bit(6, C);
                    break;
                }

            case 0x72: { // BIT 6, D
                    bit(6, D);
                    break;
                }

            case 0x73: { // BIT 6, E
                    bit(6, E);
                    break;
                }

            case 0x74: { // BIT 6, H
                    bit(6, H);
                    break;
                }

            case 0x75: { // BIT 6, L
                    bit(6, L);
                    break;
                }

            case 0x76: { // BIT 6, (HL)
                    bit(6, mem.read(hl()));
                    break;
                }

            case 0x77: { // BIT 6, A
                    bit(6, A);
                    break;
                }

            case 0x78: { // BIT 7, B
                    bit(7, B);
                    break;
                }

            case 0x79: { // BIT 7, C
                    bit(7, C);
                    break;
                }

            case 0x7A: { // BIT 7, D
                    bit(7, D);
                    break;
                }

            case 0x7B: { // BIT 7, E
                    bit(7, E);
                    break;
                }

            case 0x7C: { // BIT 7, H
                    bit(7, H);
                    break;
                }

            case 0x7D: { // BIT 7, L
                    bit(7, L);
                    break;
                }

            case 0x7E: { // BIT 7, (HL)
                    bit(7, mem.read(hl()));
                    break;
                }

            case 0x7F: { // BIT 7, A
                    bit(7, A);
                    break;
                }

            case 0x80: { // RES 0, B
                    B = res(0, B);
                    break;
                }

            case 0x81: { // RES 0, C
                    C = res(0, C);
                    break;
                }

            case 0x82: { // RES 0, D
                    D = res(0, D);
                    break;
                }

            case 0x83: { // RES 0, E
                    E = res(0, E);
                    break;
                }

            case 0x84: { // RES 0, H
                    H = res(0, H);
                    break;
                }

            case 0x85: { // RES 0, L
                    L = res(0, L);
                    break;
                }

            case 0x86: { // RES 0, (HL)
                    mem.write(hl(), res(0, mem.read(hl())));
                    break;
                }

            case 0x87: { // RES 0, A
                    A = res(0, A);
                    break;
                }

            case 0x88: { // RES 1, B
                    B = res(1, B);
                    break;
                }

            case 0x89: { // RES 1, C
                    C = res(1, C);
                    break;
                }

            case 0x8A: { // RES 1, D
                    D = res(1, D);
                    break;
                }

            case 0x8B: { // RES 1, E
                    E = res(1, E);
                    break;
                }

            case 0x8C: { // RES 1, H
                    H = res(1, H);
                    break;
                }

            case 0x8D: { // RES 1, L
                    L = res(1, L);
                    break;
                }

            case 0x8E: { // RES 1, (HL)
                    mem.write(hl(), res(1, mem.read(hl())));
                    break;
                }

            case 0x8F: { // RES 1, A
                    A = res(1, A);
                    break;
                }


            case 0x90: { // RES 2, B
                    B = res(2, B);
                    break;
                }

            case 0x91: { // RES 2, C
                    C = res(2, C);
                    break;
                }

            case 0x92: { // RES 2, D
                    D = res(2, D);
                    break;
                }

            case 0x93: { // RES 2, E
                    E = res(2, E);
                    break;
                }

            case 0x94: { // RES 2, H
                    H = res(2, H);
                    break;
                }

            case 0x95: { // RES 2, L
                    L = res(2, L);
                    break;
                }

            case 0x96: { // RES 2, (HL)
                    mem.write(hl(), res(2, mem.read(hl())));
                    break;
                }

            case 0x97: { // RES 2, A
                    A = res(2, A);
                    break;
                }

            case 0x98: { // RES 3, B
                    B = res(3, B);
                    break;
                }

            case 0x99: { // RES 3, C
                    C = res(3, C);
                    break;
                }

            case 0x9A: { // RES 3, D
                    D = res(3, D);
                    break;
                }

            case 0x9B: { // RES 3, E
                    E = res(3, E);
                    break;
                }

            case 0x9C: { // RES 3, H
                    H = res(3, H);
                    break;
                }

            case 0x9D: { // RES 3, L
                    L = res(3, L);
                    break;
                }

            case 0x9E: { // RES 3, (HL)
                    mem.write(hl(), res(3, mem.read(hl())));
                    break;
                }

            case 0x9F: { // RES 3, A
                    A = res(3, A);
                    break;
                }


            case 0xA0: { // RES 4, B
                    B = res(4, B);
                    break;
                }

            case 0xA1: { // RES 4, C
                    C = res(4, C);
                    break;
                }

            case 0xA2: { // RES 4, D
                    D = res(4, D);
                    break;
                }

            case 0xA3: { // RES 4, E
                    E = res(4, E);
                    break;
                }

            case 0xA4: { // RES 4, H
                    H = res(4, H);
                    break;
                }

            case 0xA5: { // RES 4, L
                    L = res(4, L);
                    break;
                }

            case 0xA6: { // RES 4, (HL)
                    mem.write(hl(), res(4, mem.read(hl())));
                    break;
                }

            case 0xA7: { // RES 4, A
                    A = res(4, A);
                    break;
                }

            case 0xA8: { // RES 5, B
                    B = res(5, B);
                    break;
                }

            case 0xA9: { // RES 5, C
                    C = res(5, C);
                    break;
                }

            case 0xAA: { // RES 5, D
                    D = res(5, D);
                    break;
                }

            case 0xAB: { // RES 5, E
                    E = res(5, E);
                    break;
                }

            case 0xAC: { // RES 5, H
                    H = res(5, H);
                    break;
                }

            case 0xAD: { // RES 5, L
                    L = res(5, L);
                    break;
                }

            case 0xAE: { // RES 5, (HL)
                    mem.write(hl(), res(5, mem.read(hl())));
                    break;
                }

            case 0xAF: { // RES 5, A
                    A = res(5, A);
                    break;
                }


            case 0xB0: { // RES 6, B
                    B = res(6, B);
                    break;
                }

            case 0xB1: { // RES 6, C
                    C = res(6, C);
                    break;
                }

            case 0xB2: { // RES 6, D
                    D = res(6, D);
                    break;
                }

            case 0xB3: { // RES 6, E
                    E = res(6, E);
                    break;
                }

            case 0xB4: { // RES 6, H
                    H = res(6, H);
                    break;
                }

            case 0xB5: { // RES 6, L
                    L = res(6, L);
                    break;
                }

            case 0xB6: { // RES 6, (HL)
                    mem.write(hl(), res(6, mem.read(hl())));
                    break;
                }

            case 0xB7: { // RES 6, A
                    A = res(6, A);
                    break;
                }

            case 0xB8: { // RES 7, B
                    B = res(7, B);
                    break;
                }

            case 0xB9: { // RES 7, C
                    C = res(7, C);
                    break;
                }

            case 0xBA: { // RES 7, D
                    D = res(7, D);
                    break;
                }

            case 0xBB: { // RES 7, E
                    E = res(7, E);
                    break;
                }

            case 0xBC: { // RES 7, H
                    H = res(7, H);
                    break;
                }

            case 0xBD: { // RES 7, L
                    L = res(7, L);
                    break;
                }

            case 0xBE: { // RES 7, (HL)
                    mem.write(hl(), res(7, mem.read(hl())));
                    break;
                }

            case 0xBF: { // RES 7, A
                    A = res(7, A);
                    break;
                }

            case 0xC0: { // SET 0, B
                    B = set_bit(0, B);
                    break;
                }

            case 0xC1: { // SET 0, C
                    C = set_bit(0, C);
                    break;
                }

            case 0xC2: { // SET 0, D
                    D = set_bit(0, D);
                    break;
                }

            case 0xC3: { // SET 0, E
                    E = set_bit(0, E);
                    break;
                }

            case 0xC4: { // SET 0, H
                    H = set_bit(0, H);
                    break;
                }

            case 0xC5: { // SET 0, L
                    L = set_bit(0, L);
                    break;
                }

            case 0xC6: { // SET 0, (HL)
                    mem.write(hl(), set_bit(0, mem.read(hl())));
                    break;
                }

            case 0xC7: { // SET 0, A
                    A = set_bit(0, A);
                    break;
                }

            case 0xC8: { // SET 1, B
                    B = set_bit(1, B);
                    break;
                }

            case 0xC9: { // SET 1, C
                    C = set_bit(1, C);
                    break;
                }

            case 0xCA: { // SET 1, D
                    D = set_bit(1, D);
                    break;
                }

            case 0xCB: { // SET 1, E
                    E = set_bit(1, E);
                    break;
                }

            case 0xCC: { // SET 1, H
                    H = set_bit(1, H);
                    break;
                }

            case 0xCD: { // SET 1, L
                    L = set_bit(1, L);
                    break;
                }

            case 0xCE: { // SET 1, (HL)
                    mem.write(hl(), set_bit(1, mem.read(hl())));
                    break;
                }

            case 0xCF: { // SET 1, A
                    A = set_bit(1, A);
                    break;
                }


            case 0xD0: { // SET 2, B
                    B = set_bit(2, B);
                    break;
                }

            case 0xD1: { // SET 2, C
                    C = set_bit(2, C);
                    break;
                }

            case 0xD2: { // SET 2, D
                    D = set_bit(2, D);
                    break;
                }

            case 0xD3: { // SET 2, E
                    E = set_bit(2, E);
                    break;
                }

            case 0xD4: { // SET 2, H
                    H = set_bit(2, H);
                    break;
                }

            case 0xD5: { // SET 2, L
                    L = set_bit(2, L);
                    break;
                }

            case 0xD6: { // SET 2, (HL)
                    mem.write(hl(), set_bit(2, mem.read(hl())));
                    break;
                }

            case 0xD7: { // SET 2, A
                    A = set_bit(2, A);
                    break;
                }

            case 0xD8: { // SET 3, B
                    B = set_bit(3, B);
                    break;
                }

            case 0xD9: { // SET 3, C
                    C = set_bit(3, C);
                    break;
                }

            case 0xDA: { // SET 3, D
                    D = set_bit(3, D);
                    break;
                }

            case 0xDB: { // SET 3, E
                    E = set_bit(3, E);
                    break;
                }

            case 0xDC: { // SET 3, H
                    H = set_bit(3, H);
                    break;
                }

            case 0xDD: { // SET 3, L
                    L = set_bit(3, L);
                    break;
                }

            case 0xDE: { // SET 3, (HL)
                    mem.write(hl(), set_bit(3, mem.read(hl())));
                    break;
                }

            case 0xDF: { // SET 3, A
                    A = set_bit(3, A);
                    break;
                }


            case 0xE0: { // SET 4, B
                    B = set_bit(4, B);
                    break;
                }

            case 0xE1: { // SET 4, C
                    C = set_bit(4, C);
                    break;
                }

            case 0xE2: { // SET 4, D
                    D = set_bit(4, D);
                    break;
                }

            case 0xE3: { // SET 4, E
                    E = set_bit(4, E);
                    break;
                }

            case 0xE4: { // SET 4, H
                    H = set_bit(4, H);
                    break;
                }

            case 0xE5: { // SET 4, L
                    L = set_bit(4, L);
                    break;
                }

            case 0xE6: { // SET 4, (HL)
                    mem.write(hl(), set_bit(4, mem.read(hl())));
                    break;
                }

            case 0xE7: { // SET 4, A
                    A = set_bit(4, A);
                    break;
                }

            case 0xE8: { // SET 5, B
                    B = set_bit(5, B);
                    break;
                }

            case 0xE9: { // SET 5, C
                    C = set_bit(5, C);
                    break;
                }

            case 0xEA: { // SET 5, D
                    D = set_bit(5, D);
                    break;
                }

            case 0xEB: { // SET 5, E
                    E = set_bit(5, E);
                    break;
                }

            case 0xEC: { // SET 5, H
                    H = set_bit(5, H);
                    break;
                }

            case 0xED: { // SET 5, L
                    L = set_bit(5, L);
                    break;
                }

            case 0xEE: { // SET 5, (HL)
                    mem.write(hl(), set_bit(5, mem.read(hl())));
                    break;
                }

            case 0xEF: { // SET 5, A
                    A = set_bit(5, A);
                    break;
                }


            case 0xF0: { // SET 6, B
                    B = set_bit(6, B);
                    break;
                }

            case 0xF1: { // SET 6, C
                    C = set_bit(6, C);
                    break;
                }

            case 0xF2: { // SET 6, D
                    D = set_bit(6, D);
                    break;
                }

            case 0xF3: { // SET 6, E
                    E = set_bit(6, E);
                    break;
                }

            case 0xF4: { // SET 6, H
                    H = set_bit(6, H);
                    break;
                }

            case 0xF5: { // SET 6, L
                    L = set_bit(6, L);
                    break;
                }

            case 0xF6: { // SET 6, (HL)
                    mem.write(hl(), set_bit(6, mem.read(hl())));
                    break;
                }

            case 0xF7: { // SET 6, A
                    A = set_bit(6, A);
                    break;
                }

            case 0xF8: { // SET 7, B
                    B = set_bit(7, B);
                    break;
                }

            case 0xF9: { // SET 7, C
                    C = set_bit(7, C);
                    break;
                }

            case 0xFA: { // SET 7, D
                    D = set_bit(7, D);
                    break;
                }

            case 0xFB: { // SET 7, E
                    E = set_bit(7, E);
                    break;
                }

            case 0xFC: { // SET 7, H
                    H = set_bit(7, H);
                    break;
                }

            case 0xFD: { // SET 7, L
                    L = set_bit(7, L);
                    break;
                }

            case 0xFE: { // SET 7, (HL)
                    mem.write(hl(), set_bit(7, mem.read(hl())));
                    break;
                }

            case 0xFF: { // SET 7, A
                    A = set_bit(7, A);
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

    case 0xCC: { // CALL Z, a16
            if (F & FLAG_Z)
                call();
            else
                PC += 2;
            break;
        }

    case 0xCD: { // CALL a16
            call();
            break;
        }

    case 0xCE: { // ADC A, d8
            uint8_t operand = mem.read(PC);
            A = add(operand, true);
            PC++;
            break;
        }

    case 0xCF: { // RST 1
            rst(0x08);
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

    case 0xD2: { // JP NC, a16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            PC += 2;
            if (!(F & FLAG_C))
                PC = combine(high, low);
            break;
        }

    case 0xD4: { // CALL NC, a16
            if (!(F & FLAG_C))
            call();
            else
                PC += 2;
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

    case 0xD7: { // RST 2
            rst(0x10);
            break;
        }

    case 0xD8: { // RET C
            if (F & FLAG_C)
                ret();
            break;
        }

    case 0xD9: { // RETI
            ret();
            IME = true;
            break;
        }

    case 0xDA: { // JP C, a16
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            PC += 2;
            if (F & FLAG_C)
                PC = combine(high, low);
            break;
        }

    case 0xDC: { // CALL C, a16
            if (F & FLAG_C)
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

    case 0xDE: { // SBC A, d8
            uint8_t operand  = mem.read(PC);
            A = sub(operand, true);
            PC++;
            break;
        }

    case 0xDF: { // RST 3
            rst(0x18);
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

    case 0xE2: { // LD (C), A
            mem.write(0xFF00 + C, A);
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

    case 0xE7: { // RST 4
            rst(0x20);
            break;
        }

    case 0xE8: { // ADD SP, s8
            uint8_t operand = mem.read(PC);
            int8_t offset = static_cast<int8_t>(operand);
            PC++;
            bool half_carry = ((SP & 0x0F) + (operand & 0x0F)) > 0x0F;
            bool carry = ((SP & 0xFF) + (operand & 0xFF)) > 0xFF;
            SP += offset;
            set_flag(FLAG_Z, false);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, half_carry);
            set_flag(FLAG_C, carry);
            break;
        }

    case 0xE9: { // JP HL
            PC = hl();
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

    case 0xEF: { // RST 5
            rst(0x28);
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

    case 0xF2: { // LD A, (C)
            A = mem.read(0xFF00 + C);
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

    case 0xF7: { // RST 6
            rst(0x30);
            break;
        }

    case 0xF8: { // LD HL, SP+s8
            uint8_t operand = mem.read(PC);
            int8_t offset = static_cast<int8_t>(operand);
            PC++;
            set_hl(SP + offset);
            bool half_carry = ((SP & 0x0F) + (operand & 0x0F)) > 0x0F;
            bool carry = ((SP & 0xFF) + (operand & 0xFF)) > 0xFF;
            set_flag(FLAG_Z, false);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, half_carry);
            set_flag(FLAG_C, carry);
            break;
        }

    case 0xF9: { // LD SP, HL
            SP = hl();
            break;
        }

    case 0xFA: { // LD A, (a16)
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            A = mem.read(combine(high, low));
            PC += 2;
            break;
        }

    case 0xFB: { // EI
            IME = true;
            break;
        }

    case 0xFE: { // CP d8
            uint8_t operand = mem.read(PC);
            cp(operand);
            PC++;
            break;
        }

    case 0xFF: { // RST 7
            rst(0x38);
            break;
        }

    default:
        std::cerr << "Unknown opcode 0x" << std::hex
        << static_cast<int>(opcode) << " at PC = 0x" << PC - 1 << "\n";
        std::exit(1);
    }
}