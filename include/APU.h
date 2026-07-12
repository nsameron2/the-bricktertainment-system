#pragma once

#include <cstddef>


struct SDL_AudioStream;

class APU {
public:
    APU() = default;
    ~APU();

    APU(const APU&) = delete;
    APU& operator=(const APU&) = delete;

    bool initialize();
    void shutdown();

private:
    // SDL Stuff
    SDL_AudioStream* stream = nullptr;

    bool sdlAudioInitialized = false;
    bool initialized = false;

    // Pure APU stuff
    bool queueSamples(const float* samples, const std::size_t sampleCount);
    bool queueTestTone();
};
