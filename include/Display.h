#pragma once

#include <array>
#include <cstdint>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

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
    bool present(const std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT>& framebuffer);
    void shutdown();

private:
    bool createWindow();
    bool createRenderer();
    bool createTexture();

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;

    bool sdlInitialized = false;
    bool initialized = false;
};
