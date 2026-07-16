#include "core/CPUBus.h"
#include "core/Cartridge.h"
#include "core/PPU.h"
#include "core/PPUBus.h"
#include "input/Controller.h"

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
constexpr uint16_t DMA_PAGE_SIZE = 0x0100;
constexpr uint16_t OAM_ADDRESS_REGISTER = 0x2003;
constexpr uint16_t OAM_DATA_REGISTER = 0x2004;
constexpr uint16_t OAM_DMA_REGISTER = 0x4014;
constexpr uint16_t DMC_DMA_STALL_CYCLES = 0x0004;

void expectTrue(bool value, const char* message) {
    if(!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void expectEqual(uint8_t actual, uint8_t expected, const char* message) {
    if(actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%02X, got 0x%02X)\n",
                     message,
                     expected,
                     actual);
        std::exit(EXIT_FAILURE);
    }
}

void expectEqual16(uint16_t actual, uint16_t expected, const char* message) {
    if(actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%04X, got 0x%04X)\n",
                     message,
                     expected,
                     actual);
        std::exit(EXIT_FAILURE);
    }
}

uint16_t runDma(CPUBus& bus, bool firstCycleOdd) {
    uint16_t cycles = 0x0000;
    bool oddCpuCycle = firstCycleOdd;

    while(bus.isDmaActive()) {
        bus.clockDma(oddCpuCycle);
        oddCpuCycle = !oddCpuCycle;
        cycles++;
    }

    return cycles;
}

