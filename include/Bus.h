#include <cstdint>
#include <array>


// Our bus will connect the CPU, RAM, PPU, and loaded ROMs
class Bus {
    // Memory array, for the 2KB of RAM
    std::array<uint8_t, 2048> memory;
    
    // Function for writing into memory
    void write(uint16_t address, uint8_t data) {
        // Slot our data into our given address
        memory[address] = data;
    }

    // Function for reading memory
    uint8_t read(uint16_t address) const {
        return memory[address];
    }
};