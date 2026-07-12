#include "APU.h"

#include <SDL3/SDL.h>

#include <iostream>


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
    return true;
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
