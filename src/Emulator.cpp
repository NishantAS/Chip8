#include <Emulator.hpp>
#include <Instruction.hpp>

#include <SFML/Graphics.hpp>

#include <fstream>
#include <ranges>
#include <numeric>
#include <version>
#include <random>

constexpr uint8_t font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80, // F
};

Chip8::Emulator::Emulator() noexcept
    : m_Window(sf::VideoMode(sf::Vector2u(640u, 320u)), "Chip-8 Emulator"),
      m_Clock(),
      m_Memory(),
      m_Registers(),
      m_I(0),
      m_Stack(),
      m_DelayTimer(0),
      m_SoundTimer(0),
      m_Display(),
      m_Keys(0),
      m_PC(0x200),
      m_RandomEngine(std::random_device{}())
{
    m_Window.setFramerateLimit(60);
    std::ranges::fill(m_Memory, 0);
    std::ranges::fill(m_Registers, 0);
    std::ranges::fill(m_Display, std::bitset<64>(0));
}

Chip8::Emulator::~Emulator() noexcept
{
}

bool Chip8::Emulator::LoadRom(const std::filesystem::path &path) noexcept
{
    if (!std::filesystem::is_regular_file(path))
        return false;

    std::ifstream rom{path, std::ios::binary};

    const auto size = std::filesystem::file_size(path);
    if (size > 4096 - 0x200)
        return false;
    else
        rom.read(reinterpret_cast<char *>(m_Memory.data() + 512), size);

    rom.close();
}

bool Chip8::Emulator::LoadRom(const std::uint8_t *data, std::size_t size) noexcept
{
    if (not data)
        return false;

    if (size > 4096 - 0x200)
        return false;
    else
        std::copy(data, data + size, m_Memory.begin() + 0x200);
}

void Chip8::Emulator::Run() noexcept
{
    while (m_Window.isOpen())
    {
        bool keyPress = false;
        std::uint8_t key = 0xFF;
        while (auto event = m_Window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                m_Window.close();
            }
        }

        update(m_Clock.restart().asSeconds(), keyPress, key);
        render();
    }
}

