#include "Emulator.hpp"
#include <iostream>
int main()
{
    Chip8::Emulator emulator{};
    emulator.LoadRom("/home/nish/proj/cpp/Chip8/1-chip8-logo.ch8");
    emulator.Run();
    std::cin.get();
    return 0;
}
