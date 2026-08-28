//
// Created by edi on 4/22/26.
//

#include "memory.h"
#include "apu.h"

void Memory::set_button(int button, bool pressed) {
    if (pressed)
        button_state &= ~(1 << button);
    else
        button_state |= (1 << button);
}

// the dma engine has its own view of the bus: everything from echo ram up reads out of
// work ram, so a transfer sourced at $FE00 or $FF00 copies $DE00 or $DF00
uint8_t Memory::dma_read(uint16_t address) {
    if (address >= 0xE000)
        address -= 0x2000;
    return read(address);
}

void Memory::step_dma() {
    if (!dma_active && dma_delay == 0) return;
    if (++dma_tick < 4) return;
    dma_tick = 0;
    if (dma_delay) {
        // a transfer already in flight keeps copying while the new one starts up
        if (dma_active && dma_index < 160) {
            data[0xFE00 + dma_index] = dma_read(dma_source + dma_index);
            dma_index++;
        }
        if (--dma_delay == 0) {
            dma_source = dma_pending_source;
            dma_index  = 0;
            dma_active = true;
        }
        return;
    }
    data[0xFE00 + dma_index] = dma_read(dma_source + dma_index);
    if (++dma_index == 160) dma_active = false;
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

// mbc2 picks its register from address bit 8 rather than from the address range, and
// its ram is 512 nibbles built into the mapper instead of sitting in the cartridge
void Memory::write_mbc2(uint16_t address, uint8_t value) {
    if (address >= 0x4000)
        return;

    if (address & 0x0100) {
        rom_bank = value & 0x0F;
        if (rom_bank == 0)
            rom_bank = 1;
    } else {
        ram_enabled = ((value & 0x0F) == 0x0A);
    }
}

// the clock counters are not plain binary: seconds and minutes are six bits wide and
// only carry when they roll past 59, so a game that parks 62 in there watches it count
// to 63 and wrap with no carry at all. hours are five bits, days nine plus a sticky
// overflow flag in the top bit of the high register
void Memory::rtc_step_second() {
    bool carry = (rtc[0] & 0x3F) == 59;
    rtc[0] = carry ? 0 : (uint8_t)((rtc[0] + 1) & 0x3F);
    if (!carry)
        return;

    carry = (rtc[1] & 0x3F) == 59;
    rtc[1] = carry ? 0 : (uint8_t)((rtc[1] + 1) & 0x3F);
    if (!carry)
        return;

    carry = (rtc[2] & 0x1F) == 23;
    rtc[2] = carry ? 0 : (uint8_t)((rtc[2] + 1) & 0x1F);
    if (!carry)
        return;

    uint16_t days = (uint16_t)rtc[3] | ((uint16_t)(rtc[4] & 0x01) << 8);
    days++;
    rtc[3] = days & 0xFF;
    rtc[4] = (rtc[4] & 0xFE) | ((days >> 8) & 0x01);
    if (days > 0x1FF)
        rtc[4] |= 0x80;   // the overflow flag stays up until the game clears it
}

void Memory::rtc_tick() {
    if (!has_rtc || (rtc[4] & 0x40))   // bit 6 halts the crystal
        return;
    if (++rtc_sub < 4194304)
        return;
    rtc_sub = 0;
    rtc_step_second();
}

// catching up after the console was off, done a second at a time so the carries and the
// overflow flag land exactly as they would have while it ran
void Memory::rtc_advance(uint64_t seconds) {
    if (!has_rtc || (rtc[4] & 0x40))
        return;
    // more than the counter can hold only ever leaves the overflow flag set, so there is
    // no point walking years of it
    if (seconds > 0x1FF * 86400ull + 86400ull) {
        rtc[4] |= 0x80;
        seconds %= 0x1FF * 86400ull;
    }
    while (seconds--)
        rtc_step_second();
}

uint8_t Memory::rtc_read() const {
    switch (rtc_select) {
        case 0x08: return rtc_latched[0] & 0x3F;
        case 0x09: return rtc_latched[1] & 0x3F;
        case 0x0A: return rtc_latched[2] & 0x1F;
        case 0x0B: return rtc_latched[3];
        case 0x0C: return rtc_latched[4] & 0xC1;
        default:   return 0xFF;
    }
}

void Memory::rtc_write(uint8_t value) {
    switch (rtc_select) {
        // writing the seconds also restarts the divider feeding them, so the game gets a
        // full second before the value it just set moves on
        case 0x08: rtc[0] = value & 0x3F; rtc_sub = 0; break;
        case 0x09: rtc[1] = value & 0x3F; break;
        case 0x0A: rtc[2] = value & 0x1F; break;
        case 0x0B: rtc[3] = value; break;
        case 0x0C: rtc[4] = value & 0xC1; break;
        default: return;
    }
    ram_dirty = true;
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
        // mbc30 carts carry eight ram banks, the plain part four, and the clock sits in
        // the same window one register at a time
        if (value <= 0x07) {
            ram_bank = value;
            rtc_select = 0;
        } else if (value >= 0x08 && value <= 0x0C) {
            rtc_select = value;
        }
    }

    // a zero followed by a one freezes the live counters into the copy the game reads
    if (address >= 0x6000 && address <= 0x7FFF) {
        if (rtc_last_latch == 0x00 && value == 0x01)
            for (int i = 0; i < 5; i++)
                rtc_latched[i] = rtc[i];
        rtc_last_latch = value;
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

// the colour hardware can run the port thirty two times faster, which moves the shift
// onto a much lower divider bit
bool Memory::serial_fast() const {
    return cgb_mode && (data[0xFF02] & 0x02);
}

void Memory::serial_shift() {
    // no cable means the far end is pulled high, so the byte fills with ones
    data[0xFF01] = (uint8_t)((data[0xFF01] << 1) | 1);
    if (--serial_bits == 0) {
        serial_active = false;
        data[0xFF02] &= ~0x80;
        data[0xFF0F] |= 0x08;
    }
}

void Memory::sync_div(uint8_t value) {
    data[0xFF04] = value;
}

// bits an io register does not implement read back as 1, and the whole of the io block
// that the dmg leaves unmapped reads 0xFF
static uint8_t io_read_mask(uint16_t address) {
    switch (address) {
        case 0xFF00: return 0xC0;   // P1, the two top bits are not wired
        case 0xFF01: return 0x00;   // SB
        case 0xFF02: return 0x7E;   // SC, the colour speed bit is added in Memory::read
        case 0xFF07: return 0xF8;   // TAC
        case 0xFF0F: return 0xE0;   // IF
        case 0xFF41: return 0x80;   // STAT
        default: break;
    }
    if (address >= 0xFF04 && address <= 0xFF06) return 0x00;
    if (address >= 0xFF40 && address <= 0xFF4B) return 0x00;
    return 0xFF;
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
            bank = mbc1_multicart ? (upper_bank << 4) : (upper_bank << 5);
        size_t num_banks = rom.size() / 0x4000;
        if (num_banks > 0)
            bank %= num_banks;
        return rom[bank * 0x4000 + address];
    }

    // this is the switchable bank
    if (address >= 0x4000 && address <= 0x7FFF) {
        uint16_t effective_bank;
        if (mbc_type == MbcType::MBC1)
            effective_bank = mbc1_multicart ? ((upper_bank << 4) | (rom_bank & 0x0F))
                                            : ((upper_bank << 5) | (rom_bank & 0x1F));
        else
            effective_bank = rom_bank;  // MBC3 and MBC5 use rom_bank directly

        size_t num_banks = rom.size() / 0x4000;
        if (num_banks > 0)
            effective_bank %= num_banks;
        return rom[effective_bank * 0x4000 + (address - 0x4000)];
    }

    // addresses for ram banking
    if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled)
            return 0xFF;
        // a clock register is mapped in place of the ram bank, and a timer cartridge can
        // carry no ram at all
        if (has_rtc && rtc_select >= 0x08 && rtc_select <= 0x0C)
            return rtc_read();
        if (external_ram.empty())
            return 0xFF;
        if (mbc_type == MbcType::MBC2)
            return external_ram[(address - 0xA000) & 0x01FF] | 0xF0;
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

    if (address >= 0x8000 && address <= 0x9FFF)
        return vram[vram_bank][address - 0x8000];
    if (address >= 0xC000 && address <= 0xCFFF)
        return wram[0][address - 0xC000];
    if (address >= 0xD000 && address <= 0xDFFF)
        return wram[wram_bank][address - 0xD000];

    if (cgb_enabled) {
        switch (address) {
            case 0xFF4F: return vram_bank | 0xFE;
            case 0xFF68: return data[0xFF68] | 0x40;
            case 0xFF6A: return data[0xFF6A] | 0x40;
            case 0xFF72: return data[0xFF72];
            case 0xFF73: return data[0xFF73];
            case 0xFF75: return data[0xFF75] | 0x8F;
            case 0xFF76: return 0x00;
            case 0xFF77: return 0x00;
            default: break;
        }
    }

    if (cgb_mode) {
        switch (address) {
            case 0xFF4D:
                return (double_speed ? 0x80 : 0x00)
                     | (speed_switch_armed ? 0x01 : 0x00) | 0x7E;
            // bit 7 clear reports a transfer still in flight, the low bits count the
            // blocks that are left
            case 0xFF55:
                return hdma_running ? (uint8_t)((hdma_left - 1) & 0x7F) : 0xFF;
            case 0xFF69: return bg_palette[data[0xFF68] & 0x3F];
            case 0xFF6B: return obj_palette[data[0xFF6A] & 0x3F];
            case 0xFF70: return (data[0xFF70] & 0x07) | 0xF8;
            case 0xFF74: return data[0xFF74];
            default: break;
        }
    }

    // the colour hardware wires up a transfer speed bit the dmg leaves floating
    if (address == 0xFF02)
        return data[0xFF02] | (cgb_mode ? 0x7C : 0x7E);

    if (address >= 0xFF00 && address <= 0xFF7F)
        return data[address] | io_read_mask(address);

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
            case MbcType::MBC2: write_mbc2(address, value); break;
            case MbcType::MBC3: write_mbc3(address, value); break;
            case MbcType::MBC5: write_mbc5(address, value); break;
            // NONE means there is nothing to bank
            default: break;
        }
        return;
    }

    // cartridge external ram only writable when enabled
    if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled)
            return;
        if (has_rtc && rtc_select >= 0x08 && rtc_select <= 0x0C) {
            rtc_write(value);
            return;
        }
        if (external_ram.empty())
            return;
        if (mbc_type == MbcType::MBC2) {
            external_ram[(address - 0xA000) & 0x01FF] = value & 0x0F;
            ram_dirty = true;
            return;
        }
        uint8_t bank;
        if (mbc_type == MbcType::MBC1)
            bank = (banking_mode == 1) ? upper_bank : 0;
        else
            bank = ram_bank;
        size_t offset = bank * 0x2000 + (address - 0xA000);
        offset %= external_ram.size();
        external_ram[offset] = value;
        ram_dirty = true;
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
        dma_pending_source = value << 8;
        dma_delay = 2;
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

    if (cgb_mode) {
        switch (address) {
            case 0xFF4D: speed_switch_armed = value & 0x01; return;
            case 0xFF4F: vram_bank = value & 0x01; return;
            case 0xFF51: hdma_src = (hdma_src & 0x00FF) | (uint16_t)(value << 8); return;
            case 0xFF52: hdma_src = (hdma_src & 0xFF00) | (value & 0xF0); return;
            case 0xFF53: hdma_dst = (hdma_dst & 0x00FF) | (uint16_t)((value & 0x1F) << 8); return;
            case 0xFF54: hdma_dst = (hdma_dst & 0xFF00) | (value & 0xF0); return;
            case 0xFF55: {
                uint8_t blocks = (value & 0x7F) + 1;
                if (value & 0x80) {
                    hdma_left = blocks;
                    hdma_hblank = true;
                    hdma_running = true;
                } else if (hdma_running && hdma_hblank) {
                    // clearing bit 7 mid transfer cancels the rest of it
                    hdma_running = false;
                    hdma_hblank = false;
                } else {
                    hdma_left = blocks;
                    hdma_hblank = false;
                    hdma_running = true;
                    while (hdma_running)
                        hdma_block();
                }
                return;
            }
            case 0xFF69: {
                uint8_t index = data[0xFF68] & 0x3F;
                bg_palette[index] = value;
                if (data[0xFF68] & 0x80)
                    data[0xFF68] = 0x80 | ((index + 1) & 0x3F);
                return;
            }
            case 0xFF6B: {
                uint8_t index = data[0xFF6A] & 0x3F;
                obj_palette[index] = value;
                if (data[0xFF6A] & 0x80)
                    data[0xFF6A] = 0x80 | ((index + 1) & 0x3F);
                return;
            }
            case 0xFF70:
                wram_bank = (value & 0x07) ? (value & 0x07) : 1;
                data[0xFF70] = value;
                return;
            default: break;
        }
    }

    if (address >= 0x8000 && address <= 0x9FFF) {
        vram[vram_bank][address - 0x8000] = value;
        return;
    }
    if (address >= 0xC000 && address <= 0xCFFF) {
        wram[0][address - 0xC000] = value;
        return;
    }
    if (address >= 0xD000 && address <= 0xDFFF) {
        wram[wram_bank][address - 0xD000] = value;
        return;
    }

    data[address] = value;

    if (address == 0xFF02) {
        // only a transfer clocked from this side can finish on its own, one waiting on
        // an absent cable's clock hangs exactly as it would on hardware
        if ((value & 0x81) == 0x81) {
            serial_active = true;
            serial_bits = 8;
        } else {
            serial_active = false;
        }
    }

    if (address == 0xFF05)
        tima_written = true;

    // serial output, test roms print their results through it
    if (address == 0xFF01)
        serial_buffer.push_back(static_cast<char>(value));
}

