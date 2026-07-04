#pragma once

#include <array>
#include <cstdint>


class Cartridge {
    public:
        // Verify, load function
        void load(const char* path);

    private:
        bool verify(const std::array<uint8_t, 16>& header);
};