//
// Created by edi on 8/22/26.
//

#ifndef GAMEBOY_EMU_APU_H
#define GAMEBOY_EMU_APU_H

#include <cstdint>
#include <array>
#include <vector>

class Apu {
    private:
        std::array<uint8_t, 0x17> registers{};
        std::array<uint8_t, 0x10> wave_ram{};
        static constexpr uint32_t kCpuHz = 4194304;
        static constexpr uint32_t kSampleRate = 48000;
        uint32_t sample_clock = 0;
        uint16_t tone_phase = 0;
    public:
        std::vector<int16_t> samples;
        uint8_t read(uint16_t address);
        void write(uint16_t address, uint8_t value);
        void step(uint16_t cycles);
};

#endif //GAMEBOY_EMU_APU_H