namespace {
    // the cgb boot rom colours a monochrome cartridge from tables keyed on the sum of
    // its title bytes, these are those tables, transcribed from the boot rom itself

    const uint16_t kBootColors[128] = {
        0x7FFF, 0x32BF, 0x00D0, 0x0000, 0x639F, 0x4279, 0x15B0, 0x04CB,
        0x7FFF, 0x6E31, 0x454A, 0x0000, 0x7FFF, 0x1BEF, 0x0200, 0x0000,
        0x7FFF, 0x421F, 0x1CF2, 0x0000, 0x7FFF, 0x5294, 0x294A, 0x0000,
        0x7FFF, 0x03FF, 0x012F, 0x0000, 0x7FFF, 0x03EF, 0x01D6, 0x0000,
        0x7FFF, 0x42B5, 0x3DC8, 0x0000, 0x7E74, 0x03FF, 0x0180, 0x0000,
        0x67FF, 0x77AC, 0x1A13, 0x2D6B, 0x7ED6, 0x4BFF, 0x2175, 0x0000,
        0x53FF, 0x4A5F, 0x7E52, 0x0000, 0x4FFF, 0x7ED2, 0x3A4C, 0x1CE0,
        0x03ED, 0x7FFF, 0x255F, 0x0000, 0x036A, 0x021F, 0x03FF, 0x7FFF,
        0x7FFF, 0x01DF, 0x0112, 0x0000, 0x231F, 0x035F, 0x00F2, 0x0009,
        0x7FFF, 0x03EA, 0x011F, 0x0000, 0x299F, 0x001A, 0x000C, 0x0000,
        0x7FFF, 0x027F, 0x001F, 0x0000, 0x7FFF, 0x03E0, 0x0206, 0x0120,
        0x7FFF, 0x7EEB, 0x001F, 0x7C00, 0x7FFF, 0x3FFF, 0x7E00, 0x001F,
        0x7FFF, 0x03FF, 0x001F, 0x0000, 0x03FF, 0x001F, 0x000C, 0x0000,
        0x7FFF, 0x033F, 0x0193, 0x0000, 0x0000, 0x4200, 0x037F, 0x7FFF,
        0x7FFF, 0x7E8C, 0x7C00, 0x0000, 0x7FFF, 0x1BEF, 0x6180, 0x0000,
        0x7FFF, 0x7FEA, 0x7D5F, 0x0000, 0x4778, 0x3290, 0x1D87, 0x0861,
    };

