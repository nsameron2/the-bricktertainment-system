#pragma once

#include <cstdint>
#include <array>


// Our bus will connect the CPU, RAM, PPU, and loaded ROMs
class Bus {
public:
    // Function for writing into memory
    void write(uint16_t address, uint8_t data) {
        // Make sure we're given valid parameters and insert our data into our given address
        if(address < memory.size()) {
            memory[address] = data;
        } 
        
    }

    // Function for reading memory
    uint8_t read(uint16_t address) const {
        // Make sure we're given valid parameters and read our data from the given address
        // If the address is invalid, return 0x00
        if(address < memory.size()) {
            return memory[address];
        }

        return 0x00;
    }

private:
    // Memory array, for the 2KB of RAM
    std::array<uint8_t, 2048> memory;
};