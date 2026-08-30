//
// Created by edi on 8/22/26.
//

#include "apu.h"

static constexpr uint8_t kReadMask[0x17] = {
    0x80, 0x3F, 0x00, 0xFF, 0xBF,
    0xFF, 0x3F, 0x00, 0xFF, 0xBF,
    0x7F, 0xFF, 0x9F, 0xFF, 0xBF,
    0xFF, 0xFF, 0x00, 0x00, 0xBF,
    0x00, 0x00, 0x70
};

static constexpr uint8_t kDuty[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0}
};

// channel 3 shifts its 4 bit sample right by this much, index is the two bit volume code
static constexpr uint8_t kWaveShift[4] = {4, 0, 1, 2};

// channel 4 timer period is one of these divided down, index is the low three bits of NR43
static constexpr uint16_t kNoiseDivisor[8] = {8, 16, 32, 48, 64, 80, 96, 112};

// register indexes into registers[], the two squares sit five apart so one set of
// helpers can drive either by taking the base
enum {
    CH1 = 0x00, CH2 = 0x05,
    NR10 = 0x00, NR30 = 0x0A, NR32 = 0x0C, NR34 = 0x0E,
    NR42 = 0x11, NR43 = 0x12, NR44 = 0x13,
    NR50 = 0x14, NR51 = 0x15
};

// the boot rom leaves a few sound registers set, and channel 1's dac switched on
Apu::Apu() {
    registers[CH1 + 1] = 0x80;   // NR11, duty 2
    registers[CH1 + 2] = 0xF3;   // NR12, volume 15 with a decreasing envelope
    registers[NR50]    = 0x77;
    registers[NR51]    = 0xF3;
    ch1.dac_on = true;
    ch1.enabled = true;          // the boot rom's jingle leaves channel 1 running
}

uint8_t Apu::read(uint16_t address) {
    if (address >= 0xFF30) {
        // while channel 3 runs the cpu never sees the byte it asked for, it sees the one
        // the channel is on. a dmg only allows that in the moment of the channel's own
        // read and answers 0xFF the rest of the time, the cgb allows it always
        if (ch3.enabled && ch3.dac_on) {
            if (cgb || ch3.access > 0)
                return wave_ram[ch3.position >> 1];
            return 0xFF;
        }
        return wave_ram[address - 0xFF30];
    }
    if (address >= 0xFF27)
        return 0xFF;
    if (address == 0xFF26)
        return (power ? 0x80 : 0x00) | 0x70
             | (ch1.enabled ? 0x01 : 0x00) | (ch2.enabled ? 0x02 : 0x00)
             | (ch3.enabled ? 0x04 : 0x00) | (ch4.enabled ? 0x08 : 0x00);
    return registers[address - 0xFF10] | kReadMask[address - 0xFF10];
}