    const uint8_t kTitleChecksums[94] = {
        0x00, 0x88, 0x16, 0x36, 0xD1, 0xDB, 0xF2, 0x3C, 0x8C, 0x92, 0x3D, 0x5C, 0x58, 0xC9, 0x3E,
        0x70, 0x1D, 0x59, 0x69, 0x19, 0x35, 0xA8, 0x14, 0xAA, 0x75, 0x95, 0x99, 0x34, 0x6F, 0x15,
        0xFF, 0x97, 0x4B, 0x90, 0x17, 0x10, 0x39, 0xF7, 0xF6, 0xA2, 0x49, 0x4E, 0x43, 0x68, 0xE0,
        0x8B, 0xF0, 0xCE, 0x0C, 0x29, 0xE8, 0xB7, 0x86, 0x9A, 0x52, 0x01, 0x9D, 0x71, 0x9C, 0xBD,
        0x5D, 0x6D, 0x67, 0x3F, 0x6B, 0xB3, 0x46, 0x28, 0xA5, 0xC6, 0xD3, 0x27, 0x61, 0x18, 0x66,
        0x6A, 0xBF, 0x0D, 0xF4, 0xB3, 0x46, 0x28, 0xA5, 0xC6, 0xD3, 0x27, 0x61, 0x18, 0x66, 0x6A,
        0xBF, 0x0D, 0xF4, 0xB3,
    };

