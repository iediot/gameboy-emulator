//
// Created by edi on 4/22/26.
//

#ifndef GAMEBOY_EMU_CPU_H
#define GAMEBOY_EMU_CPU_H
#include <cstdint>
#include <iostream>
#include "memory.h"


class Cpu {
private:
    Memory& mem;
public:
    // Registers
    uint8_t A = 0;
    uint8_t B = 0;
    uint8_t C = 0;
    uint8_t D = 0;
    uint8_t E = 0;
    uint8_t F = 0;
    uint8_t H = 0;
    uint8_t L = 0;
    uint16_t SP = 0;
    uint16_t PC = 0;

    static constexpr uint8_t FLAG_Z = 0x80; // 10000000
    static constexpr uint8_t FLAG_N = 0x40; // 01000000
    static constexpr uint8_t FLAG_H = 0x20; // 00100000
    static constexpr uint8_t FLAG_C = 0x10; // 00010000

    bool IME = false; // need to implement interrupts

    // Helpers
    void set_flag(uint8_t flag, bool value) {
        if (value) {
            F |= flag;
        } else {
            F &= ~flag;
        }
    }

    uint16_t combine(uint8_t high, uint8_t low) const {
        return static_cast<uint16_t>(high) << 8 | low;
    }

    void split(uint8_t& high, uint8_t& low, uint16_t value) {
        high = value >> 8;
        low = value & 0xFF;
    }

    uint16_t af() const {
        return combine(A, F);
    }

    void set_af(uint16_t value) {
        A = value >> 8;
        F = value & 0xF0;
    }

    uint16_t bc() const {
        return combine(B, C);
    }

    void set_bc(uint16_t value) {
        split(B, C, value);
    }

    uint16_t de() const {
        return combine(D, E);
    }

    void set_de(uint16_t value) {
        split(D, E, value);
    }

    uint16_t hl() const {
        return combine(H, L);
    }

    void set_hl(uint16_t value) {
        split(H, L, value);
    }

    Cpu(Memory& memory) : mem(memory) {
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
    void step() {
        uint8_t opcode = mem.read(PC);
        PC++;

        switch (opcode)
        {
        case 0x00:
            break;

        case 0x01: {
                uint8_t low = mem.read(PC);
                uint8_t high = mem.read(PC + 1);
                set_bc(combine(high, low));
                PC += 2;
                break;
            }

        case 0x02: {
                mem.write(bc(), A);
                break;
            }

        case 0x03: {
                set_bc(bc() + 1);
                break;
            }

        case 0x04: {
                B++;
                set_flag(FLAG_Z, B == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, (B & 0x0F) == 0x00);
                break;
            }

        case 0x05: {
                B--;
                set_flag(FLAG_Z, B == 0);
                set_flag(FLAG_N, true);
                set_flag(FLAG_H, (B & 0x0F) == 0x0F);
                break;
            }

        case 0x0C: {
                C++;
                set_flag(FLAG_Z, C == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, (C & 0x0F) == 0x00);
                break;
            }

        case 0x0D: {
                C--;
                set_flag(FLAG_Z, C == 0);
                set_flag(FLAG_N, true);
                set_flag(FLAG_H, (C & 0x0F) == 0x0F);
                break;
            }

        case 0x0E: {
                C = mem.read(PC);
                PC++;
                break;
            }

        case 0x11: {
                uint8_t low = mem.read(PC);
                uint8_t high = mem.read(PC + 1);
                set_de(combine(high, low));
                PC += 2;
                break;
            }

        case 0x12: {
                mem.write(de(), A);
                break;
            }

        case 0x14: {
                D++;
                set_flag(FLAG_Z, D == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, (D & 0x0F) == 0x00);
                break;
            }

        case 0x15: {
                D--;
                set_flag(FLAG_Z, D == 0);
                set_flag(FLAG_N, true);
                set_flag(FLAG_H, (D & 0x0F) == 0x0F);
                break;
            }

        case 0x18: {
                int8_t offset = static_cast<int8_t>(mem.read(PC));
                PC += offset + 1;
                break;
            }

        case 0x1C: {
                E++;
                set_flag(FLAG_Z, E == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, (E & 0x0F) == 0x00);
                break;
            }

        case 0x1D: {
                E--;
                set_flag(FLAG_Z, E == 0);
                set_flag(FLAG_N, true);
                set_flag(FLAG_H, (E & 0x0F) == 0x0F);
                break;
            }

        case 0x20: {
                int8_t offset = static_cast<int8_t>(mem.read(PC));
                PC++;
                bool z = F & FLAG_Z;
                if (!z) {
                    PC += offset;
                }
                break;
            }

        case 0x21: {
                uint8_t low = mem.read(PC);
                uint8_t high = mem.read(PC + 1);
                set_hl(combine(high, low));
                PC += 2;
                break;
            }

        case 0x2A: {
                A = mem.read(hl());
                set_hl(hl() + 1);
                break;
            }

        case 0x31: {
                uint8_t low = mem.read(PC);
                uint8_t high = mem.read(PC + 1);
                SP = combine(high, low);
                PC += 2;
                break;
            }

        case 0x3E: {
                A = mem.read(PC);
                PC++;
                break;
            }

        case 0x40:
            break;

        case 0x47: {
                B = A;
                break;
            }

        case 0x52:
            break;

        case 0x64:
            break;

        case 0x78: {
                A = B;
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

        case 0xC3: {
                uint8_t low = mem.read(PC);
                uint8_t high = mem.read(PC + 1);
                PC = combine(high, low);
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

        case 0xE0: {
                mem.write(0xFF00 + mem.read(PC), A);
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

        case 0xF3: {
                IME = false;
                break;
            }

        default:
            std::cerr << "Unknown opcode 0x" << std::hex
            << static_cast<int>(opcode) << " at PC = 0x" << PC - 1 << "\n";
            std::exit(1);
        }
    }
};


#endif //GAMEBOY_EMU_CPU_H