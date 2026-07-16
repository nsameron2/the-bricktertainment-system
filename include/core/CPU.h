#pragma once

#include <cstdint>


// All we need for pointers
class CPUBus;

// 6502
class CPU {
public:
    // We need this public because it will be accessed from main.cpp.
    void connectBus(CPUBus* b) {
        bus = b;
    }

    // Main console functions
    void reset();
    void powerOn();
    void nmi();

    void clock();

private:
    // --- REGISTERS ---
    uint8_t A;
    uint8_t X, Y;
    uint8_t S;
    uint8_t P; // Status Flag
    
    uint16_t PC;

    // Cycle tracker
    uint8_t cycles;

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


    // -- OPCODE STUFF -- 
    void executeOpcode(uint8_t opcode);

    enum class AddressMode : uint8_t {
        Immediate,
        ZeroPage,
        ZeroPageX,
        ZeroPageY,
        Absolute,
        AbsoluteX,
        AbsoluteY,
        IndirectX,
        IndirectY,
    };

    uint8_t fetchByte();
    uint16_t fetchWord();
    uint16_t getOperandAddress(AddressMode mode, bool* pageCrossed = nullptr);
    uint8_t readOperand(AddressMode mode, bool* pageCrossed = nullptr);
    void push(uint8_t data);
    uint8_t pull();
    void pushWord(uint16_t data);
    uint16_t pullWord();
    void updateZeroAndNegativeFlags(uint8_t value);
    void compare(uint8_t reg, uint8_t value);
    void branch(bool condition);

    void adc(uint8_t value);
    void andOp(uint8_t value);
    void aslAccumulator();
    void aslMemory(uint16_t address);
    void bit(uint8_t value);
    void dec(uint16_t address);
    void eor(uint8_t value);
    void inc(uint16_t address);
    void lda(uint8_t value);
    void ldx(uint8_t value);
    void ldy(uint8_t value);
    void lsrAccumulator();
    void lsrMemory(uint16_t address);
    void ora(uint8_t value);
    void rolAccumulator();
    void rolMemory(uint16_t address);
    void rorAccumulator();
    void rorMemory(uint16_t address);
    void sbc(uint8_t value);


    // --- BUS ---
    CPUBus* bus = nullptr;


    // -- BUS HELPER FUNCTIONS --
    // These will be defined in the .cpp files, since we can't access bus here.
    void write(uint16_t address, uint8_t data);
    uint8_t read(uint16_t address) const;

};
