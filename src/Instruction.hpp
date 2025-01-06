#include <cstdint>

namespace Chip8
{
    struct Instruction
    {
        std::uint8_t opcode;
        std::uint16_t nnn;
        std::uint8_t nn;
        std::uint8_t n;
        struct Register
        {
            std::uint8_t x : 4;
            std::uint8_t y : 4;
        } reg;
    };

} // namespace Chip8
