//
// Created by edi on 5/10/26.
//

#include "ppu.h"

Ppu::Ppu(Memory& memory) : mem(memory) {
}

void Ppu::step(uint8_t cycles) {
    scanline_cycles += cycles;
    if (scanline_cycles >= 456) {
        scanline_cycles -= 456;
        mem.write(0xFF44, mem.read(0xFF44) + 1);
        if (mem.read(0xFF44) >= 154)
            mem.write(0xFF44, 0);
    }
}
