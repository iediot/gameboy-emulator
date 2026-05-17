//
// Created by edi on 5/10/26.
//

#include "ppu.h"

Ppu::Ppu(Memory& memory) : mem(memory) {
}

uint8_t Ppu::fetch_tile_pixel(uint8_t x, uint8_t y, uint16_t map_base) {
    // find the tile in the 32x32 map which it covers
    uint8_t tile_col = x / 8;
    uint8_t tile_row = y / 8;

    uint16_t map_address = map_base + tile_row * 32 + tile_col;
    uint8_t tile_index = mem.read(map_address);

    // find the tile's pixel data in VRAM
    uint16_t tile_address;
    // check if bit 4 is 0
    if (mem.read(LCDC_ADDR) & 0x10)
        tile_address = 0x8000 + tile_index * 16;
    else
        tile_address = 0x9000 + (int8_t)tile_index * 16;

    // row of pixel data
    uint8_t pixel_row = y % 8;
    uint16_t row_address = tile_address + pixel_row * 2;
    uint8_t byte_low = mem.read(row_address);
    uint8_t byte_high = mem.read(row_address + 1);

    // 2-bit color id
    uint8_t pixel_col = x % 8;
    uint8_t low_bit = byte_low >> (7 - pixel_col) & 1;
    uint8_t high_bit = byte_high >> (7 - pixel_col) & 1;
    uint8_t color_id = (high_bit << 1) | low_bit;

    // apply the BGP palette to get the shade
    uint8_t bgp_value = mem.read(BGP_ADDR);
    uint8_t final_color = bgp_value >> (color_id * 2) & 0x03;

    return final_color;
}

void Ppu::draw_scanline() {
    // read line registers
    uint8_t SCY = mem.read(SCY_ADDR);
    uint8_t SCX = mem.read(SCX_ADDR);
    uint8_t LY = mem.read(LY_ADDR);
    uint8_t LCDC = mem.read(LCDC_ADDR);

    // check if bits 7 and 0 are 0
    if (!(LCDC & 0x80) || !(LCDC & 0x01))
        return;

    for (int x = 0; x <= 159; x++) {
        // compute the background coordinates
        uint8_t bg_y = SCY + LY;
        uint8_t bg_x = SCX + x;

        // look up the tile index in the tile map
        uint16_t map_base;
        // check if bit 3 is 0
        if (LCDC & 0x08)
            map_base = 0x9C00;
        else
            map_base = 0x9800;

        framebuffer[LY][x] = fetch_tile_pixel(bg_x, bg_y, map_base);
    }

    uint8_t WY = mem.read(WY_ADDR);
    uint8_t WX = mem.read(WX_ADDR);

    /* check if bit 5 is set or if the window
     layer position is out of bounds */
    if (!(LCDC & 0x20) || LY < WY || WX > 166)
        return;

    uint16_t window_tile_map;
    if (LCDC & 0x40)
        window_tile_map = 0x9C00;
    else
        window_tile_map = 0x9800;

    uint8_t win_y = LY - WY;
    for (int x = 0; x < 160; x++) {
        if (x < WX - 7)
            continue;
        uint8_t win_x = x - (WX - 7);

        framebuffer[LY][x] = fetch_tile_pixel(win_x, win_y, window_tile_map);
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
