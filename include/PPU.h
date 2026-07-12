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

        // PPU clocking
        void clock();

        const std::array<uint32_t, 256 * 240>& getFramebuffer() const;
        bool isFrameComplete() const;
        void clearFrameComplete();
        bool isNmiComplete();



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

        // PPU clock timing states
        int16_t scanline = 0;
        int16_t cycle = 0;
        bool frameComplete = false;
        bool nmiRequested = false;

        // PPU rendering
        std::array<uint32_t, 256 * 240> framebuffer{};

        // Pixel data shared by background and sprite rendering.
        struct Pixel {
            uint8_t colorIndex = 0x00;
            bool opaque = false;
        };

        // Sprite-specific rendering metadata attached to a sampled pixel.
        struct SpritePixel {
            Pixel pixel{};
            bool behindBackground = false;
            bool isSpriteZero = false;
        };


        uint16_t vramIncrement() const;

        // Reads and writes that go through the (ppu)bus
        void writeVram(uint16_t address, uint8_t data);
        uint8_t readVram(uint16_t address) const;
        Pixel getBackgroundPixel(uint16_t x, uint16_t y) const;
        SpritePixel getSpritePixel(uint16_t x, uint16_t y) const;
        uint32_t nesColorToRgb(uint8_t colorIndex) const;
};
