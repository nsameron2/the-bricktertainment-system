#include "core/Cartridge.h"

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
constexpr uint8_t INES_VERTICAL_MIRRORING = 1 << 0;
constexpr uint8_t INES_TRAINER_PRESENT = 1 << 2;
constexpr uint8_t INES_FOUR_SCREEN_VRAM = 1 << 3;

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

void expectEqual(uint8_t actual, uint8_t expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%02X, got 0x%02X)\n",
                     message,
                     expected,
                     actual);
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

void writeRomData(const std::filesystem::path& path,
                  const std::array<uint8_t, INES_HEADER_SIZE>& header,
                  const std::vector<uint8_t>& prgData,
                  const std::vector<uint8_t>& chrData) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(header.data()),
               static_cast<std::streamsize>(header.size()));
    writeBytes(file, prgData);
    writeBytes(file, chrData);
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
    const auto verticalMirroringPath = testPath("brick_test_vertical_mirroring.nes");
    const auto fourScreenPath = testPath("brick_test_four_screen.nes");
    const auto trainerPath = testPath("brick_test_trainer.nes");
    const auto chrRamPath = testPath("brick_test_chr_ram.nes");
    const auto invalidPrgBanksPath = testPath("brick_test_invalid_prg_banks.nes");
    const auto mapper0_16Path = testPath("brick_test_mapper0_16kb.nes");
    const auto mapper0_32Path = testPath("brick_test_mapper0_32kb.nes");
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
        expectTrue(cart.getNametableMirroring() == Cartridge::NametableMirroring::Horizontal,
                   "iNES header without mirroring flags selects horizontal mirroring");
    }

    writeRom(verticalMirroringPath, makeHeader(1, 1, INES_VERTICAL_MIRRORING), false);
    {
        Cartridge cart;
        expectTrue(cart.load(verticalMirroringPath.string().c_str()),
                   "vertical-mirroring cartridge loads successfully");
        expectTrue(cart.getNametableMirroring() == Cartridge::NametableMirroring::Vertical,
                   "iNES header bit 0 selects vertical mirroring");
    }

    writeRom(fourScreenPath, makeHeader(1, 1, INES_FOUR_SCREEN_VRAM), false);
    {
        Cartridge cart;
        expectTrue(cart.load(fourScreenPath.string().c_str()),
                   "four-screen cartridge loads successfully");
        expectTrue(cart.getNametableMirroring() == Cartridge::NametableMirroring::FourScreen,
                   "iNES header bit 3 selects four-screen VRAM");
    }

    {
        Cartridge cart;
        uint8_t data = 0x00;

        expectTrue(cart.load(validChrRomPath.string().c_str()),
                   "valid cartridge loads before failed reload");
        expectTrue(cart.cpuRead(0x8000, data),
                   "loaded cartridge handles CPU read before failed reload");
        expectFalse(cart.load(badMagicPath.string().c_str()),
                    "failed reload returns false");
        expectFalse(cart.cpuRead(0x8000, data),
                    "failed reload clears previous cartridge data");
    }

    writeRom(invalidPrgBanksPath, makeHeader(3, 1), false);
    {
        Cartridge cart;
        expectFalse(cart.load(invalidPrgBanksPath.string().c_str()),
                    "Mapper 0 cartridge with invalid PRG bank count returns false");
    }

    writeRom(trainerPath, makeHeader(1, 1, INES_TRAINER_PRESENT), true);
    {
        Cartridge cart;
        expectTrue(cart.load(trainerPath.string().c_str()),
                   "valid cartridge with trainer returns true");
    }

    writeRom(chrRamPath, makeHeader(1, 0), false);
    {
        Cartridge cart;
        uint8_t data = 0x00;

        expectTrue(cart.load(chrRamPath.string().c_str()),
                   "valid cartridge with CHR RAM returns true");
        expectTrue(cart.ppuRead(0x0000, data),
                   "PPU read from CHR RAM range is handled");
        expectEqual(data, 0x00, "new CHR RAM reads as 0x00");
        expectTrue(cart.ppuWrite(0x0000, 0x77),
                   "PPU write to CHR RAM range is handled");
        expectTrue(cart.ppuRead(0x0000, data),
                   "PPU read after CHR RAM write is handled");
        expectEqual(data, 0x77, "PPU write modifies CHR RAM");
        expectFalse(cart.ppuRead(0x2000, data),
                    "PPU read outside CHR range is not handled by cartridge");
    }

    std::vector<uint8_t> prg16(PRG_BANK_SIZE, 0xEA);
    prg16[0x0000] = 0x11;
    prg16[0x3FFF] = 0x22;
    std::vector<uint8_t> chrRom(CHR_BANK_SIZE, 0x00);
    chrRom[0x0000] = 0xA1;
    chrRom[0x1FFF] = 0xB2;
    writeRomData(mapper0_16Path, makeHeader(1, 1), prg16, chrRom);
    {
        Cartridge cart;
        uint8_t data = 0x00;

        expectTrue(cart.load(mapper0_16Path.string().c_str()),
                   "valid 16KB Mapper 0 cartridge loads");
        expectFalse(cart.cpuRead(0x7FFF, data),
                    "CPU read below 0x8000 is not handled by cartridge");
        expectTrue(cart.cpuRead(0x8000, data),
                   "CPU read at 0x8000 is handled by cartridge");
        expectEqual(data, 0x11, "16KB PRG maps 0x8000 to PRG offset 0x0000");
        expectTrue(cart.cpuRead(0xBFFF, data),
                   "CPU read at 0xBFFF is handled by cartridge");
        expectEqual(data, 0x22, "16KB PRG maps 0xBFFF to PRG offset 0x3FFF");
        expectTrue(cart.cpuRead(0xC000, data),
                   "CPU read at 0xC000 is handled by cartridge");
        expectEqual(data, 0x11, "16KB PRG mirrors 0xC000 to PRG offset 0x0000");
        expectTrue(cart.cpuRead(0xFFFF, data),
                   "CPU read at 0xFFFF is handled by cartridge");
        expectEqual(data, 0x22, "16KB PRG mirrors 0xFFFF to PRG offset 0x3FFF");

        expectTrue(cart.cpuWrite(0x8000, 0x99),
                   "CPU write to Mapper 0 PRG range is handled");
        expectTrue(cart.cpuRead(0x8000, data),
                   "CPU read after Mapper 0 write is handled");
        expectEqual(data, 0x11, "Mapper 0 PRG write does not modify ROM data");

        expectTrue(cart.ppuRead(0x0000, data),
                   "PPU read at 0x0000 is handled by CHR ROM");
        expectEqual(data, 0xA1, "PPU read maps 0x0000 to CHR offset 0x0000");
        expectTrue(cart.ppuRead(0x1FFF, data),
                   "PPU read at 0x1FFF is handled by CHR ROM");
        expectEqual(data, 0xB2, "PPU read maps 0x1FFF to CHR offset 0x1FFF");
        expectTrue(cart.ppuWrite(0x0000, 0x99),
                   "PPU write to CHR ROM range is handled");
        expectTrue(cart.ppuRead(0x0000, data),
                   "PPU read after CHR ROM write is handled");
        expectEqual(data, 0xA1, "PPU write does not modify CHR ROM");
    }

    std::vector<uint8_t> prg32(PRG_BANK_SIZE * 2, 0xEA);
    prg32[0x0000] = 0x31;
    prg32[0x4000] = 0x42;
    prg32[0x7FFF] = 0x53;
    writeRomData(mapper0_32Path,
                 makeHeader(2, 1),
                 prg32,
                 std::vector<uint8_t>(CHR_BANK_SIZE, 0x00));
    {
        Cartridge cart;
        uint8_t data = 0x00;

        expectTrue(cart.load(mapper0_32Path.string().c_str()),
                   "valid 32KB Mapper 0 cartridge loads");
        expectTrue(cart.cpuRead(0x8000, data),
                   "CPU read at 0x8000 is handled by 32KB cartridge");
        expectEqual(data, 0x31, "32KB PRG maps 0x8000 to PRG offset 0x0000");
        expectTrue(cart.cpuRead(0xC000, data),
                   "CPU read at 0xC000 is handled by 32KB cartridge");
        expectEqual(data, 0x42, "32KB PRG maps 0xC000 to PRG offset 0x4000");
        expectTrue(cart.cpuRead(0xFFFF, data),
                   "CPU read at 0xFFFF is handled by 32KB cartridge");
        expectEqual(data, 0x53, "32KB PRG maps 0xFFFF to PRG offset 0x7FFF");
        expectFalse(cart.cpuWrite(0x7FFF, 0x99),
                    "CPU write below 0x8000 is not handled by cartridge");
    }

    std::filesystem::remove(shortHeaderPath);
    std::filesystem::remove(badMagicPath);
    std::filesystem::remove(validChrRomPath);
    std::filesystem::remove(verticalMirroringPath);
    std::filesystem::remove(fourScreenPath);
    std::filesystem::remove(trainerPath);
    std::filesystem::remove(chrRamPath);
    std::filesystem::remove(invalidPrgBanksPath);
    std::filesystem::remove(mapper0_16Path);
    std::filesystem::remove(mapper0_32Path);

    std::printf("test_cartridge passed\n");

    return 0;
}
