#pragma once

#include <cstdint>
#include <array>


class PPUBus;

class PPU {
    public:
        void connectBus(PPUBus* b) {
            bus = b;
        }

    private:
        // Object Attribute Memory, internal to the PPU, not in bus
        std::array<uint8_t, 256> oam{};

        PPUBus* bus = nullptr;


        // Reads and writes that go through the (ppu)bus
        void writeVram(uint16_t address, uint8_t data);
        uint8_t readVram(uint16_t address) const;
};
