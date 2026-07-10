#pragma once

#include <array>
#include <vector>
#include <cstdint>


class Cartridge {
    public:
        enum class NametableMirroring {
            Horizontal,
            Vertical,
            FourScreen,
        };

        bool load(const char* path);
        bool cpuRead(uint16_t address, uint8_t& data) const;
        bool cpuWrite(uint16_t address, uint8_t data);
        bool ppuRead(uint16_t address, uint8_t& data) const;
        bool ppuWrite(uint16_t address, uint8_t data);
        NametableMirroring getNametableMirroring() const;

    private:
        // Cartridge metadata from the iNES header. Store for later use
        uint8_t prgBanks = 0;
        uint8_t chrBanks = 0;
        uint8_t mapperId = 0;
        NametableMirroring nametableMirroring = NametableMirroring::Horizontal; // Default to horizontal mirroring

        // Loaded cartridge data
        std::vector<uint8_t> prgData{};
        std::vector<uint8_t> chrData{};

        bool verify(const std::array<uint8_t, 16>& header); 
        void reset();   
};
