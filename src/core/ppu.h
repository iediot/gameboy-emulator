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
    void scan_oam();

    // the fetcher and the shifter, run one dot at a time through mode 3
    void line_start();
    void fetch_step_dot();
    void shift_pixel();
    void start_object_fetch(int which);
    void push_object(int which);
    void mode3_dot();
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

    // variables used throughout ppu
    uint16_t scanline_cycles = 0;
    uint8_t ly_counter = 0;
    bool lcd_was_on = true;
    bool lcd_blanked = false;   // the buffer has been cleared for this lcd off period
    bool lcd_first_line = false;
    // finished argb pixels, the dmg shades are baked in here so both the colour and the
    // monochrome path hand the frontend the same thing
    uint32_t framebuffer[144][160] = {};
    uint8_t prev_mode = 0;
    uint8_t window_line_counter = 0;

    bool frame_ready = false;
    // how many of this frame's 144 lines the fetcher actually got through. an lcd that
    // was switched off part way leaves the rest holding the previous frame's pixels
    int lines_drawn = 0;

    bool stat_line = false;

    // the ten objects the oam scan picked for this line. hardware settles on them
    // during mode 2 and mode 3 draws that list, so a write landing after the scan
    // cannot change what appears on the line it lands in
    struct ScannedSprite {
        uint8_t x;
        uint8_t tile_index;
        uint8_t flags;
        uint8_t row;
        uint8_t oam_index;
    };
    ScannedSprite line_sprites[10];
    int line_sprite_count = 0;

    /* mode 3 is not a loop over 160 columns, it is a fetcher walking the tile map two
       dots at a time filling a queue, and a shifter emptying that queue one pixel per
       dot. everything the registers say is read at the moment each pixel is fetched, so
       a game that moves the scroll or swaps a palette part way along a line gets what
       the hardware would have drawn rather than one value smeared across the whole row */
    struct BgPixel {
        uint8_t color;      // 0..3 straight out of the tile
        uint8_t palette;    // cgb background palette number
        uint8_t priority;   // the tile attribute's bit 7
    };
    struct ObjPixel {
        uint8_t color;      // 0 means nothing was drawn here
        uint8_t palette;    // cgb object palette, or obp1 on the dmg
        uint8_t priority;   // the object's own bit 7, background over object
        uint8_t index;      // oam position, which breaks ties on the cgb
        uint8_t dmg_pal;    // 0 for obp0, 1 for obp1
    };

    BgPixel bg_fifo[16];
    int bg_fifo_head = 0;
    int bg_fifo_len = 0;
    // the object queue is held aligned to the shifter, entry i is the pixel i ahead
    ObjPixel obj_fifo[8];

    int lx = 0;             // the screen column the shifter is about to hand over
    int discard = 0;        // fine scroll pixels thrown away before the line proper
    int fetch_step = 0;     // tile number, low byte, high byte, push
    int fetch_dot = 0;      // each of those takes two dots
    int fetch_x = 0;        // tile column the fetcher is on
    bool first_fetch = true;   // the throwaway fetch every line opens with
    bool in_window = false;
    bool window_started = false;
    uint8_t fetch_tile = 0;
    uint8_t fetch_attr = 0;
    uint8_t fetch_lo = 0;
    uint8_t fetch_hi = 0;
    int window_stall = 0;   // dots the window's fetcher restart is holding the line for
    int obj_stall = 0;      // dots an object fetch is holding the shifter for
    int obj_pending = -1;   // which scanned object that fetch is for
    int obj_penalty_tile = -1;  // the tile that has already paid the alignment wait
    bool obj_done[10] = {};
    /* the window's vertical condition is not a comparison, it is a latch: the ppu
       watches for ly to equal wy and remembers that it happened for the rest of the
       frame. a game that moves wy after that point does not put the window back */
    bool wy_triggered = false;
    bool line_active = false;  // mode 3 is running and has pixels left to hand over

    void step(uint8_t cycles);
};

#endif //GAMEBOY_EMU_PPU_H
