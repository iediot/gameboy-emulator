#include <iostream>
#include <iomanip>
#include <fstream>
#include "memory.h"

int main()
{
    Memory mem;

    mem.write(0x1234, 0x42);
    mem.write(0xC000, 0xAB);

    std::cout << std::hex << std::uppercase;
    std::cout << "Read 0x1234: 0x" << (int)mem.read(0x1234) << "\n";
    std::cout << "Read 0xC000: 0x" << (int)mem.read(0xC000) << "\n";
    std::cout << "Read 0x5678: 0x" << (int)mem.read(0x5678) << "\n";

    return 0;
}