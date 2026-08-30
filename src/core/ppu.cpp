//
// Created by edi on 5/10/26.
//

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


// hardware walks oam through the whole of mode 2 and keeps the first ten objects that
// cover this line. everything after that works off that list rather than off oam itself
void Ppu::scan_oam() {
    line_sprite_count = 0;
    uint8_t LCDC = mem.read_direct(LCDC_ADDR);
    if (!(LCDC & 0x02))
        return;
    uint8_t LY = mem.read_direct(LY_ADDR);
    uint8_t height = (LCDC & 0x04) ? 16 : 8;

    for (int i = 0; i < 40 && line_sprite_count < 10; i++) {
        int y = mem.read_direct(0xFE00 + i * 4) - 16;
        if (LY < y || LY >= y + height)
            continue;
        line_sprites[line_sprite_count++] = {
            mem.read_direct(0xFE00 + i * 4 + 1),
            mem.read_direct(0xFE00 + i * 4 + 2),
            mem.read_direct(0xFE00 + i * 4 + 3),
            (uint8_t)(LY - y),
            (uint8_t)i
        };
    }
}

// mode 3 opens with the fetcher at the leftmost tile and the queue empty, and with the
// fine part of the scroll marked to be thrown away once pixels start coming out
void Ppu::line_start() {
    bg_fifo_head = 0;
    bg_fifo_len = 0;
    for (int i = 0; i < 8; i++)
        obj_fifo[i] = ObjPixel{0, 0, 0, 0, 0};
    lx = 0;
    discard = mem.read_direct(SCX_ADDR) & 7;
    fetch_step = 0;
    fetch_dot = 0;
    fetch_x = 0;
    first_fetch = true;
    in_window = false;
    window_started = false;
    window_stall = 0;
    obj_stall = 0;
    obj_pending = -1;
    obj_penalty_tile = -1;
    for (int i = 0; i < 10; i++)
        obj_done[i] = false;
    line_active = true;
}

// one dot of the background fetcher. each of the four steps takes two of them, and the
// addresses are worked out here rather than once a line, which is the whole point
void Ppu::fetch_step_dot() {
    if (fetch_step != 3 && ++fetch_dot < 2)
        return;
    fetch_dot = 0;

    uint8_t LCDC = mem.read_direct(LCDC_ADDR);
    uint8_t LY   = mem.read_direct(LY_ADDR);
    uint8_t SCX  = mem.read_direct(SCX_ADDR);
    uint8_t SCY  = mem.read_direct(SCY_ADDR);

    switch (fetch_step) {
        case 0: {   // which tile
            uint16_t map_base;
            uint8_t tile_col, tile_row;
            if (in_window) {
                map_base = (LCDC & 0x40) ? 0x9C00 : 0x9800;
                tile_col = fetch_x & 0x1F;
                tile_row = (window_line_counter >> 3) & 0x1F;
            } else {
                map_base = (LCDC & 0x08) ? 0x9C00 : 0x9800;
                tile_col = (uint8_t)(((SCX >> 3) + fetch_x)) & 0x1F;
                tile_row = (uint8_t)(((uint8_t)(LY + SCY)) >> 3) & 0x1F;
            }
            uint16_t address = (uint16_t)(map_base + tile_row * 32 + tile_col);
            fetch_tile = mem.vram_read(0, address);
            fetch_attr = mem.cgb_mode ? mem.vram_read(1, address) : 0;
            fetch_step = 1;
            break;
        }
        case 1:
        case 2: {   // the two halves of the tile row
            uint8_t row = in_window ? (uint8_t)(window_line_counter & 7)
                                    : (uint8_t)((uint8_t)(LY + SCY) & 7);
            if (fetch_attr & 0x40)
                row = 7 - row;
            uint16_t base = (LCDC & 0x10) ? (uint16_t)(0x8000 + fetch_tile * 16)
                                          : (uint16_t)(0x9000 + (int8_t)fetch_tile * 16);
            uint8_t bank = (fetch_attr & 0x08) ? 1 : 0;
            if (fetch_step == 1)
                fetch_lo = mem.vram_read(bank, (uint16_t)(base + row * 2));
            else
                fetch_hi = mem.vram_read(bank, (uint16_t)(base + row * 2 + 1));
            fetch_step++;
            // every line opens with one fetch that is thrown away instead of pushed,
            // which is where mode 3's twelve dot head start comes from
            if (fetch_step == 3 && first_fetch) {
                first_fetch = false;
                fetch_step = 0;
            }
            break;
        }
        default: {  // hand the eight pixels over, but only once the queue has drained
            if (bg_fifo_len > 0)
                return;
            for (int i = 0; i < 8; i++) {
                int bit = (fetch_attr & 0x20) ? i : 7 - i;
                uint8_t color = (uint8_t)((((fetch_hi >> bit) & 1) << 1)
                                        | ((fetch_lo >> bit) & 1));
                bg_fifo[(bg_fifo_head + bg_fifo_len) & 15] =
                    BgPixel{color, (uint8_t)(fetch_attr & 0x07),
                            (uint8_t)((fetch_attr >> 7) & 1)};
                bg_fifo_len++;
            }
            fetch_x++;
            fetch_step = 0;
            break;
        }
    }
}