    const uint8_t kPalettePerChecksum[94] = {
        0, 4, 5, 35, 34, 3, 31, 15, 10, 5, 19, 36, 7, 37, 30,
        44, 21, 32, 31, 20, 5, 33, 13, 14, 5, 29, 5, 18, 9, 3,
        2, 26, 25, 25, 41, 42, 26, 45, 42, 45, 36, 38, 26, 42, 30,
        41, 34, 34, 5, 42, 6, 5, 33, 25, 42, 42, 40, 2, 16, 25,
        42, 42, 5, 0, 39, 36, 22, 25, 6, 32, 12, 36, 11, 39, 18,
        39, 24, 31, 50, 17, 46, 6, 27, 0, 47, 41, 41, 0, 0, 19,
        34, 23, 18, 29,
    };

    // three colour offsets per entry, obj0 then obj1 then bg
    const uint8_t kPaletteCombos[55][3] = {
        { 16, 16,116},
        { 72, 72, 72},
        { 80, 80, 80},
        { 96, 96, 96},
        { 36, 36, 36},
        {  0,  0,  0},
        {108,108,108},
        { 20, 20, 20},
        { 48, 48, 48},
        {104,104,104},
        { 64, 32, 32},
        { 16,112,112},
        { 16,  8,  8},
        { 12, 16, 16},
        { 16,116,116},
        {112, 16,112},
        {  8, 68,  8},
        { 64, 64, 32},
        { 16, 16, 28},
        { 16, 16, 72},
        { 16, 16, 80},
        { 76, 76, 36},
        { 15, 15, 44},
        { 68, 68,  8},
        { 16, 16,  8},
        { 16, 16, 12},
        {112,112,  0},
        { 12, 12,  0},
        {  0,  0,  4},
        { 72, 88, 72},
        { 80, 88, 80},
        { 96, 88, 96},
        { 64, 88, 32},
        { 68, 16, 52},
        {111,  0, 56},
        {111, 16, 60},
        { 76, 91, 36},
        { 64,112, 40},
        { 16, 92,112},
        { 68, 88,  8},
        { 16,  0,  8},
        { 16,112, 12},
        {112, 12,  0},
        { 12,112, 16},
        { 84,112, 16},
        { 12,112,  0},
        {100, 12,112},
        {  0,112, 32},
        { 16, 12,112},
        {112, 12, 24},
        { 16,112,116},
        {120,120,120},
        {124,124,124},
        {112, 16,  4},
        {  0,  0,  8},
    };

