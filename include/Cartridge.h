#pragma once

#include <array>
#include <vector>
#include <cstdint>


class Cartridge {
    public:
        bool load(const char* path);

    private:
        // Loaded cartridge data
        std::vector<uint8_t> prgData{};
        std::vector<uint8_t> chrData{};

        bool verify(const std::array<uint8_t, 16>& header);      
};