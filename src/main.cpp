#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "CPUBus.h"
#include "CPU.h"
#include "Cartridge.h"


int main(int argc, char* argv[]) {
    if(argc != 2) {
        std::cerr << "USAGE: " << argv[0] << " [rom.nes]\n";
        return EXIT_FAILURE;
    }


    Cartridge cart;
    CPUBus bus;
    CPU cpu;

    if(!cart.load(argv[1])) {
        std::cerr << "Failed to load ROM: " << argv[1] << '\n';
        return EXIT_FAILURE;
    }

    bus.insertCartridge(&cart);
    cpu.connectBus(&bus);
    cpu.powerOn();

    std::cout << "Loaded ROM: " << argv[1] << '\n';

    // For now, runs until interrupted
    while(true) {
        cpu.clock();
    }

    return EXIT_SUCCESS;
}