    const char kDup4thLetter[] = "BEFAARBEKEK R-URAR INAILICE R";

    // the first 65 checksums are unique, the rest collide and are told apart by the
    // fourth character of the title
    constexpr int kFirstDuplicate = 65;
}

// a monochrome cartridge gets colour on cgb hardware, picked from the title checksum
void Memory::apply_compat_palette() {
    compat_palette = false;
    if (rom.size() < 0x0150)
        return;

    // only cartridges nintendo published are looked up, the rest take the default entry
    bool nintendo = (rom[0x014B] == 0x01) ||
                    (rom[0x014B] == 0x33 && rom[0x0144] == '0' && rom[0x0145] == '1');

    int combo = 0;
    if (nintendo) {
        uint8_t sum = 0;
        for (int i = 0x0134; i <= 0x0143; i++)
            sum += rom[i];

        int found = -1;
        for (int i = 0; i < 94; i++) {
            if (kTitleChecksums[i] != sum)
                continue;
            // past the unique run a checksum is shared, the title's fourth character
            // is what tells those cartridges apart
            if (i < kFirstDuplicate || kDup4thLetter[i - kFirstDuplicate] == (char)rom[0x0137]) {
                found = i;
                break;
            }
        }
        if (found >= 0)
            combo = kPalettePerChecksum[found] & 0x7F;
    }

    const uint8_t* off = kPaletteCombos[combo];
    for (int i = 0; i < 4; i++) {
        compat_obj[0][i] = kBootColors[off[0] + i];
        compat_obj[1][i] = kBootColors[off[1] + i];
        compat_bg[i]     = kBootColors[off[2] + i];
    }
    compat_palette = true;
}

