#pragma once

#include <cstdint>
#include <vector>
#include <array>


class PPU {
    public:
        bool ppuWrite();
        bool ppuRead() const;

    private:
        // PPU memory
        std::array<uint8_t, 2048> nametableRam{};
        std::array<uint8_t, 32> paletteRam{};
        std::array<uint8_t, 256> oam{};
}