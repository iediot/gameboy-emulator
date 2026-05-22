//
// Created by edi on 4/22/26.
//

#include <iostream>
#include "memory.h"

void Memory::set_button(int button, bool pressed) {
    if (pressed)
        button_state &= ~(1 << button);
    else
        button_state |= (1 << button);
}

void Memory::sync_div(uint8_t value) {
    data[0xFF04] = value;
}

uint8_t Memory::read(uint16_t address)
{
    // bank 0 is in between these addresses
    if (address >= 0x0000 && address <= 0x3FFF) {
        return rom[address];
    }
    // this is the switchable bank
    if (address >= 0x4000 && address <= 0x7FFF) {
        return rom[rom_bank * 0x4000 + (address - 0x4000)];
    }
    // address for input
    if (address == 0xFF00) {
        uint8_t joyp_byte = data[0xFF00];
        bool bit_4_set = joyp_byte & 0x10;
        bool bit_5_set = joyp_byte & 0x20;

        uint8_t lower_nibble;
        uint8_t upper_bits = (joyp_byte & 0x30) | 0xC0;

        if (bit_4_set && bit_5_set)
            lower_nibble = 0x0F;
        else if (!bit_4_set && !bit_5_set)
            lower_nibble = (button_state & 0x0F) & ((button_state >> 4) & 0x0F);
        else if (!bit_4_set)
            lower_nibble = button_state & 0x0F;
        else
            lower_nibble = (button_state >> 4) & 0x0F;

        uint8_t result = upper_bits | lower_nibble;

        return result;
    }
    return data[address];
}

void Memory::write(uint16_t address, uint8_t value) {
    // writing here selects the lower 5 bits
    if (address >= 0x2000 && address <= 0x3FFF) {
        rom_bank = value & 0x1F;
        // mbc1 can't write to bank 0 so map to 1
        if (rom_bank == 0)
            rom_bank = 1;
        return;
    }

    /* for the joypad register only bits
       4 and 5 are writable so we mask them */
    if (address == 0xFF00) {
        data[0xFF00] = value & 0x30;
        return;
    }

    /* any write to div resets the internal
       16 bit counter*/
    if (address == 0xFF04) {
        div_reset = true;
        return;
    }

    // DMA
    if (address == 0xFF46) {
        uint16_t source = value << 8;
        for (int i = 0; i < 160; i++) {
            data[0xFE00 + i] = data[source + i];
        }
        data[address] = value;
        return;
    }

    data[address] = value;

    /*
    // serial output used by test roms to print
    if (address == 0xFF01) {
        serial_buffer.push_back(static_cast<char>(value));
        std::cout << static_cast<char>(value) << std::flush;
    }
    */
}

void Memory::loadRom(const std::vector<uint8_t>& rom_to_load) {
    rom = rom_to_load;
    mbc_type = rom[0x0147];
}