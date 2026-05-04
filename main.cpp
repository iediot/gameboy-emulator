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
        "03-op sp,hl.gb"
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