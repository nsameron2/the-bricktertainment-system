#pragma once

#include <cstdint>
#include <array>

// For RAM mirroring
constexpr uint16_t INTERNAL_RAM_SIZE = 0x0800;
constexpr uint16_t INTERNAL_RAM_MASK = 0x07FF;
constexpr uint16_t INTERNAL_RAM_MIRROR_END = 0x1FFF;

class Cartridge;


// Our bus will connect the CPU, RAM, PPU, and loaded ROMs
class Bus {
public:
    // Functions for writing or reading into memory. Delegate memory or cartridge writing/reading through the given address
    void write(uint16_t address, uint8_t data) {
        if(address <= INTERNAL_RAM_MIRROR_END) {
            memory[address & INTERNAL_RAM_MASK] = data;
        } 
        
    }

    uint8_t read(uint16_t address) const {
        if(address <= INTERNAL_RAM_MIRROR_END) {
            return memory[address & INTERNAL_RAM_MASK];
        }

        return 0x00;
    }


    // Cartridges
    void insertCartridge(Cartridge* cartridge);
private:
    // Memory array, for the 2KB of RAM. Initialize it to empty.
    std::array<uint8_t, 2048> memory{};

    // Cartridge pointer
    Cartridge* cartridge = nullptr;
};
