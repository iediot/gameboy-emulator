//
// Created by edi on 5/10/26.
//

#ifndef GAMEBOY_EMU_PPU_H
#define GAMEBOY_EMU_PPU_H

#include <cstdint>
#include "memory.h"

class Ppu
{
private:
    Memory& mem;
public:
    Ppu(Memory& memory);

    uint16_t scanline_cycles = 0;

    void step(uint8_t cycles);
};


#endif //GAMEBOY_EMU_PPU_H