#pragma once

#include <array>
#include <cstdint>


class Cartridge;

class PPUBus {
public:
    void insertCartridge(Cartridge* cart);

    void write(uint16_t address, uint8_t data);
    uint8_t read(uint16_t address) const;

private:
    // Normal cartridges mirror 2 KiB of nametable RAM; four-screen cartridges use all 4 KiB.
    std::array<uint8_t, 0x1000> nametableRam{};
    std::array<uint8_t, 32> paletteRam{};


    // For cartrdige interaction
    Cartridge* cartridge = nullptr;

    uint16_t mirrorNametableAddress(uint16_t address) const;
};