void Apu::write(uint16_t address, uint8_t value) {
    if (address >= 0xFF30) {
        if (ch3.enabled && ch3.dac_on) {
            if (cgb || ch3.access > 0)
                wave_ram[ch3.position >> 1] = value;
            return;
        }
        wave_ram[address - 0xFF30] = value;
        return;
    }
    if (address >= 0xFF27)
        return;
    if (address == 0xFF26) {
        bool on = value & 0x80;
        if (!on) {
            // the length counters are the one thing a dmg carries through a power cycle,
            // the cgb clears them with everything else
            uint8_t l1 = ch1.length, l2 = ch2.length, l4 = ch4.length;
            uint16_t l3 = ch3.length;
            registers.fill(0);
            ch1 = Square{};
            ch2 = Square{};
            ch3 = Wave{};
            ch4 = Noise{};
            if (!cgb) {
                ch1.length = l1;
                ch2.length = l2;
                ch3.length = l3;
                ch4.length = l4;
            }
        } else if (!power) {
            frame_step = 0;
            ch1.duty_pos = 0;
            ch2.duty_pos = 0;
        }
        power = on;
        return;
    }
    if (!power) {
        // and a dmg still lets them be loaded while it is off, the cgb does not
        if (cgb)
            return;
        switch (address) {
            case 0xFF11: ch1.length = 64 - (value & 0x3F); break;
            case 0xFF16: ch2.length = 64 - (value & 0x3F); break;
            case 0xFF1B: ch3.length = 256 - value; break;
            case 0xFF20: ch4.length = 64 - (value & 0x3F); break;
            default: break;
        }
        return;
    }

    uint8_t before = registers[address - 0xFF10];
    registers[address - 0xFF10] = value;

    switch (address) {
        case 0xFF11: ch1.length = 64 - (value & 0x3F); break;
        case 0xFF16: ch2.length = 64 - (value & 0x3F); break;
        case 0xFF1B: ch3.length = 256 - value; break;
        case 0xFF20: ch4.length = 64 - (value & 0x3F); break;

        // the top five bits feed the dac, all zero cuts it and the channel with it
        case 0xFF12:
            ch1.dac_on = (value & 0xF8) != 0;
            if (!ch1.dac_on) ch1.enabled = false;
            break;
        case 0xFF17:
            ch2.dac_on = (value & 0xF8) != 0;
            if (!ch2.dac_on) ch2.enabled = false;
            break;
        case 0xFF21:
            ch4.dac_on = (value & 0xF8) != 0;
            if (!ch4.dac_on) ch4.enabled = false;
            break;
        case 0xFF1A:
            ch3.dac_on = (value & 0x80) != 0;
            if (!ch3.dac_on) ch3.enabled = false;
            break;

        case 0xFF14:
            length_write(ch1.length, ch1.enabled, 64, before, value);
            if (value & 0x80) trigger_square(ch1, CH1, true);
            break;
        case 0xFF19:
            length_write(ch2.length, ch2.enabled, 64, before, value);
            if (value & 0x80) trigger_square(ch2, CH2, false);
            break;
        case 0xFF1E:
            length_write(ch3.length, ch3.enabled, 256, before, value);
            if (value & 0x80) trigger_wave();
            break;
        case 0xFF23:
            length_write(ch4.length, ch4.enabled, 64, before, value);
            if (value & 0x80) trigger_noise();
            break;

        // clearing negate after a negate calculation has happened kills the channel
        case 0xFF10:
            if (ch1.sweep_negated && !(value & 0x08))
                ch1.enabled = false;
            break;
        default: break;
    }
}

uint16_t Apu::square_freq(int base) const {
    return ((registers[base + 4] & 0x07) << 8) | registers[base + 3];
}

void Apu::trigger_square(Square& c, int base, bool with_sweep) {
    c.enabled = c.dac_on;
    c.freq_timer = (2048 - square_freq(base)) * 4;
    c.env_timer = registers[base + 2] & 0x07;
    c.volume = registers[base + 2] >> 4;

    if (!with_sweep)
        return;

    // the sweep runs off its own copy of the frequency, and a trigger with a shift set
    // does one overflow check straight away which can kill the channel before it sounds
    uint8_t period = (registers[NR10] >> 4) & 0x07;
    uint8_t shift = registers[NR10] & 0x07;
    c.shadow_freq = square_freq(base);
    c.sweep_timer = period ? period : 8;
    c.sweep_enabled = (period != 0 || shift != 0);
    c.sweep_negated = false;
    if (shift != 0)
        sweep_calc(false);
}

void Apu::trigger_wave() {
    // retriggering just as the channel reaches for its next byte smears that byte over
    // the first four, a dmg fault the colour hardware does not have
    if (!cgb && ch3.enabled && ch3.freq_timer <= 2) {
        uint8_t pos = ((ch3.position + 1) & 0x1F) >> 1;
        if (pos < 4) {
            wave_ram[0] = wave_ram[pos];
        } else {
            pos &= 0x0C;
            for (int i = 0; i < 4; i++)
                wave_ram[i] = wave_ram[pos + i];
        }
    }
    ch3.enabled = ch3.dac_on;
    // the channel does not start reading straight away, the first sample is six cycles
    // late and every later one follows from there
    ch3.freq_timer = (2048 - (((registers[NR34] & 0x07) << 8) | registers[0x0D])) * 2 + 6;
    ch3.position = 0;
}

void Apu::trigger_noise() {
    ch4.enabled = ch4.dac_on;
    ch4.env_timer = registers[NR42] & 0x07;
    ch4.volume = registers[NR42] >> 4;
    ch4.lfsr = 0x7FFF;
    ch4.freq_timer = (uint32_t)kNoiseDivisor[registers[NR43] & 0x07] << (registers[NR43] >> 4);
}

