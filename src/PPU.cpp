#include "PPU.h"
#include "PPUBus.h"

namespace {

constexpr uint16_t PPU_REGISTER_MASK = 0x0007;
constexpr uint16_t PPU_ADDRESS_MASK = 0x3FFF;
constexpr uint16_t PALETTE_START = 0x3F00;
constexpr uint16_t SPRITE_PALETTE_START = 0x3F10;

constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint16_t SCREEN_HEIGHT = 240;
constexpr uint16_t TILE_SIZE = 8;
constexpr uint16_t LEFTMOST_SCREEN_PIXELS = 8;
constexpr uint16_t NAMETABLE_BASE = 0x2000;
constexpr uint16_t ATTRIBUTE_TABLE_OFFSET = 0x03C0;
constexpr uint16_t NAMETABLE_ROW_TILES = 32;
constexpr uint16_t ATTRIBUTE_TABLE_ROW_BYTES = 8;
constexpr uint16_t BYTES_PER_PATTERN_TILE = 16;
constexpr uint16_t PATTERN_TILE_HIGH_PLANE_OFFSET = 8;
constexpr uint8_t SPRITE_COUNT = 64;
constexpr uint8_t OAM_BYTES_PER_SPRITE = 4;
constexpr uint8_t NES_PALETTE_INDEX_MASK = 0x3F;

constexpr int16_t PPU_CYCLES_PER_SCANLINE = 341;
constexpr int16_t PPU_SCANLINES_PER_FRAME = 262;
constexpr int16_t PPU_VBLANK_SCANLINE = 241;
constexpr int16_t PPU_PRE_RENDER_SCANLINE = 261;
constexpr int16_t PPU_STATUS_EVENT_CYCLE = 1;
constexpr int16_t PPU_VISIBLE_CYCLE_START = 1;
constexpr int16_t PPU_VISIBLE_CYCLE_END = 256;

constexpr uint8_t PPUCTRL_VRAM_INCREMENT = 1 << 2;
constexpr uint8_t PPUCTRL_SPRITE_PATTERN_TABLE = 1 << 3;
constexpr uint8_t PPUCTRL_BACKGROUND_PATTERN_TABLE = 1 << 4;
constexpr uint8_t PPUCTRL_NMI_ENABLE = 1 << 7;
constexpr uint8_t PPUMASK_GRAYSCALE = 1 << 0;
constexpr uint8_t PPUMASK_SHOW_BACKGROUND_LEFT = 1 << 1;
constexpr uint8_t PPUMASK_SHOW_SPRITES_LEFT = 1 << 2;
constexpr uint8_t PPUMASK_SHOW_BACKGROUND = 1 << 3;
constexpr uint8_t PPUMASK_SHOW_SPRITES = 1 << 4;
constexpr uint8_t PPUSTATUS_VBLANK = 1 << 7;
constexpr uint8_t GRAYSCALE_PALETTE_MASK = 0x30;

constexpr uint8_t SPRITE_PALETTE_MASK = 0x03;
constexpr uint8_t SPRITE_BEHIND_BACKGROUND = 0x20;
constexpr uint8_t SPRITE_FLIP_HORIZONTAL = 0x40;
constexpr uint8_t SPRITE_FLIP_VERTICAL = 0x80;

constexpr uint16_t COARSE_X_SCROLL_MASK = 0x001F;
constexpr uint16_t COARSE_Y_SCROLL_MASK = 0x03E0;
constexpr uint16_t NAMETABLE_SELECT_MASK = 0x0C00;
constexpr uint16_t NAMETABLE_SELECT_SHIFT = 10;
constexpr uint16_t NAMETABLE_AXIS_MASK = 0x0001;
constexpr uint16_t FINE_Y_SCROLL_MASK = 0x7000;

constexpr std::array<uint32_t, 64> NES_PALETTE = {
    0xFF545454, 0xFF001E74, 0xFF081090, 0xFF300088,
    0xFF440064, 0xFF5C0030, 0xFF540400, 0xFF3C1800,
    0xFF202A00, 0xFF083A00, 0xFF004000, 0xFF003C00,
    0xFF00323C, 0xFF000000, 0xFF000000, 0xFF000000,

    0xFF989698, 0xFF084CC4, 0xFF3032EC, 0xFF5C1EE4,
    0xFF8814B0, 0xFFA01464, 0xFF982220, 0xFF783C00,
    0xFF545A00, 0xFF287200, 0xFF087C00, 0xFF007628,
    0xFF006678, 0xFF000000, 0xFF000000, 0xFF000000,

    0xFFECEEEC, 0xFF4C9AEC, 0xFF787CEC, 0xFFB062EC,
    0xFFE454EC, 0xFFEC58B4, 0xFFEC6A64, 0xFFD48820,
    0xFFA0AA00, 0xFF74C400, 0xFF4CD020, 0xFF38CC6C,
    0xFF38B4CC, 0xFF3C3C3C, 0xFF000000, 0xFF000000,

    0xFFECEEEC, 0xFFA8CCEC, 0xFFBCBCEC, 0xFFD4B2EC,
    0xFFECAEEC, 0xFFECAED4, 0xFFECB4B0, 0xFFE4C490,
    0xFFCCD278, 0xFFB4DE78, 0xFFA8E290, 0xFF98E2B4,
    0xFFA0D6E4, 0xFFA0A2A0, 0xFF000000, 0xFF000000,
};

}

