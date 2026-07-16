#include "Console.h"


Console::Console() {
    cpuBus.insertCartridge(&cartridge);
    cpuBus.connectPPU(&ppu);
    cpuBus.connectAPU(&apu);
    cpuBus.connectController1(&controller1);

    ppuBus.insertCartridge(&cartridge);
    ppu.connectBus(&ppuBus);

    apu.connectBus(&cpuBus);
    cpu.connectBus(&cpuBus);
}

bool Console::initialize(const char* romPath) {
    if(!cartridge.load(romPath)) {
        return false;
    }

    cpu.powerOn();
    cpuCycle = 0;
    return true;
}

bool Console::initializeAudio() {
    return apu.initialize();
}

void Console::clock() {
    ppu.clock();
    ppu.clock();
    ppu.clock();

    const bool oddCpuCycle = (cpuCycle % 2) != 0;
    if(cpuBus.isDmcDmaActive()) {
        cpuBus.clockDmcDma();
    } else if(cpuBus.isDmaActive()) {
        cpuBus.clockDma(oddCpuCycle);
    } else {
        if(ppu.isNmiComplete()) {
            cpu.nmi();
        }

        cpu.clock();
    }

    apu.clock();
    cpuCycle++;
}

void Console::runFrame() {
    while(!ppu.isFrameComplete()) {
        clock();
    }

    ppu.clearFrameComplete();
}

const std::array<uint32_t, 256 * 240>& Console::getFramebuffer() const {
    return ppu.getFramebuffer();
}

Controller& Console::getController1() {
    return controller1;
}
