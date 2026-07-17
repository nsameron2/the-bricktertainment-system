#include "benchmark/Benchmark.h"
#include "input/InputRecording.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <type_traits>
#include <vector>

namespace {

constexpr std::array<uint8_t, 4> FILE_MAGIC = {'T', 'B', 'S', 'I'};
constexpr uint8_t FILE_FORMAT_VERSION = 0x01;
constexpr uint64_t FNV_OFFSET_BASIS = 14'695'981'039'346'656'037ULL;
constexpr uint64_t FNV_PRIME = 1'099'511'628'211ULL;
constexpr std::size_t INES_HEADER_SIZE = 16;
constexpr std::size_t PRG_ROM_SIZE = 16 * 1024;
constexpr std::size_t RESET_VECTOR_OFFSET = INES_HEADER_SIZE + 0x3FFC;

void expectTrue(bool value, const char* message) {
    if(!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void expectFalse(bool value, const char* message) {
    if(value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void expectEqual(uint64_t actual, uint64_t expected, const char* message) {
    if(actual != expected) {
        std::fprintf(
            stderr,
            "FAIL: %s (expected %llu, got %llu)\n",
            message,
            static_cast<unsigned long long>(expected),
            static_cast<unsigned long long>(actual)
        );
        std::exit(EXIT_FAILURE);
    }
}

std::filesystem::path testPath(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

void writeBytes(const std::filesystem::path& path, std::span<const uint8_t> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
}

uint64_t hashBytes(std::span<const uint8_t> bytes) {
    uint64_t hash = FNV_OFFSET_BASIS;
    for(const uint8_t byte : bytes) {
        hash ^= byte;
        hash *= FNV_PRIME;
    }
    return hash;
}

template <typename T>
void writeLittleEndian(std::ofstream& output, T value) {
    static_assert(std::is_unsigned_v<T>);

    for(std::size_t i = 0; i < sizeof(T); ++i) {
        output.put(static_cast<char>(static_cast<uint8_t>(value >> (i * 8))));
    }
}

void writeRecording(
    const std::filesystem::path& path,
    uint64_t romHash,
    uint64_t frameCount,
    std::span<const InputRecording::InputEvent> events,
    std::array<uint8_t, 4> magic = FILE_MAGIC,
    uint8_t version = FILE_FORMAT_VERSION
) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(
        reinterpret_cast<const char*>(magic.data()),
        static_cast<std::streamsize>(magic.size())
    );
    output.put(static_cast<char>(version));
    writeLittleEndian(output, romHash);
    writeLittleEndian(output, frameCount);

    for(const InputRecording::InputEvent event : events) {
        writeLittleEndian(output, event.frameDelta);
        output.put(static_cast<char>(event.controllerState));
    }
}

}

int main() {
    const auto romPath = testPath("brick_input_recording_rom.nes");
    const auto otherRomPath = testPath("brick_input_recording_other_rom.nes");
    const auto recordingPath = testPath("brick_input_recording_valid.tbsi");
    const auto badMagicPath = testPath("brick_input_recording_bad_magic.tbsi");
    const auto badVersionPath = testPath("brick_input_recording_bad_version.tbsi");
    const auto truncatedPath = testPath("brick_input_recording_truncated.tbsi");
    const auto outOfRangePath = testPath("brick_input_recording_out_of_range.tbsi");
    const auto benchmarkRomPath = testPath("brick_input_recording_benchmark.nes");
    const auto benchmarkRecordingPath = testPath("brick_input_recording_benchmark.tbsi");

    const std::array<uint8_t, 8> romBytes = {0x4E, 0x45, 0x53, 0x1A, 0x01, 0x00, 0x00, 0x00};
    const std::array<uint8_t, 8> otherRomBytes = {
        0x4E, 0x45, 0x53, 0x1A, 0x02, 0x00, 0x00, 0x00,
    };
    writeBytes(romPath, romBytes);
    writeBytes(otherRomPath, otherRomBytes);

    const std::array<InputRecording::InputEvent, 3> events = {
        InputRecording::InputEvent{0, 0x08},
        InputRecording::InputEvent{120, 0x81},
        InputRecording::InputEvent{5, 0x00},
    };
    const uint64_t romHash = hashBytes(romBytes);
    writeRecording(recordingPath, romHash, 200, events);

    InputRecording recording;
    expectTrue(
        recording.load(recordingPath.string().c_str(), romPath.string().c_str()),
        "valid input recording loads"
    );
    expectEqual(recording.getFrameCount(), 200, "total frame count is decoded");
    expectEqual(recording.getEvents().size(), events.size(), "all input events are decoded");
    expectEqual(recording.getEvents()[1].frameDelta, 120, "frame delta is decoded");
    expectEqual(recording.getEvents()[1].controllerState, 0x81, "controller state is decoded");

    expectFalse(
        recording.load(recordingPath.string().c_str(), otherRomPath.string().c_str()),
        "recording rejects a different ROM"
    );

    std::array<uint8_t, 4> badMagic = FILE_MAGIC;
    badMagic[0] = 0x00;
    writeRecording(badMagicPath, romHash, 200, events, badMagic);
    expectFalse(
        recording.load(badMagicPath.string().c_str(), romPath.string().c_str()),
        "recording rejects invalid file magic"
    );

    writeRecording(badVersionPath, romHash, 200, events, FILE_MAGIC, 0x02);
    expectFalse(
        recording.load(badVersionPath.string().c_str(), romPath.string().c_str()),
        "recording rejects unsupported format version"
    );

    writeRecording(truncatedPath, romHash, 200, events);
    std::filesystem::resize_file(truncatedPath, std::filesystem::file_size(truncatedPath) - 1);
    expectFalse(
        recording.load(truncatedPath.string().c_str(), romPath.string().c_str()),
        "recording rejects an incomplete input event"
    );

    const std::array<InputRecording::InputEvent, 1> outOfRangeEvent = {
        InputRecording::InputEvent{10, 0x01},
    };
    writeRecording(outOfRangePath, romHash, 10, outOfRangeEvent);
    expectFalse(
        recording.load(outOfRangePath.string().c_str(), romPath.string().c_str()),
        "recording rejects an event beyond the final frame"
    );

    std::vector<uint8_t> benchmarkRom(INES_HEADER_SIZE + PRG_ROM_SIZE, 0x00);
    std::fill(benchmarkRom.begin() + INES_HEADER_SIZE, benchmarkRom.end(), 0xEA);
    benchmarkRom[0] = 'N';
    benchmarkRom[1] = 'E';
    benchmarkRom[2] = 'S';
    benchmarkRom[3] = 0x1A;
    benchmarkRom[4] = 0x01;
    benchmarkRom[5] = 0x00;
    benchmarkRom[RESET_VECTOR_OFFSET] = 0x00;
    benchmarkRom[RESET_VECTOR_OFFSET + 1] = 0x80;
    writeBytes(benchmarkRomPath, benchmarkRom);

    const std::array<InputRecording::InputEvent, 2> benchmarkEvents = {
        InputRecording::InputEvent{0, 0x08},
        InputRecording::InputEvent{1, 0x00},
    };
    writeRecording(
        benchmarkRecordingPath,
        hashBytes(benchmarkRom),
        2,
        benchmarkEvents
    );

    Benchmark benchmark;
    expectEqual(
        benchmark.runInput(
            benchmarkRomPath.string().c_str(),
            benchmarkRecordingPath.string().c_str()
        ),
        EXIT_SUCCESS,
        "input benchmark replays a valid recording"
    );

    std::filesystem::remove(romPath);
    std::filesystem::remove(otherRomPath);
    std::filesystem::remove(recordingPath);
    std::filesystem::remove(badMagicPath);
    std::filesystem::remove(badVersionPath);
    std::filesystem::remove(truncatedPath);
    std::filesystem::remove(outOfRangePath);
    std::filesystem::remove(benchmarkRomPath);
    std::filesystem::remove(benchmarkRecordingPath);

    return EXIT_SUCCESS;
}