// returns the next sweep frequency and disables the channel if it runs past 2047,
// commit writes the result back so the tone actually moves
uint16_t Apu::sweep_calc(bool commit) {
    uint8_t shift = registers[NR10] & 0x07;
    uint16_t delta = ch1.shadow_freq >> shift;
    bool negate = registers[NR10] & 0x08;
    if (negate)
        ch1.sweep_negated = true;
    uint16_t next = negate ? ch1.shadow_freq - delta : ch1.shadow_freq + delta;
    if (next > 2047) {
        ch1.enabled = false;
        return next;
    }
    if (commit && shift != 0) {
        ch1.shadow_freq = next;
        registers[CH1 + 3] = next & 0xFF;
        registers[CH1 + 4] = (registers[CH1 + 4] & 0xF8) | ((next >> 8) & 0x07);
    }
    return next;
}

// switching length enable on during the half of the sequencer period where the next
// step will not clock length gives the counter one extra clock straight away
void Apu::length_write(uint16_t& length, bool& enabled, uint16_t max,
                       uint8_t before, uint8_t value) {
    bool first_half = (frame_step & 1) == 1;
    bool was = before & 0x40;
    bool now = value & 0x40;

    if (!was && now && first_half && length > 0) {
        length--;
        if (length == 0 && !(value & 0x80))
            enabled = false;
    }
    if ((value & 0x80) && length == 0) {
        length = max;
        if (now && first_half)
            length--;
    }
}

void Apu::clock_length() {
    if ((registers[CH1 + 4] & 0x40) && ch1.length > 0 && --ch1.length == 0)
        ch1.enabled = false;
    if ((registers[CH2 + 4] & 0x40) && ch2.length > 0 && --ch2.length == 0)
        ch2.enabled = false;
    if ((registers[NR34] & 0x40) && ch3.length > 0 && --ch3.length == 0)
        ch3.enabled = false;
    if ((registers[NR44] & 0x40) && ch4.length > 0 && --ch4.length == 0)
        ch4.enabled = false;
}

void Apu::clock_sweep() {
    if (ch1.sweep_timer > 0)
        ch1.sweep_timer--;
    if (ch1.sweep_timer != 0)
        return;

    uint8_t period = (registers[NR10] >> 4) & 0x07;
    ch1.sweep_timer = period ? period : 8;
    if (!ch1.sweep_enabled || period == 0)
        return;

    // a successful step is followed by a second check that only tests for overflow
    sweep_calc(true);
    sweep_calc(false);
}

void Apu::clock_envelope_for(uint8_t& volume, uint8_t& timer, uint8_t nrx2) {
    uint8_t period = nrx2 & 0x07;
    if (period == 0)
        return;
    if (timer > 0 && --timer == 0) {
        timer = period;
        bool up = nrx2 & 0x08;
        if (up && volume < 15)
            volume++;
        else if (!up && volume > 0)
            volume--;
    }
}

void Apu::clock_envelope() {
    clock_envelope_for(ch1.volume, ch1.env_timer, registers[CH1 + 2]);
    clock_envelope_for(ch2.volume, ch2.env_timer, registers[CH2 + 2]);
    clock_envelope_for(ch4.volume, ch4.env_timer, registers[NR42]);
}

// eight steps at 512 hz: length on the evens, sweep on 2 and 6, envelope on 7
void Apu::frame_tick() {
    if (!power)
        return;
    switch (frame_step) {
        case 0: case 4: clock_length(); break;
        case 2: case 6: clock_length(); clock_sweep(); break;
        case 7: clock_envelope(); break;
        default: break;
    }
    frame_step = (frame_step + 1) & 0x07;
}

float Apu::square_output(const Square& c, int base) const {
    if (!c.enabled || !c.dac_on)
        return 0.0f;
    uint8_t level = kDuty[registers[base + 1] >> 6][c.duty_pos] ? c.volume : 0;
    return level / 7.5f - 1.0f;
}

float Apu::wave_output() const {
    if (!ch3.enabled || !ch3.dac_on)
        return 0.0f;
    uint8_t byte = wave_ram[ch3.position >> 1];
    uint8_t nibble = (ch3.position & 1) ? (byte & 0x0F) : (byte >> 4);
    uint8_t level = nibble >> kWaveShift[(registers[NR32] >> 5) & 0x03];
    return level / 7.5f - 1.0f;
}

float Apu::noise_output() const {
    if (!ch4.enabled || !ch4.dac_on)
        return 0.0f;
    uint8_t level = (~ch4.lfsr & 1) ? ch4.volume : 0;
    return level / 7.5f - 1.0f;
}

