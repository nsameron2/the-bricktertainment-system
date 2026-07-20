#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "core/PPU.h"

#include "core/PPUBus.h"
#include "core/Cartridge.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

constexpr size_t INES_HEADER_SIZE = 16;
constexpr size_t PRG_BANK_SIZE = 16 * 1024;
constexpr size_t SCREEN_WIDTH = 256;
constexpr uint8_t INES_FOUR_SCREEN_VRAM = 1 << 3;
constexpr uint8_t OAM_BYTES_PER_SPRITE = 4;

constexpr int16_t PPU_CYCLES_PER_SCANLINE = 341;
constexpr int16_t PPU_SCANLINES_PER_FRAME = 262;
constexpr int16_t PPU_VBLANK_SCANLINE = 241;
constexpr int16_t PPU_PRE_RENDER_SCANLINE = 261;
constexpr int16_t PPU_STATUS_EVENT_CYCLE = 1;
constexpr uint8_t PPUCTRL_NAMETABLE_3 = 0x03;
constexpr uint8_t PPUCTRL_NMI_ENABLE = 1 << 7;
constexpr uint8_t PPUCTRL_SPRITE_PATTERN_TABLE = 1 << 3;
constexpr uint8_t PPUMASK_GRAYSCALE = 1 << 0;
constexpr uint8_t PPUMASK_SHOW_BACKGROUND_LEFT = 1 << 1;
constexpr uint8_t PPUMASK_SHOW_SPRITES_LEFT = 1 << 2;
constexpr uint8_t PPUMASK_SHOW_BACKGROUND = 1 << 3;
constexpr uint8_t PPUMASK_SHOW_SPRITES = 1 << 4;
constexpr uint8_t PPUSTATUS_SPRITE_ZERO_HIT = 1 << 6;
constexpr uint8_t PPUSTATUS_VBLANK = 1 << 7;
constexpr uint8_t SPRITE_PALETTE_ONE = 0x01;
constexpr uint8_t SPRITE_BEHIND_BACKGROUND = 0x20;
constexpr uint8_t SPRITE_FLIP_HORIZONTAL = 0x40;
constexpr uint8_t SPRITE_FLIP_VERTICAL = 0x80;

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

void setSprite(PPU& ppu,
               uint8_t spriteIndex,
               uint8_t y,
               uint8_t tileId,
               uint8_t attributes,
               uint8_t x) {
    const uint16_t oamOffset = static_cast<uint16_t>(spriteIndex) * OAM_BYTES_PER_SPRITE;
    ppu.oam[oamOffset] = y;
    ppu.oam[oamOffset + 1] = tileId;
    ppu.oam[oamOffset + 2] = attributes;
    ppu.oam[oamOffset + 3] = x;
}

void prepareSpriteCompositionScene(PPU& ppu, PPUBus& bus) {
    ppu.oam.fill(0xFF);

    bus.write(0x2000, 0x01);
    bus.write(0x2001, 0x01);
    bus.write(0x2002, 0x01);
    bus.write(0x2003, 0x01);
    bus.write(0x0011, 0x80); // Background tile 0x01, row 0x01: left pixel is color 0x01.
    bus.write(0x0019, 0x00);
    bus.write(0x0020, 0x80); // Sprite tile 0x02, row 0x00: left pixel is color 0x01.
    bus.write(0x0028, 0x00);
    bus.write(0x23C0, 0x00);

    bus.write(0x3F00, 0x0F); // Universal background color.
    bus.write(0x3F01, 0x2A); // Background palette 0x00, color 0x01.
    bus.write(0x3F11, 0x2D); // Sprite palette 0x00, color 0x01.
}

int clocksToProcessDot(int16_t scanline, int16_t cycle) {
    return (scanline * PPU_CYCLES_PER_SCANLINE) + cycle + 1;
}