void Chip8::Emulator::update(const float dt, const bool keyPress, const std::uint8_t key) noexcept
{
    std::uint16_t instruction = m_Memory[m_PC] << 8 | m_Memory[m_PC + 1];
    m_PC += 2;
    Chip8::Instruction inst{};
    inst.opcode = (instruction & 0xF00) >> 12;
    inst.nnn = instruction & 0x0FFF;
    inst.nn = instruction & 0x00FF;
    inst.n = instruction & 0x000F;
    inst.reg.x = (instruction & 0x0F00) >> 8;
    inst.reg.y = (instruction & 0x00F0) >> 4;

    m_DelayTimer -= dt;
    m_SoundTimer -= dt;

    if (inst.opcode == 0x0)
    {
        if (inst.nnn == 0x0E0) // Clear the display
            std::ranges::fill(m_Display, std::bitset<64>(0));
        else if (inst.nnn == 0x0EE) // Return from a subroutine
        {
            m_PC = m_Stack.top();
            m_Stack.pop();
        }
    }
    else if (inst.opcode == 0x1) // Jump to address nnn
    {
        m_PC = inst.nnn;
    }
    else if (inst.opcode == 0x2) // Call subroutine at nnn
    {
        m_Stack.push(m_PC);
        m_PC = inst.nnn;
    }
    else if (inst.opcode == 0x3) // Skip next inst if Vx == nn
    {
        if (m_Registers[inst.reg.x] == inst.nn)
            m_PC += 2;
    }
    else if (inst.opcode == 0x4) // Skip next inst if Vx != nn
    {
        if (m_Registers[inst.reg.x] != inst.nn)
            m_PC += 2;
    }
    else if (inst.opcode == 0x5) // Skip next inst if Vx == Vy
    {
        if (m_Registers[inst.reg.x] == m_Registers[inst.reg.y])
            m_PC += 2;
    }
    else if (inst.opcode == 0x6) // Set Vx = nn
    {
        m_Registers[inst.reg.x] = inst.nn;
    }
    else if (inst.opcode == 0x7) // Set Vx = Vx + nn
    {
        m_Registers[inst.reg.x] += inst.nn;
    }
    else if (inst.opcode == 0x8)
    {
        if (inst.n == 0x0)
            m_Registers[inst.reg.x] = m_Registers[inst.reg.y];
        else if (inst.n == 0x1)
            m_Registers[inst.reg.x] |= m_Registers[inst.reg.y];
        else if (inst.n == 0x2)
            m_Registers[inst.reg.x] &= m_Registers[inst.reg.y];
        else if (inst.n == 0x3)
            m_Registers[inst.reg.x] ^= m_Registers[inst.reg.y];
        else if (inst.n == 0x4)
        {
            std::uint16_t sum = m_Registers[inst.reg.x] + m_Registers[inst.reg.y];
            m_Registers[0xF] = sum > 0xFF;
            m_Registers[inst.reg.x] = sum & 0xFF;
        }
        else if (inst.n == 0x5)
        {
            m_Registers[0xF] = m_Registers[inst.reg.x] > m_Registers[inst.reg.y];
            m_Registers[inst.reg.x] -= m_Registers[inst.reg.y];
        }
        else if (inst.n == 0x6)
        {
            m_Registers[0xF] = m_Registers[inst.reg.x] & 0x1;
            m_Registers[inst.reg.x] >>= 1;
        }
        else if (inst.n == 0x7)
        {
            m_Registers[0xF] = m_Registers[inst.reg.y] >= m_Registers[inst.reg.x];
            m_Registers[inst.reg.x] = m_Registers[inst.reg.y] - m_Registers[inst.reg.x];
        }
        else if (inst.n == 0xE)
        {
            m_Registers[0xF] = m_Registers[inst.reg.x] && 0x80;
            m_Registers[inst.reg.x] <<= 1;
        }
    }
    else if (inst.opcode == 0x9)
    {
        if (m_Registers[inst.reg.x] != m_Registers[inst.reg.y])
            m_PC += 2;
    }
    else if (inst.opcode == 0xA)
    {
        m_I = inst.nnn;
    }
    else if (inst.opcode == 0xB)
    {
        m_PC = inst.nnn + m_Registers[0x0];
    }
    else if (inst.opcode == 0xC)
    {
        m_Registers[inst.reg.x] = m_RandomEngine() & inst.nn;
    }
    else if (inst.opcode == 0xD)
    {
        std::uint8_t x = m_Registers[inst.reg.x];
        std::uint8_t y = m_Registers[inst.reg.y];
        std::uint8_t height = inst.n;
        m_Registers[0xF] = 0;
        for (std::size_t yLine = 0; yLine < height; yLine++)
        {
            auto pixel = m_Memory[m_I + yLine];
            for (std::size_t xLine = 0; xLine < 8; xLine++)
            {
                if ((pixel & (0x80 >> xLine)) != 0)
                {
                    if (m_Display[y + yLine][x + xLine])
                        m_Registers[0xF] = 1;
                    m_Display[y + yLine][x + xLine] = m_Display[y + yLine][x + xLine] ^ 1;
                }
            }
        }
    }
    else if (inst.opcode == 0xE)
    {
        if (inst.nn == 0x9E)
        {
            if (m_Keys[m_Registers[inst.reg.x]])
                m_PC += 2;
        }
        else if (inst.nn == 0xA1)
        {
            if (!m_Keys[m_Registers[inst.reg.x]])
                m_PC += 2;
        }
    }
    else if (inst.opcode == 0xF)
    {
        if (inst.nn == 0x07)
            m_Registers[inst.reg.x] = m_DelayTimer;
        else if (inst.nn == 0x0A)
        {
            if (keyPress)
                m_Registers[inst.reg.x] = key;
            else
                m_PC -= 2;
        }
        else if (inst.nn == 0x15)
            m_DelayTimer = m_Registers[inst.reg.x];
        else if (inst.nn == 0x18)
            m_SoundTimer = m_Registers[inst.reg.x];
        else if (inst.nn == 0x1E)
            m_I += m_Registers[inst.reg.x];
        else if (inst.nn == 0x29)
            m_I = (m_Registers[inst.reg.x] & 0xF) * 5;
    }
    else
    {
        // TODO: Implement the rest of the inst
    }
}

void Chip8::Emulator::render() noexcept
{
    auto rectangle = sf::RectangleShape(sf::Vector2f(10, 10));
    rectangle.setFillColor(sf::Color::White);
    m_Window.clear(sf::Color::Black);

    for (std::size_t y = 0; y < 32; ++y)
    {
        for (std::size_t x = 0; x < 64; ++x)
        {
            if (m_Display[y][x])
            {
                rectangle.setPosition(sf::Vector2f(x * 10, y * 10));
                m_Window.draw(rectangle);
            }
        }
    }
}
