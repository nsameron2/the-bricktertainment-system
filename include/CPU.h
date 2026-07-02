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
    uint8_t P; // Status Flag
    
    uint16_t PC;

    // Status flags, the condition after an operation
    enum StatusFlag : uint8_t {
        C = 1 << 0, // CARRY
        Z = 1 << 1, // ZERO
        I = 1 << 2, // INTERRUPT
        D = 1 << 3, // DECIMAL (unused)
        B = 1 << 4, // BREAK
        U = 1 << 5, // UNUSED
        V = 1 << 6, // OVERFLOW
        N = 1 << 7, // NEGATIVE
    };

    // Register helpers
    void setFlag(StatusFlag flag, bool value);
    bool getFlag(StatusFlag flag) const;

    void reset();
    void powerOn();


    // --- BUS ---
    Bus* bus = nullptr;


    // -- BUS HELPER FUNCTIONS --
    // These will be defined in the .cpp files, since we can't access bus here.
    void write(uint16_t address, uint8_t data);
    uint8_t read(uint16_t address) const;

};