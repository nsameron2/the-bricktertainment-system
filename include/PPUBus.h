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
    // Internal PPU VRAM
    std::array<uint8_t, 2048> nametableRam{};
    std::array<uint8_t, 32> paletteRam{};


    // For cartrdige interaction
    Cartridge* cartridge = nullptr;
};
