//
// Created by edi on 5/10/26.
//

#include "ppu.h"

Ppu::Ppu(Memory& memory) : mem(memory) {
}

void Ppu::draw_scanline() {
    // read line registers
    uint8_t scy = mem.read(SCY_ADDR);
    uint8_t scx = mem.read(SCX_ADDR);
    uint8_t ly = mem.read(LY_ADDR);
    uint8_t lcdc = mem.read(LCDC_ADDR);

    if (!(lcdc & 0x80) || !(lcdc & 0x01))
        return;

    for (int x = 0; x <= 159; x++) {
        // compute the background coordinates
        uint8_t bg_y = scy + ly;
        uint8_t bg_x = scx + x;

        // find the tile in the 32x32 map which it covers
        uint8_t tile_col = bg_x / 8;
        uint8_t tile_row = bg_y / 8;

        // look up the tile index in the tile map
        uint16_t map_base;
        if (lcdc & 0x08)
            map_base = 0x9C00;
        else
            map_base = 0x9800;
        uint16_t map_address = map_base + tile_row * 32 + tile_col;
        uint8_t tile_index = mem.read(map_address);

        // find the tile's pixel data in VRAM
        uint16_t tile_address;
        if (lcdc & 0x10)
            tile_address = 0x8000 + tile_index * 16;
        else
            tile_address = 0x9000 + (int8_t)tile_index * 16;

        // row of pixel data
        uint8_t pixel_row = bg_y % 8;
        uint16_t row_address = tile_address + pixel_row * 2;
        uint8_t byte_low = mem.read(row_address);
        uint8_t byte_high = mem.read(row_address + 1);

        // 2-bit color id
        uint8_t pixel_col = bg_x % 8;
        uint8_t low_bit = byte_low >> (7 - pixel_col) & 1;
        uint8_t high_bit = byte_high >> (7 - pixel_col) & 1;
        uint8_t color_id = (high_bit << 1) | low_bit;

        // apply the BGP palette to get the shade
        uint8_t bgp_value = mem.read(BGP_ADDR);
        uint8_t final_color = bgp_value >> (color_id * 2) & 0x03;
        framebuffer[ly][x] = final_color;
    }
}

void Ppu::step(uint8_t cycles) {
    scanline_cycles += cycles;
    uint8_t mode = 0;
    if (scanline_cycles >= 456) {
        scanline_cycles -= 456;
        mem.write(LY_ADDR, mem.read(LY_ADDR) + 1); // LY increment
        if (mem.read(LY_ADDR) >= 154) // check LY if over 154
            mem.write(LY_ADDR, 0); // reset to 0 if true
        if (mem.read(LY_ADDR) == 144) {
            mem.write(IF_ADDR, mem.read(IF_ADDR) | 0x01);
            frame_ready = true;
        }
    }
    if (mem.read(LY_ADDR) >= 144) { // mode 1 - VBlank
        mode = 1;
    } else if (scanline_cycles < 80) { // mode 2 - OAM scan
        mode = 2;
    } else if (scanline_cycles < 252) { // mode 3 - Drawing
        mode = 3;
    } else { // mode 0 - HBlank
        mode = 0;
    }
    mem.write(STAT_ADDR, (mem.read(STAT_ADDR) & 0xFC) | mode);
    if (mode == 0 && prev_mode != 0 && mem.read(LY_ADDR) < 144)
        draw_scanline();
    prev_mode = mode;
}
