#include "Cartridge.h"

#include <fstream>
#include <array>
#include <cstdint>

namespace {

constexpr uint16_t CPU_PRG_ROM_START = 0x8000;
constexpr uint16_t CPU_PRG_ROM_END = 0xFFFF;
constexpr uint16_t PRG_ROM_16KB_MASK = 0x3FFF;
constexpr uint16_t PRG_ROM_32KB_MASK = 0x7FFF;
constexpr uint16_t PPU_CHR_END = 0x1FFF;
constexpr uint16_t CHR_8KB_MASK = 0x1FFF;

constexpr size_t INES_FLAGS_6_INDEX = 6;
constexpr size_t INES_FLAGS_7_INDEX = 7;
constexpr uint8_t INES_VERTICAL_MIRRORING = 1 << 0;
constexpr uint8_t INES_TRAINER_PRESENT = 1 << 2;
constexpr uint8_t INES_FOUR_SCREEN_VRAM = 1 << 3;
constexpr uint16_t TRAINER_SIZE = 0x0200;

}

bool Cartridge::load(const char* path) {
    // Clear possible old ROM data
    reset();

    
    std::ifstream cart(path, std::ios::binary);
    if (!cart) {
        return false;
    }


    // Read and verify the 16 byte magic header of the .NES file
    std::array<uint8_t, 16> header{};
    cart.read(reinterpret_cast<char*>(header.data()), header.size());

    if(!cart) { 
        return false;
    }

    if(!verify(header)) {
        return false;
    }


    // Begin ROM loading
    // header[4] = Number of PRG ROM banks
    // header[5] = number of CHR ROM banks
    prgBanks = header[4];
    chrBanks = header[5];

    constexpr size_t PRG_BANK_SIZE = 16 * 1024;
    constexpr size_t CHR_BANK_SIZE = 8 * 1024;

    const size_t prgSize = static_cast<size_t>(prgBanks) * PRG_BANK_SIZE;
    const bool hasChrRom = chrBanks != 0;
    const size_t chrSize = hasChrRom
        ? static_cast<size_t>(chrBanks) * CHR_BANK_SIZE
        : CHR_BANK_SIZE;


    // Get cartridge mapper id and handle accordingly, we only support mapper 0 for now
    const uint8_t flags6 = header[INES_FLAGS_6_INDEX];
    mapperId = (flags6 >> 4) | (header[INES_FLAGS_7_INDEX] & 0xF0);
    if(mapperId != 0) {
        return false;
    }

    if(prgBanks == 0 || prgBanks > 2) {
        return false;
    }

    // header[6] = Optional trainer, vertical mirroring, and four-screen VRAM
    NametableMirroring loadedNametableMirroring = NametableMirroring::Horizontal;
    if ((flags6 & INES_FOUR_SCREEN_VRAM) != 0x00) {
        loadedNametableMirroring = NametableMirroring::FourScreen;
    } else if ((flags6 & INES_VERTICAL_MIRRORING) != 0x00) {
        loadedNametableMirroring = NametableMirroring::Vertical;
    }

    if ((flags6 & INES_TRAINER_PRESENT) != 0x00) {
        cart.seekg(TRAINER_SIZE, std::ios::cur);
    }


    // Size our data vectors appropriately
    prgData.resize(prgSize);
    chrData.resize(chrSize);

   // Load cartridge data
   cart.read(reinterpret_cast<char*>(prgData.data()), prgData.size()); 
   if(!cart) {
        return false;
   }

   if (hasChrRom) {
       cart.read(reinterpret_cast<char*>(chrData.data()), chrData.size()); 
       if(!cart) {
            return false;
       }
   }


   // We're done here now.
   nametableMirroring = loadedNametableMirroring;
   return true;
}

// Helper functions for cartridge loading
bool Cartridge::verify(const std::array<uint8_t, 16>& header) {
    return header[0] == 0x4E
        && header[1] == 0x45
        && header[2] == 0x53
        && header[3] == 0x1A;
}

void Cartridge::reset() {
    prgBanks = 0;
    chrBanks = 0;
    mapperId = 0;
    nametableMirroring = NametableMirroring::Horizontal;
    prgData.clear();
    chrData.clear();
}

Cartridge::NametableMirroring Cartridge::getNametableMirroring() const {
    return nametableMirroring;
}


bool Cartridge::cpuRead(uint16_t address, uint8_t& data) const {
    if(address < CPU_PRG_ROM_START || address > CPU_PRG_ROM_END) {
        return false;
    }

    if(mapperId != 0 || prgData.empty()) {
        return false;
    }

    uint16_t mappedAddress = address - CPU_PRG_ROM_START;

    if(prgBanks == 1) {
        mappedAddress &= PRG_ROM_16KB_MASK;
    } else if(prgBanks == 2) {
        mappedAddress &= PRG_ROM_32KB_MASK;
    } else {
        return false;
    }

    data = prgData[mappedAddress];
    return true;
}

bool Cartridge::cpuWrite(uint16_t address, uint8_t data) {
    (void)data;

    if(address < CPU_PRG_ROM_START || address > CPU_PRG_ROM_END) {
        return false;
    }

    if(mapperId != 0 || prgData.empty()) {
        return false;
    }

    // Mapper 0 PRG ROM is read-only. The address belongs to the cartridge, but the write does not modify cartridge data.
    return prgBanks == 1 || prgBanks == 2;
}

bool Cartridge::ppuRead(uint16_t address, uint8_t& data) const {
    if(address > PPU_CHR_END) {
        return false;
    }

    if(mapperId != 0 || chrData.empty()) {
        return false;
    }

    data = chrData[address & CHR_8KB_MASK];
    return true;
}

bool Cartridge::ppuWrite(uint16_t address, uint8_t data) {
    if(address > PPU_CHR_END) {
        return false;
    }

    if(mapperId != 0 || chrData.empty()) {
        return false;
    }

    if(chrBanks == 0) {
        chrData[address & CHR_8KB_MASK] = data;
    }

    // CHR ROM and CHR RAM both occupy this PPU range; only CHR RAM is writable.
    return true;
}
