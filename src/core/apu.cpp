//
// Created by edi on 8/22/26.
//

#include "apu.h"

uint8_t Apu::read(uint16_t address) {
    if (address >= 0xFF30) {
        return wave_ram[address - 0xFF30];
    } else {
        return registers[address - 0xFF10];
    }
}

void Apu::write(uint16_t address, uint8_t value) {
    if (address >= 0xFF30) {
        wave_ram[address - 0xFF30] = value;
    } else {
        registers[address - 0xFF10] = value;
    }
}

void Apu::step(uint16_t cycles) {
    while (cycles--) {
        sample_clock += kSampleRate;
        if (sample_clock >= kCpuHz) {
            sample_clock -= kCpuHz;
            int16_t v = (tone_phase / 55) % 2 ? 1600 : -1600;
            tone_phase = (tone_phase + 1) % 110;
            samples.push_back(v);
            samples.push_back(v);
        }
    }
}
