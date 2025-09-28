#ifndef CHIP8_H
#define CHIP8_H

#include <cstdint> // For uint8_t, uint16_t
#include <array>   // For std::array
#include <string>  // For file loading

class Chip8
{
public:
    Chip8();
    ~Chip8();

    bool loadROM(const std::string &filename);
    void emulateCycle();
    void decrementTimers();

    void reset();

    // Display buffer (64x32, true = pixel on)
    std::array<bool, 64 * 32> gfx{};

    // Keyboard state (true = pressed)
    std::array<bool, 16> keys{};

    // Draw flag for main loop to know when to render
    bool drawFlag = false;

    // Beep flag for sound
    bool beepFlag = false;

private:
    // Memory
    std::array<uint8_t, 4096> memory{};

    // Registers
    std::array<uint8_t, 16> V{}; // V0-VF
    uint16_t I = 0;              // Index
    uint16_t PC = 0x200;         // Program counter

    // Stack
    std::array<uint16_t, 16> stack{};
    uint8_t sp = 0; // Stack pointer

    // Timers
    uint8_t delay_timer = 0;
    uint8_t sound_timer = 0;

    // Helper to initialize font
    void initFont();
};

#endif