#include <filesystem>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include "memory.h"
#include "cpu.h"

int main()
{
    std::string path = "../roms/test-roms/cpu_instrs/individual/";

    std::vector<std::string> roms = {
        "01-special.gb",
        "02-interrupts.gb",
        "03-op sp,hl.gb",
        "04-op r,imm.gb",
        "05-op rp.gb",
        "06-ld r,r.gb",
        "07-jr,jp,call,ret,rst.gb",
        "08-misc instrs.gb",
        "09-op r,r.gb",
        "10-bit ops.gb",
        "11-op a,(hl).gb"
    };

    for (const auto& rom_name : roms) {
        Memory mem;
        Cpu cpu(mem);

        std::ifstream rom_file(path + rom_name, std::ios::binary);

        if (!rom_file) {
            std::cerr << "Could not open: " << rom_name << "\n";
            continue;
        }

        std::vector<uint8_t> rom_data{std::istreambuf_iterator<char>(rom_file),
            std::istreambuf_iterator<char>()};
        mem.loadRom(rom_data);

        for (int i = 0; i < 10000000; i++) {
            cpu.step();
        }
    }
}