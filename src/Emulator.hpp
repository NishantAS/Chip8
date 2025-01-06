#include <bitset>
#include <filesystem>
#include <stack>

#include <SFML/Window.hpp>

namespace Chip8
{
    class Emulator
    {
    public:
        Emulator() noexcept;
        ~Emulator() noexcept;
        bool LoadRom(const std::filesystem::path &path) noexcept;
        bool LoadRom(const std::uint8_t* data, std::size_t size) noexcept;
        void Run() noexcept;

    private:
        sf::RenderWindow m_Window;
        sf::Clock m_Clock;

        std::array<std::uint8_t, 4096> m_Memory;
        std::array<std::uint8_t, 16> m_Registers;
        std::uint16_t m_I;
        std::stack<std::uint16_t> m_Stack;
        double m_DelayTimer;
        double m_SoundTimer;
        std::array<std::bitset<64>, 32> m_Display;
        std::bitset<16> m_Keys;
        std::uint16_t m_PC;
        std::mt19937_64 m_RandomEngine;

        void update(const float dt, const bool keyPressed, const std::uint8_t key) noexcept;
        void render() noexcept;
    };
} // namespace Chip8
