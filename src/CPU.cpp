#include "CPU.h"
#include "Bus.h"


// -- BASIC CPU OPERATIONS -- 
// General
void CPU::write(uint16_t address, uint8_t data) {
    bus->write(address, data);
}

uint8_t CPU::read(uint16_t address) const {
   return bus->read(address);
}


// Status flags
void CPU::setFlag(StatusFlag flag, bool value) {
    // True = P |= flag, false = P &= ~flag -- bitwise logic
    if(value) {
        P |= flag;
    } else {
        P &= ~flag;
    }
}

bool CPU::getFlag(StatusFlag flag) const {
    // Also bitwise logic, "does P have this flag turned on?" If not (0x00), return False
    return (P & flag) != 0x00;
}


// Reset button
void CPU::reset() {
    // PC is reset to the vector at addres FFFC -- but we need the full 16 bit vector, so two 8 bit chunks.
    uint8_t low = read(0xFFFC);
    uint8_t high = read(0xFFFD);

    PC = static_cast<uint16_t>(high) << 8 | low;


    S -= 3;
    setFlag(I, true);
}

// Initial CPU power on
void CPU::powerOn() {
    A = 0x00;
    X = 0x00; 
    Y = 0x00;

    // PC requires the same process for power on as it did for reset
    uint8_t low = read(0xFFFC);
    uint8_t high = read(0xFFFD);

    PC = static_cast<uint16_t>(high) << 8 | low;


    S = 0xFD;

    // Flags
    setFlag(C, false);
    setFlag(Z, false);
    setFlag(I, true);
    setFlag(D, false);
    setFlag(V, false);
    setFlag(N, false);
}