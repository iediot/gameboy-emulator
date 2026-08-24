//
// Created by edi on 4/22/26.
//

#ifndef GAMEBOY_EMU_CPU_H
#define GAMEBOY_EMU_CPU_H

#include <cstdint>
#include "memory.h"
#include "ppu.h"
#include "apu.h"

class Cpu {
private:
    Memory& mem;
    Apu& apu;
public:
    // Constructor
    Cpu(Memory& memory, Ppu& ppu, Apu& apu);
    Ppu& ppu;

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

    // Flags
    static constexpr uint8_t FLAG_Z = 0x80; // 10000000
    static constexpr uint8_t FLAG_N = 0x40; // 01000000
    static constexpr uint8_t FLAG_H = 0x20; // 00100000
    static constexpr uint8_t FLAG_C = 0x10; // 00010000

    // Interrupts
    bool IME = false;
    uint8_t ime_pending = 0;
    bool halted = false;
    bool halt_bug = false;   // halt with interrupts disabled but one pending reads the next byte twice

    uint64_t total_cycles = 0;

    // Timer
    uint16_t internal_div = 0xABCB; // post-boot 0xABCC, less one because tick advances it before use
    bool last_and_result = false; // result of '(internal_div & selected_bit) & timer_enable' from last t-cycle
    bool last_apu_bit = false;    // bit 12 of internal_div from last t-cycle, clocks the frame sequencer
    uint8_t tima_reload_delay = 0;
    void tick(uint8_t cycles);

    // the memory access an instruction is performing, carried into tick so the bus
    // transaction can land on a chosen t-cycle instead of after the whole m-cycle
    enum BusKind { BUS_NONE = 0, BUS_READ, BUS_WRITE };
    uint8_t bus_kind = BUS_NONE;
    uint16_t bus_addr = 0;
    uint8_t bus_val = 0;
    uint8_t bus_result = 0;
    uint8_t bus_at = 4;      // which t-cycle of the group it lands on, 1 based
    bool bus_late = true;    // after that cycle's peripheral work rather than before
    bool tima_reloaded = false;   // the reload fired on this t-cycle, a write now is dropped
    void do_bus();
    void oam_bug(uint16_t address, bool read);
    void sp_step(int delta);
    void oam_bug_read_inc(uint16_t address);

    uint8_t read_and_tick(uint16_t address);
    void write_and_tick(uint16_t address, uint8_t value);

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
    [[nodiscard]] uint8_t or_x(uint8_t value);
    [[nodiscard]] uint8_t xor_x(uint8_t value);
    [[nodiscard]] uint8_t and_x(uint8_t value);
    [[nodiscard]] uint8_t add(uint8_t value, bool with_carry);
    [[nodiscard]] uint8_t sub(uint8_t value, bool with_carry);
    void cp(uint8_t value);
    void ret();
    void rst(uint16_t address);
    void call();
    [[nodiscard]] uint8_t swap (uint8_t value);
    [[nodiscard]] uint8_t rr(uint8_t value, bool set_z);
    [[nodiscard]] uint8_t rl(uint8_t value, bool set_z);
    [[nodiscard]] uint8_t rrc(uint8_t value, bool set_z);
    [[nodiscard]] uint8_t rlc(uint8_t value, bool set_z);
    [[nodiscard]] uint8_t srl(uint8_t value);
    [[nodiscard]] uint8_t sla(uint8_t value);
    [[nodiscard]] uint8_t sra(uint8_t value);
    void bit(uint8_t bit_position, uint8_t value);
    [[nodiscard]] uint8_t res(uint8_t bit_position, uint8_t value);
    [[nodiscard]] uint8_t set_bit(uint8_t bit_position, uint8_t value);

    // Main loop
    uint8_t step();
};

#endif //GAMEBOY_EMU_CPU_H
