#include "PPU.h"
#include "PPUBus.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

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

    std::printf("test_ppu passed\n");

    return 0;
}
