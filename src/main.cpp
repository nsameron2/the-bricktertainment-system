#include <cstdlib>
#include <iostream>
#include <string_view>

#include "benchmark/Benchmark.h"
#include "core/Console.h"
#include "frontend/Display.h"


namespace {

constexpr std::string_view BENCHMARK_FLAG = "--benchmark";
constexpr float DEFAULT_BENCHMARK_DURATION_SECONDS = 5.0F;


void printUsage(const char* programName) {
    std::cerr << "USAGE: " << programName << " [rom.nes]\n"
              << "       " << programName << " --benchmark [rom.nes]\n";
}

}

int main(int argc, char* argv[]) {
    if(argc >= 2 && std::string_view(argv[1]) == BENCHMARK_FLAG) {
        if(argc != 3) {
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }

        Benchmark benchmark;
        return benchmark.run(argv[2], DEFAULT_BENCHMARK_DURATION_SECONDS);
    }

    if(argc != 2) {
        printUsage(argv[0]);
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
