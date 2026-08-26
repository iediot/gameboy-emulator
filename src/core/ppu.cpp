//
// Created by edi on 5/10/26.
//

#include <algorithm>
#include "ppu.h"

Ppu::Ppu(Memory& memory) : mem(memory) {}

namespace {
    constexpr uint32_t kDmgShades[4] = {0xFF627102, 0xFF4D5802, 0xFF364002, 0xFF1F2701};

    uint32_t rgb555(uint16_t raw) {
        uint8_t r = raw & 0x1F, g = (raw >> 5) & 0x1F, b = (raw >> 10) & 0x1F;
        return 0xFF000000
             | ((uint32_t)((r << 3) | (r >> 2)) << 16)
             | ((uint32_t)((g << 3) | (g >> 2)) << 8)
             | (uint32_t)((b << 3) | (b >> 2));
    }

    // colour ram holds little endian bgr555, two bytes per entry, eight per palette
    uint32_t cgb_rgb(const uint8_t* pal, uint8_t palette_index, uint8_t color_id) {
        int i = palette_index * 8 + color_id * 2;
        return rgb555((uint16_t)pal[i] | ((uint16_t)pal[i + 1] << 8));
    }

    // a mono game either runs through the boot rom's colour table or keeps the olive look
    uint32_t bg_shade(const Memory& mem, uint8_t shade) {
        return mem.compat_palette ? rgb555(mem.compat_bg[shade]) : kDmgShades[shade];
    }
    uint32_t obj_shade(const Memory& mem, int pal, uint8_t shade) {
        return mem.compat_palette ? rgb555(mem.compat_obj[pal][shade]) : kDmgShades[shade];
    }
}

uint8_t Ppu::fetch_color_id(uint8_t x, uint8_t y, uint16_t map_base, uint8_t lcdc,
                            uint8_t& attr_out) {
    // find the tile in the 32x32 map which it covers
    uint8_t tile_col = x / 8;
    uint8_t tile_row = y / 8;

    // the map itself always lives in bank 0, bank 1 holds the attribute for the same slot
    uint16_t map_address = map_base + tile_row * 32 + tile_col;
    uint8_t tile_index = mem.vram_read(0, map_address);
    uint8_t attr = mem.cgb_mode ? mem.vram_read(1, map_address) : 0;
    attr_out = attr;

    // find the tile's pixel data in VRAM
    uint16_t tile_address;

    // check if bit 4 is 0
    if (lcdc & 0x10)
        tile_address = 0x8000 + tile_index * 16;
    else
        tile_address = 0x9000 + (int8_t)tile_index * 16;

    // row of pixel data
    uint8_t pixel_row = y % 8;
    if (attr & 0x40)
        pixel_row = 7 - pixel_row;

    uint16_t row_address = tile_address + pixel_row * 2;
    uint8_t bank = (attr & 0x08) ? 1 : 0;
    uint8_t byte_low = mem.vram_read(bank, row_address);
    uint8_t byte_high = mem.vram_read(bank, row_address + 1);

    // 2-bit color id
    uint8_t pixel_col = x % 8;
    if (attr & 0x20)
        pixel_col = 7 - pixel_col;
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

    // the later a sprite is drawn the more it wins, on the dmg that order is by x with
    // oam index breaking ties, the cgb drops x from the comparison entirely
    if (mem.cgb_mode) {
        std::stable_sort(scanline_sprites, scanline_sprites + sprite_count,
                         [](const sprite_vars& a, const sprite_vars& b) {
            return a.oam_index > b.oam_index;
        });
    } else {
        std::stable_sort(scanline_sprites, scanline_sprites + sprite_count,
                         [](const sprite_vars& a, const sprite_vars& b) {
            if (a.x != b.x) return a.x > b.x;
            return a.oam_index > b.oam_index;
        });
    }

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
        uint8_t tile_bank = (mem.cgb_mode && (flags & 0x08)) ? 1 : 0;
        uint8_t low_byte = mem.vram_read(tile_bank, row_address);
        uint8_t high_byte = mem.vram_read(tile_bank, row_address + 1);

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

            // with lcdc bit 0 clear the cgb strips the background of any priority and
            // every sprite lands on top, otherwise either the tile attribute or the
            // sprite's own flag can put the background back in front
            if (mem.cgb_mode) {
                bool master_priority = LCDC & 0x01;
                bool bg_wins = master_priority
                            && bg_color_ids[LY][screen_x] != 0
                            && (bg_priority[LY][screen_x] || (flags & 0x80));
                if (bg_wins)
                    continue;
                framebuffer[LY][screen_x] =
                    cgb_rgb(mem.obj_palette, flags & 0x07, color_id);
                continue;
            }

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

            framebuffer[LY][screen_x] = obj_shade(mem, (flags & 0x10) ? 1 : 0, final_color);
        }
    }
}

