//
// Created by edi on 4/22/26.
//

#ifndef GAMEBOY_EMU_MEMORY_H
#define GAMEBOY_EMU_MEMORY_H
#include <array>
#include <cstdint>
#include <vector>


class Memory {
private:
    std::array<uint8_t, 0x10000> data{};
public:
    uint8_t read(uint16_t address) {
        return data[address];
    }

    void write(uint16_t address, uint8_t value) {
        data[address] = value;
    }

    void loadRom(const std::vector<uint8_t>& rom) {
        for (size_t i = 0; i < rom.size() && i < data.size(); i++)
            data[i] = rom[i];
    }
};


#endif //GAMEBOY_EMU_MEMORY_H