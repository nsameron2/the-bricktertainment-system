#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define private public
#include "PPU.h"
#undef private

#include "PPUBus.h"
#include "Cartridge.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

constexpr size_t INES_HEADER_SIZE = 16;
constexpr size_t PRG_BANK_SIZE = 16 * 1024;

constexpr int16_t PPU_CYCLES_PER_SCANLINE = 341;
constexpr int16_t PPU_SCANLINES_PER_FRAME = 262;
constexpr int16_t PPU_VBLANK_SCANLINE = 241;
constexpr int16_t PPU_PRE_RENDER_SCANLINE = 261;
constexpr int16_t PPU_STATUS_EVENT_CYCLE = 1;
constexpr uint8_t PPUCTRL_NMI_ENABLE = 1 << 7;
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

void expectEqual32(uint32_t actual, uint32_t expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%08X, got 0x%08X)\n",
                     message,
                     expected,
                     actual);
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

std::array<uint8_t, INES_HEADER_SIZE> makeHeader(uint8_t prgBanks,
                                                 uint8_t chrBanks) {
    std::array<uint8_t, INES_HEADER_SIZE> header{};
    header[0] = 'N';
    header[1] = 'E';
    header[2] = 'S';
    header[3] = 0x1A;
    header[4] = prgBanks;
    header[5] = chrBanks;
    return header;
}

void writeBytes(std::ofstream& file, const std::vector<uint8_t>& bytes) {
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

void writeRomData(const std::filesystem::path& path,
                  const std::array<uint8_t, INES_HEADER_SIZE>& header,
                  const std::vector<uint8_t>& prgData,
                  const std::vector<uint8_t>& chrData) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(header.data()),
               static_cast<std::streamsize>(header.size()));
    writeBytes(file, prgData);
    writeBytes(file, chrData);
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

        ppu.writeRegister(0x2000, PPUCTRL_NMI_ENABLE);
        runClocks(ppu, clocksToProcessDot(PPU_VBLANK_SCANLINE, PPU_STATUS_EVENT_CYCLE));

        expectTrue(ppu.isNmiComplete(), "PPU requests NMI when VBlank starts with PPUCTRL bit 0x80 set");
        expectFalse(ppu.isNmiComplete(), "PPU NMI request is consumed after polling");
    }

    {
        PPU ppu;

        runClocks(ppu, clocksToProcessDot(PPU_VBLANK_SCANLINE, PPU_STATUS_EVENT_CYCLE));
        expectFalse(ppu.isNmiComplete(), "PPU does not request NMI when PPUCTRL bit 0x80 is clear");

        ppu.writeRegister(0x2000, PPUCTRL_NMI_ENABLE);
        expectTrue(ppu.isNmiComplete(), "PPU requests NMI when PPUCTRL bit 0x80 is set during VBlank");
    }

    {
        PPU ppu;

        runClocks(ppu, PPU_CYCLES_PER_SCANLINE * PPU_SCANLINES_PER_FRAME);

        expectTrue(ppu.isFrameComplete(), "PPU clock marks frame complete after one full frame");
        expectEqual16(ppu.scanline, 0x0000, "PPU scanline wraps to 0x0000 after one full frame");
        expectEqual16(ppu.cycle, 0x0000, "PPU cycle wraps to 0x0000 after one full frame");

        ppu.clearFrameComplete();
        expectFalse(ppu.isFrameComplete(), "clearFrameComplete clears the frame completion flag");
    }

    const auto chrRamPath = std::filesystem::temp_directory_path() / "brick_test_ppu_background_chr_ram.nes";
    writeRomData(chrRamPath, makeHeader(1, 0), std::vector<uint8_t>(PRG_BANK_SIZE, 0xEA), {});
    {
        Cartridge cartridge;
        PPUBus bus;
        PPU ppu;

        expectTrue(cartridge.load(chrRamPath.string().c_str()),
                   "CHR RAM cartridge loads for background pixel test");

        bus.insertCartridge(&cartridge);
        ppu.connectBus(&bus);

        bus.write(0x2000, 0x02); // Tile index at nametable tile 0x0000.
        bus.write(0x0020, 0x80); // Tile 0x02, row 0, low plane: left pixel color bit 0 set.
        bus.write(0x0028, 0x00); // Tile 0x02, row 0, high plane: left pixel color bit 1 clear.
        bus.write(0x23C0, 0x02); // Top-left attribute quadrant uses background palette 0x02.
        bus.write(0x3F00, 0x0F); // Universal background color.
        bus.write(0x3F09, 0x2A); // Palette 0x02, color 0x01.

        expectEqual(ppu.getBackgroundPixel(0x0000, 0x0000),
                    0x2A,
                    "background pixel combines nametable, pattern, attribute, and palette data");
        expectEqual(ppu.getBackgroundPixel(0x0001, 0x0000),
                    0x0F,
                    "background color 0 uses universal background palette entry");
        expectEqual32(ppu.nesColorToRgb(0x2A),
                      0xFF4CD020,
                      "NES color index converts to 0xAARRGGBB");
        expectEqual32(ppu.nesColorToRgb(0x6A),
                      0xFF4CD020,
                      "NES color conversion masks color index to 0x00-0x3F");

        runClocks(ppu, 0x0002);
        expectEqual32(ppu.getFramebuffer()[0x0000],
                      0xFF4CD020,
                      "PPU clock writes converted RGB background pixel to framebuffer");
    }
    std::filesystem::remove(chrRamPath);

    std::printf("test_ppu passed\n");

    return 0;
}