// one 16 byte chunk, hblank transfers move a chunk per line while a general purpose
// one drains every chunk in a single go
void Memory::hdma_block() {
    if (!hdma_running)
        return;
    // the block costs the cpu eight m-cycles, which in double speed is twice as many of
    // its own t-cycles for the same amount of real time
    dma_stall += double_speed ? 64 : 32;
    for (int i = 0; i < 16; i++)
        vram[vram_bank][(hdma_dst + i) & 0x1FFF] = read(hdma_src + i);
    hdma_src += 16;
    hdma_dst += 16;
    if (hdma_left > 0)
        hdma_left--;
    if (hdma_left == 0) {
        hdma_running = false;
        hdma_hblank = false;
    }
}

void Memory::load_rom(const std::vector<uint8_t>& rom_to_load) {
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

    // cartridge types that back their ram with a battery, these are the ones worth
    // writing out to a .sav
    switch (mbc) {
        case 0x03:
        case 0x06:
        case 0x09:
        case 0x0D:
        case 0x0F:
        case 0x10:
        case 0x13:
        case 0x1B:
        case 0x1E:
        case 0x22:
        case 0xFF:
            has_battery = true;
            break;
        default:
            has_battery = false;
            break;
    }

    if (mbc >= 0x05 && mbc <= 0x06)
        external_ram.resize(512);

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

    // only the two mbc3 timer variants carry a crystal
    has_rtc = (mbc == 0x0F || mbc == 0x10);

    // the apu behaves differently on the two consoles, and that follows the hardware
    // rather than the cartridge
    if (apu)
        apu->cgb = cgb_enabled;

    // bit 7 of the cgb flag marks a cartridge that knows about the colour hardware,
    // anything else keeps running as a dmg
    // a multicart carries several games behind a 4 bit bank number, and announces
    // itself only by repeating the nintendo logo at each game's header
    mbc1_multicart = false;
    if (mbc_type == MbcType::MBC1 && rom.size() == 0x100000) {
        int logos = 0;
        for (size_t base = 0; base < rom.size(); base += 0x40000) {
            bool match = true;
            for (int i = 0; i < 0x30 && match; i++)
                match = rom[base + 0x104 + i] == rom[0x104 + i];
            if (match)
                logos++;
        }
        mbc1_multicart = logos >= 3;
    }

    cgb_mode = cgb_enabled && (rom.size() > 0x0143) && (rom[0x0143] & 0x80);

    vram_bank = 0;
    wram_bank = 1;
    double_speed = false;
    speed_switch_armed = false;
    hdma_running = false;
    hdma_hblank = false;
    hdma_left = 0;

    if (cgb_mode) {
        // both palette blocks power on white so a game that draws before uploading
        // its own colours does not show black
        for (int i = 0; i < 64; i++) {
            bg_palette[i]  = 0xFF;
            obj_palette[i] = 0xFF;
        }
        data[0xFF4D] = 0x7E;
        data[0xFF4F] = 0xFE;
        data[0xFF68] = 0xC0;
        data[0xFF6A] = 0xC0;
        data[0xFF70] = 0xF9;
    }

    // the boot rom leaves the palette indices where writing the compatibility set left them
    if (cgb_enabled && !cgb_mode) {
        data[0xFF68] = 0xC8;
        data[0xFF6A] = 0xD0;
    }

    if (!cgb_mode && dmg_colorize)
        apply_compat_palette();

    data[0xFF00] = cgb_enabled ? 0xFF : 0xCF; // P1
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