void Ppu::draw_scanline() {
    // read line registers
    uint8_t SCY = mem.read_direct(SCY_ADDR);
    uint8_t SCX = mem.read_direct(SCX_ADDR);
    uint8_t LY = mem.read_direct(LY_ADDR);
    uint8_t LCDC = mem.read_direct(LCDC_ADDR);

    // on the dmg bit 0 blanks the background, on the cgb it only drops its priority so
    // the tiles still have to be drawn
    if (!mem.cgb_mode && !(LCDC & 0x01)) {
        for (int x = 0; x < 160; x++) {
            bg_color_ids[LY][x] = 0;
            bg_priority[LY][x]  = 0;
            framebuffer[LY][x]  = bg_shade(mem, 0);
        }
        draw_sprite();
        return;
    }

    uint8_t bgp_value = mem.read_direct(BGP_ADDR);
    uint16_t map_base = (LCDC & 0x08) ? 0x9C00 : 0x9800;

    for (int x = 0; x <= 159; x++) {
        uint8_t bg_y = SCY + LY;
        uint8_t bg_x = SCX + x;

        uint8_t attr;
        uint8_t color_id = fetch_color_id(bg_x, bg_y, map_base, LCDC, attr);

        // put the color id into this array to keep track of drawn tiles
        bg_color_ids[LY][x] = color_id;
        bg_priority[LY][x] = (attr & 0x80) ? 1 : 0;
        framebuffer[LY][x] = mem.cgb_mode
            ? cgb_rgb(mem.bg_palette, attr & 0x07, color_id)
            : bg_shade(mem, bgp_value >> (color_id * 2) & 0x03);
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

            uint8_t attr;
            uint8_t color_id = fetch_color_id(win_x, win_y, window_tile_map, LCDC, attr);

            // put the color id into this array to keep track of drawn tiles
            bg_color_ids[LY][x] = color_id;
            bg_priority[LY][x] = (attr & 0x80) ? 1 : 0;
            framebuffer[LY][x] = mem.cgb_mode
                ? cgb_rgb(mem.bg_palette, attr & 0x07, color_id)
                : bg_shade(mem, bgp_value >> (color_id * 2) & 0x03);
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
        if (lcd_was_on) {
            // a colourised game blanks to its own lightest colour, not to white, or every
            // lcd toggle flashes and games switch it off constantly to upload tiles
            uint32_t blank = mem.cgb_mode ? 0xFFFFFFFF : bg_shade(mem, 0);
            for (int y = 0; y < 144; y++)
                for (int x = 0; x < 160; x++)
                    framebuffer[y][x] = blank;
        }
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

    if (mode == 0 && prev_mode != 0 && ly_counter < 144) {
        draw_scanline();
        // an hblank transfer moves one 16 byte chunk per line, which is how games get
        // tile data in without a visible tear
        if (mem.hdma_running && mem.hdma_hblank)
            mem.hdma_block();
    }

    prev_mode = mode;
}