void PPU::clock() {
    if (scanline >= 0 && scanline < SCREEN_HEIGHT
        && cycle >= PPU_VISIBLE_CYCLE_START && cycle <= PPU_VISIBLE_CYCLE_END) {
        const uint16_t x = static_cast<uint16_t>(cycle - PPU_VISIBLE_CYCLE_START);
        const uint16_t y = static_cast<uint16_t>(scanline);

        Pixel pixel {
            static_cast<uint8_t>(readVram(PALETTE_START) & NES_PALETTE_INDEX_MASK),
            false
        };

        const bool backgroundEnabled = (mask & PPUMASK_SHOW_BACKGROUND) != 0x00;
        const bool backgroundInLeftColumn = (mask & PPUMASK_SHOW_BACKGROUND_LEFT) != 0x00;
        const bool isInLeftColumn = x < LEFTMOST_SCREEN_PIXELS;

        if (backgroundEnabled && (!isInLeftColumn || backgroundInLeftColumn)) {
            pixel = getBackgroundPixel(x, y);
        }

        const bool spriteEnabled = (mask & PPUMASK_SHOW_SPRITES) != 0x00;
        const bool spriteInLeftColumn = (mask & PPUMASK_SHOW_SPRITES_LEFT) != 0x00;

        if(spriteEnabled && (!isInLeftColumn || spriteInLeftColumn)) {
            const Pixel spritePixel = getSpritePixel(x, y);

            // If the sprite pixel is enabled, and if bg pixel is opaque or sprite is not behind backgorund, we make
            // our final framebuffer pixel the sprite pixel. If not, we go back to the background pixel.
            if (spritePixel.opaque
                && (!pixel.opaque || !spritePixel.behindBackground)) {
                pixel = spritePixel;
            }
        }


        if ((mask & PPUMASK_GRAYSCALE) != 0x00) {
            pixel.colorIndex &= GRAYSCALE_PALETTE_MASK;
        }

        // Finalize our pixel at our calculated x and y values
        framebuffer[(y * SCREEN_WIDTH) + x] = nesColorToRgb(pixel.colorIndex);
    }


    // VBlank handling
    if (scanline == PPU_VBLANK_SCANLINE && cycle == PPU_STATUS_EVENT_CYCLE) {
        status |= PPUSTATUS_VBLANK;

        if ((control & PPUCTRL_NMI_ENABLE) != 0x00) {
            nmiRequested = true;
        }
    }

    if (scanline == PPU_PRE_RENDER_SCANLINE && cycle == PPU_STATUS_EVENT_CYCLE) {
        status &= ~PPUSTATUS_VBLANK;
        frameComplete = false;
    }


    cycle++;


    if (cycle >= PPU_CYCLES_PER_SCANLINE) {
        cycle = 0;
        scanline++;

        if (scanline >= PPU_SCANLINES_PER_FRAME) {
            scanline = 0;
            frameComplete = true;
        }
    }
}

const std::array<uint32_t, 256 * 240>& PPU::getFramebuffer() const {
    return framebuffer;
}

bool PPU::isFrameComplete() const {
    return frameComplete;
}

void PPU::clearFrameComplete() {
    frameComplete = false;
}

bool PPU::isNmiComplete() {
    const bool request = nmiRequested;
    nmiRequested = false;

    return request;
}

uint8_t PPU::readRegister(uint16_t address) {
    switch (address & PPU_REGISTER_MASK) {
        case 0x0002: { // PPUSTATUS
            const uint8_t data = status;
            status &= ~PPUSTATUS_VBLANK;
            writeLatch = false;
            return data;
        }

        case 0x0004: // OAMDATA
            return oam[oamAddress];

        case 0x0007: { // PPUDATA
            const uint16_t currentAddress = vramAddress & PPU_ADDRESS_MASK;
            uint8_t data = readVram(currentAddress);

            if (currentAddress < PALETTE_START) {
                const uint8_t bufferedData = dataBuffer;
                dataBuffer = data;
                data = bufferedData;
            } else {
                dataBuffer = readVram(currentAddress - 0x1000);
            }

            vramAddress = (vramAddress + vramIncrement()) & PPU_ADDRESS_MASK;
            return data;
        }

        default:
            return 0x00;
    }
}

