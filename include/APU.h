#pragma once

#include <cstddef>
#include <cstdint>


struct SDL_AudioStream;

class APU {
public:
    APU() = default;
    ~APU();

    APU(const APU&) = delete;
    APU& operator=(const APU&) = delete;

    bool initialize();
    void shutdown();
    void writeRegister(uint16_t address, uint8_t data);

private:
    // SDL Stuff
    SDL_AudioStream* stream = nullptr;

    bool sdlAudioInitialized = false;
    bool initialized = false;

    // Pure APU stuff
    bool queueSamples(const float* samples, const std::size_t sampleCount);


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


    PulseChannel pulse1{};
    PulseChannel pulse2{};
};
