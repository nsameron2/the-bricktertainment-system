#include "APU.h"
#include "CPUBus.h"

#include <SDL3/SDL.h>

#include <array>
#include <iostream>


namespace {

constexpr int AUDIO_SAMPLE_RATE = 48000;
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
constexpr uint8_t TRIANGLE_ENABLE = 0x04;
constexpr uint8_t NOISE_ENABLE = 0x08;
constexpr uint8_t DMC_ENABLE = 0x10;

constexpr uint8_t PULSE_DUTY_STEP_MASK = 0x07;
constexpr uint16_t PULSE_MINIMUM_TIMER_PERIOD = 0x0008;
constexpr uint8_t TRIANGLE_CONTROL_FLAG = 0x80;
constexpr uint8_t TRIANGLE_LINEAR_RELOAD_MASK = 0x7F;
constexpr uint8_t TRIANGLE_SEQUENCE_STEP_MASK = 0x1F;
constexpr uint8_t NOISE_MODE_FLAG = 0x80;
constexpr uint8_t NOISE_PERIOD_MASK = 0x0F;
constexpr uint16_t NOISE_OUTPUT_BIT = 0x0001;
constexpr uint8_t NOISE_NORMAL_FEEDBACK_TAP = 0x01;
constexpr uint8_t NOISE_MODE_FEEDBACK_TAP = 0x06;
constexpr uint8_t NOISE_FEEDBACK_SHIFT = 0x0E;
constexpr uint8_t ENVELOPE_MAX_DECAY_LEVEL = 0x0F;

constexpr uint8_t DMC_IRQ_ENABLE = 0x80;
constexpr uint8_t DMC_LOOP_FLAG = 0x40;
constexpr uint8_t DMC_RATE_MASK = 0x0F;
constexpr uint8_t DMC_OUTPUT_LEVEL_MASK = 0x7F;
constexpr uint8_t DMC_SHIFT_BIT_MASK = 0x01;
constexpr uint8_t DMC_OUTPUT_STEP = 0x02;
constexpr uint8_t DMC_OUTPUT_LEVEL_MAX = 0x7F;
constexpr uint8_t DMC_BITS_PER_SAMPLE = 0x08;
constexpr uint16_t DMC_SAMPLE_ADDRESS_BASE = 0xC000;
constexpr uint8_t DMC_SAMPLE_ADDRESS_SHIFT = 6;
constexpr uint8_t DMC_SAMPLE_LENGTH_SHIFT = 4;
constexpr uint16_t DMC_MINIMUM_SAMPLE_LENGTH = 0x0001;
constexpr uint16_t DMC_ADDRESS_END = 0xFFFF;
constexpr uint16_t DMC_ADDRESS_WRAP = 0x8000;

constexpr uint8_t FRAME_COUNTER_MODE_5_STEP = 0x80;
constexpr uint32_t FRAME_COUNTER_STEP_1 = 7457;
constexpr uint32_t FRAME_COUNTER_STEP_2 = 14913;
constexpr uint32_t FRAME_COUNTER_STEP_3 = 22371;
constexpr uint32_t FRAME_COUNTER_STEP_4 = 29829;
constexpr uint32_t FRAME_COUNTER_STEP_5 = 37281;

constexpr float PULSE_MIXER_NUMERATOR = 95.52F;
constexpr float PULSE_MIXER_DIVISOR = 8128.0F;
constexpr float PULSE_MIXER_OFFSET = 100.0F;
constexpr float TND_MIXER_NUMERATOR = 159.79F;
constexpr float TRIANGLE_MIXER_DIVISOR = 8227.0F;
constexpr float NOISE_MIXER_DIVISOR = 12241.0F;
constexpr float DMC_MIXER_DIVISOR = 22638.0F;
constexpr float TND_MIXER_OFFSET = 100.0F;

constexpr std::array<std::array<uint8_t, 8>, 4> PULSE_DUTY_SEQUENCES = {{
    {{0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00}},
    {{0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01}},
}};

constexpr std::array<uint8_t, 32> TRIANGLE_SEQUENCE = {
    0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
};

// Each entry is the CPU-period table converted to an APU-cycle divider reload.
constexpr std::array<uint16_t, 16> NOISE_TIMER_RELOAD_TABLE = {
    0x0001, 0x0003, 0x0007, 0x000F,
    0x001F, 0x002F, 0x003F, 0x004F,
    0x0064, 0x007E, 0x00BD, 0x00FD,
    0x017C, 0x01FB, 0x03F8, 0x07F1,
};

// Each entry is the NTSC CPU-period table converted to an APU-cycle divider reload.
constexpr std::array<uint16_t, 16> DMC_TIMER_RELOAD_TABLE = {
    0x00D5, 0x00BD, 0x00A9, 0x009F,
    0x008E, 0x007E, 0x0070, 0x006A,
    0x005E, 0x004F, 0x0046, 0x003F,
    0x0034, 0x0029, 0x0023, 0x001A,
};

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

void APU::connectBus(CPUBus* busp) {
    bus = busp;
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
            triangle.controlFlag = (data & TRIANGLE_CONTROL_FLAG) != 0x00;
            triangle.linearReloadValue = data & TRIANGLE_LINEAR_RELOAD_MASK;
            break;
        case 0x4009: // Unused
            break;
        case 0x400A: // Triangle timer low
            triangle.timerPeriod = (triangle.timerPeriod & TIMER_HIGH_MASK) | data;
            break;
        case 0x400B: // Triangle length counter and timer high
            triangle.timerPeriod = (triangle.timerPeriod & TIMER_LOW_MASK)
                | (static_cast<uint16_t>(data & TIMER_HIGH_DATA_MASK) << TIMER_HIGH_SHIFT);
            if(triangle.enabled) {
                triangle.lengthCounter = LENGTH_COUNTER_TABLE[data >> LENGTH_COUNTER_INDEX_SHIFT];
            }
            triangle.linearReloadFlag = true;
            break;

        case 0x400C: // Noise envelope and volume
            noise.lengthCounterHalt = (data & LENGTH_COUNTER_HALT) != 0x00;
            noise.constantVolume = (data & CONSTANT_VOLUME) != 0x00;
            noise.volume = data & VOLUME_MASK;
            break;
        case 0x400D: // Unused
            break;
        case 0x400E: // Noise mode and timer period
            noise.mode = (data & NOISE_MODE_FLAG) != 0x00;
            noise.timerPeriod = NOISE_TIMER_RELOAD_TABLE[data & NOISE_PERIOD_MASK];
            break;
        case 0x400F: // Noise length counter
            if(noise.enabled) {
                noise.lengthCounter = LENGTH_COUNTER_TABLE[data >> LENGTH_COUNTER_INDEX_SHIFT];
            }
            noise.envelopeStart = true;
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
            triangle.enabled = (data & TRIANGLE_ENABLE) != 0x00;
            noise.enabled = (data & NOISE_ENABLE) != 0x00;

            if(!pulse1.enabled) {
                pulse1.lengthCounter = 0x00;
            }

            if(!pulse2.enabled) {
                pulse2.lengthCounter = 0x00;
            }

            if(!triangle.enabled) {
                triangle.lengthCounter = 0x00;
            }

            if(!noise.enabled) {
                noise.lengthCounter = 0x00;
            }
            break;
        case 0x4017: // Frame counter
            fiveStepFrameCounter = (data & FRAME_COUNTER_MODE_5_STEP) != 0x00;
            frameCounterCycle = 0;

            // First pass applies this immediately; hardware delays it by 3 or 4 CPU cycles.
            if(fiveStepFrameCounter) {
                clockQuarterFrame();
                clockHalfFrame();
            }
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
    fillDmcSampleBuffer(dmc);
    clockTriangle(triangle);
    clockFrameCounter();

    // Pulse and noise timers advance once every two CPU cycles.
    apuTimerCycle = !apuTimerCycle;
    if(apuTimerCycle) {
        clockPulse(pulse1);
        clockPulse(pulse2);
        clockNoise(noise);
        clockDmc(dmc);
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

void APU::clockTriangle(TriangleChannel& triangleChannel) {
    if(triangleChannel.timerCounter == 0x0000) {
        triangleChannel.timerCounter = triangleChannel.timerPeriod;

        if(triangleChannel.lengthCounter != 0x00
            && triangleChannel.linearCounter != 0x00) {
            triangleChannel.sequenceStep =
                (triangleChannel.sequenceStep + 1) & TRIANGLE_SEQUENCE_STEP_MASK;
        }
    } else {
        triangleChannel.timerCounter--;
    }
}

void APU::clockNoise(NoiseChannel& noiseChannel) {
    if(noiseChannel.timerCounter == 0x0000) {
        noiseChannel.timerCounter = noiseChannel.timerPeriod;

        const uint8_t feedbackTap = noiseChannel.mode
            ? NOISE_MODE_FEEDBACK_TAP
            : NOISE_NORMAL_FEEDBACK_TAP;
        const uint16_t feedback =
            (noiseChannel.shiftRegister & NOISE_OUTPUT_BIT)
            ^ ((noiseChannel.shiftRegister >> feedbackTap) & NOISE_OUTPUT_BIT);

        noiseChannel.shiftRegister >>= 1;
        noiseChannel.shiftRegister |= feedback << NOISE_FEEDBACK_SHIFT;
    } else {
        noiseChannel.timerCounter--;
    }
}

void APU::clockDmc(DmcChannel& dmcChannel) {
    if(dmcChannel.timerCounter == 0x0000) {
        dmcChannel.timerCounter = dmcChannel.timerPeriod;

        if(!dmcChannel.silence) {
            if((dmcChannel.shiftRegister & DMC_SHIFT_BIT_MASK) != 0x00) {
                if(dmcChannel.outputLevel <= DMC_OUTPUT_LEVEL_MAX - DMC_OUTPUT_STEP) {
                    dmcChannel.outputLevel += DMC_OUTPUT_STEP;
                }
            } else if(dmcChannel.outputLevel >= DMC_OUTPUT_STEP) {
                dmcChannel.outputLevel -= DMC_OUTPUT_STEP;
            }
        }

        dmcChannel.shiftRegister >>= 1;
        dmcChannel.bitsRemaining--;

        if(dmcChannel.bitsRemaining == 0x00) {
            dmcChannel.bitsRemaining = DMC_BITS_PER_SAMPLE;

            if(dmcChannel.sampleBufferEmpty) {
                dmcChannel.silence = true;
            } else {
                dmcChannel.silence = false;
                dmcChannel.shiftRegister = dmcChannel.sampleBuffer;
                dmcChannel.sampleBufferEmpty = true;
            }
        }
    } else {
        dmcChannel.timerCounter--;
    }
}

void APU::restartDmcSample(DmcChannel& dmcChannel) {
    dmcChannel.currentAddress = dmcChannel.sampleAddress;
    dmcChannel.bytesRemaining = dmcChannel.sampleLength;
}

void APU::fillDmcSampleBuffer(DmcChannel& dmcChannel) {
    if(bus == nullptr
        || !dmcChannel.sampleBufferEmpty
        || dmcChannel.bytesRemaining == 0x0000) {
        return;
    }

    dmcChannel.sampleBuffer = bus->readDmc(dmcChannel.currentAddress);
    dmcChannel.sampleBufferEmpty = false;

    if(dmcChannel.currentAddress == DMC_ADDRESS_END) {
        dmcChannel.currentAddress = DMC_ADDRESS_WRAP;
    } else {
        dmcChannel.currentAddress++;
    }

    dmcChannel.bytesRemaining--;
    if(dmcChannel.bytesRemaining == 0x0000) {
        if(dmcChannel.loop) {
            restartDmcSample(dmcChannel);
        } else if(dmcChannel.irqEnabled) {
            dmcChannel.irqFlag = true;
        }
    }
}

void APU::clockFrameCounter() {
    frameCounterCycle++;

    switch(frameCounterCycle) {
        case FRAME_COUNTER_STEP_1:
            clockQuarterFrame();
            break;
        case FRAME_COUNTER_STEP_2:
            clockQuarterFrame();
            clockHalfFrame();
            break;
        case FRAME_COUNTER_STEP_3:
            clockQuarterFrame();
            break;
        case FRAME_COUNTER_STEP_4:
            if(!fiveStepFrameCounter) {
                clockQuarterFrame();
                clockHalfFrame();
                frameCounterCycle = 0;
            }
            break;
        case FRAME_COUNTER_STEP_5:
            if(fiveStepFrameCounter) {
                clockQuarterFrame();
                clockHalfFrame();
                frameCounterCycle = 0;
            }
            break;
        default:
            break;
    }
}

void APU::clockQuarterFrame() {
    clockEnvelope(pulse1.volume,
                  pulse1.lengthCounterHalt,
                  pulse1.envelopeDivider,
                  pulse1.envelopeDecayLevel,
                  pulse1.envelopeStart);
    clockEnvelope(pulse2.volume,
                  pulse2.lengthCounterHalt,
                  pulse2.envelopeDivider,
                  pulse2.envelopeDecayLevel,
                  pulse2.envelopeStart);
    clockEnvelope(noise.volume,
                  noise.lengthCounterHalt,
                  noise.envelopeDivider,
                  noise.envelopeDecayLevel,
                  noise.envelopeStart);

    if(triangle.linearReloadFlag) {
        triangle.linearCounter = triangle.linearReloadValue;
    } else if(triangle.linearCounter != 0x00) {
        triangle.linearCounter--;
    }

    if(!triangle.controlFlag) {
        triangle.linearReloadFlag = false;
    }
}

void APU::clockHalfFrame() {
    clockLengthCounter(pulse1.lengthCounter, pulse1.lengthCounterHalt);
    clockLengthCounter(pulse2.lengthCounter, pulse2.lengthCounterHalt);
    clockLengthCounter(triangle.lengthCounter, triangle.controlFlag);
    clockLengthCounter(noise.lengthCounter, noise.lengthCounterHalt);
}

void APU::clockEnvelope(uint8_t period,
                        bool loop,
                        uint8_t& divider,
                        uint8_t& decayLevel,
                        bool& startFlag) {
    if(startFlag) {
        startFlag = false;
        decayLevel = ENVELOPE_MAX_DECAY_LEVEL;
        divider = period;
        return;
    }

    if(divider != 0x00) {
        divider--;
        return;
    }

    divider = period;
    if(decayLevel != 0x00) {
        decayLevel--;
    } else if(loop) {
        decayLevel = ENVELOPE_MAX_DECAY_LEVEL;
    }
}

void APU::clockLengthCounter(uint8_t& counter, bool halt) {
    if(counter != 0x00 && !halt) {
        counter--;
    }
}

uint8_t APU::pulseOutput(const PulseChannel& pulse) const {
    // Make sure the channel can be played
    if (!pulse.enabled || pulse.lengthCounter == 0x00) {
        return 0x00;
    }

    if (pulse.timerPeriod < PULSE_MINIMUM_TIMER_PERIOD) {
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

uint8_t APU::triangleOutput(const TriangleChannel& triangleChannel) const {
    if(!triangleChannel.enabled
        || triangleChannel.lengthCounter == 0x00
        || triangleChannel.linearCounter == 0x00) {
        return 0x00;
    }

    return TRIANGLE_SEQUENCE[triangleChannel.sequenceStep];
}

uint8_t APU::noiseOutput(const NoiseChannel& noiseChannel) const {
    if(!noiseChannel.enabled
        || noiseChannel.lengthCounter == 0x00
        || (noiseChannel.shiftRegister & NOISE_OUTPUT_BIT) != 0x0000) {
        return 0x00;
    }

    return noiseChannel.constantVolume
        ? noiseChannel.volume
        : noiseChannel.envelopeDecayLevel;
}

uint8_t APU::dmcOutput(const DmcChannel& dmcChannel) const {
    // The DMC DAC always contributes its current level, even when sample playback is disabled.
    return dmcChannel.outputLevel;
}

float APU::mixSample() const {
    // Get the level of all instruments
    const uint8_t pulse1Level = pulseOutput(pulse1);
    const uint8_t pulse2Level = pulseOutput(pulse2);
    const uint8_t pulseSum = pulse1Level + pulse2Level;
    const uint8_t triangleLevel = triangleOutput(triangle);
    const uint8_t noiseLevel = noiseOutput(noise);
    const uint8_t dmcLevel = dmcOutput(dmc);

    float output = 0.0F;
    if(pulseSum != 0x00) {
        output += PULSE_MIXER_NUMERATOR
            / ((PULSE_MIXER_DIVISOR / pulseSum) + PULSE_MIXER_OFFSET);
    }

    const float tndInput =
        (static_cast<float>(triangleLevel) / TRIANGLE_MIXER_DIVISOR)
        + (static_cast<float>(noiseLevel) / NOISE_MIXER_DIVISOR)
        + (static_cast<float>(dmcLevel) / DMC_MIXER_DIVISOR);
    if(tndInput > 0.0F) {
        output += TND_MIXER_NUMERATOR
            / ((1.0F / tndInput) + TND_MIXER_OFFSET);
    }

    return output;
}


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