void PPU::writeRegister(uint16_t address, uint8_t data) {
    switch (address & PPU_REGISTER_MASK) {
        case 0x0000: { // PPUCTRL
            const bool wasNmiEnabled = (control & PPUCTRL_NMI_ENABLE) != 0x00;
            control = data;
            tempVramAddress = (tempVramAddress & ~NAMETABLE_SELECT_MASK)
                | (static_cast<uint16_t>(data & 0x03) << 10);

            const bool isNmiEnabled = (control & PPUCTRL_NMI_ENABLE) != 0x00;
            const bool isVBlankSet = (status & PPUSTATUS_VBLANK) != 0x00;
            if (!wasNmiEnabled && isNmiEnabled && isVBlankSet) {
                nmiRequested = true;
            }
            break;
        }

        case 0x0001: // PPUMASK
            mask = data;
            break;

        case 0x0003: // OAMADDR
            oamAddress = data;
            break;

        case 0x0004: // OAMDATA
            oam[oamAddress] = data;
            oamAddress++;
            break;

        case 0x0005: // PPUSCROLL
            if (!writeLatch) {
                fineX = data & 0x07;
                tempVramAddress = (tempVramAddress & ~COARSE_X_SCROLL_MASK)
                    | (static_cast<uint16_t>(data >> 3) & COARSE_X_SCROLL_MASK);
                writeLatch = true;
            } else {
                tempVramAddress = (tempVramAddress & ~FINE_Y_SCROLL_MASK)
                    | (static_cast<uint16_t>(data & 0x07) << 12);
                tempVramAddress = (tempVramAddress & ~COARSE_Y_SCROLL_MASK)
                    | (static_cast<uint16_t>(data & 0xF8) << 2);
                writeLatch = false;
            }
            break;

        case 0x0006: // PPUADDR
            if (!writeLatch) {
                tempVramAddress = (tempVramAddress & 0x00FF)
                    | (static_cast<uint16_t>(data & 0x3F) << 8);
                writeLatch = true;
            } else {
                tempVramAddress = (tempVramAddress & 0xFF00) | data;
                vramAddress = tempVramAddress & PPU_ADDRESS_MASK;
                writeLatch = false;
            }
            break;

        case 0x0007: // PPUDATA
            writeVram(vramAddress, data);
            vramAddress = (vramAddress + vramIncrement()) & PPU_ADDRESS_MASK;
            break;

        default:
            break;
    }
}

uint16_t PPU::vramIncrement() const {
    return (control & PPUCTRL_VRAM_INCREMENT) ? 0x0020 : 0x0001;
}

void PPU::writeVram(uint16_t address, uint8_t data) {
    if (bus) {
        bus->write(address, data);
    }
}

uint8_t PPU::readVram(uint16_t address) const {
    if (bus) {
        return bus->read(address);
    }

    return 0x00;
}

PPU::Pixel PPU::getBackgroundPixel(uint16_t x, uint16_t y) const {
    // Scroll handling. We get the amount to scroll, offset by that amount, then pass it into nametable handling
    const uint16_t scrollX = ((tempVramAddress & COARSE_X_SCROLL_MASK) << 3) | this->fineX;
    const uint16_t scrollY = ((tempVramAddress & COARSE_Y_SCROLL_MASK) >> 2)
        | ((tempVramAddress & FINE_Y_SCROLL_MASK) >> 12);

    const uint16_t sourceX = x + scrollX;
    const uint16_t sourceY = y + scrollY;

    const uint16_t localX = sourceX % SCREEN_WIDTH;
    const uint16_t localY = sourceY % SCREEN_HEIGHT;

    const uint16_t tileX = localX / TILE_SIZE;
    const uint16_t tileY = localY / TILE_SIZE;
    const uint16_t tileFineX = localX % TILE_SIZE;
    const uint16_t tileFineY = localY % TILE_SIZE;

    const uint16_t initialNametable =
        (tempVramAddress & NAMETABLE_SELECT_MASK) >> NAMETABLE_SELECT_SHIFT;
    const uint16_t horizontalNametable =
        (sourceX / SCREEN_WIDTH) & NAMETABLE_AXIS_MASK;
    const uint16_t verticalNametable =
        (sourceY / SCREEN_HEIGHT) & NAMETABLE_AXIS_MASK;
    const uint16_t selectedNametable = initialNametable
        ^ horizontalNametable
        ^ (verticalNametable << 1);

    const uint16_t nametableBase = NAMETABLE_BASE
        + (selectedNametable << NAMETABLE_SELECT_SHIFT);
    const uint16_t nametableAddress = nametableBase
        + (tileY * NAMETABLE_ROW_TILES)
        + tileX;
    const uint8_t tileId = readVram(nametableAddress);

    const uint16_t patternBase = (control & PPUCTRL_BACKGROUND_PATTERN_TABLE) ? 0x1000 : 0x0000;
    const uint16_t patternAddress = patternBase
        + (static_cast<uint16_t>(tileId) * BYTES_PER_PATTERN_TILE)
        + tileFineY;

    const uint8_t patternLow = readVram(patternAddress);
    const uint8_t patternHigh = readVram(patternAddress + PATTERN_TILE_HIGH_PLANE_OFFSET);

    const uint8_t bit = static_cast<uint8_t>(7 - tileFineX);
    const uint8_t colorId = static_cast<uint8_t>(
        (((patternHigh >> bit) & 0x01) << 1)
        | ((patternLow >> bit) & 0x01));

    const uint16_t attributeAddress = (nametableBase + ATTRIBUTE_TABLE_OFFSET)
        + ((tileY / 4) * ATTRIBUTE_TABLE_ROW_BYTES)
        + (tileX / 4);
    const uint8_t attributeByte = readVram(attributeAddress);

    const uint8_t quadrantX = static_cast<uint8_t>((tileX % 4) / 2);
    const uint8_t quadrantY = static_cast<uint8_t>((tileY % 4) / 2);
    const uint8_t attributeShift = static_cast<uint8_t>((quadrantY * 4) + (quadrantX * 2));
    const uint8_t paletteId = static_cast<uint8_t>((attributeByte >> attributeShift) & 0x03);

    uint16_t paletteAddress = PALETTE_START + (static_cast<uint16_t>(paletteId) * 4) + colorId;
    if (colorId == 0x00) {
        paletteAddress = PALETTE_START;
    }

    return {
        static_cast<uint8_t>(readVram(paletteAddress) & NES_PALETTE_INDEX_MASK),
        colorId != 0x00,
    };
}

