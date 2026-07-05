#include "Cartridge.h"
#include "PPUBus.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

std::array<uint8_t, INES_HEADER_SIZE> makeHeader(uint8_t prgBanks,
                                                 uint8_t chrBanks) {
    std::array<uint8_t, INES_HEADER_SIZE> header{};
    header[0] = 'N';
    header[1] = 'E';
    header[2] = 'S';
    header[3] = 0x1A;
    header[4] = prgBanks;
    header[5] = chrBanks;
    return header;
}

void writeBytes(std::ofstream& file, const std::vector<uint8_t>& bytes) {
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
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

}

int main() {
    {
        PPUBus bus;

        expectEqual(bus.read(0x0000), 0x00, "CHR read without cartridge returns 0x00");
        expectEqual(bus.read(0x2000), 0x00, "nametable RAM initializes to 0x00");
        expectEqual(bus.read(0x3F00), 0x00, "palette RAM initializes to 0x00");

        bus.write(0x2000, 0x12);
        expectEqual(bus.read(0x2000), 0x12, "nametable write/read at 0x2000");
        expectEqual(bus.read(0x2800), 0x12, "0x2800 mirrors nametable RAM offset 0x0000");
        expectEqual(bus.read(0x3000), 0x12, "0x3000 mirrors nametable RAM offset 0x0000");

        bus.write(0x27FF, 0x34);
        expectEqual(bus.read(0x2FFF), 0x34, "nametable RAM offset 0x07FF mirrors through 0x2FFF");

        bus.write(0x3F00, 0x0F);
        expectEqual(bus.read(0x3F00), 0x0F, "palette write/read at 0x3F00");
        expectEqual(bus.read(0x3F10), 0x0F, "0x3F10 mirrors 0x3F00");
        expectEqual(bus.read(0x3F20), 0x0F, "palette mirrors every 0x20 bytes");

        bus.write(0x3F14, 0x22);
        expectEqual(bus.read(0x3F04), 0x22, "0x3F14 mirrors 0x3F04");
    }

    const auto chrRomPath = std::filesystem::temp_directory_path() / "brick_test_ppu_bus_chr_rom.nes";
    const auto chrRamPath = std::filesystem::temp_directory_path() / "brick_test_ppu_bus_chr_ram.nes";

    std::vector<uint8_t> prgData(PRG_BANK_SIZE, 0xEA);
    std::vector<uint8_t> chrRom(CHR_BANK_SIZE, 0x00);
    chrRom[0x0000] = 0xA1;
    chrRom[0x1FFF] = 0xB2;

    writeRomData(chrRomPath, makeHeader(1, 1), prgData, chrRom);
    {
        Cartridge cartridge;
        PPUBus bus;

        expectTrue(cartridge.load(chrRomPath.string().c_str()),
                   "CHR ROM cartridge loads successfully");

        bus.insertCartridge(&cartridge);
        expectEqual(bus.read(0x0000), 0xA1, "PPUBus routes 0x0000 to cartridge CHR ROM");
        expectEqual(bus.read(0x1FFF), 0xB2, "PPUBus routes 0x1FFF to cartridge CHR ROM");

        bus.write(0x0000, 0x99);
        expectEqual(bus.read(0x0000), 0xA1, "PPUBus write does not modify CHR ROM");
    }

    writeRomData(chrRamPath, makeHeader(1, 0), prgData, {});
    {
        Cartridge cartridge;
        PPUBus bus;

        expectTrue(cartridge.load(chrRamPath.string().c_str()),
                   "CHR RAM cartridge loads successfully");

        bus.insertCartridge(&cartridge);
        bus.write(0x0002, 0x5A);
        expectEqual(bus.read(0x0002), 0x5A, "PPUBus write modifies cartridge CHR RAM");
    }

    std::filesystem::remove(chrRomPath);
    std::filesystem::remove(chrRamPath);

    std::printf("test_ppu_bus passed\n");

    return 0;
}
