#include "Cartridge.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr size_t INES_HEADER_SIZE = 16;
constexpr size_t PRG_BANK_SIZE = 16 * 1024;
constexpr size_t CHR_BANK_SIZE = 8 * 1024;

void expectTrue(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void expectFalse(bool value, const char* message) {
    if (value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

std::filesystem::path testPath(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

void writeBytes(std::ofstream& file, const std::vector<uint8_t>& bytes) {
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

std::array<uint8_t, INES_HEADER_SIZE> makeHeader(uint8_t prgBanks,
                                                 uint8_t chrBanks,
                                                 uint8_t flags6 = 0x00) {
    std::array<uint8_t, INES_HEADER_SIZE> header{};
    header[0] = 'N';
    header[1] = 'E';
    header[2] = 'S';
    header[3] = 0x1A;
    header[4] = prgBanks;
    header[5] = chrBanks;
    header[6] = flags6;
    return header;
}

void writeRom(const std::filesystem::path& path,
              const std::array<uint8_t, INES_HEADER_SIZE>& header,
              bool includeTrainer) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(header.data()),
               static_cast<std::streamsize>(header.size()));

    if (includeTrainer) {
        writeBytes(file, std::vector<uint8_t>(512, 0xAA));
    }

    writeBytes(file, std::vector<uint8_t>(header[4] * PRG_BANK_SIZE, 0xEA));
    writeBytes(file, std::vector<uint8_t>(header[5] * CHR_BANK_SIZE, 0x00));
}

void writeRawFile(const std::filesystem::path& path,
                  const std::vector<uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary);
    writeBytes(file, bytes);
}

}

int main() {
    const auto shortHeaderPath = testPath("brick_test_short_header.nes");
    const auto badMagicPath = testPath("brick_test_bad_magic.nes");
    const auto validChrRomPath = testPath("brick_test_valid_chr_rom.nes");
    const auto trainerPath = testPath("brick_test_trainer.nes");
    const auto chrRamPath = testPath("brick_test_chr_ram.nes");
    const auto missingPath = testPath("brick_test_missing_file.nes");
    std::filesystem::remove(missingPath);

    {
        Cartridge cart;
        expectFalse(cart.load(missingPath.string().c_str()),
                    "missing cartridge file returns false");
    }

    writeRawFile(shortHeaderPath, std::vector<uint8_t>(8, 0x00));
    {
        Cartridge cart;
        expectFalse(cart.load(shortHeaderPath.string().c_str()),
                    "file shorter than 16-byte iNES header returns false");
    }

    auto badMagicHeader = makeHeader(1, 1);
    badMagicHeader[0] = 0x00;
    writeRom(badMagicPath, badMagicHeader, false);
    {
        Cartridge cart;
        expectFalse(cart.load(badMagicPath.string().c_str()),
                    "bad iNES magic header returns false");
    }

    writeRom(validChrRomPath, makeHeader(1, 1), false);
    {
        Cartridge cart;
        expectTrue(cart.load(validChrRomPath.string().c_str()),
                   "valid cartridge with PRG ROM and CHR ROM returns true");
    }

    writeRom(trainerPath, makeHeader(1, 1, 0x04), true);
    {
        Cartridge cart;
        expectTrue(cart.load(trainerPath.string().c_str()),
                   "valid cartridge with trainer returns true");
    }

    writeRom(chrRamPath, makeHeader(1, 0), false);
    {
        Cartridge cart;
        expectTrue(cart.load(chrRamPath.string().c_str()),
                   "valid cartridge with CHR RAM returns true");
    }

    std::filesystem::remove(shortHeaderPath);
    std::filesystem::remove(badMagicPath);
    std::filesystem::remove(validChrRomPath);
    std::filesystem::remove(trainerPath);
    std::filesystem::remove(chrRamPath);

    std::printf("test_cartridge passed\n");

    return 0;
}