PPU::Pixel PPU::getSpritePixel(uint16_t x, uint16_t y) const {
    // Scan the OAM for the right sprite
    for (uint8_t spriteIndex = 0; spriteIndex < SPRITE_COUNT; spriteIndex++) {
        const uint16_t oamOffset = static_cast<uint16_t>(spriteIndex) * OAM_BYTES_PER_SPRITE;

        // OAM:
        // byte 0: Y
        // byte 1: tile ID
        // byte 2: attributes
        // byte 3: X
        const uint16_t spriteY = static_cast<uint16_t>(oam[oamOffset]) + 1;
        const uint16_t spriteX = oam[oamOffset + 3];

        // If the gotten sprite is out of bounds, go next
        if (x < spriteX || x >= spriteX + TILE_SIZE
            || y < spriteY || y >= spriteY + TILE_SIZE) {
            continue;
        }

        const uint8_t tileId = oam[oamOffset + 1];
        const uint8_t attrs = oam[oamOffset + 2];

        // Pattern decoding will decide whether this covering sprite is transparent
        uint16_t spriteLocalX = x - spriteX;
        uint16_t spriteLocalY = y - spriteY;

        if ((attrs & SPRITE_FLIP_HORIZONTAL) != 0x00) {
            spriteLocalX = (TILE_SIZE - 1) - spriteLocalX;
        }

        if ((attrs & SPRITE_FLIP_VERTICAL) != 0x00) {
            spriteLocalY = (TILE_SIZE - 1) - spriteLocalY;
        }

        const uint16_t patternBase = (control & PPUCTRL_SPRITE_PATTERN_TABLE) ? 0x1000 : 0x0000;
        const uint16_t patternAddress = patternBase
            + (static_cast<uint16_t>(tileId) * BYTES_PER_PATTERN_TILE)
            + spriteLocalY;

        const uint8_t patternLow = readVram(patternAddress);
        const uint8_t patternHigh = readVram(patternAddress + PATTERN_TILE_HIGH_PLANE_OFFSET);

        const uint8_t bit = static_cast<uint8_t>(7 - spriteLocalX);
        const uint8_t colorId = static_cast<uint8_t>(
            (((patternHigh >> bit) & 0x01) << 1)
            | ((patternLow >> bit) & 0x01));

        if (colorId == 0x00) {
            continue;
        }

        const uint8_t paletteId = attrs & SPRITE_PALETTE_MASK;
        const uint16_t paletteAddress = SPRITE_PALETTE_START
            + (static_cast<uint16_t>(paletteId) * 4)
            + colorId;

        return {
            static_cast<uint8_t>(readVram(paletteAddress) & NES_PALETTE_INDEX_MASK),
            true,
            (attrs & SPRITE_BEHIND_BACKGROUND) != 0x00,
        };
    }

    return {0x00, false};
}

uint32_t PPU::nesColorToRgb(uint8_t colorIndex) const {
    return NES_PALETTE[colorIndex & NES_PALETTE_INDEX_MASK];
}
