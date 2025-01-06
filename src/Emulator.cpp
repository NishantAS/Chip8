#include "Emulator.hpp"
#include "Instruction.hpp"

#include <SFML/Graphics.hpp>

#include <fstream>
#include <iostream>
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
    if (not std::filesystem::is_regular_file(path))
        return false;

    std::ifstream rom{path, std::ios::binary};
    if (rom.is_open())
    {

        const auto size = std::filesystem::file_size(path);
        if (size > 4096 - 0x200)
            return false;
        else
            rom.read(reinterpret_cast<char *>(m_Memory.data() + 512), size);

        rom.close();
        return true;
    }
    return false;
}

bool Chip8::Emulator::LoadRom(const std::uint8_t *data, std::size_t size) noexcept
{
    if (not data)
        return false;

    if (size > 4096 - 0x200)
        return false;
    else
        std::copy(data, data + size, m_Memory.begin() + 0x200);

    return true;
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
            else if (event->is<sf::Event::KeyPressed>())
            {
                handleKeyPress(*event->getIf<sf::Event::KeyPressed>());
            }
            else if (event->is<sf::Event::KeyReleased>())
            {
                handleKeyRelease(*event->getIf<sf::Event::KeyReleased>());
            }
        }

        update(m_Clock.restart().asSeconds(), keyPress, key);
        render();
    }
}