void Apu::mix_cycle() {
    float out[4] = {
        square_output(ch1, CH1),
        square_output(ch2, CH2),
        wave_output(),
        noise_output()
    };

    float left = 0.0f;
    float right = 0.0f;
    for (int i = 0; i < 4; i++) {
        if (registers[NR51] & (0x10 << i)) left += out[i];
        if (registers[NR51] & (0x01 << i)) right += out[i];
    }

    acc_left  += left  * ((((registers[NR50] >> 4) & 0x07) + 1) / 8.0f);
    acc_right += right * (((registers[NR50] & 0x07) + 1) / 8.0f);
    acc_count++;
}

void Apu::emit_sample() {
    float scale = acc_count ? 1.0f / (float)acc_count : 0.0f;
    float left  = acc_left * scale;
    float right = acc_right * scale;
    acc_left = acc_right = 0.0f;
    acc_count = 0;

    float lo = left - hp_left;
    hp_left = left - lo * 0.996f;
    float ro = right - hp_right;
    hp_right = right - ro * 0.996f;

    float gain = 5000.0f;
    if (fade > 0) {
        float f = (float)(kFadeSamples - fade) / (float)kFadeSamples;
        gain *= f * f;
        fade--;
    }

    int32_t li = (int32_t)(lo * gain);
    int32_t ri = (int32_t)(ro * gain);
    if (li > 32767) li = 32767; else if (li < -32768) li = -32768;
    if (ri > 32767) ri = 32767; else if (ri < -32768) ri = -32768;
    samples.push_back((int16_t)li);
    samples.push_back((int16_t)ri);
}

void Apu::step(uint8_t cycles) {
    while (cycles--) {
        if (ch1.freq_timer > 0 && --ch1.freq_timer == 0) {
            ch1.freq_timer = (2048 - square_freq(CH1)) * 4;
            ch1.duty_pos = (ch1.duty_pos + 1) & 0x07;
        }
        if (ch2.freq_timer > 0 && --ch2.freq_timer == 0) {
            ch2.freq_timer = (2048 - square_freq(CH2)) * 4;
            ch2.duty_pos = (ch2.duty_pos + 1) & 0x07;
        }
        if (ch3.access > 0)
            ch3.access--;
        if (ch3.freq_timer > 0 && --ch3.freq_timer == 0) {
            ch3.freq_timer = (2048 - (((registers[NR34] & 0x07) << 8) | registers[0x0D])) * 2;
            ch3.position = (ch3.position + 1) & 0x1F;
            ch3.access = 2;
        }
        if (ch4.freq_timer > 0 && --ch4.freq_timer == 0) {
            ch4.freq_timer = (uint32_t)kNoiseDivisor[registers[NR43] & 0x07] << (registers[NR43] >> 4);
            uint16_t feedback = (ch4.lfsr & 1) ^ ((ch4.lfsr >> 1) & 1);
            ch4.lfsr = (ch4.lfsr >> 1) | (feedback << 14);
            if (registers[NR43] & 0x08)
                ch4.lfsr = (ch4.lfsr & ~(uint16_t)0x0040) | (feedback << 6);
        }

        mix_cycle();

        sample_clock += kSampleRate;
        if (sample_clock >= kCpuHz) {
            sample_clock -= kCpuHz;
            emit_sample();
        }
    }
}

// every field the channels carry, so a state resumes mid note rather than restarting it
void Apu::save_state(state::Writer& w) const {
    w.bytes(registers.data(), registers.size());
    w.bytes(wave_ram.data(), wave_ram.size());
    w.raw(sample_clock);
    w.raw(power);
    w.raw(frame_step);
    w.raw(cgb);
    w.raw(ch1);
    w.raw(ch2);
    w.raw(ch3);
    w.raw(ch4);
    w.raw(hp_left);
    w.raw(hp_right);
    w.raw(acc_left);
    w.raw(acc_right);
    w.raw(acc_count);
    w.raw(fade);
}

void Apu::load_state(state::Reader& r) {
    r.bytes(registers.data(), registers.size());
    r.bytes(wave_ram.data(), wave_ram.size());
    r.raw(sample_clock);
    r.raw(power);
    r.raw(frame_step);
    r.raw(cgb);
    r.raw(ch1);
    r.raw(ch2);
    r.raw(ch3);
    r.raw(ch4);
    r.raw(hp_left);
    r.raw(hp_right);
    r.raw(acc_left);
    r.raw(acc_right);
    r.raw(acc_count);
    r.raw(fade);
    // whatever was queued belongs to the moment the state was taken, not to now
    samples.clear();
}
