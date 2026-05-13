//
// Created by edi on 4/22/26.
//

#include <iostream>
#include "memory.h"

void Memory::sync_div(uint8_t value) {
    data[0xFF04] = value;
}

uint8_t Memory::read(uint16_t address) {
    return data[address];
}

void Memory::write(uint16_t address, uint8_t value) {
    if (address == 0xFF04) {
        div_reset = true;
    } else {
        data[address] = value;
    }
    if (address == 0xFF01) {
        serial_buffer.push_back(static_cast<char>(value));
        std::cout << static_cast<char>(value) << std::flush;
        if (serial_buffer.ends_with("Passed") || serial_buffer.ends_with("Failed"))
            test_done = true;
    }
}

void Memory::loadRom(const std::vector<uint8_t>& rom) {
    for (size_t i = 0; i < rom.size() && i < data.size(); i++)
        data[i] = rom[i];
}