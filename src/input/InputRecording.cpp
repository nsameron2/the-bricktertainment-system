#include "input/InputRecording.h"
#include "core/Console.h"
#include "frontend/Display.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>

namespace {
constexpr std::array<uint8_t, 4> FILE_MAGIC = {'T', 'B', 'S', 'I'};
constexpr uint8_t FILE_FORMAT_VERSION = 0x01;
constexpr std::size_t ROM_HASH_SIZE = sizeof(uint64_t);
constexpr std::streamoff FRAME_COUNT_OFFSET =
    static_cast<std::streamoff>(FILE_MAGIC.size() + sizeof(FILE_FORMAT_VERSION) + ROM_HASH_SIZE);

constexpr uint64_t FNV_OFFSET_BASIS = 14'695'981'039'346'656'037ULL;
constexpr uint64_t FNV_PRIME = 1'099'511'628'211ULL;
constexpr std::size_t HASH_BUFFER_SIZE = 4096;

template <typename T>
bool writeLittleEndian(std::ostream& output, T value) {
    static_assert(std::is_unsigned_v<T>);

    std::array<uint8_t, sizeof(T)> bytes{};
    for(std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(value >> (i * 8));
    }

    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    return output.good();
}

bool hashFile(const char* path, uint64_t& hash) {
    std::ifstream input(path, std::ios::binary);
    if(!input.is_open()) {
        return false;
    }

    hash = FNV_OFFSET_BASIS;
    std::array<char, HASH_BUFFER_SIZE> buffer{};

    while(input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytesRead = input.gcount();

        for(std::streamsize i = 0; i < bytesRead; ++i) {
            hash ^= static_cast<uint8_t>(buffer[static_cast<std::size_t>(i)]);
            hash *= FNV_PRIME;
        }
    }

    return input.eof();
}

bool writeHeader(std::ofstream& recordingFile, uint64_t romHash) {
    recordingFile.write(
        reinterpret_cast<const char*>(FILE_MAGIC.data()),
        static_cast<std::streamsize>(FILE_MAGIC.size())
    );
    recordingFile.put(static_cast<char>(FILE_FORMAT_VERSION));

    return recordingFile.good()
        && writeLittleEndian(recordingFile, romHash)
        && writeLittleEndian(recordingFile, uint64_t{0});
}

bool writeInputEvent(std::ofstream& recordingFile, uint32_t frameDelta, uint8_t controllerState) {
    return writeLittleEndian(recordingFile, frameDelta)
        && static_cast<bool>(recordingFile.put(static_cast<char>(controllerState)));
}
}


bool InputRecording::startRecording(const char* recordingPath, const char* romPath) {
    events.clear();
    frameCount = 0;
    lastError.clear();

    if(recordingPath == nullptr || recordingPath[0] == '\0') {
        lastError = "Input recording path cannot be empty.";
        return false;
    }

    if(romPath == nullptr || romPath[0] == '\0') {
        lastError = "ROM path cannot be empty.";
        return false;
    }

    std::error_code pathError;
    if(std::filesystem::equivalent(recordingPath, romPath, pathError) && !pathError) {
        lastError = "The input recording path cannot overwrite the ROM.";
        return false;
    }

    Console recConsole;
    if(!recConsole.initialize(romPath)) {
        lastError = std::string("Failed to load ROM: ") + romPath;
        return false;
    }

    Display recDisplay;
    if(!recDisplay.initialize()) {
        lastError = "Failed to initialize display.";
        return false;
    }

    if(!recConsole.initializeAudio()) {
        lastError = "Failed to initialize audio.";
        return false;
    }

    uint64_t romHash = 0;
    if(!hashFile(romPath, romHash)) {
        lastError = std::string("Failed to hash ROM: ") + romPath;
        return false;
    }

    std::ofstream recordingFile(recordingPath, std::ios::binary | std::ios::trunc);
    if(!recordingFile.is_open()) {
        lastError = std::string("Failed to open input recording: ") + recordingPath;
        return false;
    }

    if(!writeHeader(recordingFile, romHash)) {
        lastError = std::string("Failed to write input recording header: ") + recordingPath;
        return false;
    }

    Controller& controller = recConsole.getController1();
    uint8_t previousControllerState = controller.getButtonState();
    uint32_t framesSinceEvent = 0;

    while(!recDisplay.pollEvents(controller)) {
        const uint8_t controllerState = controller.getButtonState();

        const bool stateChanged = controllerState != previousControllerState;
        const bool frameDeltaFull = framesSinceEvent == std::numeric_limits<uint32_t>::max();
        if(stateChanged || frameDeltaFull) {
            if(!writeInputEvent(recordingFile, framesSinceEvent, controllerState)) {
                lastError = std::string("Failed to write input recording: ") + recordingPath;
                return false;
            }

            events.push_back({framesSinceEvent, controllerState});
            previousControllerState = controllerState;
            framesSinceEvent = 0;
        }

        recConsole.runFrame();
        ++framesSinceEvent;
        ++frameCount;

        if(!recDisplay.present(recConsole.getFramebuffer())) {
            lastError = "Failed to present recorded frame.";
            return false;
        }
    }

    recordingFile.seekp(FRAME_COUNT_OFFSET);
    if(!recordingFile || !writeLittleEndian(recordingFile, frameCount)) {
        lastError = std::string("Failed to finalize input recording: ") + recordingPath;
        return false;
    }

    recordingFile.flush();
    if(!recordingFile) {
        lastError = std::string("Failed to flush input recording: ") + recordingPath;
        return false;
    }

    recordingFile.close();
    if(recordingFile.fail()) {
        lastError = std::string("Failed to close input recording: ") + recordingPath;
        return false;
    }

    return true;
}

std::span<const InputRecording::InputEvent> InputRecording::getEvents() const {
    return events;
}

uint64_t InputRecording::getFrameCount() const {
    return frameCount;
}

const std::string& InputRecording::getLastError() const {
    return lastError;
}
