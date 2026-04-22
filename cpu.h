//
// Created by edi on 4/22/26.
//

#ifndef GAMEBOY_EMU_CPU_H
#define GAMEBOY_EMU_CPU_H
#include <cstdint>
#include <iostream>
#include "memory.h"


class Cpu
{
private:
    Memory& mem;
public:
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
    Cpu(Memory& memory) : mem(memory)
    {
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
    void step()
    {
        uint8_t opcode = mem.read(PC);
        PC++;

        switch (opcode)
        {
        case 0x00:
            break;
        default:
            std::cerr << "Unknown opcode 0x" << std::hex
            << (int)opcode << " at PC = 0x" << PC - 1 << "\n";
            std::exit(1);
        }
    }
};


#endif //GAMEBOY_EMU_CPU_H