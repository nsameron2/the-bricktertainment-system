#include "Bus.h"
#include "Cartridge.h"


// Functions for writing or reading into memory. Delegate memory or cartridge writing/reading through the given address
void Bus::write(uint16_t address, uint8_t data) {
    if(address <= INTERNAL_RAM_MIRROR_END) {
        memory[address & INTERNAL_RAM_MASK] = data;
        return;
    }

    if(cartridge && cartridge->cpuWrite(address, data)) {
        return;
    }
}

uint8_t Bus::read(uint16_t address) const {
    if(address <= INTERNAL_RAM_MIRROR_END) {
        return memory[address & INTERNAL_RAM_MASK];
    }

    // Safety for if there is a cartrdige read error
    uint8_t data = 0x00;
    if(cartridge && cartridge->cpuRead(address, data)) {
        return data;
    }


    return 0x00;
}


// Cartridge
void Bus::insertCartridge(Cartridge* cart) {
    cartridge = cart;
}
