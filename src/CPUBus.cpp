#include "CPUBus.h"
#include "Cartridge.h"
#include "Controller.h"
#include "PPU.h"

namespace {

constexpr uint16_t INTERNAL_RAM_MASK = 0x07FF;
constexpr uint16_t INTERNAL_RAM_MIRROR_END = 0x1FFF;
constexpr uint16_t PPU_REGISTER_START = 0x2000;
constexpr uint16_t PPU_REGISTER_MIRROR_END = 0x3FFF;
constexpr uint16_t PPU_REGISTER_MASK = 0x0007;
constexpr uint16_t CONTROLLER_STROBE_ADDRESS = 0x4016;
constexpr uint16_t CONTROLLER_ONE_READ_ADDRESS = 0x4016;
constexpr uint16_t CONTROLLER_TWO_READ_ADDRESS = 0x4017;

}


// Functions for writing or reading into CPU-visible memory.
void CPUBus::write(uint16_t address, uint8_t data) {
    if(address <= INTERNAL_RAM_MIRROR_END) {
        memory[address & INTERNAL_RAM_MASK] = data;
        return;
    }

    if(address >= PPU_REGISTER_START && address <= PPU_REGISTER_MIRROR_END) {
        if(ppu) {
            ppu->writeRegister(address & PPU_REGISTER_MASK, data);
        }

        return;
    }

    if(address == CONTROLLER_STROBE_ADDRESS) {
        if(controller1) {
            controller1->write(data);
        }

        if(controller2) {
            controller2->write(data);
        }

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

    if(address >= PPU_REGISTER_START && address <= PPU_REGISTER_MIRROR_END) {
        if(ppu) {
            return ppu->readRegister(address & PPU_REGISTER_MASK);
        }

        return 0x00;
    }

    if(address == CONTROLLER_ONE_READ_ADDRESS) {
        if(controller1) {
            return controller1->read();
        }

        return 0x00;
    }

    if(address == CONTROLLER_TWO_READ_ADDRESS) {
        if(controller2) {
            return controller2->read();
        }

        return 0x00;
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

void CPUBus::connectController1(Controller* controller) {
    controller1 = controller;
}

void CPUBus::connectController2(Controller* controller) {
    controller2 = controller;
}
