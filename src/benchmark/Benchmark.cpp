#include "benchmark/Benchmark.h"
#include "core/Console.h"
#include "input/InputRecording.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>


namespace {

constexpr double NTSC_CPU_CLOCK_RATE = 1'789'773.0;
constexpr double CYCLES_PER_MEGACYCLE = 1'000'000.0;
constexpr uint32_t FNV_OFFSET_BASIS = 2'166'136'261U;
constexpr uint32_t FNV_PRIME = 16'777'619U;

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
    uint64_t frames = 0;
    uint64_t cpuCycles = 0;
    uint64_t ppuCycles = 0;
    uint64_t apuCycles = 0;
    double elapsedSeconds = 0.0;
};


uint32_t hashFramebuffer(const std::array<uint32_t, 256 * 240>& framebuffer) {
    uint32_t hash = FNV_OFFSET_BASIS;

    for(const uint32_t pixel : framebuffer) {
        hash ^= pixel;
        hash *= FNV_PRIME;
    }

    return hash;
}

BenchmarkResult makeResult(
    const Console& console,
    uint64_t frames,
    uint64_t startingCpuCycles,
    uint64_t startingPpuCycles,
    uint64_t startingApuCycles,
    Clock::time_point start,
    Clock::time_point end
) {
    return {
        frames,
        console.getCpuCycleCount() - startingCpuCycles,
        console.getPpuCycleCount() - startingPpuCycles,
        console.getApuCycleCount() - startingApuCycles,
        std::chrono::duration<double>(end - start).count(),
    };
}

void printResult(const Console& console, const BenchmarkResult& result) {
    const double framesPerSecond = static_cast<double>(result.frames) / result.elapsedSeconds;
    const double cpuCyclesPerSecond =
        static_cast<double>(result.cpuCycles) / result.elapsedSeconds;
    const double ppuCyclesPerSecond =
        static_cast<double>(result.ppuCycles) / result.elapsedSeconds;
    const double apuCyclesPerSecond =
        static_cast<double>(result.apuCycles) / result.elapsedSeconds;
    const double realtimeMultiplier = cpuCyclesPerSecond / NTSC_CPU_CLOCK_RATE;
    const uint32_t framebufferHash = hashFramebuffer(console.getFramebuffer());

    std::cout << "Actual time:        " << result.elapsedSeconds << " s\n"
              << "Frames completed:   " << result.frames << '\n'
              << "Frame rate:         " << framesPerSecond << " FPS\n"
              << "CPU throughput:     " << cpuCyclesPerSecond / CYCLES_PER_MEGACYCLE
              << " M cycles/s\n"
              << "PPU throughput:     " << ppuCyclesPerSecond / CYCLES_PER_MEGACYCLE
              << " M cycles/s\n"
              << "APU throughput:     " << apuCyclesPerSecond / CYCLES_PER_MEGACYCLE
              << " M cycles/s\n"
              << "Real-time speed:    " << realtimeMultiplier << "x\n"
              << "Framebuffer hash:   0x"
              << std::hex << std::uppercase << std::setfill('0') << std::setw(8)
              << framebufferHash << std::dec << std::nouppercase << '\n';
}

void printHeader(const char* romPath) {
    std::cout << "Console benchmark\n"
              << "ROM: " << std::filesystem::path(romPath).filename().string() << "\n\n"
              << std::fixed << std::setprecision(3);
}

}

int Benchmark::runTimed(const char* romPath, const float runTime) {
    if(runTime <= 0.0F || !std::isfinite(runTime)) {
        std::cerr << "Benchmark duration must be a positive number of seconds.\n";
        return 1;
    }

    Console benchConsole;
    if(!benchConsole.initialize(romPath)) {
        std::cerr << "Failed to load ROM: " << romPath << '\n';
        return 1;
    }


    const uint64_t startingCpuCycles = benchConsole.getCpuCycleCount();
    const uint64_t startingPpuCycles = benchConsole.getPpuCycleCount();
    const uint64_t startingApuCycles = benchConsole.getApuCycleCount();
    const auto benchStart = Clock::now();
    const auto benchmarkDuration = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(runTime)
    );
    const auto benchmarkEnd = benchStart + benchmarkDuration;

    // Run benchmark
    uint64_t frames = 0;

    while(Clock::now() < benchmarkEnd) {
        benchConsole.runFrame();

        frames++;
    }

    const auto actualEnd = Clock::now();

    if(frames == 0) {
        std::cerr << "Benchmark completed no frames.\n";
        return 1;
    }

    const BenchmarkResult result = makeResult(
        benchConsole,
        frames,
        startingCpuCycles,
        startingPpuCycles,
        startingApuCycles,
        benchStart,
        actualEnd
    );

    printHeader(romPath);
    std::cout << "Requested time:     " << runTime << " s\n";
    printResult(benchConsole, result);

    return 0;
}

int Benchmark::runInput(const char* romPath, const char* moviePath) {
    InputRecording recording;
    if(!recording.load(moviePath, romPath)) {
        std::cerr << recording.getLastError() << '\n';
        return 1;
    }

    if(recording.getFrameCount() == 0) {
        std::cerr << "Input recording contains no frames.\n";
        return 1;
    }

    Console benchConsole;
    if(!benchConsole.initialize(romPath)) {
        std::cerr << "Failed to load ROM: " << romPath << '\n';
        return 1;
    }

    const std::span<const InputRecording::InputEvent> inputEvents = recording.getEvents();
    Controller& controller = benchConsole.getController1();
    std::size_t eventIndex = 0;
    uint64_t nextEventFrame = inputEvents.empty()
        ? std::numeric_limits<uint64_t>::max()
        : inputEvents.front().frameDelta;

    const uint64_t startingCpuCycles = benchConsole.getCpuCycleCount();
    const uint64_t startingPpuCycles = benchConsole.getPpuCycleCount();
    const uint64_t startingApuCycles = benchConsole.getApuCycleCount();
    const auto benchStart = Clock::now();

    for(uint64_t frame = 0; frame < recording.getFrameCount(); ++frame) {
        while(eventIndex < inputEvents.size() && frame == nextEventFrame) {
            controller.setButtonState(inputEvents[eventIndex].controllerState);
            ++eventIndex;

            if(eventIndex < inputEvents.size()) {
                nextEventFrame += inputEvents[eventIndex].frameDelta;
            }
        }

        benchConsole.runFrame();
    }

    const auto benchEnd = Clock::now();
    const BenchmarkResult result = makeResult(
        benchConsole,
        recording.getFrameCount(),
        startingCpuCycles,
        startingPpuCycles,
        startingApuCycles,
        benchStart,
        benchEnd
    );

    printHeader(romPath);
    std::cout << "Input recording:    "
              << std::filesystem::path(moviePath).filename().string() << '\n';
    printResult(benchConsole, result);

    return 0;
}
