#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "CPUBus.h"
#include "APU.h"

namespace {

constexpr uint32_t FIRST_QUARTER_FRAME_CPU_CYCLE = 7457;
constexpr uint32_t AUDIO_SAMPLE_RATE = 48000;
constexpr uint32_t NTSC_CPU_CLOCK_RATE = 1789773;
constexpr uint32_t FIRST_AUDIO_SAMPLE_CPU_CYCLE =
    (NTSC_CPU_CLOCK_RATE + AUDIO_SAMPLE_RATE - 1) / AUDIO_SAMPLE_RATE;

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

void expectNear(float actual, float expected, float tolerance, const char* message) {
    if(std::fabs(actual - expected) > tolerance) {
        std::fprintf(stderr,
                     "FAIL: %s (expected %.6f, got %.6f)\n",
                     message,
                     expected,
                     actual);
        std::exit(EXIT_FAILURE);
    }
}

}

int main() {
    APU apu;
    CPUBus cpuBus;

    apu.connectBus(&cpuBus);
    expectTrue(apu.bus == &cpuBus, "APU connects to CPU bus for DMC reads");
    expectEqual16(apu.dmc.sampleAddress, 0xC000, "DMC starts with hardware sample base address");
    expectEqual16(apu.dmc.sampleLength, 0x0001, "DMC starts with minimum sample length");
    expectEqual(apu.dmc.bitsRemaining, 0x08, "DMC output unit starts with eight bits remaining");
    expectTrue(apu.dmc.sampleBufferEmpty, "DMC sample buffer starts empty");
    expectTrue(apu.dmc.silence, "DMC output unit starts silent");

    apu.dmc.sampleAddress = 0xC040;
    apu.dmc.sampleLength = 0x0011;
    apu.restartDmcSample(apu.dmc);
    expectEqual16(apu.dmc.currentAddress, 0xC040, "restarting DMC restores configured sample address");
    expectEqual16(apu.dmc.bytesRemaining, 0x0011, "restarting DMC restores configured sample length");

    cpuBus.write(0x0005, 0xA5);
    apu.dmc.currentAddress = 0x0005;
    apu.dmc.bytesRemaining = 0x0002;
    apu.dmc.sampleBufferEmpty = true;
    apu.dmc.loop = false;
    apu.dmc.irqEnabled = false;
    apu.fillDmcSampleBuffer(apu.dmc);
    expectEqual(apu.dmc.sampleBuffer, 0xA5, "DMC reader fetches a byte through CPU bus");
    expectFalse(apu.dmc.sampleBufferEmpty, "DMC reader marks fetched sample buffer as full");
    expectEqual16(apu.dmc.currentAddress, 0x0006, "DMC reader advances current address");
    expectEqual16(apu.dmc.bytesRemaining, 0x0001, "DMC reader decrements remaining byte count");

    apu.dmc.currentAddress = 0xFFFF;
    apu.dmc.bytesRemaining = 0x0001;
    apu.dmc.sampleBufferEmpty = true;
    apu.dmc.irqEnabled = true;
    apu.fillDmcSampleBuffer(apu.dmc);
    expectEqual16(apu.dmc.currentAddress, 0x8000, "DMC reader wraps address from 0xFFFF to 0x8000");
    expectEqual16(apu.dmc.bytesRemaining, 0x0000, "DMC reader finishes final sample byte");
    expectTrue(apu.dmc.irqFlag, "DMC reader requests IRQ when an enabled sample finishes");

    apu.dmc.sampleAddress = 0xC080;
    apu.dmc.sampleLength = 0x0021;
    apu.dmc.currentAddress = 0x0005;
    apu.dmc.bytesRemaining = 0x0001;
    apu.dmc.sampleBufferEmpty = true;
    apu.dmc.loop = true;
    apu.dmc.irqFlag = false;
    apu.fillDmcSampleBuffer(apu.dmc);
    expectEqual16(apu.dmc.currentAddress, 0xC080, "looping DMC restarts configured sample address");
    expectEqual16(apu.dmc.bytesRemaining, 0x0021, "looping DMC restarts configured sample length");
    expectFalse(apu.dmc.irqFlag, "looping DMC does not request end-of-sample IRQ");

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

    APU tndApu;
    tndApu.writeRegister(0x4015, 0x0C);

    tndApu.writeRegister(0x4008, 0xA5);
    expectTrue(tndApu.triangle.controlFlag, "triangle control write sets control flag");
    expectEqual(tndApu.triangle.linearReloadValue, 0x25, "triangle control write sets linear reload value");

    tndApu.writeRegister(0x400A, 0xCD);
    tndApu.writeRegister(0x400B, 0x1D);
    expectEqual16(tndApu.triangle.timerPeriod, 0x05CD, "triangle timer combines low and high writes");
    expectEqual(tndApu.triangle.lengthCounter, 0x02, "triangle high write loads enabled length counter");
    expectTrue(tndApu.triangle.linearReloadFlag, "triangle high write sets linear reload flag");

    tndApu.writeRegister(0x400C, 0x3A);
    expectTrue(tndApu.noise.lengthCounterHalt, "noise control write sets length halt");
    expectTrue(tndApu.noise.constantVolume, "noise control write selects constant volume");
    expectEqual(tndApu.noise.volume, 0x0A, "noise control write sets volume");

    tndApu.writeRegister(0x400E, 0x8C);
    expectTrue(tndApu.noise.mode, "noise period write selects short feedback mode");
    expectEqual16(tndApu.noise.timerPeriod, 0x017C, "noise period write selects timer reload");

    tndApu.writeRegister(0x400F, 0x18);
    expectEqual(tndApu.noise.lengthCounter, 0x02, "noise length write loads enabled length counter");
    expectTrue(tndApu.noise.envelopeStart, "noise length write requests envelope restart");

    tndApu.clockQuarterFrame();
    expectEqual(tndApu.triangle.linearCounter, 0x25, "quarter frame reloads triangle linear counter");
    expectEqual(tndApu.noise.envelopeDecayLevel, 0x0F, "quarter frame starts noise envelope");
    expectFalse(tndApu.noise.envelopeStart, "quarter frame clears noise envelope start flag");

    tndApu.triangle.timerPeriod = 0x0002;
    tndApu.triangle.timerCounter = 0x0000;
    tndApu.triangle.sequenceStep = 0x00;
    tndApu.clockTriangle(tndApu.triangle);
    expectEqual16(tndApu.triangle.timerCounter, 0x0002, "triangle timer reloads at zero");
    expectEqual(tndApu.triangle.sequenceStep, 0x01, "active triangle timer advances sequencer");
    expectEqual(tndApu.triangleOutput(tndApu.triangle), 0x0E, "triangle output returns current sequence level");

    tndApu.noise.timerPeriod = 0x0001;
    tndApu.noise.timerCounter = 0x0000;
    tndApu.noise.mode = false;
    tndApu.noise.shiftRegister = 0x0001;
    tndApu.clockNoise(tndApu.noise);
    expectEqual16(tndApu.noise.shiftRegister, 0x4000, "normal noise mode feeds back bit 1");
    expectEqual(tndApu.noiseOutput(tndApu.noise), 0x0A, "noise output uses constant volume when output bit is clear");

    tndApu.noise.mode = true;
    tndApu.noise.timerCounter = 0x0000;
    tndApu.noise.shiftRegister = 0x0041;
    tndApu.clockNoise(tndApu.noise);
    expectEqual16(tndApu.noise.shiftRegister, 0x0020, "short noise mode feeds back bit 6");

    const float triangleInput = 14.0F / 8227.0F;
    tndApu.noise.enabled = false;
    expectNear(tndApu.mixSample(),
               159.79F / ((1.0F / triangleInput) + 100.0F),
               0.000001F,
               "mixer includes triangle output");

    tndApu.writeRegister(0x4015, 0x00);
    expectFalse(tndApu.triangle.enabled, "status write disables triangle");
    expectEqual(tndApu.triangle.lengthCounter, 0x00, "disabling triangle clears length counter");
    expectFalse(tndApu.noise.enabled, "status write disables noise");
    expectEqual(tndApu.noise.lengthCounter, 0x00, "disabling noise clears length counter");

    APU frameApu;
    frameApu.triangle.linearReloadValue = 0x07;
    frameApu.triangle.linearReloadFlag = true;
    frameApu.frameCounterCycle = FIRST_QUARTER_FRAME_CPU_CYCLE - 1;
    frameApu.clock();
    expectEqual(frameApu.triangle.linearCounter, 0x07, "APU clock generates first quarter-frame clock");

    frameApu.triangle.lengthCounter = 0x02;
    frameApu.triangle.controlFlag = false;
    frameApu.noise.lengthCounter = 0x02;
    frameApu.noise.lengthCounterHalt = true;
    frameApu.clockHalfFrame();
    expectEqual(frameApu.triangle.lengthCounter, 0x01, "half frame decrements active triangle length");
    expectEqual(frameApu.noise.lengthCounter, 0x02, "half frame preserves halted noise length");

    APU timingApu;
    timingApu.pulse1.timerPeriod = 0x0002;
    timingApu.pulse1.timerCounter = 0x0002;
    timingApu.pulse2.timerPeriod = 0x0003;
    timingApu.pulse2.timerCounter = 0x0003;
    timingApu.triangle.timerPeriod = 0x0003;
    timingApu.triangle.timerCounter = 0x0002;
    timingApu.noise.timerPeriod = 0x0003;
    timingApu.noise.timerCounter = 0x0002;

    timingApu.clock();
    expectEqual16(timingApu.pulse1.timerCounter, 0x0001, "first APU clock advances Pulse 1 timer");
    expectEqual16(timingApu.pulse2.timerCounter, 0x0002, "first APU clock advances Pulse 2 timer");
    expectEqual16(timingApu.triangle.timerCounter, 0x0001, "first APU clock advances triangle timer");
    expectEqual16(timingApu.noise.timerCounter, 0x0001, "first APU clock advances noise timer");

    timingApu.clock();
    expectEqual16(timingApu.pulse1.timerCounter, 0x0001, "second APU clock leaves Pulse 1 timer unchanged");
    expectEqual16(timingApu.pulse2.timerCounter, 0x0002, "second APU clock leaves Pulse 2 timer unchanged");
    expectEqual16(timingApu.triangle.timerCounter, 0x0000, "second APU clock advances triangle timer again");
    expectEqual16(timingApu.noise.timerCounter, 0x0001, "second APU clock leaves noise timer unchanged");

    APU sampleTimingApu;
    for(uint32_t cycle = 0; cycle < FIRST_AUDIO_SAMPLE_CPU_CYCLE - 1; cycle++) {
        sampleTimingApu.clock();
    }
    expectTrue(sampleTimingApu.sampleBufferIndex == 0,
               "audio sample is not produced before its clock-rate boundary");

    sampleTimingApu.clock();
    expectTrue(sampleTimingApu.sampleBufferIndex == 1,
               "audio sample is produced at its clock-rate boundary");

    std::printf("test_apu passed\n");
    return 0;
}
