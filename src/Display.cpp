#include "Display.h"
#include "Controller.h"

#include <SDL3/SDL.h>

#include <iostream>


namespace {

constexpr const char* WINDOW_TITLE = "The Bricktertainment System";


void logSdlError(const char* operation) {
    std::cerr << operation << " failed: " << SDL_GetError() << '\n';
}

bool isQuitEvent(const SDL_Event& event) {
    return event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED;
}

bool mapKeyToButton(SDL_Keycode key, Controller::Button& button) {
    switch (key) {
        case SDLK_X:
            button = Controller::Button::A;
            return true;
        case SDLK_Z:
            button = Controller::Button::B;
            return true;
        case SDLK_SPACE:
            button = Controller::Button::Select;
            return true;
        case SDLK_RETURN:
            button = Controller::Button::Start;
            return true;
        case SDLK_UP:
            button = Controller::Button::Up;
            return true;
        case SDLK_DOWN:
            button = Controller::Button::Down;
            return true;
        case SDLK_LEFT:
            button = Controller::Button::Left;
            return true;
        case SDLK_RIGHT:
            button = Controller::Button::Right;
            return true;
        default:
            return false;
    }
}

}

Display::~Display() {
    shutdown();
}

bool Display::initialize() {
    if (initialized) {
        return true;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        logSdlError("SDL_Init");
        return false;
    }
    sdlInitialized = true;

    if (!createWindow() || !createRenderer() || !createTexture()) {
        shutdown();
        return false;
    }

    initialized = true;
    return true;
}

bool Display::pollQuit() {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (isQuitEvent(event)) {
            return true;
        }
    }

    return false;
}

bool Display::pollEvents(Controller& controller) {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (isQuitEvent(event)) {
            return true;
        }

        if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
            Controller::Button button{};
            if (mapKeyToButton(event.key.key, button)) {
                controller.setButton(button, event.key.down);
            }
        }
    }

    return false;
}

bool Display::present(const std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT>& framebuffer) {
    if (!initialized) {
        return false;
    }

    if (!SDL_UpdateTexture(texture,
                           nullptr,
                           framebuffer.data(),
                           SCREEN_WIDTH * static_cast<int>(sizeof(uint32_t)))) {
        logSdlError("SDL_UpdateTexture");
        return false;
    }

    if (!SDL_RenderClear(renderer)) {
        logSdlError("SDL_RenderClear");
        return false;
    }

    const SDL_FRect destination{
        0.0f,
        0.0f,
        static_cast<float>(SCREEN_WIDTH * WINDOW_SCALE),
        static_cast<float>(SCREEN_HEIGHT * WINDOW_SCALE),
    };

    if (!SDL_RenderTexture(renderer, texture, nullptr, &destination)) {
        logSdlError("SDL_RenderTexture");
        return false;
    }

    if (!SDL_RenderPresent(renderer)) {
        logSdlError("SDL_RenderPresent");
        return false;
    }

    return true;
}

void Display::shutdown() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }

    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    if (sdlInitialized) {
        SDL_Quit();
        sdlInitialized = false;
    }

    initialized = false;
}

bool Display::createWindow() {
    window = SDL_CreateWindow(WINDOW_TITLE,
                              SCREEN_WIDTH * WINDOW_SCALE,
                              SCREEN_HEIGHT * WINDOW_SCALE,
                              0);
    if (!window) {
        logSdlError("SDL_CreateWindow");
        return false;
    }

    return true;
}

bool Display::createRenderer() {
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        logSdlError("SDL_CreateRenderer");
        return false;
    }

    if (!SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF)) {
        logSdlError("SDL_SetRenderDrawColor");
        return false;
    }

    return true;
}

bool Display::createTexture() {
    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                SCREEN_WIDTH,
                                SCREEN_HEIGHT);
    if (!texture) {
        logSdlError("SDL_CreateTexture");
        return false;
    }

    if (!SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) {
        logSdlError("SDL_SetTextureScaleMode");
        return false;
    }

    return true;
}
