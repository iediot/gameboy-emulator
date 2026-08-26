//
// Created by edi on 5/10/26.
//

#ifndef GAMEBOY_EMU_PPU_H
#define GAMEBOY_EMU_PPU_H

#include <cstdint>
#include "memory.h"

class Ppu {
private:
    Memory& mem;
    // helper for our draw method
    uint8_t fetch_color_id(uint8_t x, uint8_t y, uint16_t map_base, uint8_t lcdc,
                           uint8_t& attr_out);
    uint16_t mode3_length_extra();
public:
    // very used memory addresses
    static constexpr uint16_t IF_ADDR = 0xFF0F;
    static constexpr uint16_t LCDC_ADDR = 0xFF40;
    static constexpr uint16_t STAT_ADDR = 0xFF41;
    static constexpr uint16_t SCY_ADDR = 0xFF42;
    static constexpr uint16_t SCX_ADDR = 0xFF43;
    static constexpr uint16_t LY_ADDR = 0xFF44;
    static constexpr uint16_t LYC_ADDR = 0xFF45;
    static constexpr uint16_t BGP_ADDR = 0xFF47;
    static constexpr uint16_t WY_ADDR = 0xFF4A;
    static constexpr uint16_t WX_ADDR = 0xFF4B;
    static constexpr uint16_t OBP0_ADDR = 0xFF48;
    static constexpr uint16_t OBP1_ADDR = 0xFF49;

    // constructor
    Ppu(Memory& memory);

    // array to keep track of drawn pixels
    uint8_t bg_color_ids[144][160];
    // cgb tile attribute bit 7, the background wins over a sprite on this pixel
    uint8_t bg_priority[144][160];

    // variables used throughout ppu
    uint16_t scanline_cycles = 0;
    uint8_t ly_counter = 0;
    bool lcd_was_on = true;
    // finished argb pixels, the dmg shades are baked in here so both the colour and the
    // monochrome path hand the frontend the same thing
    uint32_t framebuffer[144][160] = {};
    uint8_t prev_mode = 0;
    uint8_t window_line_counter = 0;

    bool frame_ready = false;

    bool stat_line = false;

    uint16_t mode3_extra = 0;

    // the draw functions
    void draw_sprite();
    void draw_scanline();

    void step(uint8_t cycles);
};

#endif //GAMEBOY_EMU_PPU_H
