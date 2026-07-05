#include "PPU.h"
#include "PPUBus.h"

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
