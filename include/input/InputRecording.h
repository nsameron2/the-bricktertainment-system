#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>


class InputRecording {
public:
    struct InputEvent {
        uint32_t frameDelta = 0;
        uint8_t controllerState = 0x00;
    };

    bool startRecording(const char* recordingPath, const char* romPath);
    bool load(const char* recordingPath, const char* romPath);

    std::span<const InputEvent> getEvents() const;
    uint64_t getFrameCount() const;
    const std::string& getLastError() const;

private:
    std::vector<InputEvent> events;
    uint64_t frameCount = 0;
    std::string lastError;
};
