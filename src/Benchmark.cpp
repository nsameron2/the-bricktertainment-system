#include "Benchmark.h"
#include "Console.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>


namespace {

constexpr double NTSC_CPU_CLOCK_RATE = 1'789'773.0;
constexpr double CYCLES_PER_MEGACYCLE = 1'000'000.0;
constexpr uint32_t FNV_OFFSET_BASIS = 2'166'136'261U;
constexpr uint32_t FNV_PRIME = 16'777'619U;


uint32_t hashFramebuffer(const std::array<uint32_t, 256 * 240>& framebuffer) {
    uint32_t hash = FNV_OFFSET_BASIS;

    for(const uint32_t pixel : framebuffer) {
        hash ^= pixel;
        hash *= FNV_PRIME;
    }

    return hash;
}

}

int Benchmark::run(const char* romPath, const float runTime) {
    if(runTime <= 0.0F || !std::isfinite(runTime)) {
        std::cerr << "Benchmark duration must be a positive number of seconds.\n";
        return 1;
    }

    Console benchConsole;
    if(!benchConsole.initialize(romPath)) {
        std::cerr << "Failed to load ROM: " << romPath << '\n';
        return 1;
    }


    // Start time of benchmark
    using Clock = std::chrono::steady_clock;

    const uint64_t startingCpuCycles = benchConsole.getCpuCycleCount();
    const uint64_t startingPpuCycles = benchConsole.getPpuCycleCount();
    const uint64_t startingApuCycles = benchConsole.getApuCycleCount();
    const auto benchStart = Clock::now();
    const auto benchmarkDuration = std::chrono::duration<float>(runTime);
    const auto benchmarkEnd = benchStart + benchmarkDuration;

    // Run benchmark
    uint64_t frames = 0;

    while(Clock::now() < benchmarkEnd) {
        benchConsole.runFrame();

        frames++;
    }

    const auto actualEnd = Clock::now();
    const uint64_t cpuCycles = benchConsole.getCpuCycleCount() - startingCpuCycles;
    const uint64_t ppuCycles = benchConsole.getPpuCycleCount() - startingPpuCycles;
    const uint64_t apuCycles = benchConsole.getApuCycleCount() - startingApuCycles;

    if(frames == 0) {
        std::cerr << "Benchmark completed no frames.\n";
        return 1;
    }

    const double elapsedSeconds =
        std::chrono::duration<double>(actualEnd - benchStart).count();
    const double framesPerSecond = static_cast<double>(frames) / elapsedSeconds;
    const double cpuCyclesPerSecond = static_cast<double>(cpuCycles) / elapsedSeconds;
    const double ppuCyclesPerSecond = static_cast<double>(ppuCycles) / elapsedSeconds;
    const double apuCyclesPerSecond = static_cast<double>(apuCycles) / elapsedSeconds;
    const double realtimeMultiplier = cpuCyclesPerSecond / NTSC_CPU_CLOCK_RATE;
    const uint32_t framebufferHash = hashFramebuffer(benchConsole.getFramebuffer());

    std::cout << "Console benchmark\n"
              << "ROM: " << std::filesystem::path(romPath).filename().string() << "\n\n"
              << std::fixed << std::setprecision(3)
              << "Requested time:     " << runTime << " s\n"
              << "Actual time:        " << elapsedSeconds << " s\n"
              << "Frames completed:   " << frames << '\n'
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

    return 0;
}
