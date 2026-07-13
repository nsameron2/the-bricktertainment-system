#include "APU.h"

#include <SDL3/SDL.h>

#include <array>
#include <iostream>


namespace {

constexpr int AUDIO_SAMPLE_RATE = 44100;
constexpr int AUDIO_CHANNEL_COUNT = 1;
constexpr uint32_t NTSC_CPU_CLOCK_RATE = 1789773;

constexpr uint8_t PULSE_DUTY_SHIFT = 6;
constexpr uint8_t PULSE_DUTY_MASK = 0x03;
constexpr uint8_t LENGTH_COUNTER_HALT = 0x20;
constexpr uint8_t CONSTANT_VOLUME = 0x10;
constexpr uint8_t VOLUME_MASK = 0x0F;

constexpr uint8_t SWEEP_ENABLED = 0x80;
constexpr uint8_t SWEEP_PERIOD_SHIFT = 4;
constexpr uint8_t SWEEP_PERIOD_MASK = 0x07;
constexpr uint8_t SWEEP_NEGATE = 0x08;
constexpr uint8_t SWEEP_SHIFT_MASK = 0x07;

constexpr uint16_t TIMER_HIGH_MASK = 0x0700;
constexpr uint16_t TIMER_LOW_MASK = 0x00FF;
constexpr uint8_t TIMER_HIGH_DATA_MASK = 0x07;
constexpr uint8_t TIMER_HIGH_SHIFT = 8;
constexpr uint8_t LENGTH_COUNTER_INDEX_SHIFT = 3;

constexpr uint8_t PULSE_1_ENABLE = 0x01;
constexpr uint8_t PULSE_2_ENABLE = 0x02;

constexpr uint8_t PULSE_DUTY_STEP_MASK = 0x07;
constexpr uint16_t PULSE_MINIMUM_TIMER_PERIOD = 0x0008;

constexpr float PULSE_MIXER_NUMERATOR = 95.52F;
constexpr float PULSE_MIXER_DIVISOR = 8128.0F;
constexpr float PULSE_MIXER_OFFSET = 100.0F;

constexpr std::array<std::array<uint8_t, 8>, 4> PULSE_DUTY_SEQUENCES = {{
    {{0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00}},
    {{0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01}},
}};

constexpr std::array<uint8_t, 32> LENGTH_COUNTER_TABLE = {
    0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
    0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
    0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
    0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E,
};


void logSdlError(const char* operation) {
    std::cerr << operation << " failed: " << SDL_GetError() << '\n';
}

}

APU::~APU() {
    shutdown();
}

bool APU::initialize() {
    if(initialized) {
        return true;
    }

    if(!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        logSdlError("SDL_InitSubSystem");
        return false;
    }
    sdlAudioInitialized = true;

    const SDL_AudioSpec spec{
        .format = SDL_AUDIO_F32,
        .channels = AUDIO_CHANNEL_COUNT,
        .freq = AUDIO_SAMPLE_RATE,
    };

    stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &spec,
        nullptr,
        nullptr
    );

    if(stream == nullptr) {
        logSdlError("SDL_OpenAudioDeviceStream");
        shutdown();
        return false;
    }

    if(!SDL_ResumeAudioStreamDevice(stream)) {
        logSdlError("SDL_ResumeAudioStreamDevice");
        shutdown();
        return false;
    }

    initialized = true;
    return true;
}

