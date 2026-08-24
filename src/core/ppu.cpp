//
// Created by edi on 5/10/26.
//

#include <algorithm>
#include "ppu.h"

Ppu::Ppu(Memory& memory) : mem(memory) {}

uint8_t Ppu::fetch_color_id(uint8_t x, uint8_t y, uint16_t map_base, uint8_t lcdc) {
    // find the tile in the 32x32 map which it covers
    uint8_t tile_col = x / 8;
    uint8_t tile_row = y / 8;

    uint16_t map_address = map_base + tile_row * 32 + tile_col;
    uint8_t tile_index = mem.read_direct(map_address);

    // find the tile's pixel data in VRAM
    uint16_t tile_address;

    // check if bit 4 is 0
    if (lcdc & 0x10)
        tile_address = 0x8000 + tile_index * 16;
    else
        tile_address = 0x9000 + (int8_t)tile_index * 16;

    // row of pixel data
    uint8_t pixel_row = y % 8;
    uint16_t row_address = tile_address + pixel_row * 2;
    uint8_t byte_low = mem.read_direct(row_address);
    uint8_t byte_high = mem.read_direct(row_address + 1);

    // 2-bit color id
    uint8_t pixel_col = x % 8;
    uint8_t low_bit = byte_low >> (7 - pixel_col) & 1;
    uint8_t high_bit = byte_high >> (7 - pixel_col) & 1;
    uint8_t color_id = (high_bit << 1) | low_bit;

    return color_id;
}

// mode 3 runs 172 dots plus the fine scroll, plus a penalty for every object on the line
// and one for the window, all of which push back the moment hblank starts
uint16_t Ppu::mode3_length_extra() {
    uint8_t LCDC = mem.read_direct(LCDC_ADDR);
    uint8_t SCX  = mem.read_direct(SCX_ADDR);
    uint8_t LY   = mem.read_direct(LY_ADDR);

    uint16_t extra = SCX & 7;

    if (LCDC & 0x02) {
        uint8_t height = (LCDC & 0x04) ? 16 : 8;
        uint8_t xs[10];
        int count = 0;
        for (int i = 0; i < 40 && count < 10; i++) {
            int y = mem.read_direct(0xFE00 + i * 4) - 16;
            if (LY < y || LY >= y + height)
                continue;
            xs[count++] = mem.read_direct(0xFE00 + i * 4 + 1);
        }
        // objects are handled left to right, and only the first to land in a background
        // tile pays that tile's share
        std::sort(xs, xs + count);

        bool tile_done[33] = {};
        for (int i = 0; i < count; i++) {
            if (xs[i] == 0) {          // wholly off the left edge, always a flat 11
                extra += 11;
                continue;
            }
            int pixel = (int)SCX + (int)xs[i] - 8;
            int tile = (pixel >> 3) & 31;
            if (!tile_done[tile]) {
                tile_done[tile] = true;
                int right_of = 7 - (pixel & 7);
                if (right_of > 2) extra += right_of - 2;
            }
            extra += 6;
        }
    }

    if ((LCDC & 0x20) && LY >= mem.read_direct(WY_ADDR) && mem.read_direct(WX_ADDR) <= 166)
        extra += 6;

    return extra;
}

