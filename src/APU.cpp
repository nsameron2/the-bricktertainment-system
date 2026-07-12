#include "APU.h"

#include <SDL3/SDL.h>

#include <array>
#include <cmath>
#include <iostream>
#include <numbers>


namespace {

constexpr int AUDIO_SAMPLE_RATE = 44100;
constexpr int AUDIO_CHANNEL_COUNT = 1;


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
    if(!queueTestTone()) {
        logSdlError("SDL_PutAudioStreamData");
        shutdown();
        return false;
    }

    return true;
}

bool APU::queueSamples(const float* samples, const std::size_t sampleCount) {
    if(stream == nullptr || samples == nullptr) {
        return false;
    }

    // SDL needs byte count
    const std::size_t byteCount = sampleCount * sizeof(float);


    return SDL_PutAudioStreamData(stream, samples, static_cast<int>(byteCount));
}

bool APU::queueTestTone() {
    constexpr float FREQUENCY = 440.0f;
    constexpr float VOLUME = 0.10f;
    constexpr std::size_t SAMPLE_COUNT = AUDIO_SAMPLE_RATE / 2;

    std::array<float, SAMPLE_COUNT> samples{};

    for(std::size_t i = 0; i < samples.size(); i++) {
        const float time = static_cast<float>(i) / AUDIO_SAMPLE_RATE;
        samples[i] = VOLUME * std::sin(
            2.0f * std::numbers::pi_v<float> * FREQUENCY * time
        );
    }

    return queueSamples(samples.data(), samples.size());
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
