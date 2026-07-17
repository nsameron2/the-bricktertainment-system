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
constexpr std::size_t FRAME_COUNT_SIZE = sizeof(uint64_t);
constexpr std::size_t INPUT_EVENT_SIZE = sizeof(uint32_t) + sizeof(uint8_t);
constexpr std::size_t FILE_HEADER_SIZE =
    FILE_MAGIC.size() + sizeof(FILE_FORMAT_VERSION) + ROM_HASH_SIZE + FRAME_COUNT_SIZE;
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

template <typename T>
bool readLittleEndian(std::istream& input, T& value) {
    static_assert(std::is_unsigned_v<T>);

    std::array<uint8_t, sizeof(T)> bytes{};
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    if(!input) {
        return false;
    }

    value = 0;
    for(std::size_t i = 0; i < bytes.size(); ++i) {
        value |= static_cast<T>(bytes[i]) << (i * 8);
    }

    return true;
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

bool InputRecording::load(const char* recordingPath, const char* romPath) {
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

    uint64_t expectedRomHash = 0;
    if(!hashFile(romPath, expectedRomHash)) {
        lastError = std::string("Failed to hash ROM: ") + romPath;
        return false;
    }

    std::ifstream recordingFile(recordingPath, std::ios::binary | std::ios::ate);
    if(!recordingFile.is_open()) {
        lastError = std::string("Failed to open input recording: ") + recordingPath;
        return false;
    }

    const std::streamoff fileSize = recordingFile.tellg();
    if(fileSize < static_cast<std::streamoff>(FILE_HEADER_SIZE)) {
        lastError = "Input recording is too small to contain a valid header.";
        return false;
    }

    const std::streamoff eventDataSize =
        fileSize - static_cast<std::streamoff>(FILE_HEADER_SIZE);
    if(eventDataSize % static_cast<std::streamoff>(INPUT_EVENT_SIZE) != 0) {
        lastError = "Input recording contains an incomplete event.";
        return false;
    }

    recordingFile.seekg(0);

    std::array<uint8_t, FILE_MAGIC.size()> magic{};
    recordingFile.read(
        reinterpret_cast<char*>(magic.data()),
        static_cast<std::streamsize>(magic.size())
    );
    if(!recordingFile || magic != FILE_MAGIC) {
        lastError = "Input recording has an invalid file signature.";
        return false;
    }

    const int version = recordingFile.get();
    if(version != FILE_FORMAT_VERSION) {
        lastError = "Input recording uses an unsupported format version.";
        return false;
    }

    uint64_t recordedRomHash = 0;
    uint64_t recordedFrameCount = 0;
    if(!readLittleEndian(recordingFile, recordedRomHash)
        || !readLittleEndian(recordingFile, recordedFrameCount)) {
        lastError = "Input recording has an incomplete header.";
        return false;
    }

    if(recordedRomHash != expectedRomHash) {
        lastError = "Input recording was created for a different ROM.";
        return false;
    }

    const std::size_t eventCount = static_cast<std::size_t>(
        eventDataSize / static_cast<std::streamoff>(INPUT_EVENT_SIZE)
    );
    std::vector<InputEvent> loadedEvents;
    loadedEvents.reserve(eventCount);

    uint64_t eventFrame = 0;
    for(std::size_t i = 0; i < eventCount; ++i) {
        InputEvent event{};
        if(!readLittleEndian(recordingFile, event.frameDelta)) {
            lastError = "Input recording contains an incomplete event.";
            return false;
        }

        const int controllerState = recordingFile.get();
        if(controllerState == std::char_traits<char>::eof()) {
            lastError = "Input recording contains an incomplete event.";
            return false;
        }
        event.controllerState = static_cast<uint8_t>(controllerState);

        if(eventFrame > recordedFrameCount
            || event.frameDelta > recordedFrameCount - eventFrame) {
            lastError = "Input recording event occurs beyond the recorded frame count.";
            return false;
        }

        eventFrame += event.frameDelta;
        if(eventFrame >= recordedFrameCount) {
            lastError = "Input recording event occurs beyond the recorded frame count.";
            return false;
        }

        loadedEvents.push_back(event);
    }

    events = std::move(loadedEvents);
    frameCount = recordedFrameCount;
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
