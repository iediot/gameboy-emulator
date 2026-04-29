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

uint8_t Cpu::rr(uint8_t value, bool set_z) { // helper for the rr operations
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

uint8_t Cpu::rrc(uint8_t value, bool set_z) { // helper for the rrc operations
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

uint8_t Cpu::srl(uint8_t value) { // helper for the srl operations
    uint8_t saved_bit = value & 1;
    value = value >> 1;
    set_flag(FLAG_Z, value == 0);
    set_flag(FLAG_N, false);
    set_flag(FLAG_H, false);
    set_flag(FLAG_C, saved_bit != 0x00);
    return value;
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
            uint8_t saved_bit = (A >> 7) & 1;
            A = A << 1;
            A |= saved_bit;
            set_flag(FLAG_Z, false);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, saved_bit != 0x00);
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
            uint8_t old_carry = (F & FLAG_C) ? 1 : 0;
            uint8_t saved_bit = (A >> 7) & 1;
            A = A << 1;
            A |= old_carry;
            set_flag(FLAG_Z, false);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, saved_bit != 0x00);
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

    case 0x28: { // JR Z, s8
            int8_t offset = static_cast<int8_t>(mem.read(PC));
            PC++;
            if (F & FLAG_Z) {
                PC += offset;
            }
            break;
        }

    case 0x2A: { // LD A, (HL+)
            A = mem.read(hl());
            set_hl(hl() + 1);
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

    case 0x38: { // JR C, s8
            int8_t offset = static_cast<int8_t>(mem.read(PC));
            PC++;
            if (F & FLAG_C) {
                PC += offset;
            }
            break;
        }

    case 0x3E: { // LD A, d8
            A = mem.read(PC);
            PC++;
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

    case 0xA9: { // XOR C
            A ^= C;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xAE: { // XOR (HL)
            A ^= mem.read(hl());
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xB0: {
            A |= B;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xB1: {
            A |= C;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xB2: {
            A |= D;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xB3: {
            A |= E;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xB4: {
            A |= H;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xB5: {
            A |= L;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xB6: {
            A |= mem.read(hl());
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xB7: {
            A |= A;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, false);
            set_flag(FLAG_C, false);
            break;
        }

    case 0xC1: {
            uint8_t low = mem.read(SP);
            SP++;
            uint8_t high = mem.read(SP);
            SP++;
            set_bc(combine(high, low));
            break;
        }

    case 0xC3: {
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            PC = combine(high, low);
            break;
        }

    case 0xC4: {
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

    case 0xC5: {
            uint8_t low = bc();
            uint8_t high = bc() >> 8;
            mem.write(SP - 1, high);
            mem.write(SP - 2, low);
            SP -= 2;
            break;
        }

    case 0xC6: { // ADD A, d8
            uint8_t operand  = mem.read(PC);
            uint16_t sum = static_cast<uint16_t>(A) + operand;
            bool half_carry = ((A & 0x0F) + (operand & 0x0F)) > 0x0F;
            bool carry = sum > 0xFF;
            A = A + operand;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, half_carry);
            set_flag(FLAG_C,  carry);
            PC++;
            break;
        }

    case 0xC9: {
            uint8_t low = mem.read(SP);
            SP++;
            uint8_t high = mem.read(SP);
            SP++;
            PC = combine(high, low);
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

    case 0xCD: {
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

    case 0xD1: {
            uint8_t low = mem.read(SP);
            SP++;
            uint8_t high = mem.read(SP);
            SP++;
            set_de(combine(high, low));
            break;
        }

    case 0xD5: {
            uint8_t low = de();
            uint8_t high = de() >> 8;
            mem.write(SP - 1, high);
            mem.write(SP - 2, low);
            SP -= 2;
            break;
        }

    case 0xD6: { // SUB d8
            uint8_t operand  = mem.read(PC);
            bool half_carry = (A & 0x0F) < (operand & 0x0F);
            bool carry = A < operand;
            A = A - operand;
            set_flag(FLAG_Z, A == 0);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, half_carry);
            set_flag(FLAG_C,  carry);
            PC++;
            break;
        }

    case 0xE0: {
            mem.write(0xFF00 + mem.read(PC), A);
            PC++;
            break;
        }

    case 0xE1: {
            uint8_t low = mem.read(SP);
            SP++;
            uint8_t high = mem.read(SP);
            SP++;
            set_hl(combine(high, low));
            break;
        }

    case 0xE5: {
            uint8_t low = hl();
            uint8_t high = hl() >> 8;
            mem.write(SP - 1, high);
            mem.write(SP - 2, low);
            SP -= 2;
            break;
        }

    case 0xE6: {
            A = A & mem.read(PC);
            set_flag(FLAG_Z, A == 0x00);
            set_flag(FLAG_N, false);
            set_flag(FLAG_H, true);
            set_flag(FLAG_C, false);
            PC++;
            break;
        }

    case 0xEA: {
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            mem.write(combine(high, low), A);
            PC += 2;
            break;
        }

    case 0xF0: {
            A = mem.read(0xFF00 + mem.read(PC));
            PC++;
            break;
        }

    case 0xF1: {
            uint8_t low = mem.read(SP);
            SP++;
            uint8_t high = mem.read(SP);
            SP++;
            set_af(combine(high, low));
            break;
        }

    case 0xF3: {
            IME = false;
            break;
        }

    case 0xF5: {
            uint8_t low = af();
            uint8_t high = af() >> 8;
            mem.write(SP - 1, high);
            mem.write(SP - 2, low);
            SP -= 2;
            break;
        }

    case 0xFA: {
            uint8_t low = mem.read(PC);
            uint8_t high = mem.read(PC + 1);
            A = mem.read(combine(high, low));
            PC += 2;
            break;
        }

    case 0xFE: {
            uint8_t d8 = mem.read(PC);
            set_flag(FLAG_Z, A == d8);
            set_flag(FLAG_N, true);
            set_flag(FLAG_H, (A & 0x0F) < (d8 & 0x0F));
            set_flag(FLAG_C, A < d8);
            PC++;
            break;
        }

    default:
        std::cerr << "Unknown opcode 0x" << std::hex
        << static_cast<int>(opcode) << " at PC = 0x" << PC - 1 << "\n";
        std::exit(1);
    }
}