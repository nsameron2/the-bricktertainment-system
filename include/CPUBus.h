#pragma once

#include <array>
#include <cstdint>

// For CPU RAM mirroring.
constexpr uint16_t INTERNAL_RAM_SIZE = 0x0800;
constexpr uint16_t INTERNAL_RAM_MASK = 0x07FF;
constexpr uint16_t INTERNAL_RAM_MIRROR_END = 0x1FFF;

class Cartridge;
class PPU;


// The CPU bus maps CPU-visible RAM, PPU registers, APU/input registers, and cartridge PRG space.
class CPUBus {
public:
    // Memory handling
    void write(uint16_t address, uint8_t data);
    uint8_t read(uint16_t address) const;


    // Component connecting functions
    void insertCartridge(Cartridge* cartridge);
    void connectPPU(PPU* ppup);

private:
    // Memory array, for the 2KB of RAM. Initialize it to empty.
    std::array<uint8_t, INTERNAL_RAM_SIZE> memory{};

    // Component pointers
    Cartridge* cartridge = nullptr;
    PPU* ppu = nullptr;
};
