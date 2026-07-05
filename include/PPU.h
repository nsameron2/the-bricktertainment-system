#pragma once

#include <cstdint>
#include <array>


class PPUBus;

class PPU {
    public:
        void connectBus(PPUBus* b) {
            bus = b;
        }

        uint8_t readRegister(uint16_t address);
        void writeRegister(uint16_t address, uint8_t data);

    private:
        // Object Attribute Memory, internal to the PPU, not in bus
        std::array<uint8_t, 256> oam{};

        PPUBus* bus = nullptr;

        // CPU-visible PPU registers
        uint8_t control = 0x00;
        uint8_t mask = 0x00;
        uint8_t status = 0x00;
        uint8_t oamAddress = 0x00;

        // Internal PPU address/scroll state used by PPUADDR, PPUSCROLL, and PPUDATA
        uint16_t vramAddress = 0x0000;
        uint16_t tempVramAddress = 0x0000;
        uint8_t fineX = 0x00;
        bool writeLatch = false;
        uint8_t dataBuffer = 0x00;

        uint16_t vramIncrement() const;

        // Reads and writes that go through the (ppu)bus
        void writeVram(uint16_t address, uint8_t data);
        uint8_t readVram(uint16_t address) const;
};
