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
    bool power = true;
    uint8_t frame_step = 0;

    // channels 1 and 2 are the same square generator, only channel 1 has the sweep
    struct Square {
        bool enabled = false;
        bool dac_on = false;
        uint16_t freq_timer = 0;
        uint8_t duty_pos = 0;
        uint16_t length = 0;
        uint8_t volume = 0;
        uint8_t env_timer = 0;
        uint16_t shadow_freq = 0;
        uint8_t sweep_timer = 0;
        bool sweep_enabled = false;
        bool sweep_negated = false;   // a negate step since trigger arms the disable quirk
    };

    struct Wave {
        bool enabled = false;
        bool dac_on = false;
        uint16_t freq_timer = 0;
        uint8_t position = 0;
        uint16_t length = 0;
        uint8_t access = 0;   // cycles left in the window where the cpu can reach wave ram
    };

    struct Noise {
        bool enabled = false;
        bool dac_on = false;
        uint32_t freq_timer = 0;
        uint16_t lfsr = 0x7FFF;
        uint16_t length = 0;
        uint8_t volume = 0;
        uint8_t env_timer = 0;
    };

    Square ch1;
    Square ch2;
    Wave ch3;
    Noise ch4;

    // the dac idles at -1 rather than 0, the console filters that offset out with a
    // capacitor and without the same here every channel start would pop
    float hp_left = 0.0f;
    float hp_right = 0.0f;

    // every cycle's mix is summed and averaged into the output sample, taking one
    // instantaneous reading every 87 cycles aliases anything moving faster than that,
    // which is exactly what pcm through the wave channel does
    float acc_left = 0.0f;
    float acc_right = 0.0f;
    uint32_t acc_count = 0;

    // games switch several dacs on a few frames after boot, which is a real step of
    // most of full scale, ramping the first fifth of a second hides it
    static constexpr uint32_t kFadeSamples = kSampleRate * 2 / 5;
    uint32_t fade = kFadeSamples;

    uint16_t square_freq(int base) const;
    void trigger_square(Square& c, int base, bool with_sweep);
    void trigger_wave();
    void trigger_noise();
    uint16_t sweep_calc(bool commit);
    void length_write(uint16_t& length, bool& enabled, uint16_t max,
                      uint8_t before, uint8_t value);
    void clock_length();
    void clock_sweep();
    void clock_envelope_for(uint8_t& volume, uint8_t& timer, uint8_t nrx2);
    void clock_envelope();
    float square_output(const Square& c, int base) const;
    float wave_output() const;
    float noise_output() const;
    void mix_cycle();
    void emit_sample();
public:
    std::vector<int16_t> samples;
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);
    void step(uint8_t cycles);
    void frame_tick();
};

#endif //GAMEBOY_EMU_APU_H
