#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define private public
#include "PPU.h"
#undef private

#include "PPUBus.h"

namespace {

constexpr int16_t PPU_CYCLES_PER_SCANLINE = 341;
constexpr int16_t PPU_SCANLINES_PER_FRAME = 262;
constexpr int16_t PPU_VBLANK_SCANLINE = 241;
constexpr int16_t PPU_PRE_RENDER_SCANLINE = 261;
constexpr int16_t PPU_STATUS_EVENT_CYCLE = 1;
constexpr uint8_t PPUSTATUS_VBLANK = 1 << 7;

void expectEqual(uint8_t actual, uint8_t expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%02X, got 0x%02X)\n",
                     message,
                     expected,
                     actual);
        std::exit(EXIT_FAILURE);
    }
}

void expectEqual16(int16_t actual, int16_t expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%04X, got 0x%04X)\n",
                     message,
                     static_cast<uint16_t>(expected),
                     static_cast<uint16_t>(actual));
        std::exit(EXIT_FAILURE);
    }
}

void expectTrue(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void expectFalse(bool value, const char* message) {
    if (value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void runClocks(PPU& ppu, int clocks) {
    for (int i = 0; i < clocks; i++) {
        ppu.clock();
    }
}

int clocksToProcessDot(int16_t scanline, int16_t cycle) {
    return (scanline * PPU_CYCLES_PER_SCANLINE) + cycle + 1;
}

}

int main() {
    {
        PPU ppu;
        PPUBus bus;
        ppu.connectBus(&bus);

        ppu.writeRegister(0x2006, 0x23);
        ppu.writeRegister(0x2006, 0xC0);
        ppu.writeRegister(0x2007, 0x12);
        ppu.writeRegister(0x2007, 0x34);

        expectEqual(bus.read(0x23C0), 0x12, "PPUDATA writes to the current VRAM address");
        expectEqual(bus.read(0x23C1), 0x34, "PPUDATA increments VRAM address by 0x0001 by default");
    }

    {
        PPU ppu;
        PPUBus bus;
        ppu.connectBus(&bus);

        ppu.writeRegister(0x2000, 0x04);
        ppu.writeRegister(0x2006, 0x24);
        ppu.writeRegister(0x2006, 0x00);
        ppu.writeRegister(0x2007, 0x56);
        ppu.writeRegister(0x2007, 0x78);

        expectEqual(bus.read(0x2400), 0x56, "PPUCTRL bit 2 writes first PPUDATA byte");
        expectEqual(bus.read(0x2420), 0x78, "PPUCTRL bit 2 increments VRAM address by 0x0020");
    }

    {
        PPU ppu;

        ppu.writeRegister(0x2003, 0x80);
        ppu.writeRegister(0x2004, 0xA5);
        ppu.writeRegister(0x2003, 0x80);

        expectEqual(ppu.readRegister(0x2004), 0xA5, "OAMDATA reads from the selected OAM address");
    }

    {
        PPU ppu;
        PPUBus bus;
        ppu.connectBus(&bus);

        ppu.writeRegister(0x2006, 0x21);
        ppu.readRegister(0x2002);
        ppu.writeRegister(0x2006, 0x22);
        ppu.writeRegister(0x2006, 0x00);
        ppu.writeRegister(0x2007, 0x99);

        expectEqual(bus.read(0x2200), 0x99, "PPUSTATUS read resets the PPUADDR write latch");
    }

    {
        PPU ppu;
        PPUBus bus;
        ppu.connectBus(&bus);

        bus.write(0x2300, 0xBC);
        ppu.writeRegister(0x2006, 0x23);
        ppu.writeRegister(0x2006, 0x00);

        expectEqual(ppu.readRegister(0x2007), 0x00, "PPUDATA nametable read returns the old buffer first");
        expectEqual(ppu.readRegister(0x2007), 0xBC, "PPUDATA nametable read returns buffered data next");
    }

    {
        PPU ppu;
        PPUBus bus;
        ppu.connectBus(&bus);

        bus.write(0x3F00, 0x0F);
        ppu.writeRegister(0x2006, 0x3F);
        ppu.writeRegister(0x2006, 0x00);

        expectEqual(ppu.readRegister(0x2007), 0x0F, "PPUDATA palette read bypasses the read buffer");
    }

    {
        PPU ppu;

        runClocks(ppu, clocksToProcessDot(PPU_VBLANK_SCANLINE, PPU_STATUS_EVENT_CYCLE));
        expectTrue((ppu.status & PPUSTATUS_VBLANK) != 0x00, "PPU clock sets VBlank at scanline 0x00F1 cycle 0x0001");
        expectEqual(ppu.readRegister(0x2002) & PPUSTATUS_VBLANK,
                    PPUSTATUS_VBLANK,
                    "PPUSTATUS read returns VBlank set");
        expectFalse((ppu.status & PPUSTATUS_VBLANK) != 0x00, "PPUSTATUS read clears VBlank");
    }

    {
        PPU ppu;

        const int vblankClocks = clocksToProcessDot(PPU_VBLANK_SCANLINE, PPU_STATUS_EVENT_CYCLE);
        const int preRenderClocks = clocksToProcessDot(PPU_PRE_RENDER_SCANLINE, PPU_STATUS_EVENT_CYCLE);

        runClocks(ppu, vblankClocks);
        expectTrue((ppu.status & PPUSTATUS_VBLANK) != 0x00, "VBlank is set before pre-render scanline");

        runClocks(ppu, preRenderClocks - vblankClocks);
        expectFalse((ppu.status & PPUSTATUS_VBLANK) != 0x00, "pre-render scanline clears VBlank");
    }

    {
        PPU ppu;

        runClocks(ppu, PPU_CYCLES_PER_SCANLINE * PPU_SCANLINES_PER_FRAME);

        expectTrue(ppu.frameComplete, "PPU clock marks frame complete after one full frame");
        expectEqual16(ppu.scanline, 0x0000, "PPU scanline wraps to 0x0000 after one full frame");
        expectEqual16(ppu.cycle, 0x0000, "PPU cycle wraps to 0x0000 after one full frame");
    }

    std::printf("test_ppu passed\n");

    return 0;
}