void Chip8::Emulator::update(const float dt, const bool keyPress, const std::uint8_t key) noexcept
{
    std::uint16_t instruction = m_Memory[m_PC] << 8 bitor m_Memory[m_PC + 1];
    m_PC += 2;
    Chip8::Instruction inst{};
    inst.opcode = (instruction bitand 0xF000) >> 12;
    inst.nnn = instruction bitand 0x0FFF;
    inst.nn = instruction bitand 0x00FF;
    inst.n = instruction bitand 0x000F;
    inst.reg.x = (instruction bitand 0x0F00) >> 8;
    inst.reg.y = (instruction bitand 0x00F0) >> 4;

    m_DelayTimer -= dt;
    m_SoundTimer -= dt;

    std::print(std::cout, "PC: {:X}\n", m_PC);
    std::print(std::cout, "Instruction: {:X}\n", instruction);

    if (inst.opcode == 0x0)
    {
        if (inst.nnn == 0x0E0) // Clear the display
            std::ranges::fill(m_Display, std::bitset<64>(0));
        else if (inst.nnn == 0x0EE) // Return from a subroutine
        {
            m_PC = m_Stack.top();
            m_Stack.pop();
        }
        else
        {
            std::print(std::cout, "Problem with {:X}\n", instruction);
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
        else
            std::print(std::cout, "Problem with {:X}\n", instruction);
    }
    else if (inst.opcode == 0x4) // Skip next inst if Vx not_eq nn
    {
        if (m_Registers[inst.reg.x] not_eq inst.nn)
            m_PC += 2;
        else
            std::print(std::cout, "Problem with {:X}\n", instruction);
    }
    else if (inst.opcode == 0x5) // Skip next inst if Vx == Vy
    {
        if (m_Registers[inst.reg.x] == m_Registers[inst.reg.y])
            m_PC += 2;
        else
            std::print(std::cout, "Problem with {:X}\n", instruction);
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
            m_Registers[inst.reg.x] and_eq m_Registers[inst.reg.y];
        else if (inst.n == 0x3)
            m_Registers[inst.reg.x] xor_eq m_Registers[inst.reg.y];
        else if (inst.n == 0x4)
        {
            std::uint16_t sum = m_Registers[inst.reg.x] + m_Registers[inst.reg.y];
            m_Registers[0xF] = sum > 0xFF;
            m_Registers[inst.reg.x] = sum bitand 0xFF;
        }
        else if (inst.n == 0x5)
        {
            m_Registers[0xF] = m_Registers[inst.reg.x] > m_Registers[inst.reg.y];
            m_Registers[inst.reg.x] -= m_Registers[inst.reg.y];
        }
        else if (inst.n == 0x6)
        {
            m_Registers[0xF] = m_Registers[inst.reg.x] bitand 0x1;
            m_Registers[inst.reg.x] >>= 1;
        }
        else if (inst.n == 0x7)
        {
            m_Registers[0xF] = m_Registers[inst.reg.y] >= m_Registers[inst.reg.x];
            m_Registers[inst.reg.x] = m_Registers[inst.reg.y] - m_Registers[inst.reg.x];
        }
        else if (inst.n == 0xE)
        {
            m_Registers[0xF] = m_Registers[inst.reg.x] bitand 0x80;
            m_Registers[inst.reg.x] <<= 1;
        }
        else
            std::print(std::cout, "Problem with {:X}\n", instruction);
    }
    else if (inst.opcode == 0x9)
    {
        if (m_Registers[inst.reg.x] not_eq m_Registers[inst.reg.y])
            m_PC += 2;
        else
            std::print(std::cout, "Problem with {:X}\n", instruction);
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
        m_Registers[inst.reg.x] = m_RandomEngine() bitand inst.nn;
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
                if ((pixel bitand (0x80 >> xLine)) not_eq 0)
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
            else
                std::print(std::cout, "Problem with {:X}\n", instruction);
        }
        else if (inst.nn == 0xA1)
        {
            if (not m_Keys[m_Registers[inst.reg.x]])
                m_PC += 2;
            else
                std::print(std::cout, "Problem with {:X}\n", instruction);
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
            m_I = (m_Registers[inst.reg.x] bitand 0xF) * 5;
        else if (inst.nn == 0x33)
        {
            m_Memory[m_I] = m_Registers[inst.reg.x] / 100;
            m_Memory[m_I + 1] = (m_Registers[inst.reg.x] / 10) % 10;
            m_Memory[m_I + 2] = m_Registers[inst.reg.x] % 10;
        }
        else if (inst.nn == 0x55)
        {
            for (std::size_t i = 0; i <= inst.reg.x; i++)
                m_Memory[m_I + i] = m_Registers[i];
        }
        else if (inst.nn == 0x65)
        {
            for (std::size_t i = 0; i <= inst.reg.x; i++)
                m_Registers[i] = m_Memory[m_I + i];
        }
        else
            std::print(std::cout, "Problem with {:X}\n", instruction);
    }
    else
        std::print(std::cout, "Problem with {:X}\n", instruction);
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

    m_Window.display();
}

void Chip8::Emulator::handleKeyPress(const sf::Event::KeyPressed &event) noexcept
{
    if (event.code == sf::Keyboard::Key::Num0 or event.code == sf::Keyboard::Key::Numpad0)
        m_Keys[0] = true;
    else if (event.code == sf::Keyboard::Key::Num1 or event.code == sf::Keyboard::Key::Numpad1)
        m_Keys[1] = true;
    else if (event.code == sf::Keyboard::Key::Num2 or event.code == sf::Keyboard::Key::Numpad2)
        m_Keys[2] = true;
    else if (event.code == sf::Keyboard::Key::Num3 or event.code == sf::Keyboard::Key::Numpad3)
        m_Keys[3] = true;
    else if (event.code == sf::Keyboard::Key::Num4 or event.code == sf::Keyboard::Key::Numpad4)
        m_Keys[4] = true;
    else if (event.code == sf::Keyboard::Key::Num5 or event.code == sf::Keyboard::Key::Numpad5)
        m_Keys[5] = true;
    else if (event.code == sf::Keyboard::Key::Num6 or event.code == sf::Keyboard::Key::Numpad6)
        m_Keys[6] = true;
    else if (event.code == sf::Keyboard::Key::Num7 or event.code == sf::Keyboard::Key::Numpad7)
        m_Keys[7] = true;
    else if (event.code == sf::Keyboard::Key::Num8 or event.code == sf::Keyboard::Key::Numpad8)
        m_Keys[8] = true;
    else if (event.code == sf::Keyboard::Key::Num9 or event.code == sf::Keyboard::Key::Numpad9)
        m_Keys[9] = true;
    else if (event.code == sf::Keyboard::Key::A)
        m_Keys[10] = true;
    else if (event.code == sf::Keyboard::Key::B)
        m_Keys[11] = true;
    else if (event.code == sf::Keyboard::Key::C)
        m_Keys[12] = true;
    else if (event.code == sf::Keyboard::Key::D)
        m_Keys[13] = true;
    else if (event.code == sf::Keyboard::Key::E)
        m_Keys[14] = true;
    else if (event.code == sf::Keyboard::Key::F)
        m_Keys[15] = true;
}

void Chip8::Emulator::handleKeyRelease(const sf::Event::KeyReleased &event) noexcept
{
    if (event.code == sf::Keyboard::Key::Num0 or event.code == sf::Keyboard::Key::Numpad0)
        m_Keys[0] = false;
    else if (event.code == sf::Keyboard::Key::Num1 or event.code == sf::Keyboard::Key::Numpad1)
        m_Keys[1] = false;
    else if (event.code == sf::Keyboard::Key::Num2 or event.code == sf::Keyboard::Key::Numpad2)
        m_Keys[2] = false;
    else if (event.code == sf::Keyboard::Key::Num3 or event.code == sf::Keyboard::Key::Numpad3)
        m_Keys[3] = false;
    else if (event.code == sf::Keyboard::Key::Num4 or event.code == sf::Keyboard::Key::Numpad4)
        m_Keys[4] = false;
    else if (event.code == sf::Keyboard::Key::Num5 or event.code == sf::Keyboard::Key::Numpad5)
        m_Keys[5] = false;
    else if (event.code == sf::Keyboard::Key::Num6 or event.code == sf::Keyboard::Key::Numpad6)
        m_Keys[6] = false;
    else if (event.code == sf::Keyboard::Key::Num7 or event.code == sf::Keyboard::Key::Numpad7)
        m_Keys[7] = false;
    else if (event.code == sf::Keyboard::Key::Num8 or event.code == sf::Keyboard::Key::Numpad8)
        m_Keys[8] = false;
    else if (event.code == sf::Keyboard::Key::Num9 or event.code == sf::Keyboard::Key::Numpad9)
        m_Keys[9] = false;
    else if (event.code == sf::Keyboard::Key::A)
        m_Keys[10] = false;
    else if (event.code == sf::Keyboard::Key::B)
        m_Keys[11] = false;
    else if (event.code == sf::Keyboard::Key::C)
        m_Keys[12] = false;
    else if (event.code == sf::Keyboard::Key::D)
        m_Keys[13] = false;
    else if (event.code == sf::Keyboard::Key::E)
        m_Keys[14] = false;
    else if (event.code == sf::Keyboard::Key::F)
        m_Keys[15] = false;
}