std::array<uint8_t, INES_HEADER_SIZE> makeHeader(uint8_t prgBanks,
                                                 uint8_t chrBanks,
                                                 uint8_t flags6 = 0x00) {
    std::array<uint8_t, INES_HEADER_SIZE> header{};
    header[0] = 'N';
    header[1] = 'E';
    header[2] = 'S';
    header[3] = 0x1A;
    header[4] = prgBanks;
    header[5] = chrBanks;
    header[6] = flags6;
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
    writeRomData(chrRamPath,
                 makeHeader(1, 0, INES_FOUR_SCREEN_VRAM),
                 std::vector<uint8_t>(PRG_BANK_SIZE, 0xEA),
                 {});
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

        expectEqual(ppu.getBackgroundPixel(0x0000, 0x0000).colorIndex,
                    0x2A,
                    "background pixel combines nametable, pattern, attribute, and palette data");
        expectTrue(ppu.getBackgroundPixel(0x0000, 0x0000).opaque,
                   "nonzero background pattern color is opaque");
        expectEqual(ppu.getBackgroundPixel(0x0001, 0x0000).colorIndex,
                    0x0F,
                    "background color 0 uses universal background palette entry");
        expectFalse(ppu.getBackgroundPixel(0x0001, 0x0000).opaque,
                    "background pattern color 0 is transparent to sprites");
        expectEqual32(ppu.nesColorToRgb(0x2A),
                      0xFF4CD020,
                      "NES color index converts to 0xAARRGGBB");
        expectEqual32(ppu.nesColorToRgb(0x6A),
                      0xFF4CD020,
                      "NES color conversion masks color index to 0x00-0x3F");

        bus.write(0x3F11, 0x2D); // Sprite palette 0x00, color 0x01.
        bus.write(0x3F15, 0x2E); // Sprite palette 0x01, color 0x01.
        bus.write(0x0010, 0x80); // Tile 0x01, row 0: left pixel color bit 0 set.
        setSprite(ppu, 0x00, 0x00, 0x00, 0x00, 0x00); // Transparent sprite pixel.
        setSprite(ppu,
                  0x01,
                  0x00,
                  0x01,
                  SPRITE_PALETTE_ONE | SPRITE_BEHIND_BACKGROUND,
                  0x00);

        ppu.evaluateSpritesForScanline(0x0001);
        const auto firstSpritePixel = ppu.getSpritePixel(0x0000);
        expectEqual(firstSpritePixel.pixel.colorIndex,
                    0x2E,
                    "sprite sampler skips transparent sprites and reads the sprite palette");
        expectTrue(firstSpritePixel.pixel.opaque,
                   "nonzero sprite pattern color is opaque");
        expectTrue(firstSpritePixel.behindBackground,
                   "sprite attribute bit 5 marks a sprite behind the background");
        expectFalse(firstSpritePixel.isSpriteZero,
                    "transparent sprite zero does not mark a later opaque sprite as sprite zero");

        bus.write(0x0020, 0x01); // Tile 0x02, row 0: right pixel color bit 0 set.
        setSprite(ppu, 0x02, 0x00, 0x02, SPRITE_FLIP_HORIZONTAL, 0x08);
        ppu.evaluateSpritesForScanline(0x0001);
        const auto horizontalFlipPixel = ppu.getSpritePixel(0x0008);
        expectEqual(horizontalFlipPixel.pixel.colorIndex,
                    0x2D,
                    "horizontal sprite flip reverses the pattern column");

        bus.write(0x0037, 0x80); // Tile 0x03, row 7: left pixel color bit 0 set.
        setSprite(ppu, 0x03, 0x00, 0x03, SPRITE_FLIP_VERTICAL, 0x10);
        ppu.evaluateSpritesForScanline(0x0001);
        const auto verticalFlipPixel = ppu.getSpritePixel(0x0010);
        expectEqual(verticalFlipPixel.pixel.colorIndex,
                    0x2D,
                    "vertical sprite flip reverses the pattern row");

        bus.write(0x1040, 0x80); // Tile 0x04, row 0 in sprite pattern table 0x1000.
        setSprite(ppu, 0x04, 0x00, 0x04, 0x00, 0x18);
        ppu.writeRegister(0x2000, PPUCTRL_SPRITE_PATTERN_TABLE);
        ppu.evaluateSpritesForScanline(0x0001);
        const auto spritePatternTablePixel = ppu.getSpritePixel(0x0018);
        expectEqual(spritePatternTablePixel.pixel.colorIndex,
                    0x2D,
                    "PPUCTRL bit 3 selects sprite pattern table 0x1000");
        ppu.writeRegister(0x2000, 0x00);
        bus.write(0x0020, 0x80); // Restore tile 0x02 for the background framebuffer test.

        bus.write(0x2001, 0x03); // Tile index at nametable tile 0x0001.
        bus.write(0x0030, 0x80); // Tile 0x03, row 0: left pixel color bit 0 set.
        ppu.writeRegister(0x2005, 0x08);
        ppu.writeRegister(0x2005, 0x00);
        expectEqual(ppu.getBackgroundPixel(0x0000, 0x0000).colorIndex,
                    0x2A,
                    "PPUSCROLL coarse X selects the next background tile");

        bus.write(0x2000, 0x04); // Tile index at nametable tile 0x0000.
        bus.write(0x0040, 0x40); // Tile 0x04, row 0: second pixel has color bit 0 set.
        ppu.writeRegister(0x2005, 0x01);
        ppu.writeRegister(0x2005, 0x00);
        expectEqual(ppu.getBackgroundPixel(0x0000, 0x0000).colorIndex,
                    0x2A,
                    "PPUSCROLL fine X uses the stored fine-scroll value");

        bus.write(0x2400, 0x05); // First tile in the right nametable.
        bus.write(0x0050, 0x80); // Tile 0x05, row 0: left pixel color bit 0 set.
        bus.write(0x27C0, 0x01); // Right nametable uses background palette 0x01.
        bus.write(0x3F05, 0x29); // Palette 0x01, color 0x01.
        ppu.writeRegister(0x2005, 0xFF);
        ppu.writeRegister(0x2005, 0x00);
        expectEqual(ppu.getBackgroundPixel(0x0001, 0x0000).colorIndex,
                    0x29,
                    "PPUSCROLL wraps right into the adjacent nametable and attribute table");

        bus.write(0x2800, 0x06); // First tile in the lower nametable.
        bus.write(0x0060, 0x80); // Tile 0x06, row 0: left pixel color bit 0 set.
        bus.write(0x2BC0, 0x03); // Lower nametable uses background palette 0x03.
        bus.write(0x3F0D, 0x2B); // Palette 0x03, color 0x01.
        ppu.writeRegister(0x2005, 0x00);
        ppu.writeRegister(0x2005, 0xEF);
        expectEqual(ppu.getBackgroundPixel(0x0000, 0x0001).colorIndex,
                    0x2B,
                    "PPUSCROLL wraps down into the adjacent nametable and attribute table");

        bus.write(0x2000, 0x02);
        ppu.writeRegister(0x2005, 0x00);
        ppu.writeRegister(0x2005, 0x00);

        ppu.writeRegister(0x2001, PPUMASK_SHOW_BACKGROUND | PPUMASK_SHOW_BACKGROUND_LEFT);
        runClocks(ppu, 0x0002);
        expectEqual32(ppu.getFramebuffer()[0x0000],
                      0xFF4CD020,
                      "PPU clock writes converted RGB background pixel to framebuffer");
    }

    {
        Cartridge cartridge;
        PPUBus bus;
        PPU ppu;

        expectTrue(cartridge.load(chrRamPath.string().c_str()),
                   "CHR RAM cartridge loads for sprite composition test");

        bus.insertCartridge(&cartridge);
        ppu.connectBus(&bus);
        prepareSpriteCompositionScene(ppu, bus);

        setSprite(ppu, 0x00, 0x00, 0x02, 0x00, 0x00);
        setSprite(ppu, 0x01, 0x00, 0x02, 0x00, 0x08);
        setSprite(ppu, 0x02, 0x00, 0x02, SPRITE_BEHIND_BACKGROUND, 0x10);
        setSprite(ppu, 0x03, 0x00, 0x00, 0x00, 0x18);
        setSprite(ppu, 0x04, 0x00, 0x02, SPRITE_BEHIND_BACKGROUND, 0x20);

        ppu.writeRegister(0x2001,
                          PPUMASK_SHOW_BACKGROUND_LEFT
                              | PPUMASK_SHOW_BACKGROUND
                              | PPUMASK_SHOW_SPRITES);
        runClocks(ppu, clocksToProcessDot(0x0001, 0x0021));

        const auto& framebuffer = ppu.getFramebuffer();
        expectEqual32(framebuffer[SCREEN_WIDTH],
                      0xFF4CD020,
                      "PPUMASK clips sprites from the leftmost eight pixels");
        expectEqual32(framebuffer[SCREEN_WIDTH + 0x0008],
                      0xFF3C3C3C,
                      "opaque foreground sprite replaces an opaque background pixel");
        expectEqual32(framebuffer[SCREEN_WIDTH + 0x0010],
                      0xFF4CD020,
                      "sprite behind an opaque background keeps the background pixel");
        expectEqual32(framebuffer[SCREEN_WIDTH + 0x0018],
                      0xFF4CD020,
                      "transparent sprite keeps the background pixel");
        expectEqual32(framebuffer[SCREEN_WIDTH + 0x0020],
                      0xFF3C3C3C,
                      "sprite behind a transparent background remains visible");
        expectFalse((ppu.status & PPUSTATUS_SPRITE_ZERO_HIT) != 0x00,
                    "left-column clipping prevents sprite zero hit");
    }

    {
        Cartridge cartridge;
        PPUBus bus;
        PPU ppu;

        expectTrue(cartridge.load(chrRamPath.string().c_str()),
                   "CHR RAM cartridge loads for sprite zero hit test");

        bus.insertCartridge(&cartridge);
        ppu.connectBus(&bus);
        prepareSpriteCompositionScene(ppu, bus);
        setSprite(ppu, 0x00, 0x00, 0x02, SPRITE_BEHIND_BACKGROUND, 0x08);

        ppu.writeRegister(0x2001, PPUMASK_SHOW_BACKGROUND | PPUMASK_SHOW_SPRITES);
        const int spriteZeroHitClocks = clocksToProcessDot(0x0001, 0x0009);
        runClocks(ppu, spriteZeroHitClocks);

        expectTrue((ppu.status & PPUSTATUS_SPRITE_ZERO_HIT) != 0x00,
                   "opaque sprite zero sets PPUSTATUS bit 6 over an opaque background");
        expectEqual(ppu.readRegister(0x2002) & PPUSTATUS_SPRITE_ZERO_HIT,
                    PPUSTATUS_SPRITE_ZERO_HIT,
                    "PPUSTATUS read returns sprite zero hit");
        expectTrue((ppu.status & PPUSTATUS_SPRITE_ZERO_HIT) != 0x00,
                   "PPUSTATUS read does not clear sprite zero hit");

        const int preRenderClocks = clocksToProcessDot(PPU_PRE_RENDER_SCANLINE,
                                                       PPU_STATUS_EVENT_CYCLE);
        runClocks(ppu, preRenderClocks - spriteZeroHitClocks);
        expectFalse((ppu.status & PPUSTATUS_SPRITE_ZERO_HIT) != 0x00,
                    "pre-render scanline clears sprite zero hit");
    }

    {
        Cartridge cartridge;
        PPUBus bus;
        PPU ppu;

        expectTrue(cartridge.load(chrRamPath.string().c_str()),
                   "CHR RAM cartridge loads for disabled sprite rendering test");

        bus.insertCartridge(&cartridge);
        ppu.connectBus(&bus);
        prepareSpriteCompositionScene(ppu, bus);
        setSprite(ppu, 0x00, 0x00, 0x02, 0x00, 0x08);

        ppu.writeRegister(0x2001, PPUMASK_SHOW_BACKGROUND);
        runClocks(ppu, clocksToProcessDot(0x0001, 0x0009));

        expectEqual32(ppu.getFramebuffer()[SCREEN_WIDTH + 0x0008],
                      0xFF4CD020,
                      "disabled sprite rendering leaves the background pixel visible");
    }

    {
        Cartridge cartridge;
        PPUBus bus;
        PPU ppu;

        expectTrue(cartridge.load(chrRamPath.string().c_str()),
                   "CHR RAM cartridge loads for left-column sprite rendering test");

        bus.insertCartridge(&cartridge);
        ppu.connectBus(&bus);
        prepareSpriteCompositionScene(ppu, bus);
        setSprite(ppu, 0x00, 0x00, 0x02, 0x00, 0x00);

        ppu.writeRegister(0x2001,
                          PPUMASK_GRAYSCALE
                              | PPUMASK_SHOW_SPRITES_LEFT
                              | PPUMASK_SHOW_SPRITES);
        runClocks(ppu, clocksToProcessDot(0x0001, 0x0001));

        expectEqual32(ppu.getFramebuffer()[SCREEN_WIDTH],
                      0xFFECEEEC,
                      "left-column sprite rendering and grayscale apply to the final sprite pixel");
    }

    {
        Cartridge cartridge;
        PPUBus bus;
        PPU ppu;

        expectTrue(cartridge.load(chrRamPath.string().c_str()),
                   "CHR RAM cartridge loads for vertical scroll wrap test");

        bus.insertCartridge(&cartridge);
        ppu.connectBus(&bus);

        bus.write(0x2C00, 0x07); // First tile in nametable 0x03.
        bus.write(0x0070, 0x80); // Tile 0x07, row 0: left pixel color bit 0 set.
        bus.write(0x2FC0, 0x00); // Nametable 0x03 uses background palette 0x00.
        bus.write(0x3F00, 0x03); // Distinguishes an incorrect palette-region attribute read.
        bus.write(0x3F01, 0x2C); // Palette 0x00, color 0x01.
        bus.write(0x3F0D, 0x2B); // Palette 0x03, color 0x01.

        ppu.writeRegister(0x2000, PPUCTRL_NAMETABLE_3);
        ppu.writeRegister(0x2005, 0x00);
        ppu.writeRegister(0x2005, 0xFF);

        expectEqual(ppu.getBackgroundPixel(0x0000, 0x00E1).colorIndex,
                    0x2C,
                    "vertical scroll wraps twice without leaving nametables 0x00-0x03");
    }
    std::filesystem::remove(chrRamPath);

    std::printf("test_ppu passed\n");

    return 0;
}
