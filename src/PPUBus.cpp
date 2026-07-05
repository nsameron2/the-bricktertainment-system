#include "PPUBus.h"
#include "Cartridge.h"

namespace {

constexpr uint16_t PPU_ADDRESS_MASK = 0x3FFF;
constexpr uint16_t PATTERN_TABLE_END = 0x1FFF;
constexpr uint16_t NAMETABLE_START = 0x2000;
constexpr uint16_t NAMETABLE_END = 0x3EFF;
constexpr uint16_t NAMETABLE_RAM_MASK = 0x07FF;
constexpr uint16_t PALETTE_START = 0x3F00;
constexpr uint16_t PALETTE_END = 0x3FFF;
constexpr uint16_t PALETTE_RAM_MASK = 0x001F;

uint16_t mirrorPaletteAddress(uint16_t address) {
    uint16_t paletteAddress = address & PALETTE_RAM_MASK;

    // Background palette mirrors used by the NES PPU.
    if (paletteAddress == 0x0010) {
        paletteAddress = 0x0000;
    } else if (paletteAddress == 0x0014) {
        paletteAddress = 0x0004;
    } else if (paletteAddress == 0x0018) {
        paletteAddress = 0x0008;
    } else if (paletteAddress == 0x001C) {
        paletteAddress = 0x000C;
    }

    return paletteAddress;
}

}

void PPUBus::insertCartridge(Cartridge* cart) {
    cartridge = cart;
}

void PPUBus::write(uint16_t address, uint8_t data) {
    address &= PPU_ADDRESS_MASK;

    if (address <= PATTERN_TABLE_END) {
        if (cartridge) {
            cartridge->ppuWrite(address, data);
        }

        return;
    }

    if (address >= NAMETABLE_START && address <= NAMETABLE_END) {
        nametableRam[(address - NAMETABLE_START) & NAMETABLE_RAM_MASK] = data;
        return;
    }

    if (address >= PALETTE_START && address <= PALETTE_END) {
        paletteRam[mirrorPaletteAddress(address)] = data;
        return;
    }
}

uint8_t PPUBus::read(uint16_t address) const {
    address &= PPU_ADDRESS_MASK;

    if (address <= PATTERN_TABLE_END) {
        uint8_t data = 0x00;
        if (cartridge && cartridge->ppuRead(address, data)) {
            return data;
        }

        return 0x00;
    }

    if (address >= NAMETABLE_START && address <= NAMETABLE_END) {
        return nametableRam[(address - NAMETABLE_START) & NAMETABLE_RAM_MASK];
    }

    if (address >= PALETTE_START && address <= PALETTE_END) {
        return paletteRam[mirrorPaletteAddress(address)];
    }

    return 0x00;
}
