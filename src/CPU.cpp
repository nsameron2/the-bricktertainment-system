#include "CPU.h"
#include "Bus.h"


void CPU::write(uint16_t address, uint8_t data) {
    bus->write(address, data);
}

uint8_t CPU::read(uint16_t address) const {
   return bus->read(address);
}