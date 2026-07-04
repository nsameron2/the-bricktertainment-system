#include "Cartridge.h"

#include <iostream>
#include <fstream>
#include <array>
#include <cstdint>


bool Cartridge::load(const char* path) {
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
    mapperId = (header[6] >> 4) | (header[7] & 0xF0);
    if(mapperId != 0) {
        return false;
    }



    // header[6] = Optional trainer
    if (header[6] & 0x04) {
        cart.seekg(512, std::ios::cur);
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
   return true;
}

bool Cartridge::verify(const std::array<uint8_t, 16>& header) {
    return header[0] == 0x4E
        && header[1] == 0x45
        && header[2] == 0x53
        && header[3] == 0x1A;
}
