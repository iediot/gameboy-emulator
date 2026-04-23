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

    // Helper functions
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
                mem.write((static_cast<uint16_t>(B) << 8) | C, A);
                break;
            }
        case 0x03: {
                set_bc(bc() + 1);
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