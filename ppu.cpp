//
// Created by edi on 5/10/26.
//

#include "ppu.h"

Ppu::Ppu(Memory& memory) : mem(memory) {}

uint8_t Ppu::fetch_color_id(uint8_t x, uint8_t y, uint16_t map_base) {
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

    return color_id;
}

void Ppu::draw_sprite() {
    uint8_t LCDC = mem.read(LCDC_ADDR);
    uint8_t LY = mem.read(LY_ADDR);

    if (!(LCDC & 0x02))
        return;

    uint8_t sprite_height = (LCDC & 0x04) ? 16 : 8;

    int sprites_on_line = 0;

    for (int i = 0; i < 40; i++) {
        // we use int here to avoid underflow
        int y = mem.read(0xFE00 + i*4) - 16;
        uint8_t x = mem.read(0xFE00 + i*4 + 1);
        uint8_t tile_index = mem.read(0xFE00 + i*4 + 2);
        uint8_t flags = mem.read(0xFE00 + i*4 + 3);

        // scanline filter
        if (LY < y || LY >= (y + sprite_height))
            continue;

        sprites_on_line++;
        if (sprites_on_line > 10)
            break;

        int row = LY - y;

        if (sprite_height == 16)
            tile_index &= 0xFE;

        if (flags & 0x40)
            row = (sprite_height - 1) - row;

        uint16_t row_address = 0x8000 + tile_index * 16 + row * 2;
        uint8_t low_byte = mem.read(row_address);
        uint8_t high_byte = mem.read(row_address + 1);

        for (int c = 0; c < 8; c++) {
            int screen_x = (x - 8) + c;

            // skip if out of bounds
            if (screen_x < 0 || screen_x >= 160)
                continue;

            // reverse the column order if flip is set
            int rev_c;
            if (flags & 0x20)
                rev_c = c;
            else
                rev_c = 7 - c;

            // calculate the color id
            uint8_t low_bit = (low_byte >> rev_c) & 1;
            uint8_t high_bit = (high_byte >> rev_c) & 1;
            uint8_t color_id = (high_bit << 1) | low_bit;

            // if the color id is 0 (transparent), skip
            if (color_id == 0)
                continue;

            /* calculate the final color using the
            respective palette, OBP0 OR OBP1 */
            uint8_t palette;
            if (flags & 0x10)
                palette = mem.read(OBP1_ADDR);
            else
                palette = mem.read(OBP0_ADDR);

            // calculate the final color the same way as before
            uint8_t final_color = palette >> (color_id * 2) & 0x03;

            if ((flags & 0x80) && bg_color_ids[LY][screen_x] != 0)
                continue;

            framebuffer[LY][screen_x] = final_color;
        }
    }
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

        uint8_t color_id = fetch_color_id(bg_x, bg_y, map_base);

        // apply the BGP palette to get the shade
        uint8_t bgp_value = mem.read(BGP_ADDR);
        uint8_t final_color = bgp_value >> (color_id * 2) & 0x03;

        // put the color id into this array to keep track of drawn tiles
        bg_color_ids[LY][x] = color_id;
        framebuffer[LY][x] = final_color;
    }

    uint8_t WY = mem.read(WY_ADDR);
    uint8_t WX = mem.read(WX_ADDR);

    /* check if bit 5 is set or if the window
     layer position is out of bounds */
    if ((LCDC & 0x20) && LY >= WY && WX <= 166) {
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

            uint8_t color_id = fetch_color_id(win_x, win_y, window_tile_map);

            // apply the BGP palette to get the shade
            uint8_t bgp_value = mem.read(BGP_ADDR);
            uint8_t final_color = bgp_value >> (color_id * 2) & 0x03;

            // put the color id into this array to keep track of drawn tiles
            bg_color_ids[LY][x] = color_id;
            framebuffer[LY][x] = final_color;
        }
    }

    draw_sprite();
}

void Ppu::step(uint8_t cycles) {
    scanline_cycles += cycles;
    uint8_t mode = 0;

    if (scanline_cycles >= 456) {
        scanline_cycles -= 456;
        mem.write(LY_ADDR, mem.read(LY_ADDR) + 1); // LY increment

        // set bit 2 of STAT if LY == LYC
        if (mem.read(LY_ADDR) == mem.read(LYC_ADDR))
            mem.write(STAT_ADDR, mem.read(STAT_ADDR) | 0x04);
        // raise the STAT interrupt if LY == LYC and if STAT bit 6 is set
        if (mem.read(LY_ADDR) == mem.read(LYC_ADDR) &&
            (mem.read(STAT_ADDR) & 0x40))
            mem.write(IF_ADDR, mem.read(IF_ADDR) | 0x02);
        // if LY != LYC clear bit 2 of STAT
        if (mem.read(LY_ADDR) != mem.read(LYC_ADDR))
            mem.write(STAT_ADDR, mem.read(STAT_ADDR) & ~0x04);

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

    if (prev_mode != mode) {
        uint8_t STAT = mem.read(STAT_ADDR);
        if (mode == 0 && (STAT & 0x08))
            mem.write(IF_ADDR, mem.read(IF_ADDR) | 0x02);
        if (mode == 1 && (STAT & 0x10))
            mem.write(IF_ADDR, mem.read(IF_ADDR) | 0x02);
        if (mode == 2 && (STAT & 0x20))
            mem.write(IF_ADDR, mem.read(IF_ADDR) | 0x02);
    }

    mem.write(STAT_ADDR, (mem.read(STAT_ADDR) & 0xFC) | mode);

    if (mode == 0 && prev_mode != 0 && mem.read(LY_ADDR) < 144)
        draw_scanline();

    prev_mode = mode;
}
