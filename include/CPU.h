#pragma once

#include <cstdint>


// All we need for pointers
class Bus;

// 6502
class CPU {
public:
    // We need this public because it will be accessed from main.cpp.
    void connectBus(Bus* b) {
        bus = b;
    }



private:
    // --- REGISTERS ---
    uint8_t A;
    uint8_t X, Y;
    uint8_t S;
    uint8_t P;
    
    uint16_t PC;


    // --- BUS ---
    Bus* bus = nullptr;


    // -- BUS HELPER FUNCTIONS --
    // These will be defined in the .cpp files, since we can't access bus here.
    void write(uint16_t address, uint8_t data);
    uint8_t read(uint16_t address) const;

};