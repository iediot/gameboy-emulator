//
// Created by edi on 4/22/26.
//

#include <iostream>
#include <cstdio>
#include "memory.h"
#include "apu.h"

void Memory::write_mbc1(uint16_t address, uint8_t value) {
    if (address >= 0x0000 && address <= 0x1FFF) {
        // ram enabled if the low nibble of value is 0xA
        ram_enabled = ((value & 0x0F) == 0x0A);
    }

    if (address >= 0x2000 && address <= 0x3FFF) {
        rom_bank = value & 0x1F;
        if (rom_bank == 0)
            rom_bank = 1;
    }

    if (address >= 0x4000 && address <= 0x5FFF) {
        upper_bank = value & 0x03;
    }

    if (address >= 0x6000 && address <= 0x7FFF) {
        banking_mode = value & 0x01;
    }
}

void Memory::write_mbc3(uint16_t address, uint8_t value) {
    if (address >= 0x0000 && address <= 0x1FFF) {
        ram_enabled = ((value & 0x0F) == 0x0A);
    }
    if (address >= 0x2000 && address <= 0x3FFF) {
        rom_bank = value & 0x7F;
        // mbc3 can't either
        if (rom_bank == 0)
            rom_bank = 1;
    }
    if (address >= 0x4000 && address <= 0x5FFF) {
        if (value <= 0x03)
            ram_bank = value;
        // values 0x08-0x0C select RTC registers defer
    }
}

void Memory::write_mbc5(uint16_t address, uint8_t value) {
    if (address >= 0x0000 && address <= 0x1FFF) {
        ram_enabled = ((value & 0x0F) == 0x0A);
    }
    if (address >= 0x4000 && address <= 0x5FFF) {
        ram_bank = value & 0x0F;  // MBC5 supports up to 16 RAM banks
    }
    if (address >= 0x2000 && address <= 0x2FFF) { // keep bit 9 set low 8
        rom_bank = (rom_bank & 0x100) | value;
    }
    else if (address >= 0x3000 && address <= 0x3FFF) { // keep low 8 set bit 9
        rom_bank = (rom_bank & 0xFF) | ((value & 0x01) << 8);
    }
}

void Memory::set_button(int button, bool pressed) {
    if (pressed)
        button_state &= ~(1 << button);
    else
        button_state |= (1 << button);
}

void Memory::sync_div(uint8_t value) {
    data[0xFF04] = value;
}

uint16_t Memory::oam_word(int row, int word) const {
    uint16_t a = 0xFE00 + row * 8 + word * 2;
    return (uint16_t)(data[a] | (data[a + 1] << 8));
}

void Memory::set_oam_word(int row, int word, uint16_t value) {
    uint16_t a = 0xFE00 + row * 8 + word * 2;
    data[a] = value & 0xFF;
    data[a + 1] = value >> 8;
}

// oam is twenty rows of four words, the row being scanned gets its first word mangled
// with the row before it and the rest copied straight from it, row 0 is immune
void Memory::oam_corrupt(int row, bool read) {
    if (row <= 0 || row > 19)
        return;
    uint16_t a = oam_word(row, 0);
    uint16_t b = oam_word(row - 1, 0);
    uint16_t c = oam_word(row - 1, 2);
    set_oam_word(row, 0, read ? (uint16_t)(b | (a & c))
                              : (uint16_t)(((a ^ c) & (b ^ c)) ^ c));
    for (int w = 1; w < 4; w++)
        set_oam_word(row, w, oam_word(row - 1, w));
}

// a read that happens in the same step as the register increment mangles the preceding
// row first and smears it over its neighbours, then the ordinary read corruption lands
void Memory::oam_corrupt_read_inc(int row) {
    if (row >= 4 && row < 19) {
        uint16_t a = oam_word(row - 2, 0);
        uint16_t b = oam_word(row - 1, 0);
        uint16_t c = oam_word(row, 0);
        uint16_t d = oam_word(row - 1, 2);
        set_oam_word(row - 1, 0, (uint16_t)((b & (a | c | d)) | (a & c & d)));
        for (int w = 0; w < 4; w++) {
            uint16_t v = oam_word(row - 1, w);
            set_oam_word(row, w, v);
            set_oam_word(row - 2, w, v);
        }
    }
    oam_corrupt(row, true);
}

void Memory::step_dma() {
    if (!dma_active) return;
    if (++dma_tick < 4) return;
    dma_tick = 0;
    if (dma_delay) {
        dma_delay--; return;
    }
    data[0xFE00 + dma_index] = read(dma_source + dma_index);
    if (++dma_index == 160) dma_active = false;
}

uint8_t Memory::ppu_read(uint16_t address) const {
    return data[address];
}

