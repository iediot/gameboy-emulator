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

uint8_t Memory::read(uint16_t address) {
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

    data[address] = value;

    // serial output used by test roms to print
    if (address == 0xFF01) {
        serial_buffer.push_back(static_cast<char>(value));
        std::cout << static_cast<char>(value) << std::flush;
    }
}

void Memory::loadRom(const std::vector<uint8_t>& rom) {
    for (size_t i = 0; i < rom.size() && i < data.size(); i++)
        data[i] = rom[i];
}