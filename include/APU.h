#pragma once

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
    SDL_AudioStream* stream = nullptr;

    bool sdlAudioInitialized = false;
    bool initialized = false;
};
