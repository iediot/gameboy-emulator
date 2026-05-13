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
    // very used memory addresses
    static constexpr uint16_t IF_ADDR = 0xFF0F;
    static constexpr uint16_t LCDC_ADDR = 0xFF40;
    static constexpr uint16_t STAT_ADDR = 0xFF41;
    static constexpr uint16_t SCY_ADDR = 0xFF42;
    static constexpr uint16_t SCX_ADDR = 0xFF43;
    static constexpr uint16_t LY_ADDR = 0xFF44;
    static constexpr uint16_t BGP_ADDR = 0xFF47;

    // constructor
    Ppu(Memory& memory);

    // variables used throughout ppu
    uint16_t scanline_cycles = 0;
    uint8_t framebuffer[144][160] = {};
    uint8_t prev_mode = 0;

    bool frame_ready = false;

    // functions used throughout ppu
    void draw_scanline();
    void step(uint8_t cycles);
};


#endif //GAMEBOY_EMU_PPU_H