// an object's eight pixels are merged into the queue rather than overwriting it. a slot
// already holding something opaque belongs to an object that got there first, and only a
// lower oam index takes it away, which is the rule the colour hardware follows
void Ppu::push_object(int which) {
    const ScannedSprite& s = line_sprites[which];
    uint8_t LCDC = mem.read_direct(LCDC_ADDR);
    uint8_t height = (LCDC & 0x04) ? 16 : 8;

    uint8_t tile = s.tile_index;
    if (height == 16)
        tile &= 0xFE;
    int row = s.row;
    if (s.flags & 0x40)
        row = (height - 1) - row;

    uint16_t address = (uint16_t)(0x8000 + tile * 16 + row * 2);
    uint8_t bank = (mem.cgb_mode && (s.flags & 0x08)) ? 1 : 0;
    uint8_t low  = mem.vram_read(bank, address);
    uint8_t high = mem.vram_read(bank, (uint16_t)(address + 1));

    // an object hanging off the left edge starts part way into its own row
    int start = lx - ((int)s.x - 8);
    if (start < 0)
        start = 0;
    for (int i = start; i < 8; i++) {
        int slot = i - start;
        if (slot > 7)
            break;
        int bit = (s.flags & 0x20) ? i : 7 - i;
        uint8_t color = (uint8_t)((((high >> bit) & 1) << 1) | ((low >> bit) & 1));
        if (color == 0)
            continue;
        ObjPixel& here = obj_fifo[slot];
        bool take = here.color == 0 || (mem.cgb_mode && s.oam_index < here.index);
        if (!take)
            continue;
        here = ObjPixel{color, (uint8_t)(s.flags & 0x07),
                        (uint8_t)((s.flags >> 7) & 1), s.oam_index,
                        (uint8_t)((s.flags & 0x10) ? 1 : 0)};
    }
}

void Ppu::start_object_fetch(int which) {
    obj_pending = which;
    obj_done[which] = true;

    /* the fetch itself is six dots, and on top of that the object has to wait for the
       background fetcher to finish the tile it is in the middle of. that wait is longest
       when the object lands right on a tile boundary and nothing when it lands near the
       end of one, and only the first object in a given tile pays it, because the ones
       after it find the fetcher already settled */
    // the wait is measured from where the object sits, not from where the shifter had
    // to clamp it to, so one hanging off the left edge still pays for its own column
    int pixel = (int)mem.read_direct(SCX_ADDR) + (int)line_sprites[which].x - 8;
    int tile = (pixel >> 3) & 31;
    int align = 0;
    if (tile != obj_penalty_tile) {
        obj_penalty_tile = tile;
        align = 5 - (pixel & 7);
        if (align < 0)
            align = 0;
    }
    // the dot this was decided on is itself lost to the object, so the counter holds
    // one fewer than the total
    obj_stall = 5 + align;
}

