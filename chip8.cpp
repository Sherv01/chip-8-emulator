#include "chip8.h"
#include <fstream>
#include <random> // For random opcode

Chip8::Chip8()
{
    initFont();
    // Random seed for CXNN opcode
    std::random_device rd;
    // We'll use this later
}

Chip8::~Chip8() {}

void Chip8::initFont()
{
    // Font sprites (0-F), each 5 bytes, at 0x050
    const std::array<uint8_t, 80> font = {
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
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    for (size_t i = 0; i < font.size(); ++i)
    {
        memory[0x050 + i] = font[i];
    }
}

bool Chip8::loadROM(const std::string &filename)
{
    reset(); // clear state before loading

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > 4096 - 0x200)
        return false; // Too big

    file.read(reinterpret_cast<char *>(&memory[0x200]), size);
    return true;
}

void Chip8::emulateCycle()
{
    // Fetch opcode
    uint16_t opcode = (memory[PC] << 8) | memory[PC + 1];
    PC += 2;

    // Decode and execute
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t n = opcode & 0x000F;
    uint8_t nn = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;

    switch (opcode & 0xF000)
    {
    case 0x0000:
        if (opcode == 0x00E0)
        { // CLS
            gfx.fill(false);
            drawFlag = true;
        }
        else if (opcode == 0x00EE)
        { // RET
            --sp;
            PC = stack[sp];
        }
        // Ignore 0NNN
        break;
    case 0x1000: // JP addr
        PC = nnn;
        break;
    case 0x2000: // CALL addr
        stack[sp] = PC;
        ++sp;
        PC = nnn;
        break;
    case 0x3000: // SE Vx, byte
        if (V[x] == nn)
            PC += 2;
        break;
    case 0x4000: // SNE Vx, byte
        if (V[x] != nn)
            PC += 2;
        break;
    case 0x5000: // SE Vx, Vy
        if (V[x] == V[y])
            PC += 2;
        break;
    case 0x6000: // LD Vx, byte
        V[x] = nn;
        break;
    case 0x7000: // ADD Vx, byte
        V[x] += nn;
        break;
    case 0x8000:
        switch (n)
        {
        case 0x0:
            V[x] = V[y];
            break; // LD Vx, Vy
        case 0x1:
            V[x] |= V[y];
            break; // OR Vx, Vy
        case 0x2:
            V[x] &= V[y];
            break; // AND Vx, Vy
        case 0x3:
            V[x] ^= V[y];
            break; // XOR Vx, Vy
        case 0x4:
        { // ADD Vx, Vy
            uint16_t sum = V[x] + V[y];
            V[x] = sum & 0xFF;
            V[0xF] = (sum > 0xFF) ? 1 : 0;
        }
        break;
        case 0x5:
        { // SUB Vx, Vy
            V[0xF] = (V[x] >= V[y]) ? 1 : 0;
            V[x] -= V[y];
        }
        break;
        case 0x6:
        { // SHR Vx {, Vy}
            V[0xF] = V[x] & 0x01;
            V[x] >>= 1;
        }
        break;
        case 0x7:
        { // SUBN Vx, Vy
            V[0xF] = (V[y] >= V[x]) ? 1 : 0;
            V[x] = V[y] - V[x];
        }
        break;
        case 0xE:
        { // SHL Vx {, Vy}
            V[0xF] = (V[x] >> 7) & 0x01;
            V[x] <<= 1;
        }
        break;
        }
        break;
    case 0x9000: // SNE Vx, Vy
        if (V[x] != V[y])
            PC += 2;
        break;
    case 0xA000: // LD I, addr
        I = nnn;
        break;
    case 0xB000: // JP V0, addr
        PC = nnn + V[0];
        break;
    case 0xC000: // RND Vx, byte
    {
        static std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        V[x] = dist(gen) & nn;
    }
    break;
    case 0xD000: // DRW Vx, Vy, nibble
    {
        V[0xF] = 0;
        uint8_t vx = V[x] % 64;
        uint8_t vy = V[y] % 32;
        for (uint8_t row = 0; row < n; ++row)
        {
            uint8_t sprite = memory[I + row];
            for (uint8_t col = 0; col < 8; ++col)
            {
                if ((sprite & (0x80 >> col)) != 0)
                {
                    size_t idx = (vy + row) * 64 + (vx + col);
                    if (idx < 64 * 32)
                    { // Clip to screen
                        if (gfx[idx])
                            V[0xF] = 1;
                        gfx[idx] ^= true;
                    }
                }
            }
        }
        drawFlag = true;
    }
    break;
    case 0xE000:
        if (nn == 0x9E)
        { // SKP Vx
            if (keys[V[x]])
                PC += 2;
        }
        else if (nn == 0xA1)
        { // SKNP Vx
            if (!keys[V[x]])
                PC += 2;
        }
        break;
    case 0xF000:
        switch (nn)
        {
        case 0x07:
            V[x] = delay_timer;
            break; // LD Vx, DT
        case 0x0A:
        { // LD Vx, K
            bool keyPressed = false;
            for (uint8_t i = 0; i < 16; ++i)
            {
                if (keys[i])
                {
                    V[x] = i;
                    keyPressed = true;
                }
            }
            if (!keyPressed)
                PC -= 2; // Wait: repeat this instruction
        }
        break;
        case 0x15:
            delay_timer = V[x];
            break; // LD DT, Vx
        case 0x18:
            sound_timer = V[x];
            break; // LD ST, Vx
        case 0x1E:
            I += V[x];
            break; // ADD I, Vx (no carry flag)
        case 0x29:
            I = 0x050 + (V[x] & 0x0F) * 5;
            break; // LD F, Vx (font)
        case 0x33:
        { // LD B, Vx (BCD)
            memory[I] = V[x] / 100;
            memory[I + 1] = (V[x] / 10) % 10;
            memory[I + 2] = V[x] % 10;
        }
        break;
        case 0x55:
        { // LD [I], Vx
            for (uint8_t i = 0; i <= x; ++i)
            {
                memory[I + i] = V[i];
            }
            I += x + 1; // Increment I (original behavior)
        }
        break;
        case 0x65:
        { // LD Vx, [I]
            for (uint8_t i = 0; i <= x; ++i)
            {
                V[i] = memory[I + i];
            }
            I += x + 1; // Increment I (original behavior)
        }
        break;
        }
        break;
    default:
        // Unknown opcode - handle error or ignore
        break;
    }
}

void Chip8::decrementTimers()
{
    if (delay_timer > 0)
        --delay_timer;

    if (sound_timer > 0)
    {
        --sound_timer;
        beepFlag = true; // set flag whenever sound_timer is non-zero
    }
    else
    {
        beepFlag = false; // reset flag when sound_timer is zero
    }
}

void Chip8::reset()
{
    memory.fill(0);
    V.fill(0);
    gfx.fill(false);
    keys.fill(false);
    stack.fill(0);

    I = 0;
    PC = 0x200; // programs start at 0x200
    sp = 0;
    delay_timer = 0;
    sound_timer = 0;

    drawFlag = false;
    beepFlag = false;

    initFont(); // reload font sprites
}
