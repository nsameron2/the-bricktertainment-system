#pragma once

#include <array>
#include <chrono>
#include <cstdint>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

class Controller;

class Display {
public:
    static constexpr int SCREEN_WIDTH = 256;
    static constexpr int SCREEN_HEIGHT = 240;
    static constexpr int WINDOW_SCALE = 3;

    Display() = default;
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    bool initialize();
    bool pollQuit();
    bool pollEvents(Controller& controller);
    bool present(const std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT>& framebuffer);
    void shutdown();

private:
    using FrameClock = std::chrono::steady_clock;

    bool createWindow();
    bool createRenderer();
    bool createTexture();
    void paceFrame();

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;

    FrameClock::time_point nextFrameDeadline{};
    bool sdlInitialized = false;
    bool initialized = false;
};