// one dot of the shifter: take the front of the queue, decide between it and whatever
// object pixel sits in the same slot, and hand the result to the panel
void Ppu::shift_pixel() {
    if (bg_fifo_len == 0)
        return;

    BgPixel b = bg_fifo[bg_fifo_head];
    bg_fifo_head = (bg_fifo_head + 1) & 15;
    bg_fifo_len--;

    // the fine part of the scroll is dropped at the shifter, which is what makes a
    // scroll that is not a multiple of eight cost mode 3 those extra dots
    if (discard > 0) {
        discard--;
        return;
    }

    uint8_t LCDC = mem.read_direct(LCDC_ADDR);
    uint8_t LY   = mem.read_direct(LY_ADDR);
    ObjPixel o = obj_fifo[0];
    for (int i = 0; i < 7; i++)
        obj_fifo[i] = obj_fifo[i + 1];
    obj_fifo[7] = ObjPixel{0, 0, 0, 0, 0};

    // on the dmg bit 0 blanks the background outright, on the colour hardware it only
    // strips its priority and the tiles still show
    bool bg_on = mem.cgb_mode || (LCDC & 0x01);
    uint8_t bg_color = bg_on ? b.color : 0;

    uint32_t out;
    if (bg_on)
        out = mem.cgb_mode
            ? cgb_rgb(mem.bg_palette, b.palette, bg_color)
            : bg_shade(mem, mem.read_direct(BGP_ADDR) >> (bg_color * 2) & 0x03);
    else
        out = bg_shade(mem, 0);

    bool obj_won = false;
    if ((LCDC & 0x02) && o.color != 0) {
        bool bg_wins;
        if (mem.cgb_mode)
            bg_wins = (LCDC & 0x01) && bg_color != 0 && (b.priority || o.priority);
        else
            bg_wins = o.priority && bg_color != 0;

        if (!bg_wins) {
            obj_won = true;
            if (mem.cgb_mode) {
                out = cgb_rgb(mem.obj_palette, o.palette, o.color);
            } else {
                uint8_t pal = mem.read_direct(o.dmg_pal ? OBP1_ADDR : OBP0_ADDR);
                out = obj_shade(mem, o.dmg_pal, pal >> (o.color * 2) & 0x03);
            }
        }
    }

    if (LY < 144 && lx < 160) {
        framebuffer[LY][lx] = out;
        obj_pixel[LY][lx] = obj_won ? 1 : 0;
    }
    lx++;
    if (lx >= 160) {
        line_active = false;
        lines_drawn++;
    }
}

// one dot of mode 3, in the order the hardware does it: the window can take the fetcher
// over, an object can seize it, otherwise fetcher and shifter both advance
void Ppu::mode3_dot() {
    uint8_t LCDC = mem.read_direct(LCDC_ADDR);

    // clearing the enable part way along a line drops the fetcher back onto the
    // background there and then, mid tile and all
    if (in_window && !(LCDC & 0x20))
        in_window = false;

    // the window replaces the background from its column onward, and restarting the
    // fetcher on it is what costs the line its six dots
    if (!in_window && (LCDC & 0x20) && wy_triggered) {
        int wx = (int)mem.read_direct(WX_ADDR) - 7;
        if (discard == 0 && lx >= wx && wx < 160) {
            bool first = !window_started;
            in_window = true;
            window_started = true;
            bg_fifo_head = 0;
            bg_fifo_len = 0;
            fetch_step = 0;
            fetch_dot = 0;
            // the window keeps its own column counter for the line, so switching the
            // window off and back on again picks up where it left off
            if (first)
                fetch_x = 0;
            /* reloading the fetcher onto the window's map costs the line six dots. part
               way along one, emptying the queue exacts that on its own because the
               shifter then has to wait for a whole fetch. at the very start of a line
               there was nothing in the queue to throw away, so it has to be charged */
            if (lx == 0)
                window_stall = 5;
            return;
        }
    }

    if (window_stall > 0) {
        window_stall--;
        return;
    }

    if (obj_stall > 0) {
        if (--obj_stall == 0 && obj_pending >= 0) {
            push_object(obj_pending);
            obj_pending = -1;
        }
        return;
    }

    fetch_step_dot();

    // an object cannot be fetched until the background queue has something in it, so a
    // fetch caught mid tile pays for the rest of that tile too. an object sitting off
    // the left edge draws nothing but is still fetched, and still costs the line for it
    if ((LCDC & 0x02) && bg_fifo_len > 0 && discard == 0) {
        for (int i = 0; i < line_sprite_count; i++) {
            if (obj_done[i])
                continue;
            if ((int)line_sprites[i].x - 8 <= lx && line_sprites[i].x < 168) {
                start_object_fetch(i);
                return;
            }
        }
    }

    shift_pixel();
}

