//
// Created by edi on 5/10/26.
//

#include "ppu.h"

Ppu::Ppu(Memory& memory) : mem(memory) {
}

void Ppu::step(uint8_t cycles) {
    scanline_cycles += cycles;
    uint8_t mode = 0;
    if (scanline_cycles >= 456) {
        scanline_cycles -= 456;
        mem.write(0xFF44, mem.read(0xFF44) + 1); // LY increment
        if (mem.read(0xFF44) >= 154) // check LY if over 154
            mem.write(0xFF44, 0); // reset to 0 if true
        if (mem.read(0xFF44) == 144)
            mem.write(0xFF0F, mem.read(0xFF0F) | 0x01);
    }
    if (mem.read(0xFF44) >= 144) { // mode 1 - VBlank
        mode = 1;
    } else if (scanline_cycles < 80) { // mode 2 - OAM scan
        mode = 2;
    } else if (scanline_cycles < 252) { // mode 3 - Drawing
        mode = 3;
    } else { // mode 0 - HBlank
        mode = 0;
    }
    mem.write(0xFF41, (mem.read(0xFF41) & 0xFC) | mode);
}
