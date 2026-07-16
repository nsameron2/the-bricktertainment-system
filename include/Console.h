#pragma once

#include <array>
#include <cstdint>

#include "APU.h"
#include "CPUBus.h"
#include "CPU.h"
#include "Cartridge.h"
#include "Controller.h"
#include "PPU.h"
#include "PPUBus.h"


class Console {
public:
    Console();

    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    bool initialize(const char* romPath);
    bool initializeAudio();

    // Advances the complete console by one CPU cycle.
    void clock();
    void runFrame();

    const std::array<uint32_t, 256 * 240>& getFramebuffer() const;
    Controller& getController1();
    uint64_t getCpuCycleCount() const;
    uint64_t getPpuCycleCount() const;
    uint64_t getApuCycleCount() const;

private:
    static constexpr uint64_t PPU_CYCLES_PER_CPU_CYCLE = 3;

    Cartridge cartridge;
    CPUBus cpuBus;
    PPUBus ppuBus;
    CPU cpu;
    PPU ppu;
    APU apu;
    Controller controller1;

    // Includes CPU-rate cycles during which DMA stalls instruction execution.
    uint64_t cpuCycleCount = 0;
};
