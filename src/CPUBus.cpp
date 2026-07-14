#include "CPUBus.h"
#include "Cartridge.h"
#include "Controller.h"
#include "PPU.h"
#include "APU.h"

namespace {

constexpr uint16_t INTERNAL_RAM_MASK = 0x07FF;
constexpr uint16_t INTERNAL_RAM_MIRROR_END = 0x1FFF;
constexpr uint16_t PPU_REGISTER_START = 0x2000;
constexpr uint16_t PPU_REGISTER_MIRROR_END = 0x3FFF;
constexpr uint16_t PPU_REGISTER_MASK = 0x0007;
constexpr uint16_t APU_REGISTER_START = 0x4000;
constexpr uint16_t APU_REGISTER_END = 0x4013;
constexpr uint16_t APU_STATUS_REGISTER = 0x4015;
constexpr uint16_t APU_FRAME_COUNTER_REGISTER = 0x4017;
constexpr uint16_t OAM_DMA_ADDRESS = 0x4014;
constexpr uint16_t CONTROLLER_STROBE_ADDRESS = 0x4016;
constexpr uint16_t CONTROLLER_ONE_READ_ADDRESS = 0x4016;
constexpr uint16_t CONTROLLER_TWO_READ_ADDRESS = 0x4017;
constexpr uint16_t PPU_OAM_DATA_REGISTER = 0x0004;
constexpr uint8_t CPU_PAGE_SHIFT = 8;
constexpr uint8_t DMC_DMA_STALL_CYCLES = 0x04;

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

    if((address >= APU_REGISTER_START && address <= APU_REGISTER_END)
        || address == APU_STATUS_REGISTER 
        || address == APU_FRAME_COUNTER_REGISTER) {
        if(apu) {
            apu->writeRegister(address, data);
        }

        return;
    }


    if(address == OAM_DMA_ADDRESS) {
        dmaPage = data;
        dmaOffset = 0x00;
        dmaActive = true;
        dmaWaiting = true;
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

uint8_t CPUBus::readDmc(uint16_t address) {
    // CPU bus accesses are currently instruction-atomic, so use the longest
    // normal DMC DMA stall instead of attempting read/write-cycle alignment.
    dmcDmaCyclesRemaining = DMC_DMA_STALL_CYCLES;
    return read(address);
}

void CPUBus::clockDma(bool oddCpuCycle) {
    if(!dmaActive) {
        return;
    }

    // DMA waits for a read-aligned CPU cycle before beginning the transfer.
    if(dmaWaiting) {
        if(oddCpuCycle) {
            dmaWaiting = false;
        }

        return;
    }

    if(!oddCpuCycle) {
        const uint16_t sourceAddress =
            (static_cast<uint16_t>(dmaPage) << CPU_PAGE_SHIFT) | dmaOffset;
        dmaData = read(sourceAddress);
        return;
    }

    if(ppu) {
        ppu->writeRegister(PPU_OAM_DATA_REGISTER, dmaData);
    }

    dmaOffset++;
    if(dmaOffset == 0x00) {
        dmaActive = false;
        dmaWaiting = true;
    }
}

bool CPUBus::isDmaActive() const {
    return dmaActive;
}

void CPUBus::clockDmcDma() {
    if(dmcDmaCyclesRemaining != 0x00) {
        dmcDmaCyclesRemaining--;
    }
}

bool CPUBus::isDmcDmaActive() const {
    return dmcDmaCyclesRemaining != 0x00;
}


// Component connections
void CPUBus::insertCartridge(Cartridge* cart) {
    cartridge = cart;
}

void CPUBus::connectPPU(PPU* ppup) {
    ppu = ppup;
}

void CPUBus::connectAPU(APU* apup) {
    apu = apup;
}

void CPUBus::connectController1(Controller* controller) {
    controller1 = controller;
}

void CPUBus::connectController2(Controller* controller) {
    controller2 = controller;
}
