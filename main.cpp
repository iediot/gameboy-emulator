#include <filesystem>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include "memory.h"
#include "cpu.h"

int main()
{
    Memory mem;
    Cpu cpu(mem);
    std::string path = "../roms/test-roms/cpu_instrs/individual/01-special.gb";

    std::ifstream rom(path, std::ios::binary);

    if (rom) {
        std::vector<uint8_t> rom_data{std::istreambuf_iterator<char>(rom),
            std::istreambuf_iterator<char>()};
        mem.loadRom(rom_data);
        while (true) {
            cpu.step();
        }
    } else {
        std::cerr << "Could not open ROM at: " << path;
        std::exit(1);
    }
}