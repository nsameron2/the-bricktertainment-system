#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "APU.h"
#include "CPUBus.h"
#include "CPU.h"
#include "Cartridge.h"
#include "Controller.h"
#include "Display.h"
#include "PPU.h"
#include "PPUBus.h"


int main(int argc, char* argv[]) {
    if(argc != 2) {
        std::cerr << "USAGE: " << argv[0] << " [rom.nes]\n";
        return EXIT_FAILURE;
    }


    Cartridge cart;
    CPUBus bus;
    PPUBus ppuBus;
    CPU cpu;
    PPU ppu;
    Display display;
    APU apu;
    Controller controller1;

    if(!cart.load(argv[1])) {
        std::cerr << "Failed to load ROM: " << argv[1] << '\n';
        return EXIT_FAILURE;
    }

    bus.insertCartridge(&cart);
    bus.connectPPU(&ppu);
    bus.connectController1(&controller1);
    ppuBus.insertCartridge(&cart);
    ppu.connectBus(&ppuBus);
    cpu.connectBus(&bus);
    cpu.powerOn();

    if(!display.initialize()) {
        return EXIT_FAILURE;
    }

    if(!apu.initialize()) {
        return EXIT_FAILURE;
    }

    std::cout << "Loaded ROM: " << argv[1] << '\n';

    uint64_t cpuCycle = 0;

    while(!display.pollEvents(controller1)) {
        while(!ppu.isFrameComplete()) {
            ppu.clock();
            ppu.clock();
            ppu.clock();

            const bool oddCpuCycle = (cpuCycle % 2) != 0;
            if(bus.isDmaActive()) {
                bus.clockDma(oddCpuCycle);
            } else {
                if(ppu.isNmiComplete()) {
                    cpu.nmi();
                }

                cpu.clock();
            }

            cpuCycle++;
        }

        if(!display.present(ppu.getFramebuffer())) {
            return EXIT_FAILURE;
        }

        ppu.clearFrameComplete();
    }

    return EXIT_SUCCESS;
}
