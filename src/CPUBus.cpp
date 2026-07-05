#include "CPUBus.h"
#include "Cartridge.h"
#include "PPU.h"


// Functions for writing or reading into CPU-visible memory.
void CPUBus::write(uint16_t address, uint8_t data) {
    if(address <= INTERNAL_RAM_MIRROR_END) {
        memory[address & INTERNAL_RAM_MASK] = data;
        return;
    }

    if(cartridge && cartridge->cpuWrite(address, data)) {
        return;
    }
}

uint8_t CPUBus::read(uint16_t address) const {
    if(address <= INTERNAL_RAM_MIRROR_END) {
        return memory[address & INTERNAL_RAM_MASK];
    }

    // Safety for if there is a cartridge read error.
    uint8_t data = 0x00;
    if(cartridge && cartridge->cpuRead(address, data)) {
        return data;
    }


    return 0x00;
}


// Component connections
void CPUBus::insertCartridge(Cartridge* cart) {
    cartridge = cart;
}

void CPUBus::connectPPU(PPU* ppup) {
    ppu = ppup;
}
