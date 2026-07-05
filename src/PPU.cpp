#include "PPU.h"
#include "PPUBus.h"

namespace {

constexpr uint16_t PPU_REGISTER_MASK = 0x0007;
constexpr uint16_t PPU_ADDRESS_MASK = 0x3FFF;
constexpr uint16_t PALETTE_START = 0x3F00;

constexpr uint8_t PPUCTRL_VRAM_INCREMENT = 1 << 2;
constexpr uint8_t PPUSTATUS_VBLANK = 1 << 7;

constexpr uint16_t COARSE_X_SCROLL_MASK = 0x001F;
constexpr uint16_t COARSE_Y_SCROLL_MASK = 0x03E0;
constexpr uint16_t NAMETABLE_SELECT_MASK = 0x0C00;
constexpr uint16_t FINE_Y_SCROLL_MASK = 0x7000;

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
        case 0x0000: // PPUCTRL
            control = data;
            tempVramAddress = (tempVramAddress & ~NAMETABLE_SELECT_MASK)
                | (static_cast<uint16_t>(data & 0x03) << 10);
            break;

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
