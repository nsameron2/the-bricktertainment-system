#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "CPUBus.h"
#include "CPU.h"
#include "Cartridge.h"
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

    if(!cart.load(argv[1])) {
        std::cerr << "Failed to load ROM: " << argv[1] << '\n';
        return EXIT_FAILURE;
    }

    bus.insertCartridge(&cart);
    bus.connectPPU(&ppu);
    ppuBus.insertCartridge(&cart);
    ppu.connectBus(&ppuBus);
    cpu.connectBus(&bus);
    cpu.powerOn();

    std::cout << "Loaded ROM: " << argv[1] << '\n';

    // For now, runs until interrupted
    while(true) {
        ppu.clock();
        ppu.clock();
        ppu.clock();
        cpu.clock();
    }

    return EXIT_SUCCESS;
}