void APU::writeRegister(uint16_t address, uint8_t data) {
    switch(address) {
        case 0x4000: // Pulse 1 duty, envelope, and volume
            pulse1.duty = (data >> PULSE_DUTY_SHIFT) & PULSE_DUTY_MASK;
            pulse1.lengthCounterHalt = (data & LENGTH_COUNTER_HALT) != 0x00;
            pulse1.constantVolume = (data & CONSTANT_VOLUME) != 0x00;
            pulse1.volume = data & VOLUME_MASK;
            break;
        case 0x4001: // Pulse 1 sweep
            pulse1.sweepEnabled = (data & SWEEP_ENABLED) != 0x00;
            pulse1.sweepPeriod = (data >> SWEEP_PERIOD_SHIFT) & SWEEP_PERIOD_MASK;
            pulse1.sweepNegate = (data & SWEEP_NEGATE) != 0x00;
            pulse1.sweepShift = data & SWEEP_SHIFT_MASK;
            pulse1.sweepReload = true;
            break;
        case 0x4002: // Pulse 1 timer low
            pulse1.timerPeriod = (pulse1.timerPeriod & TIMER_HIGH_MASK) | data;
            break;
        case 0x4003: // Pulse 1 length counter and timer high
            pulse1.timerPeriod = (pulse1.timerPeriod & TIMER_LOW_MASK)
                | (static_cast<uint16_t>(data & TIMER_HIGH_DATA_MASK) << TIMER_HIGH_SHIFT);
            if(pulse1.enabled) {
                pulse1.lengthCounter = LENGTH_COUNTER_TABLE[data >> LENGTH_COUNTER_INDEX_SHIFT];
            }
            pulse1.dutyStep = 0x00;
            pulse1.envelopeStart = true;
            break;

        case 0x4004: // Pulse 2 duty, envelope, and volume
            pulse2.duty = (data >> PULSE_DUTY_SHIFT) & PULSE_DUTY_MASK;
            pulse2.lengthCounterHalt = (data & LENGTH_COUNTER_HALT) != 0x00;
            pulse2.constantVolume = (data & CONSTANT_VOLUME) != 0x00;
            pulse2.volume = data & VOLUME_MASK;
            break;
        case 0x4005: // Pulse 2 sweep
            pulse2.sweepEnabled = (data & SWEEP_ENABLED) != 0x00;
            pulse2.sweepPeriod = (data >> SWEEP_PERIOD_SHIFT) & SWEEP_PERIOD_MASK;
            pulse2.sweepNegate = (data & SWEEP_NEGATE) != 0x00;
            pulse2.sweepShift = data & SWEEP_SHIFT_MASK;
            pulse2.sweepReload = true;
            break;
        case 0x4006: // Pulse 2 timer low
            pulse2.timerPeriod = (pulse2.timerPeriod & TIMER_HIGH_MASK) | data;
            break;
        case 0x4007: // Pulse 2 length counter and timer high
            pulse2.timerPeriod = (pulse2.timerPeriod & TIMER_LOW_MASK)
                | (static_cast<uint16_t>(data & TIMER_HIGH_DATA_MASK) << TIMER_HIGH_SHIFT);
            if(pulse2.enabled) {
                pulse2.lengthCounter = LENGTH_COUNTER_TABLE[data >> LENGTH_COUNTER_INDEX_SHIFT];
            }
            pulse2.dutyStep = 0x00;
            pulse2.envelopeStart = true;
            break;

        case 0x4008: // Triangle linear counter
            break;
        case 0x4009: // Unused
            break;
        case 0x400A: // Triangle timer low
            break;
        case 0x400B: // Triangle length counter and timer high
            break;

        case 0x400C: // Noise envelope and volume
            break;
        case 0x400D: // Unused
            break;
        case 0x400E: // Noise mode and timer period
            break;
        case 0x400F: // Noise length counter
            break;

        case 0x4010: // DMC control and frequency
            break;
        case 0x4011: // DMC direct load
            break;
        case 0x4012: // DMC sample address
            break;
        case 0x4013: // DMC sample length
            break;

        case 0x4015: // Channel enables and status control
            pulse1.enabled = (data & PULSE_1_ENABLE) != 0x00;
            pulse2.enabled = (data & PULSE_2_ENABLE) != 0x00;

            if(!pulse1.enabled) {
                pulse1.lengthCounter = 0x00;
            }

            if(!pulse2.enabled) {
                pulse2.lengthCounter = 0x00;
            }
            break;
        case 0x4017: // Frame counter
            break;

        default:
            break;
    }
}

bool APU::queueSamples(const float* samples, const std::size_t sampleCount) {
    if(stream == nullptr || samples == nullptr) {
        return false;
    }

    // SDL needs byte count
    const std::size_t byteCount = sampleCount * sizeof(float);


    return SDL_PutAudioStreamData(stream, samples, static_cast<int>(byteCount));
}

void APU::clock() {
    // Pulse timers advance once every two CPU cycles.
    pulseTimerCycle = !pulseTimerCycle;
    if(pulseTimerCycle) {
        clockPulse(pulse1);
        clockPulse(pulse2);
    }

    // Convert CPU clocks to the configured audio sample rate without timing drift.
    sampleAccumulator += AUDIO_SAMPLE_RATE;
    if(sampleAccumulator < NTSC_CPU_CLOCK_RATE) {
        return;
    }

    sampleAccumulator -= NTSC_CPU_CLOCK_RATE;
    sampleBuffer[sampleBufferIndex++] = mixSample();

    if(sampleBufferIndex == sampleBuffer.size()) {
        queueSamples(sampleBuffer.data(), sampleBuffer.size());
        sampleBufferIndex = 0;
    }
}

void APU::clockPulse(PulseChannel& pulse) {
    if (pulse.timerCounter == 0x0000) {
        pulse.timerCounter = pulse.timerPeriod;
        pulse.dutyStep = (pulse.dutyStep + 1) & PULSE_DUTY_STEP_MASK;
    } else {
        pulse.timerCounter--;
    }
}

uint8_t APU::pulseOutput(const PulseChannel& pulse) const {
    // Make sure the channel can be played
    if (!pulse.enabled || pulse.lengthCounter == 0x00) {
        return 0x00;
    }

    if (pulse.timerPeriod < 0x0008) {
        return 0x00;
    }

    if (PULSE_DUTY_SEQUENCES[pulse.duty][pulse.dutyStep] == 0x00) {
        return 0x00;
    }

    // If volume is consant, simply return the volume. If not, we need the decay level
    return pulse.constantVolume
        ? pulse.volume
        : pulse.envelopeDecayLevel;
}

float APU::mixSample() const {
    // Get the level of all instruments
    const uint8_t pulse1Level = pulseOutput(pulse1);
    const uint8_t pulse2Level = pulseOutput(pulse2);
    const uint8_t pulseSum = pulse1Level + pulse2Level;

    if (pulseSum == 0x00) {
        return 0.0f;
    }

    return 95.52f / ((8128.0f / pulseSum) + 100.0f);
};


void APU::shutdown() {
    if(stream != nullptr) {
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }

    if(sdlAudioInitialized) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        sdlAudioInitialized = false;
    }

    initialized = false;
}
