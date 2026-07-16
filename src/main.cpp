#include <cstdlib>
#include <iostream>

#include "Console.h"
#include "Display.h"


int main(int argc, char* argv[]) {
    if(argc != 2) {
        std::cerr << "USAGE: " << argv[0] << " [rom.nes]\n";
        return EXIT_FAILURE;
    }


    Display display;
    Console console;

    if(!console.initialize(argv[1])) {
        std::cerr << "Failed to load ROM: " << argv[1] << '\n';
        return EXIT_FAILURE;
    }

    if(!display.initialize()) {
        return EXIT_FAILURE;
    }

    if(!console.initializeAudio()) {
        return EXIT_FAILURE;
    }

    std::cout << "Loaded ROM: " << argv[1] << '\n';

    while(!display.pollEvents(console.getController1())) {
        console.runFrame();

        if(!display.present(console.getFramebuffer())) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
