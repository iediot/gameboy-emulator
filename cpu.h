//
// Created by edi on 4/22/26.
//

#ifndef GAMEBOY_EMU_CPU_H
#define GAMEBOY_EMU_CPU_H


#include <cstdint>
#include "memory.h"


class Cpu {
private:
    Memory& mem;
public:
    // Registers
    uint8_t A = 0;
    uint8_t B = 0;
    uint8_t C = 0;
    uint8_t D = 0;
    uint8_t E = 0;
    uint8_t F = 0;
    uint8_t H = 0;
    uint8_t L = 0;
    uint16_t SP = 0;
    uint16_t PC = 0;

    static constexpr uint8_t FLAG_Z = 0x80; // 10000000
    static constexpr uint8_t FLAG_N = 0x40; // 01000000
    static constexpr uint8_t FLAG_H = 0x20; // 00100000
    static constexpr uint8_t FLAG_C = 0x10; // 00010000

    bool IME = false; // need to implement interrupts

    // Constructor
    Cpu(Memory& memory);

    // Helpers
    void set_flag(uint8_t flag, bool value);
    [[nodiscard]] uint16_t combine(uint8_t high, uint8_t low) const;
    void split(uint8_t& high, uint8_t& low, uint16_t value);
    [[nodiscard]] uint16_t af() const;
    void set_af(uint16_t value);
    [[nodiscard]] uint16_t bc() const;
    void set_bc(uint16_t value);
    [[nodiscard]] uint16_t de() const;
    void set_de(uint16_t value);
    [[nodiscard]] uint16_t hl() const;
    void set_hl(uint16_t value);
    [[nodiscard]] uint8_t rr(uint8_t value, bool set_z);
    [[nodiscard]] uint8_t rrc(uint8_t value, bool set_z);

    // Main loop
    void step();
};


#endif //GAMEBOY_EMU_CPU_H