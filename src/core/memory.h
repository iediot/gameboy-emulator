//
// Created by edi on 4/22/26.
//

#ifndef GAMEBOY_EMU_MEMORY_H
#define GAMEBOY_EMU_MEMORY_H

#include <cstdint>
#include <array>
#include <string>
#include <vector>

enum class MbcType {
    NONE,
    MBC1,
    MBC2,
    MBC3,
    MBC4,
    MBC5
};

class Apu;

class Memory {
private:
    std::array<uint8_t, 0x10000> data{};
public:
    Apu* apu = nullptr;
    bool div_reset = false;

    uint8_t button_state = 0xFF;
    void set_button(int button, bool pressed);

    std::string serial_buffer;

    std::vector<uint8_t> rom;

    uint8_t banking_mode = 0;
    uint16_t rom_bank = 1;
    uint8_t upper_bank = 0;

    std::vector<uint8_t> external_ram;
    bool ram_enabled = false;
    bool has_battery = false;
    bool ram_dirty = false;
    uint8_t ram_bank = 0;

    uint8_t mbc = 0;
    MbcType mbc_type;

    bool dma_active = false;
    uint16_t dma_source = 0;
    uint8_t dma_index = 0;
    uint8_t dma_tick = 0;
    uint8_t dma_delay = 0;
    void step_dma();

    // dmg oam corruption bug, the ppu is mid scan and the bus fight mangles a row
    uint16_t oam_word(int row, int word) const;
    void set_oam_word(int row, int word, uint16_t value);
    void oam_corrupt(int row, bool read);
    void oam_corrupt_read_inc(int row);

    void write_mbc1(uint16_t address, uint8_t value);
    void write_mbc3(uint16_t address, uint8_t value);
    void write_mbc5(uint16_t address, uint8_t value);
    void sync_div(uint8_t value);
    uint8_t read(uint16_t address);
    // the ppu has its own path to vram and oam, the cpu facing blocking does not apply
    uint8_t ppu_read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);
    void load_rom(const std::vector<uint8_t>& rom_to_load);
};

#endif //GAMEBOY_EMU_MEMORY_H
