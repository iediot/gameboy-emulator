//
// Created by edi on 4/22/26.
//

#ifndef GAMEBOY_EMU_MEMORY_H
#define GAMEBOY_EMU_MEMORY_H
#include <array>
#include <cstdint>
#include <vector>
#include <string>

enum class MbcType {
    NONE,
    MBC1,
    MBC2,
    MBC3,
    MBC4,
    MBC5
};

class Memory {
private:
    std::array<uint8_t, 0x10000> data{};
public:
    bool div_reset = false;

    // state for the input buttons
    uint8_t button_state = 0xFF;
    // method to set the state of the button
    void set_button(int button, bool pressed);

    std::string serial_buffer;

    std::vector<uint8_t> rom;

    uint8_t banking_mode = 0;
    uint16_t rom_bank = 1;
    uint8_t upper_bank = 0;

    std::vector<uint8_t> external_ram;
    bool ram_enabled = false;
    uint8_t ram_bank = 0;

    uint8_t mbc = 0;
    MbcType mbc_type;

    bool dma_active = false;
    uint16_t dma_source = 0;
    uint8_t dma_index = 0;
    uint8_t dma_tick = 0;
    uint8_t dma_delay = 0;
    void step_dma();

    void write_mbc1(uint16_t address, uint8_t value);
    void write_mbc3(uint16_t address, uint8_t value);
    void write_mbc5(uint16_t address, uint8_t value);
    void sync_div(uint8_t value);
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);
    void loadRom(const std::vector<uint8_t>& rom_to_load);
    uint8_t read_raw(uint16_t address) { return data[address]; }
};

#endif //GAMEBOY_EMU_MEMORY_H