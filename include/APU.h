#pragma once

#include <array>
#include <cstddef>
#include <cstdint>


struct SDL_AudioStream;
class CPUBus;

class APU {
public:
    APU() = default;
    ~APU();

    APU(const APU&) = delete;
    APU& operator=(const APU&) = delete;

    bool initialize();
    void shutdown();
    void writeRegister(uint16_t address, uint8_t data);
    void clock();
    void connectBus(CPUBus* busp);
#if defined(TBS_APU_TEST_ACCESS)
public:
#else
private:
#endif
    // SDL Stuff
    SDL_AudioStream* stream = nullptr;

    bool sdlAudioInitialized = false;
    bool initialized = false;

    // Pure APU stuff
    bool queueSamples(const float* samples, const std::size_t sampleCount);

    static constexpr std::size_t AUDIO_BUFFER_SIZE = 512;

    std::array<float, AUDIO_BUFFER_SIZE> sampleBuffer{};
    std::size_t sampleBufferIndex = 0;
    uint32_t sampleAccumulator = 0;
    uint32_t frameCounterCycle = 0;
    bool apuTimerCycle = false;
    bool fiveStepFrameCounter = false;
    CPUBus* bus = nullptr;


    // Channels
    struct PulseChannel {
        // Timer and duty sequencer
        uint16_t timerPeriod = 0x0000;
        uint16_t timerCounter = 0x0000;
        uint8_t duty = 0x00;
        uint8_t dutyStep = 0x00;

        // Length counter
        uint8_t lengthCounter = 0x00;
        bool lengthCounterHalt = false; // Also enables envelope looping.

        // Envelope
        // Fixed volume when constantVolume is set; envelope period otherwise.
        uint8_t volume = 0x00;
        uint8_t envelopeDivider = 0x00;
        uint8_t envelopeDecayLevel = 0x00;
        bool constantVolume = false;
        bool envelopeStart = false;

        // Sweep unit
        uint8_t sweepPeriod = 0x00;
        uint8_t sweepDivider = 0x00;
        uint8_t sweepShift = 0x00;
        bool sweepEnabled = false;
        bool sweepNegate = false;
        bool sweepReload = false;

        // Channel control
        bool enabled = false;
    };

    struct TriangleChannel {
        // Timer and 32-step sequencer
        uint16_t timerPeriod = 0x0000;
        uint16_t timerCounter = 0x0000;
        uint8_t sequenceStep = 0x00;

        // Length and linear counters
        uint8_t lengthCounter = 0x00;
        uint8_t linearCounter = 0x00;
        uint8_t linearReloadValue = 0x00;
        bool linearReloadFlag = false;
        bool controlFlag = false; // Also halts the length counter.

        bool enabled = false;
    };

    struct NoiseChannel {
        // Timer and 15-bit linear-feedback shift register
        uint16_t timerPeriod = 0x0000;
        uint16_t timerCounter = 0x0000;
        uint16_t shiftRegister = 0x0001;
        bool mode = false;

        // Length counter
        uint8_t lengthCounter = 0x00;
        bool lengthCounterHalt = false; // Also enables envelope looping.

        // Envelope
        uint8_t volume = 0x00;
        uint8_t envelopeDivider = 0x00;
        uint8_t envelopeDecayLevel = 0x00;
        bool constantVolume = false;
        bool envelopeStart = false;

        bool enabled = false;
    };

    struct DmcChannel {
        // Configuration
        uint16_t timerPeriod = 0x0000;
        uint16_t timerCounter = 0x0000;
        uint16_t sampleAddress = 0xC000;
        uint16_t sampleLength = 0x0001;
        uint8_t outputLevel = 0x00;
        bool irqEnabled = false;
        bool irqFlag = false;
        bool loop = false;

        // Memory reader
        uint16_t currentAddress = 0xC000;
        uint16_t bytesRemaining = 0x0000;
        uint8_t sampleBuffer = 0x00;
        bool sampleBufferEmpty = true;

        // Output unit
        uint8_t shiftRegister = 0x00;
        uint8_t bitsRemaining = 0x08;
        bool silence = true;
    };

    PulseChannel pulse1{};
    PulseChannel pulse2{};
    TriangleChannel triangle{};
    NoiseChannel noise{};
    DmcChannel dmc{};

    void clockPulse(PulseChannel& pulse);
    void clockTriangle(TriangleChannel& triangleChannel);
    void clockNoise(NoiseChannel& noiseChannel);
    void clockDmc(DmcChannel& dmcChannel);
    void restartDmcSample(DmcChannel& dmcChannel);
    void fillDmcSampleBuffer(DmcChannel& dmcChannel);
    void clockFrameCounter();
    void clockQuarterFrame();
    void clockHalfFrame();

    static void clockEnvelope(uint8_t period,
                              bool loop,
                              uint8_t& divider,
                              uint8_t& decayLevel,
                              bool& startFlag);
    static void clockLengthCounter(uint8_t& counter, bool halt);

    uint8_t pulseOutput(const PulseChannel& pulse) const;
    uint8_t triangleOutput(const TriangleChannel& triangleChannel) const;
    uint8_t noiseOutput(const NoiseChannel& noiseChannel) const;
    uint8_t dmcOutput(const DmcChannel& dmcChannel) const;
    float mixSample() const;
};