uint8_t Memory::read(uint16_t address) {
    if (apu != nullptr && address >= 0xFF10 && address <= 0xFF3F) {
                return apu->read(address);
            }

    if (address >= 0xE000 && address <= 0xFDFF)
        address -= 0x2000;

    // bank 0
    if (address >= 0x0000 && address <= 0x3FFF) {
        uint16_t bank = 0;
        if (mbc_type == MbcType::MBC1 && banking_mode == 1)
            bank = (upper_bank << 5);
        size_t num_banks = rom.size() / 0x4000;
        if (num_banks > 0)
            bank %= num_banks;
        return rom[bank * 0x4000 + address];
    }

    // this is the switchable bank
    if (address >= 0x4000 && address <= 0x7FFF) {
        uint16_t effective_bank;
        if (mbc_type == MbcType::MBC1)
            effective_bank = (upper_bank << 5) | (rom_bank & 0x1F);
        else
            effective_bank = rom_bank;  // MBC3 and MBC5 use rom_bank directly

        size_t num_banks = rom.size() / 0x4000;
        if (num_banks > 0)
            effective_bank %= num_banks;
        return rom[effective_bank * 0x4000 + (address - 0x4000)];
    }

    // addresses for ram banking
    if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled || external_ram.empty())
            return 0xFF;
        uint8_t bank;
        if (mbc_type == MbcType::MBC1)
            bank = (banking_mode == 1) ? upper_bank : 0;
        else
            bank = ram_bank;
        size_t offset = bank * 0x2000 + (address - 0xA000);
        offset %= external_ram.size();
        return external_ram[offset];
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

    if (address >= 0xFE00 && address <= 0xFE9F && dma_active)
        return 0xFF;

    uint8_t mode = data[0xFF41] & 0x03;
    bool lcd_on  = data[0xFF40] & 0x80;
    if (lcd_on && mode == 3 && address >= 0x8000 && address <= 0x9FFF)
        return 0xFF;
    if (lcd_on && mode >= 2  && address >= 0xFE00 && address <= 0xFE9F)
        return 0xFF;

    return data[address];
}

void Memory::write(uint16_t address, uint8_t value) {
    if (apu != nullptr && address >= 0xFF10 && address <= 0xFF3F) {
                apu->write(address, value);
                return;
            }

    if (address >= 0xE000 && address <= 0xFDFF)
        address -= 0x2000;

    if (address < 0x8000) {
        switch (mbc_type) {
            case MbcType::MBC1: write_mbc1(address, value); break;
            case MbcType::MBC3: write_mbc3(address, value); break;
            case MbcType::MBC5: write_mbc5(address, value); break;
            // NONE and MBC2 means ignoring cartridge writes
            default: break;
        }
        return;
    }

    // cartridge external ram only writable when enabled
    if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled || external_ram.empty())
            return;
        uint8_t bank;
        if (mbc_type == MbcType::MBC1)
            bank = (banking_mode == 1) ? upper_bank : 0;
        else
            bank = ram_bank;
        size_t offset = bank * 0x2000 + (address - 0xA000);
        offset %= external_ram.size();
        external_ram[offset] = value;
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
        data[address] = value;
        dma_source = value << 8;
        dma_index  = 0;
        dma_tick   = 0;
        dma_delay  = 2;
        dma_active = true;
        return;
    }

    if (address >= 0xFE00 && address <= 0xFE9F && dma_active)
        return;

    // a blocked write is thrown away, the byte keeps whatever it already held, writing
    // 0xFF into it instead corrupts tiles and oam entries
    uint8_t mode = data[0xFF41] & 0x03;
    bool lcd_on  = data[0xFF40] & 0x80;
    if (lcd_on && mode == 3 && address >= 0x8000 && address <= 0x9FFF)
        return;
    if (lcd_on && mode >= 2  && address >= 0xFE00 && address <= 0xFE9F)
        return;

    data[address] = value;

    // serial output, test roms print their results through it
    if (address == 0xFF01)
        serial_buffer.push_back(static_cast<char>(value));
}

void Memory::loadRom(const std::vector<uint8_t>& rom_to_load) {
    rom = rom_to_load;
    mbc = rom[0x0147];
    uint8_t ram_size = rom[0x0149]; // ram size code
    switch (ram_size) {
        case 0: external_ram.resize(0 * 1024); break; // no ram
        // the following are in kb so that's the 1024
        case 1: external_ram.resize(2 * 1024); break; // rare quarter of a bank
        case 2: external_ram.resize(8 * 1024); break; // one bank
        case 3: external_ram.resize(32 * 1024); break; // 4 banks
        case 4: external_ram.resize(128 * 1024); break; // 16 banks
        case 5: external_ram.resize(64 * 1024); break; // 8 banks
    }

    if (mbc == 0x00)
        mbc_type = MbcType::NONE;
    else if (mbc >= 0x01 && mbc <= 0x03)
        mbc_type = MbcType::MBC1;
    else if (mbc >= 0x05 && mbc <= 0x06)
        mbc_type = MbcType::MBC2;
    else if (mbc >= 0x0F && mbc <= 0x13)
        mbc_type = MbcType::MBC3;
    else if (mbc >= 0x19 && mbc <= 0x1E)
        mbc_type = MbcType::MBC5;
    else
        mbc_type = MbcType::NONE;

    data[0xFF00] = 0xCF; // P1
    data[0xFF02] = 0x7E; // SC
    data[0xFF07] = 0xF8; // TAC
    data[0xFF0F] = 0xE1; // IF
    data[0xFF40] = 0x91; // LCDC
    
    data[0xFF41] = 0x85; // STAT
    data[0xFF46] = 0xFF; // DMA
    data[0xFF47] = 0xFC; // BGP
    data[0xFF48] = 0xFF; // OBP0
    data[0xFF49] = 0xFF; // OBP1
}