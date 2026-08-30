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
    bool cgb_enabled = true;
    bool cgb_mode = false;
    // a mono cartridge coloured the way the cgb boot rom would colour it
    bool dmg_colorize = true;
    bool compat_palette = false;
    uint16_t compat_bg[4]{};
    uint16_t compat_obj[2][4]{};
    void apply_compat_palette();

    uint8_t vram[2][0x2000]{};
    uint8_t vram_bank = 0;
    uint8_t wram[8][0x1000]{};
    uint8_t wram_bank = 1;

    uint8_t bg_palette[64]{};
    uint8_t obj_palette[64]{};

    bool double_speed = false;
    bool speed_switch_armed = false;

    uint16_t hdma_src = 0;
    uint16_t hdma_dst = 0;
    uint8_t hdma_left = 0;
    bool hdma_hblank = false;
    bool hdma_running = false;
    void hdma_block();
    // a vram transfer stops the cpu dead while it runs, eight m-cycles per block
    uint32_t dma_stall = 0;
    Apu* apu = nullptr;
    /* writing stat on a dmg momentarily drives every one of its interrupt sources as if
       it were enabled, so any condition that happens to be true right then fires a
       spurious stat interrupt. the colour hardware fixed it */
    bool stat_glitch = false;
    bool div_reset = false;
    bool tima_written = false;  // a write during the reload delay cancels it

    uint8_t button_state = 0xFF;
    void set_button(int button, bool pressed);

    std::string serial_buffer;

    // the link port shifts one bit per falling edge of a divider bit, and with nothing
    // plugged in every bit that comes back is a 1
    bool serial_active = false;
    uint8_t serial_bits = 0;
    bool serial_fast() const;
    void serial_shift();

    std::vector<uint8_t> rom;

    uint8_t banking_mode = 0;
    uint16_t rom_bank = 1;
    uint8_t upper_bank = 0;
    bool mbc1_multicart = false;

    std::vector<uint8_t> external_ram;
    bool ram_enabled = false;
    bool has_battery = false;
    bool ram_dirty = false;
    uint8_t ram_bank = 0;

    uint8_t mbc = 0;
    MbcType mbc_type;

    // mbc3's clock. five registers tick off the cartridge's own crystal, and the game
    // reads a frozen copy that it refreshes with a latch write
    bool has_rtc = false;
    uint8_t rtc[5]{};          // seconds, minutes, hours, day low, day high
    uint8_t rtc_latched[5]{};
    uint8_t rtc_select = 0;    // 0x08..0x0C picks a clock register over a ram bank
    uint8_t rtc_last_latch = 0xFF;
    uint32_t rtc_sub = 0;      // t-cycles elapsed inside the current second
    void rtc_tick();
    void rtc_step_second();
    void rtc_advance(uint64_t seconds);
    uint8_t rtc_read() const;
    void rtc_write(uint8_t value);

    bool dma_active = false;
    uint16_t dma_source = 0;
    uint16_t dma_pending_source = 0;
    uint8_t dma_index = 0;
    uint8_t dma_tick = 0;
    uint8_t dma_delay = 0;
    void step_dma();
    uint8_t dma_read(uint16_t address);

    // dmg oam corruption bug, the ppu is mid scan and the bus fight mangles a row
    uint16_t oam_word(int row, int word) const;
    void set_oam_word(int row, int word, uint16_t value);
    void oam_corrupt(int row, bool read);
    void oam_corrupt_read_inc(int row);

    void write_mbc1(uint16_t address, uint8_t value);
    void write_mbc2(uint16_t address, uint8_t value);
    void write_mbc3(uint16_t address, uint8_t value);
    void write_mbc5(uint16_t address, uint8_t value);
    void sync_div(uint8_t value);
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);
    // the ppu and the timer own these registers, they do not go through the cpu bus
    uint8_t read_direct(uint16_t address) const {
        if (address >= 0x8000 && address <= 0x9FFF)
            return vram[vram_bank][address - 0x8000];
        if (address >= 0xC000 && address <= 0xCFFF)
            return wram[0][address - 0xC000];
        if (address >= 0xD000 && address <= 0xDFFF)
            return wram[wram_bank][address - 0xD000];
        return data[address];
    }
    void write_direct(uint16_t address, uint8_t value) {
        if (address >= 0x8000 && address <= 0x9FFF)
            vram[vram_bank][address - 0x8000] = value;
        else if (address >= 0xC000 && address <= 0xCFFF)
            wram[0][address - 0xC000] = value;
        else if (address >= 0xD000 && address <= 0xDFFF)
            wram[wram_bank][address - 0xD000] = value;
        else
            data[address] = value;
    }
    // the ppu needs a specific bank, tile attributes always live in bank 1 while the
    // pixel data they describe can sit in either
    uint8_t vram_read(uint8_t bank, uint16_t address) const {
        return vram[bank & 1][address - 0x8000];
    }
    void load_rom(const std::vector<uint8_t>& rom_to_load);
};

#endif //GAMEBOY_EMU_MEMORY_H