void Ppu::step(uint8_t cycles) {
    if (!(mem.read_direct(LCDC_ADDR) & 0x80)) {
        scanline_cycles = 0;
        ly_counter = 0;
        mem.write_direct(LY_ADDR, 0);
        mem.write_direct(STAT_ADDR, mem.read_direct(STAT_ADDR) & 0xFC);
        window_line_counter = 0;
        wy_triggered = false;
        line_active = false;
        lines_drawn = 0;
        // the mode sources go quiet but the retained coincidence bit still drives the
        // interrupt line, so switching back on with the same result raises no edge
        uint8_t off_stat = mem.read_direct(STAT_ADDR);
        stat_line = (off_stat & 0x40) && (off_stat & 0x04);
        prev_mode = 0;
        /* the panel goes blank with the lcd. holding the last frame instead was tried
           and backed out: it leaves whatever the lcd interrupted part way through
           sitting under the next thing drawn, which shows as debris on a loading screen */
        if (!lcd_blanked) {
            lcd_blanked = true;
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
        lcd_blanked = false;
        scanline_cycles = 4;
        lcd_first_line = true;
    }

    scanline_cycles += cycles;
    uint8_t mode = 0;

    if (scanline_cycles >= 456) {
        scanline_cycles -= 456;
        ly_counter++;
        lcd_first_line = false;

        if (ly_counter == 0 || ly_counter >= 154)
            lines_drawn = 0;
        if (ly_counter >= 154) {
            ly_counter = 0;
            window_line_counter = 0; // also reset the window counter
            wy_triggered = false;
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

    // the window's vertical latch is only armed while the ppu is scanning oam, not all
    // line long
    if (ly_counter < 144 && scanline_cycles < 80 + kDotBias
        && ly == mem.read_direct(WY_ADDR))
        wy_triggered = true;

    // the oam scan closes as mode 3 opens and the fetcher takes over from there. how
    // long mode 3 then runs is not a formula, it is however many dots the fetcher needs
    if (ly_counter < 144 && scanline_cycles == 80 + kDotBias) {
        scan_oam();
        line_start();
    }

    // the dot that hands over the last pixel is still a mode 3 dot, the change only
    // shows from the next one, so the mode is read from where the line stood on entry
    bool was_active = line_active;
    // a zero cycle step is the cpu telling the ppu to re-read a register it just wrote,
    // not the passage of a dot, so the pipeline must not move for it
    if (cycles > 0 && ly_counter < 144 && line_active)
        mode3_dot();

    if (ly_counter >= 144) { // mode 1 - VBlank
        mode = 1;
    } else if (scanline_cycles < 80 + kDotBias) { // mode 2 - OAM scan
        mode = 2;
    } else if (was_active) { // mode 3 - Drawing
        mode = 3;
    } else { // mode 0 - HBlank
        mode = 0;
    }

    // the line the lcd comes back on has no oam scan, stat reports hblank where mode 2
    // would be and no mode 2 interrupt is raised
    if (lcd_first_line && mode == 2)
        mode = 0;

    uint8_t STAT = mem.read_direct(STAT_ADDR);
    bool coincidence = ly == mem.read_direct(LYC_ADDR);

    // the oam source also fires on the line vblank starts, so a game with only the
    // mode 2 interrupt enabled still gets one at line 144 alongside the vblank
    // colour hardware raises it one m-cycle ahead of the vblank instead of alongside it
    bool oam_window = mode == 2
                   || (ly_counter == 144 && scanline_cycles < 80 + kDotBias)
                   || (mem.cgb_enabled && ly_counter == 143
                       && scanline_cycles >= 456 - 8 + kDotBias);

    // the hblank source goes up as the fetcher finishes, one dot before stat starts
    // reporting mode 0
    bool hblank_int = ly_counter < 144 && !line_active
                   && scanline_cycles >= 80 + kDotBias;

    bool line = (hblank_int && (STAT & 0x08))
             || (mode == 1 && (STAT & 0x10))
             || (oam_window && (STAT & 0x20))
             || (coincidence && (STAT & 0x40));

    /* the write drives every source for that one instant, so whichever condition is true
       right then gets through even though the game never enabled it. the oam source is
       not one of them. it is a pulse and not a state: latching it into stat_line would
       swallow the next real edge */
    if (mem.stat_glitch) {
        mem.stat_glitch = false;
        if (!stat_line && (hblank_int || mode == 1 || coincidence))
            mem.write_direct(IF_ADDR, mem.read_direct(IF_ADDR) | 0x02);
    }

    if (line && !stat_line)
        mem.write_direct(IF_ADDR, mem.read_direct(IF_ADDR) | 0x02);
    stat_line = line;

    mem.write_direct(STAT_ADDR, (mem.read_direct(STAT_ADDR) & 0xFC) | mode);

    if (mode == 0 && prev_mode != 0 && ly_counter < 144) {
        // the window only advances its own row counter on the lines it actually drew on
        if (window_started)
            window_line_counter++;
        // an hblank transfer moves one 16 byte chunk per line, which is how games get
        // tile data in without a visible tear
        if (mem.hdma_running && mem.hdma_hblank)
            mem.hdma_block();
    }

    prev_mode = mode;
}

/* the fetcher and the shifter are part of the machine's state just as much as the
   registers are, so a state taken part way along a scanline resumes on that same dot
   with the same pixels still queued */
void Ppu::save_state(state::Writer& w) const {
    w.raw(scanline_cycles);
    w.raw(ly_counter);
    w.raw(lcd_was_on);
    w.raw(lcd_first_line);
    w.raw(lcd_blanked);
    w.bytes(framebuffer, sizeof framebuffer);
    w.raw(prev_mode);
    w.raw(window_line_counter);
    w.raw(frame_ready);
    w.raw(lines_drawn);
    w.raw(stat_line);
    w.bytes(line_sprites, sizeof line_sprites);
    w.raw(line_sprite_count);
    w.bytes(bg_fifo, sizeof bg_fifo);
    w.raw(bg_fifo_head);
    w.raw(bg_fifo_len);
    w.bytes(obj_fifo, sizeof obj_fifo);
    w.raw(lx);
    w.raw(discard);
    w.raw(fetch_step);
    w.raw(fetch_dot);
    w.raw(fetch_x);
    w.raw(first_fetch);
    w.raw(in_window);
    w.raw(window_started);
    w.raw(fetch_tile);
    w.raw(fetch_attr);
    w.raw(fetch_lo);
    w.raw(fetch_hi);
    w.raw(window_stall);
    w.raw(obj_stall);
    w.raw(obj_pending);
    w.raw(obj_penalty_tile);
    w.bytes(obj_done, sizeof obj_done);
    w.raw(wy_triggered);
    w.raw(line_active);
}

void Ppu::load_state(state::Reader& r) {
    r.raw(scanline_cycles);
    r.raw(ly_counter);
    r.raw(lcd_was_on);
    r.raw(lcd_first_line);
    r.raw(lcd_blanked);
    r.bytes(framebuffer, sizeof framebuffer);
    r.raw(prev_mode);
    r.raw(window_line_counter);
    r.raw(frame_ready);
    r.raw(lines_drawn);
    r.raw(stat_line);
    r.bytes(line_sprites, sizeof line_sprites);
    r.raw(line_sprite_count);
    r.bytes(bg_fifo, sizeof bg_fifo);
    r.raw(bg_fifo_head);
    r.raw(bg_fifo_len);
    r.bytes(obj_fifo, sizeof obj_fifo);
    r.raw(lx);
    r.raw(discard);
    r.raw(fetch_step);
    r.raw(fetch_dot);
    r.raw(fetch_x);
    r.raw(first_fetch);
    r.raw(in_window);
    r.raw(window_started);
    r.raw(fetch_tile);
    r.raw(fetch_attr);
    r.raw(fetch_lo);
    r.raw(fetch_hi);
    r.raw(window_stall);
    r.raw(obj_stall);
    r.raw(obj_pending);
    r.raw(obj_penalty_tile);
    r.bytes(obj_done, sizeof obj_done);
    r.raw(wy_triggered);
    r.raw(line_active);
}
