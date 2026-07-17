#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "benchmark/Benchmark.h"
#include "core/Console.h"
#include "frontend/Display.h"
#include "input/InputRecording.h"


namespace {

constexpr std::string_view BENCHMARK_FLAG = "--benchmark";
constexpr std::string_view BENCHMARK_TIME_FLAG = "--benchmark-time";
constexpr std::string_view BENCHMARK_INPUT_FLAG = "--benchmark-input";
constexpr std::string_view RECORD_INPUT_FLAG = "--record-input";
constexpr float DEFAULT_BENCHMARK_DURATION_SECONDS = 5.0F;


void printUsage(const char* programName) {
    std::cerr << "USAGE: " << programName << " [rom.nes]\n"
              << "       " << programName << " --benchmark [rom.nes]\n"
              << "       " << programName << " [rom.nes] --benchmark-time [seconds]\n"
              << "       " << programName << " [rom.nes] --benchmark-input [recording]\n"
              << "       " << programName << " [rom.nes] --record-input [recording]\n";
}

bool parseDuration(std::string_view text, float& duration) {
    if(text.ends_with('s')) {
        text.remove_suffix(1);
    }

    if(text.empty()) {
        return false;
    }

    const auto result = std::from_chars(text.data(), text.data() + text.size(), duration);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

}

int main(int argc, char* argv[]) {
    if(argc >= 2 && std::string_view(argv[1]) == BENCHMARK_FLAG) {
        if(argc != 3) {
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }

        Benchmark benchmark;
        return benchmark.runTimed(argv[2], DEFAULT_BENCHMARK_DURATION_SECONDS);
    }

    if(argc == 4) {
        const std::string_view mode = argv[2];

        if(mode == RECORD_INPUT_FLAG) {
            InputRecording recording;
            if(!recording.startRecording(argv[3], argv[1])) {
                std::cerr << recording.getLastError() << '\n';
                return EXIT_FAILURE;
            }

            return EXIT_SUCCESS;
        }

        Benchmark benchmark;
        if(mode == BENCHMARK_INPUT_FLAG) {
            return benchmark.runInput(argv[1], argv[3]);
        }

        if(mode == BENCHMARK_TIME_FLAG) {
            float duration = 0.0F;
            if(!parseDuration(argv[3], duration)) {
                std::cerr << "Invalid benchmark duration: " << argv[3] << '\n';
                return EXIT_FAILURE;
            }

            return benchmark.runTimed(argv[1], duration);
        }
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
