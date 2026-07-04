#include "Cartridge.h"

#include <iostream>
#include <fstream>
#include <array>
#include <cstdint>


void Cartridge::load(const char* path) {
    std::ifstream cart(path, std::ios::binary);

    // Read and verify the 16 bit magic header of the .NES file
    std::array<uint8_t, 16> header{};
    cart.read(reinterpret_cast<char*>(header.data()), header.size());

    if(!cart) { 
        return;
    }

    if(!verify(header)) {
        // TODO: Throw an exception
        return;
    }


    // TODO: Begin ROM loading
}

bool Cartridge::verify(const std::array<uint8_t, 16>& header) {
    return header[0] == 0x4E
        && header[1] == 0x45
        && header[2] == 0x53
        && header[3] == 0x1A;
}