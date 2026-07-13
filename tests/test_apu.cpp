#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define private public
#include "APU.h"
#undef private

namespace {

void expectEqual(uint8_t actual, uint8_t expected, const char* message) {
    if(actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%02X, got 0x%02X)\n",
                     message,
                     expected,
                     actual);
        std::exit(EXIT_FAILURE);
    }
}

void expectEqual16(uint16_t actual, uint16_t expected, const char* message) {
    if(actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%04X, got 0x%04X)\n",
                     message,
                     expected,
                     actual);
        std::exit(EXIT_FAILURE);
    }
}

void expectTrue(bool value, const char* message) {
    if(!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void expectFalse(bool value, const char* message) {
    if(value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

}

int main() {
    APU apu;

    apu.writeRegister(0x4015, 0x03);

    apu.writeRegister(0x4000, 0xBF);
    expectEqual(apu.pulse1.duty, 0x02, "Pulse 1 control sets duty");
    expectTrue(apu.pulse1.lengthCounterHalt, "Pulse 1 control sets length halt");
    expectTrue(apu.pulse1.constantVolume, "Pulse 1 control sets constant volume mode");
    expectEqual(apu.pulse1.volume, 0x0F, "Pulse 1 control sets volume");

    apu.writeRegister(0x4001, 0xDB);
    expectTrue(apu.pulse1.sweepEnabled, "Pulse 1 sweep enables sweep");
    expectEqual(apu.pulse1.sweepPeriod, 0x05, "Pulse 1 sweep sets period");
    expectTrue(apu.pulse1.sweepNegate, "Pulse 1 sweep sets negate mode");
    expectEqual(apu.pulse1.sweepShift, 0x03, "Pulse 1 sweep sets shift count");
    expectTrue(apu.pulse1.sweepReload, "Pulse 1 sweep requests divider reload");

    apu.pulse1.dutyStep = 0x07;
    apu.writeRegister(0x4002, 0xCD);
    apu.writeRegister(0x4003, 0x1D);
    expectEqual16(apu.pulse1.timerPeriod, 0x05CD, "Pulse 1 timer combines low and high writes");
    expectEqual(apu.pulse1.lengthCounter, 0x02, "Pulse 1 high write loads enabled length counter");
    expectEqual(apu.pulse1.dutyStep, 0x00, "Pulse 1 high write resets duty sequence");
    expectTrue(apu.pulse1.envelopeStart, "Pulse 1 high write requests envelope restart");

    apu.writeRegister(0x4004, 0x4A);
    expectEqual(apu.pulse2.duty, 0x01, "Pulse 2 control sets duty");
    expectFalse(apu.pulse2.lengthCounterHalt, "Pulse 2 control clears length halt");
    expectFalse(apu.pulse2.constantVolume, "Pulse 2 control selects envelope volume");
    expectEqual(apu.pulse2.volume, 0x0A, "Pulse 2 control sets envelope period");

    apu.writeRegister(0x4005, 0xA6);
    expectTrue(apu.pulse2.sweepEnabled, "Pulse 2 sweep enables sweep");
    expectEqual(apu.pulse2.sweepPeriod, 0x02, "Pulse 2 sweep sets period");
    expectFalse(apu.pulse2.sweepNegate, "Pulse 2 sweep clears negate mode");
    expectEqual(apu.pulse2.sweepShift, 0x06, "Pulse 2 sweep sets shift count");
    expectTrue(apu.pulse2.sweepReload, "Pulse 2 sweep requests divider reload");

    apu.pulse2.dutyStep = 0x07;
    apu.writeRegister(0x4006, 0x34);
    apu.writeRegister(0x4007, 0x2A);
    expectEqual16(apu.pulse2.timerPeriod, 0x0234, "Pulse 2 timer combines low and high writes");
    expectEqual(apu.pulse2.lengthCounter, 0x04, "Pulse 2 high write loads enabled length counter");
    expectEqual(apu.pulse2.dutyStep, 0x00, "Pulse 2 high write resets duty sequence");
    expectTrue(apu.pulse2.envelopeStart, "Pulse 2 high write requests envelope restart");

    apu.writeRegister(0x4015, 0x02);
    expectFalse(apu.pulse1.enabled, "APU status write disables Pulse 1");
    expectEqual(apu.pulse1.lengthCounter, 0x00, "disabling Pulse 1 clears its length counter");
    expectTrue(apu.pulse2.enabled, "APU status write leaves Pulse 2 enabled");
    expectEqual(apu.pulse2.lengthCounter, 0x04, "enabled Pulse 2 keeps its length counter");

    apu.writeRegister(0x4015, 0x00);
    apu.writeRegister(0x4015, 0x03);
    expectEqual(apu.pulse1.lengthCounter, 0x00, "enabling Pulse 1 does not reload its length counter");
    expectEqual(apu.pulse2.lengthCounter, 0x00, "enabling Pulse 2 does not reload its length counter");

    apu.pulse1.timerPeriod = 0x0002;
    apu.pulse1.timerCounter = 0x0002;
    apu.pulse2.timerPeriod = 0x0003;
    apu.pulse2.timerCounter = 0x0003;

    apu.clock();
    expectEqual16(apu.pulse1.timerCounter, 0x0001, "first APU clock advances Pulse 1 timer");
    expectEqual16(apu.pulse2.timerCounter, 0x0002, "first APU clock advances Pulse 2 timer");

    apu.clock();
    expectEqual16(apu.pulse1.timerCounter, 0x0001, "second APU clock leaves Pulse 1 timer unchanged");
    expectEqual16(apu.pulse2.timerCounter, 0x0002, "second APU clock leaves Pulse 2 timer unchanged");

    for(uint8_t cycle = 0x00; cycle < 0x26; cycle++) {
        apu.clock();
    }
    expectTrue(apu.sampleBufferIndex == 0, "40 CPU clocks do not produce an audio sample");

    apu.clock();
    expectTrue(apu.sampleBufferIndex == 1, "41 CPU clocks produce one audio sample");

    std::printf("test_apu passed\n");
    return 0;
}
