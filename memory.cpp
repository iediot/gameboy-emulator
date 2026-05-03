//
// Created by edi on 4/22/26.
//

#include <iostream>
#include "memory.h"

uint8_t Memory::read(uint16_t address) {
    return data[address];
}

void Memory::write(uint16_t address, uint8_t value) {
    data[address] = value;
    if (address == 0xFF01) {
        std::cout << static_cast<char>(value) << std::flush;
    }
}

void Memory::loadRom(const std::vector<uint8_t>& rom) {
    for (size_t i = 0; i < rom.size() && i < data.size(); i++)
        data[i] = rom[i];
}