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