void Ppu::draw_sprite() {
    // sprites need to be drawn after sorting to be just the way gameboy logic draws them
    struct sprite_vars {
        uint8_t x;
        uint8_t tile_index;
        uint8_t flags;
        uint8_t row;
        uint8_t oam_index;
    };
    sprite_vars scanline_sprites[10];
    int sprite_count = 0;

    uint8_t LCDC = mem.read_direct(LCDC_ADDR);
    uint8_t LY = mem.read_direct(LY_ADDR);

    if (!(LCDC & 0x02))
        return;

    uint8_t sprite_height = (LCDC & 0x04) ? 16 : 8;

    int sprites_on_line = 0;

    for (int i = 0; i < 40; i++) {
        // we use int here to avoid underflow
        int y = mem.read_direct(0xFE00 + i*4) - 16;
        uint8_t x = mem.read_direct(0xFE00 + i*4 + 1);
        uint8_t tile_index = mem.read_direct(0xFE00 + i*4 + 2);
        uint8_t flags = mem.read_direct(0xFE00 + i*4 + 3);

        // scanline filter
        if (LY < y || LY >= (y + sprite_height))
            continue;

        sprites_on_line++;
        if (sprites_on_line > 10)
            break;

        int row = LY - y;

        scanline_sprites[sprite_count++] = {x, tile_index, flags, (uint8_t)row, (uint8_t)i};
    }

    // sort by x descending, and for ties earlier oam index wins
    std::stable_sort(scanline_sprites, scanline_sprites + sprite_count,
                     [](const sprite_vars& a, const sprite_vars& b) {
        if (a.x != b.x) return a.x > b.x;
        return a.oam_index > b.oam_index;
    });

    for (int s = 0; s < sprite_count; s++) {
        const sprite_vars& sprite = scanline_sprites[s];
        // unpack the fields into local vars
        uint8_t x = sprite.x;
        uint8_t tile_index = sprite.tile_index;
        uint8_t flags = sprite.flags;
        int row = sprite.row;

        if (sprite_height == 16)
            tile_index &= 0xFE;

        if (flags & 0x40)
            row = (sprite_height - 1) - row;

        uint16_t row_address = 0x8000 + tile_index * 16 + row * 2;
        uint8_t low_byte = mem.read_direct(row_address);
        uint8_t high_byte = mem.read_direct(row_address + 1);

        for (int c = 0; c < 8; c++)
        {
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
                palette = mem.read_direct(OBP1_ADDR);
            else
                palette = mem.read_direct(OBP0_ADDR);

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
    uint8_t SCY = mem.read_direct(SCY_ADDR);
    uint8_t SCX = mem.read_direct(SCX_ADDR);
    uint8_t LY = mem.read_direct(LY_ADDR);
    uint8_t LCDC = mem.read_direct(LCDC_ADDR);

    // bg off - white and bg counts as colour 0
    if (!(LCDC & 0x01)) {
        for (int x = 0; x < 160; x++) {
            bg_color_ids[LY][x] = 0;
            framebuffer[LY][x]  = 0;
        }
        draw_sprite();
        return;
    }

    uint8_t bgp_value = mem.read_direct(BGP_ADDR);
    uint16_t map_base = (LCDC & 0x08) ? 0x9C00 : 0x9800;

    for (int x = 0; x <= 159; x++) {
        uint8_t bg_y = SCY + LY;
        uint8_t bg_x = SCX + x;

        uint8_t color_id = fetch_color_id(bg_x, bg_y, map_base, LCDC);
        uint8_t final_color = bgp_value >> (color_id * 2) & 0x03;

        // put the color id into this array to keep track of drawn tiles
        bg_color_ids[LY][x] = color_id;
        framebuffer[LY][x] = final_color;
    }

    uint8_t WY = mem.read_direct(WY_ADDR);
    uint8_t WX = mem.read_direct(WX_ADDR);

    /* check if bit 5 is set or if the window
     layer position is out of bounds */
    if ((LCDC & 0x20) && LY >= WY && WX <= 166) {
        uint16_t window_tile_map;
        if (LCDC & 0x40)
            window_tile_map = 0x9C00;
        else
            window_tile_map = 0x9800;

        uint8_t win_y = window_line_counter;
        for (int x = 0; x < 160; x++) {
            if (x < WX - 7)
                continue;
            uint8_t win_x = x - (WX - 7);

            uint8_t color_id = fetch_color_id(win_x, win_y, window_tile_map, LCDC);
            uint8_t final_color = bgp_value >> (color_id * 2) & 0x03;

            // put the color id into this array to keep track of drawn tiles
            bg_color_ids[LY][x] = color_id;
            framebuffer[LY][x] = final_color;
        }

        window_line_counter++;
    }

    draw_sprite();
}

void Ppu::step(uint8_t cycles) {
    if (!(mem.read_direct(LCDC_ADDR) & 0x80)) {
        scanline_cycles = 0;
        ly_counter = 0;
        mem.write_direct(LY_ADDR, 0);
        mem.write_direct(STAT_ADDR, mem.read_direct(STAT_ADDR) & 0xFC);
        window_line_counter = 0;
        stat_line = false;
        prev_mode = 0;
        // the panel goes blank with the lcd, holding the last frame instead shows
        // whatever vram happened to contain while a game uploads with it switched off
        if (lcd_was_on)
            for (int y = 0; y < 144; y++)
                for (int x = 0; x < 160; x++)
                    framebuffer[y][x] = 0;
        lcd_was_on = false;
        return;
    }

    // switching the lcd on does not restart the scanline from zero, the ppu picks up
    // one m-cycle in, which is what oam_bug/1-lcd_sync measures
    if (!lcd_was_on) {
        lcd_was_on = true;
        scanline_cycles = 4;
    }

    scanline_cycles += cycles;
    uint8_t mode = 0;

    if (scanline_cycles >= 456) {
        scanline_cycles -= 456;
        ly_counter++;

        if (ly_counter >= 154) {
            ly_counter = 0;
            window_line_counter = 0; // also reset the window counter
        }
        if (ly_counter == 144) {
            mem.write_direct(IF_ADDR, mem.read_direct(IF_ADDR) | 0x01);
            frame_ready = true;
        }
    }

    // line 153 runs its full length but LY only reads 153 for the first few cycles and
    // reports 0 for the rest, so a game polling for LY==0 gets a scanline of head start
    uint8_t ly = (ly_counter == 153 && scanline_cycles >= 4) ? 0 : ly_counter;
    mem.write_direct(LY_ADDR, ly);

    if (ly == mem.read_direct(LYC_ADDR))
        mem.write_direct(STAT_ADDR, mem.read_direct(STAT_ADDR) | 0x04);
    else
        mem.write_direct(STAT_ADDR, mem.read_direct(STAT_ADDR) & ~0x04);

    // scanline_cycles is advanced before the mode is evaluated, so a line's first
    // evaluation reads 1 rather than 0 and every boundary sits one past its dot number
    constexpr uint16_t kDotBias = 1;

    if (scanline_cycles == 80 + kDotBias)
        mode3_extra = mode3_length_extra();

    if (ly_counter >= 144) { // mode 1 - VBlank
        mode = 1;
    } else if (scanline_cycles < 80 + kDotBias) { // mode 2 - OAM scan
        mode = 2;
    } else if (scanline_cycles < 252 + mode3_extra + kDotBias) { // mode 3 - Drawing
        mode = 3;
    } else { // mode 0 - HBlank
        mode = 0;
    }

    uint8_t STAT = mem.read_direct(STAT_ADDR);
    bool coincidence = ly == mem.read_direct(LYC_ADDR);

    bool line = (mode == 0 && (STAT & 0x08))
             || (mode == 1 && (STAT & 0x10))
             || (mode == 2 && (STAT & 0x20))
             || (coincidence && (STAT & 0x40));

    if (line && !stat_line)
        mem.write_direct(IF_ADDR, mem.read_direct(IF_ADDR) | 0x02);
    stat_line = line;

    mem.write_direct(STAT_ADDR, (mem.read_direct(STAT_ADDR) & 0xFC) | mode);

    if (mode == 0 && prev_mode != 0 && ly_counter < 144)
        draw_scanline();

    prev_mode = mode;
}
