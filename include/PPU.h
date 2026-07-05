#pragma once

#include <cstdint>
#include <array>


class PPU {
    public:

    private:
        // PPU memory
        std::array<uint8_t, 2048> nametableRam{};
        std::array<uint8_t, 32> paletteRam{};
        std::array<uint8_t, 256> oam{};

        void writeVram(uint16_t address, uint8_t data);
        uint8_t readVram(uint16_t address) const;
};