uint8_t readOam(CPUBus& bus, uint8_t address) {
    bus.write(OAM_ADDRESS_REGISTER, address);
    return bus.read(OAM_DATA_REGISTER);
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
    CPUBus bus;

    // MIRRORING TEST
    expectEqual(bus.read(0x0000), 0x00, "RAM initializes to 0x00 at 0x0000");
    expectEqual(bus.read(0x07FF), 0x00, "RAM initializes to 0x00 at 0x07FF");

    bus.write(0x0000, 0x42);
    expectEqual(bus.read(0x0000), 0x42, "write/read at 0x0000");

    bus.write(0x07FF, 0x99);
    expectEqual(bus.read(0x07FF), 0x99, "write/read at 0x07FF");

    bus.write(0x0800, 0x11);
    expectEqual(bus.read(0x0000), 0x11, "0x0800 mirrors 0x0000");
    expectEqual(bus.read(0x1000), 0x11, "0x1000 mirrors 0x0000");
    expectEqual(bus.read(0x1800), 0x11, "0x1800 mirrors 0x0000");

    bus.write(0x1FFF, 0x7E);
    expectEqual(bus.read(0x07FF), 0x7E, "0x1FFF mirrors 0x07FF");

    bus.write(0x0001, 0x5A);
    expectTrue(!bus.isDmcDmaActive(), "DMC DMA starts inactive");
    expectEqual(bus.readDmc(0x0001), 0x5A, "DMC DMA reads through CPU memory map");
    expectTrue(bus.isDmcDmaActive(), "DMC read starts CPU stall");

    uint16_t dmcStallCycles = 0x0000;
    while(bus.isDmcDmaActive()) {
        bus.clockDmcDma();
        dmcStallCycles++;
    }
    expectEqual16(dmcStallCycles,
                  DMC_DMA_STALL_CYCLES,
                  "DMC DMA stalls CPU for four cycles");

    bus.write(0x2000, 0x55);
    expectEqual(bus.read(0x2000), 0x00, "PPU register address reads as 0x00 without connected PPU");
    expectEqual(bus.read(0x0000), 0x11, "PPU register write without connected PPU does not alter RAM");

    PPU ppu;
    PPUBus ppuBus;
    ppu.connectBus(&ppuBus);
    bus.connectPPU(&ppu);

    bus.write(0x2006, 0x23);
    bus.write(0x2006, 0xC0);
    bus.write(0x2007, 0x66);
    expectEqual(ppuBus.read(0x23C0), 0x66, "CPUBus routes PPUADDR/PPUDATA writes to PPU registers");

    bus.write(0x2008, 0x04);
    bus.write(0x2006, 0x24);
    bus.write(0x2006, 0x00);
    bus.write(0x2007, 0x77);
    bus.write(0x2007, 0x88);
    expectEqual(ppuBus.read(0x2400), 0x77, "CPUBus mirrors 0x2008 to PPUCTRL");
    expectEqual(ppuBus.read(0x2420), 0x88, "mirrored PPUCTRL write changes PPUDATA increment");

    for(uint16_t offset = 0x0000; offset < DMA_PAGE_SIZE; offset++) {
        bus.write(0x0200 + offset, static_cast<uint8_t>(offset ^ 0xA5));
    }

    bus.write(OAM_ADDRESS_REGISTER, 0x80);
    bus.write(OAM_DMA_REGISTER, 0x02);
    expectTrue(bus.isDmaActive(), "write to 0x4014 starts OAM DMA");

    expectEqual16(runDma(bus, true),
                  0x0201,
                  "OAM DMA takes 0x0201 CPU cycles without an alignment cycle");
    expectTrue(!bus.isDmaActive(), "OAM DMA stops after copying 0x0100 bytes");
    expectEqual(readOam(bus, 0x80),
                static_cast<uint8_t>(0x00 ^ 0xA5),
                "OAM DMA writes the first source byte at the current OAMADDR");
    expectEqual(readOam(bus, 0xFF),
                static_cast<uint8_t>(0x7F ^ 0xA5),
                "OAM DMA writes through the end of OAM");
    expectEqual(readOam(bus, 0x00),
                static_cast<uint8_t>(0x80 ^ 0xA5),
                "OAM DMA wraps OAMADDR from 0xFF to 0x00");
    expectEqual(readOam(bus, 0x7F),
                static_cast<uint8_t>(0xFF ^ 0xA5),
                "OAM DMA writes all 0x0100 source bytes after wrapping");

    for(uint16_t offset = 0x0000; offset < DMA_PAGE_SIZE; offset++) {
        bus.write(0x0300 + offset, static_cast<uint8_t>(0xFF - offset));
    }

    bus.write(OAM_ADDRESS_REGISTER, 0x00);
    bus.write(OAM_DMA_REGISTER, 0x03);

    expectEqual16(runDma(bus, false),
                  0x0202,
                  "OAM DMA takes 0x0202 CPU cycles when alignment is required");
    expectEqual(readOam(bus, 0x00),
                0xFF,
                "aligned OAM DMA copies the first byte from its selected CPU page");
    expectEqual(readOam(bus, 0xFF),
                0x00,
                "aligned OAM DMA copies the final byte from its selected CPU page");

    expectEqual(bus.read(0x4016), 0x00, "controller 1 read returns 0x00 without connected controller");
    expectEqual(bus.read(0x4017), 0x00, "controller 2 read returns 0x00 without connected controller");

    Controller controller1;
    Controller controller2;
    bus.connectController1(&controller1);
    bus.connectController2(&controller2);

    controller1.setButton(Controller::Button::A, true);
    controller1.setButton(Controller::Button::Start, true);
    controller1.setButton(Controller::Button::Left, true);

    controller2.setButton(Controller::Button::B, true);
    controller2.setButton(Controller::Button::Up, true);
    controller2.setButton(Controller::Button::Right, true);

    bus.write(0x4016, 0x01);
    bus.write(0x4016, 0x00);

    expectEqual(bus.read(0x4016), 0x01, "controller 1 read 1 returns A");
    expectEqual(bus.read(0x4016), 0x00, "controller 1 read 2 returns B");
    expectEqual(bus.read(0x4016), 0x00, "controller 1 read 3 returns Select");
    expectEqual(bus.read(0x4016), 0x01, "controller 1 read 4 returns Start");
    expectEqual(bus.read(0x4016), 0x00, "controller 1 read 5 returns Up");
    expectEqual(bus.read(0x4016), 0x00, "controller 1 read 6 returns Down");
    expectEqual(bus.read(0x4016), 0x01, "controller 1 read 7 returns Left");
    expectEqual(bus.read(0x4016), 0x00, "controller 1 read 8 returns Right");
    expectEqual(bus.read(0x4016), 0x01, "controller 1 reads after 8 buttons return 0x01");

    expectEqual(bus.read(0x4017), 0x00, "controller 2 read 1 returns A");
    expectEqual(bus.read(0x4017), 0x01, "controller 2 read 2 returns B");
    expectEqual(bus.read(0x4017), 0x00, "controller 2 read 3 returns Select");
    expectEqual(bus.read(0x4017), 0x00, "controller 2 read 4 returns Start");
    expectEqual(bus.read(0x4017), 0x01, "controller 2 read 5 returns Up");
    expectEqual(bus.read(0x4017), 0x00, "controller 2 read 6 returns Down");
    expectEqual(bus.read(0x4017), 0x00, "controller 2 read 7 returns Left");
    expectEqual(bus.read(0x4017), 0x01, "controller 2 read 8 returns Right");
    expectEqual(bus.read(0x4017), 0x01, "controller 2 reads after 8 buttons return 0x01");

    expectEqual(bus.read(0x8000), 0x00, "cartridge address 0x8000 reads as 0x00 without cartridge");

    const auto cartridgePath = std::filesystem::temp_directory_path() / "brick_test_bus_cartridge.nes";
    std::vector<uint8_t> prgData(PRG_BANK_SIZE, 0xEA);
    prgData[0x0000] = 0xA9;
    prgData[0x3FFF] = 0x60;

    writeRomData(cartridgePath,
                 makeHeader(1, 1),
                 prgData,
                 std::vector<uint8_t>(CHR_BANK_SIZE, 0x00));

    Cartridge cartridge;
    expectTrue(cartridge.load(cartridgePath.string().c_str()),
               "test cartridge loads successfully");

    bus.insertCartridge(&cartridge);
    expectEqual(bus.read(0x8000), 0xA9, "bus routes 0x8000 read to cartridge PRG ROM");
    expectEqual(bus.read(0xC000), 0xA9, "bus routes 0xC000 read to mirrored 16KB PRG ROM");
    expectEqual(bus.read(0xFFFF), 0x60, "bus routes 0xFFFF read to mirrored PRG ROM end");

    bus.write(0x8000, 0x00);
    expectEqual(bus.read(0x8000), 0xA9, "bus cartridge write does not alter Mapper 0 PRG ROM");

    std::filesystem::remove(cartridgePath);

    std::printf("test_cpu_bus passed\n");

    return 0;
}
