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

uint8_t CPU::fetchByte() {
    uint8_t data = read(PC);
    PC++;
    return data;
}

uint16_t CPU::fetchWord() {
    uint8_t low = fetchByte();
    uint8_t high = fetchByte();
    return static_cast<uint16_t>(high) << 8 | low;
}

uint16_t CPU::getOperandAddress(AddressMode mode, bool* pageCrossed) {
    if(pageCrossed != nullptr) {
        *pageCrossed = false;
    }

    switch(mode) {
        case AddressMode::Immediate:
            return PC++;

        case AddressMode::ZeroPage:
            return fetchByte();

        case AddressMode::ZeroPageX:
            return static_cast<uint8_t>(fetchByte() + X);

        case AddressMode::ZeroPageY:
            return static_cast<uint8_t>(fetchByte() + Y);

        case AddressMode::Absolute:
            return fetchWord();

        case AddressMode::AbsoluteX: {
            uint16_t base = fetchWord();
            uint16_t address = base + X;
            if(pageCrossed != nullptr) {
                *pageCrossed = (base & 0xFF00) != (address & 0xFF00);
            }
            return address;
        }

        case AddressMode::AbsoluteY: {
            uint16_t base = fetchWord();
            uint16_t address = base + Y;
            if(pageCrossed != nullptr) {
                *pageCrossed = (base & 0xFF00) != (address & 0xFF00);
            }
            return address;
        }

        case AddressMode::IndirectX: {
            uint8_t pointer = static_cast<uint8_t>(fetchByte() + X);
            uint8_t low = read(pointer);
            uint8_t high = read(static_cast<uint8_t>(pointer + 1));
            return static_cast<uint16_t>(high) << 8 | low;
        }

        case AddressMode::IndirectY: {
            uint8_t pointer = fetchByte();
            uint8_t low = read(pointer);
            uint8_t high = read(static_cast<uint8_t>(pointer + 1));
            uint16_t base = static_cast<uint16_t>(high) << 8 | low;
            uint16_t address = base + Y;
            if(pageCrossed != nullptr) {
                *pageCrossed = (base & 0xFF00) != (address & 0xFF00);
            }
            return address;
        }
    }

    return 0x0000;
}

uint8_t CPU::readOperand(AddressMode mode, bool* pageCrossed) {
    return read(getOperandAddress(mode, pageCrossed));
}

void CPU::push(uint8_t data) {
    constexpr uint16_t STACK_BASE = 0x0100;
    write(STACK_BASE | S, data);
    S--;
}

uint8_t CPU::pull() {
    constexpr uint16_t STACK_BASE = 0x0100;
    S++;
    return read(STACK_BASE | S);
}

void CPU::pushWord(uint16_t data) {
    push(static_cast<uint8_t>(data >> 8));
    push(static_cast<uint8_t>(data & 0x00FF));
}

uint16_t CPU::pullWord() {
    uint8_t low = pull();
    uint8_t high = pull();
    return static_cast<uint16_t>(high) << 8 | low;
}

void CPU::updateZeroAndNegativeFlags(uint8_t value) {
    setFlag(Z, value == 0x00);
    setFlag(N, (value & 0x80) != 0x00);
}

void CPU::compare(uint8_t reg, uint8_t value) {
    uint8_t result = reg - value;
    setFlag(C, reg >= value);
    updateZeroAndNegativeFlags(result);
}

void CPU::branch(bool condition) {
    int8_t offset = static_cast<int8_t>(fetchByte());
    if(condition) {
        uint16_t oldPC = PC;
        PC = static_cast<uint16_t>(PC + offset);
        cycles++;
        if((oldPC & 0xFF00) != (PC & 0xFF00)) {
            cycles++;
        }
    }
}

void CPU::adc(uint8_t value) {
    uint16_t sum = static_cast<uint16_t>(A) + value + (getFlag(C) ? 0x01 : 0x00);
    uint8_t result = static_cast<uint8_t>(sum);
    setFlag(C, sum > 0x00FF);
    setFlag(V, (~(A ^ value) & (A ^ result) & 0x80) != 0x00);
    A = result;
    updateZeroAndNegativeFlags(A);
}

void CPU::andOp(uint8_t value) {
    A &= value;
    updateZeroAndNegativeFlags(A);
}

void CPU::aslAccumulator() {
    setFlag(C, (A & 0x80) != 0x00);
    A = static_cast<uint8_t>(A << 1);
    updateZeroAndNegativeFlags(A);
}

void CPU::aslMemory(uint16_t address) {
    uint8_t value = read(address);
    setFlag(C, (value & 0x80) != 0x00);
    value = static_cast<uint8_t>(value << 1);
    write(address, value);
    updateZeroAndNegativeFlags(value);
}

void CPU::bit(uint8_t value) {
    setFlag(Z, (A & value) == 0x00);
    setFlag(V, (value & 0x40) != 0x00);
    setFlag(N, (value & 0x80) != 0x00);
}

void CPU::dec(uint16_t address) {
    uint8_t value = static_cast<uint8_t>(read(address) - 1);
    write(address, value);
    updateZeroAndNegativeFlags(value);
}

void CPU::eor(uint8_t value) {
    A ^= value;
    updateZeroAndNegativeFlags(A);
}

void CPU::inc(uint16_t address) {
    uint8_t value = static_cast<uint8_t>(read(address) + 1);
    write(address, value);
    updateZeroAndNegativeFlags(value);
}

void CPU::lda(uint8_t value) {
    A = value;
    updateZeroAndNegativeFlags(A);
}

void CPU::ldx(uint8_t value) {
    X = value;
    updateZeroAndNegativeFlags(X);
}

void CPU::ldy(uint8_t value) {
    Y = value;
    updateZeroAndNegativeFlags(Y);
}

void CPU::lsrAccumulator() {
    setFlag(C, (A & 0x01) != 0x00);
    A >>= 1;
    updateZeroAndNegativeFlags(A);
}

void CPU::lsrMemory(uint16_t address) {
    uint8_t value = read(address);
    setFlag(C, (value & 0x01) != 0x00);
    value >>= 1;
    write(address, value);
    updateZeroAndNegativeFlags(value);
}

void CPU::ora(uint8_t value) {
    A |= value;
    updateZeroAndNegativeFlags(A);
}

void CPU::rolAccumulator() {
    bool carryIn = getFlag(C);
    setFlag(C, (A & 0x80) != 0x00);
    A = static_cast<uint8_t>((A << 1) | (carryIn ? 0x01 : 0x00));
    updateZeroAndNegativeFlags(A);
}

void CPU::rolMemory(uint16_t address) {
    uint8_t value = read(address);
    bool carryIn = getFlag(C);
    setFlag(C, (value & 0x80) != 0x00);
    value = static_cast<uint8_t>((value << 1) | (carryIn ? 0x01 : 0x00));
    write(address, value);
    updateZeroAndNegativeFlags(value);
}

void CPU::rorAccumulator() {
    bool carryIn = getFlag(C);
    setFlag(C, (A & 0x01) != 0x00);
    A = static_cast<uint8_t>((A >> 1) | (carryIn ? 0x80 : 0x00));
    updateZeroAndNegativeFlags(A);
}

void CPU::rorMemory(uint16_t address) {
    uint8_t value = read(address);
    bool carryIn = getFlag(C);
    setFlag(C, (value & 0x01) != 0x00);
    value = static_cast<uint8_t>((value >> 1) | (carryIn ? 0x80 : 0x00));
    write(address, value);
    updateZeroAndNegativeFlags(value);
}

void CPU::sbc(uint8_t value) {
    adc(static_cast<uint8_t>(value ^ 0xFF));
}


// (Physical) reset button
void CPU::reset() {
    // PC is reset to the vector at addres FFFC -- but we need the full 16 bit vector, so two 8 bit chunks.
    uint8_t low = read(0xFFFC);
    uint8_t high = read(0xFFFD);

    PC = static_cast<uint16_t>(high) << 8 | low;


    S -= 3;
    setFlag(I, true);
    setFlag(U, true);
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
    setFlag(U, true);
    setFlag(V, false);
    setFlag(N, false);

    cycles = 0x00;
}

void CPU::clock() {
    if(cycles == 0) {
        // Fetch the opcode we need to do
        uint8_t opcode = read(PC);
        PC++;


        executeOpcode(opcode);
    }


    cycles--;
}

void CPU::executeOpcode(uint8_t opcode) {
    bool pageCrossed = false;

    switch(opcode) {
        // ADC - Add with Carry
        case 0x69: adc(readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0x65: adc(readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0x75: adc(readOperand(AddressMode::ZeroPageX)); cycles = 0x04; break; // Zero Page,X
        case 0x6D: adc(readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute
        case 0x7D: adc(readOperand(AddressMode::AbsoluteX, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,X
        case 0x79: adc(readOperand(AddressMode::AbsoluteY, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,Y
        case 0x61: adc(readOperand(AddressMode::IndirectX)); cycles = 0x06; break; // (Indirect,X)
        case 0x71: adc(readOperand(AddressMode::IndirectY, &pageCrossed)); cycles = 0x05 + pageCrossed; break; // (Indirect),Y

        // AND - Logical AND
        case 0x29: andOp(readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0x25: andOp(readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0x35: andOp(readOperand(AddressMode::ZeroPageX)); cycles = 0x04; break; // Zero Page,X
        case 0x2D: andOp(readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute
        case 0x3D: andOp(readOperand(AddressMode::AbsoluteX, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,X
        case 0x39: andOp(readOperand(AddressMode::AbsoluteY, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,Y
        case 0x21: andOp(readOperand(AddressMode::IndirectX)); cycles = 0x06; break; // (Indirect,X)
        case 0x31: andOp(readOperand(AddressMode::IndirectY, &pageCrossed)); cycles = 0x05 + pageCrossed; break; // (Indirect),Y

        // ASL - Arithmetic Shift Left
        case 0x0A: aslAccumulator(); cycles = 0x02; break; // Accumulator
        case 0x06: aslMemory(getOperandAddress(AddressMode::ZeroPage)); cycles = 0x05; break; // Zero Page
        case 0x16: aslMemory(getOperandAddress(AddressMode::ZeroPageX)); cycles = 0x06; break; // Zero Page,X
        case 0x0E: aslMemory(getOperandAddress(AddressMode::Absolute)); cycles = 0x06; break; // Absolute
        case 0x1E: aslMemory(getOperandAddress(AddressMode::AbsoluteX)); cycles = 0x07; break; // Absolute,X

        // BCC - Branch if Carry Clear
        case 0x90: cycles = 0x02; branch(!getFlag(C)); break; // Relative

        // BCS - Branch if Carry Set
        case 0xB0: cycles = 0x02; branch(getFlag(C)); break; // Relative

        // BEQ - Branch if Equal
        case 0xF0: cycles = 0x02; branch(getFlag(Z)); break; // Relative

        // BIT - Bit Test
        case 0x24: bit(readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0x2C: bit(readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute

        // BMI - Branch if Minus
        case 0x30: cycles = 0x02; branch(getFlag(N)); break; // Relative

        // BNE - Branch if Not Equal
        case 0xD0: cycles = 0x02; branch(!getFlag(Z)); break; // Relative

        // BPL - Branch if Positive
        case 0x10: cycles = 0x02; branch(!getFlag(N)); break; // Relative

        // BRK - Force Interrupt
        case 0x00:
            PC++;
            pushWord(PC);
            push(P | B | U);
            setFlag(I, true);
            PC = static_cast<uint16_t>(read(0xFFFF)) << 8 | read(0xFFFE);
            cycles = 0x07;
            break; // Implied

        // BVC - Branch if Overflow Clear
        case 0x50: cycles = 0x02; branch(!getFlag(V)); break; // Relative

        // BVS - Branch if Overflow Set
        case 0x70: cycles = 0x02; branch(getFlag(V)); break; // Relative

        // CLC - Clear Carry Flag
        case 0x18: setFlag(C, false); cycles = 0x02; break; // Implied

        // CLD - Clear Decimal Mode
        case 0xD8: setFlag(D, false); cycles = 0x02; break; // Implied

        // CLI - Clear Interrupt Disable
        case 0x58: setFlag(I, false); cycles = 0x02; break; // Implied

        // CLV - Clear Overflow Flag
        case 0xB8: setFlag(V, false); cycles = 0x02; break; // Implied

        // CMP - Compare Accumulator
        case 0xC9: compare(A, readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0xC5: compare(A, readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0xD5: compare(A, readOperand(AddressMode::ZeroPageX)); cycles = 0x04; break; // Zero Page,X
        case 0xCD: compare(A, readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute
        case 0xDD: compare(A, readOperand(AddressMode::AbsoluteX, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,X
        case 0xD9: compare(A, readOperand(AddressMode::AbsoluteY, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,Y
        case 0xC1: compare(A, readOperand(AddressMode::IndirectX)); cycles = 0x06; break; // (Indirect,X)
        case 0xD1: compare(A, readOperand(AddressMode::IndirectY, &pageCrossed)); cycles = 0x05 + pageCrossed; break; // (Indirect),Y

        // CPX - Compare X Register
        case 0xE0: compare(X, readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0xE4: compare(X, readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0xEC: compare(X, readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute

        // CPY - Compare Y Register
        case 0xC0: compare(Y, readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0xC4: compare(Y, readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0xCC: compare(Y, readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute

        // DEC - Decrement Memory
        case 0xC6: dec(getOperandAddress(AddressMode::ZeroPage)); cycles = 0x05; break; // Zero Page
        case 0xD6: dec(getOperandAddress(AddressMode::ZeroPageX)); cycles = 0x06; break; // Zero Page,X
        case 0xCE: dec(getOperandAddress(AddressMode::Absolute)); cycles = 0x06; break; // Absolute
        case 0xDE: dec(getOperandAddress(AddressMode::AbsoluteX)); cycles = 0x07; break; // Absolute,X

        // DEX - Decrement X Register
        case 0xCA: X--; updateZeroAndNegativeFlags(X); cycles = 0x02; break; // Implied

        // DEY - Decrement Y Register
        case 0x88: Y--; updateZeroAndNegativeFlags(Y); cycles = 0x02; break; // Implied

        // EOR - Exclusive OR
        case 0x49: eor(readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0x45: eor(readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0x55: eor(readOperand(AddressMode::ZeroPageX)); cycles = 0x04; break; // Zero Page,X
        case 0x4D: eor(readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute
        case 0x5D: eor(readOperand(AddressMode::AbsoluteX, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,X
        case 0x59: eor(readOperand(AddressMode::AbsoluteY, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,Y
        case 0x41: eor(readOperand(AddressMode::IndirectX)); cycles = 0x06; break; // (Indirect,X)
        case 0x51: eor(readOperand(AddressMode::IndirectY, &pageCrossed)); cycles = 0x05 + pageCrossed; break; // (Indirect),Y

        // INC - Increment Memory
        case 0xE6: inc(getOperandAddress(AddressMode::ZeroPage)); cycles = 0x05; break; // Zero Page
        case 0xF6: inc(getOperandAddress(AddressMode::ZeroPageX)); cycles = 0x06; break; // Zero Page,X
        case 0xEE: inc(getOperandAddress(AddressMode::Absolute)); cycles = 0x06; break; // Absolute
        case 0xFE: inc(getOperandAddress(AddressMode::AbsoluteX)); cycles = 0x07; break; // Absolute,X

        // INX - Increment X Register
        case 0xE8: X++; updateZeroAndNegativeFlags(X); cycles = 0x02; break; // Implied

        // INY - Increment Y Register
        case 0xC8: Y++; updateZeroAndNegativeFlags(Y); cycles = 0x02; break; // Implied

        // JMP - Jump
        case 0x4C: PC = fetchWord(); cycles = 0x03; break; // Absolute
        case 0x6C: {
            uint16_t pointer = fetchWord();
            uint8_t low = read(pointer);
            uint8_t high = read((pointer & 0xFF00) | static_cast<uint8_t>(pointer + 1));
            PC = static_cast<uint16_t>(high) << 8 | low;
            cycles = 0x05;
            break;
        } // Indirect

        // JSR - Jump to Subroutine
        case 0x20: {
            uint16_t address = fetchWord();
            pushWord(PC - 1);
            PC = address;
            cycles = 0x06;
            break;
        } // Absolute

        // LDA - Load Accumulator
        case 0xA9: lda(readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0xA5: lda(readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0xB5: lda(readOperand(AddressMode::ZeroPageX)); cycles = 0x04; break; // Zero Page,X
        case 0xAD: lda(readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute
        case 0xBD: lda(readOperand(AddressMode::AbsoluteX, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,X
        case 0xB9: lda(readOperand(AddressMode::AbsoluteY, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,Y
        case 0xA1: lda(readOperand(AddressMode::IndirectX)); cycles = 0x06; break; // (Indirect,X)
        case 0xB1: lda(readOperand(AddressMode::IndirectY, &pageCrossed)); cycles = 0x05 + pageCrossed; break; // (Indirect),Y

        // LDX - Load X Register
        case 0xA2: ldx(readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0xA6: ldx(readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0xB6: ldx(readOperand(AddressMode::ZeroPageY)); cycles = 0x04; break; // Zero Page,Y
        case 0xAE: ldx(readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute
        case 0xBE: ldx(readOperand(AddressMode::AbsoluteY, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,Y

        // LDY - Load Y Register
        case 0xA0: ldy(readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0xA4: ldy(readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0xB4: ldy(readOperand(AddressMode::ZeroPageX)); cycles = 0x04; break; // Zero Page,X
        case 0xAC: ldy(readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute
        case 0xBC: ldy(readOperand(AddressMode::AbsoluteX, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,X

        // LSR - Logical Shift Right
        case 0x4A: lsrAccumulator(); cycles = 0x02; break; // Accumulator
        case 0x46: lsrMemory(getOperandAddress(AddressMode::ZeroPage)); cycles = 0x05; break; // Zero Page
        case 0x56: lsrMemory(getOperandAddress(AddressMode::ZeroPageX)); cycles = 0x06; break; // Zero Page,X
        case 0x4E: lsrMemory(getOperandAddress(AddressMode::Absolute)); cycles = 0x06; break; // Absolute
        case 0x5E: lsrMemory(getOperandAddress(AddressMode::AbsoluteX)); cycles = 0x07; break; // Absolute,X

        // NOP - No Operation
        case 0xEA: cycles = 0x02; break; // Implied

        // ORA - Logical Inclusive OR
        case 0x09: ora(readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0x05: ora(readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0x15: ora(readOperand(AddressMode::ZeroPageX)); cycles = 0x04; break; // Zero Page,X
        case 0x0D: ora(readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute
        case 0x1D: ora(readOperand(AddressMode::AbsoluteX, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,X
        case 0x19: ora(readOperand(AddressMode::AbsoluteY, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,Y
        case 0x01: ora(readOperand(AddressMode::IndirectX)); cycles = 0x06; break; // (Indirect,X)
        case 0x11: ora(readOperand(AddressMode::IndirectY, &pageCrossed)); cycles = 0x05 + pageCrossed; break; // (Indirect),Y

        // PHA - Push Accumulator
        case 0x48: push(A); cycles = 0x03; break; // Implied

        // PHP - Push Processor Status
        case 0x08: push(P | B | U); cycles = 0x03; break; // Implied

        // PLA - Pull Accumulator
        case 0x68: A = pull(); updateZeroAndNegativeFlags(A); cycles = 0x04; break; // Implied

        // PLP - Pull Processor Status
        case 0x28: P = (pull() & ~B) | U; cycles = 0x04; break; // Implied

        // ROL - Rotate Left
        case 0x2A: rolAccumulator(); cycles = 0x02; break; // Accumulator
        case 0x26: rolMemory(getOperandAddress(AddressMode::ZeroPage)); cycles = 0x05; break; // Zero Page
        case 0x36: rolMemory(getOperandAddress(AddressMode::ZeroPageX)); cycles = 0x06; break; // Zero Page,X
        case 0x2E: rolMemory(getOperandAddress(AddressMode::Absolute)); cycles = 0x06; break; // Absolute
        case 0x3E: rolMemory(getOperandAddress(AddressMode::AbsoluteX)); cycles = 0x07; break; // Absolute,X

        // ROR - Rotate Right
        case 0x6A: rorAccumulator(); cycles = 0x02; break; // Accumulator
        case 0x66: rorMemory(getOperandAddress(AddressMode::ZeroPage)); cycles = 0x05; break; // Zero Page
        case 0x76: rorMemory(getOperandAddress(AddressMode::ZeroPageX)); cycles = 0x06; break; // Zero Page,X
        case 0x6E: rorMemory(getOperandAddress(AddressMode::Absolute)); cycles = 0x06; break; // Absolute
        case 0x7E: rorMemory(getOperandAddress(AddressMode::AbsoluteX)); cycles = 0x07; break; // Absolute,X

        // RTI - Return from Interrupt
        case 0x40: P = (pull() & ~B) | U; PC = pullWord(); cycles = 0x06; break; // Implied

        // RTS - Return from Subroutine
        case 0x60: PC = pullWord() + 1; cycles = 0x06; break; // Implied

        // SBC - Subtract with Carry
        case 0xE9: sbc(readOperand(AddressMode::Immediate)); cycles = 0x02; break; // Immediate
        case 0xE5: sbc(readOperand(AddressMode::ZeroPage)); cycles = 0x03; break; // Zero Page
        case 0xF5: sbc(readOperand(AddressMode::ZeroPageX)); cycles = 0x04; break; // Zero Page,X
        case 0xED: sbc(readOperand(AddressMode::Absolute)); cycles = 0x04; break; // Absolute
        case 0xFD: sbc(readOperand(AddressMode::AbsoluteX, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,X
        case 0xF9: sbc(readOperand(AddressMode::AbsoluteY, &pageCrossed)); cycles = 0x04 + pageCrossed; break; // Absolute,Y
        case 0xE1: sbc(readOperand(AddressMode::IndirectX)); cycles = 0x06; break; // (Indirect,X)
        case 0xF1: sbc(readOperand(AddressMode::IndirectY, &pageCrossed)); cycles = 0x05 + pageCrossed; break; // (Indirect),Y

        // SEC - Set Carry Flag
        case 0x38: setFlag(C, true); cycles = 0x02; break; // Implied

        // SED - Set Decimal Flag
        case 0xF8: setFlag(D, true); cycles = 0x02; break; // Implied

        // SEI - Set Interrupt Disable
        case 0x78: setFlag(I, true); cycles = 0x02; break; // Implied

        // STA - Store Accumulator
        case 0x85: write(getOperandAddress(AddressMode::ZeroPage), A); cycles = 0x03; break; // Zero Page
        case 0x95: write(getOperandAddress(AddressMode::ZeroPageX), A); cycles = 0x04; break; // Zero Page,X
        case 0x8D: write(getOperandAddress(AddressMode::Absolute), A); cycles = 0x04; break; // Absolute
        case 0x9D: write(getOperandAddress(AddressMode::AbsoluteX), A); cycles = 0x05; break; // Absolute,X
        case 0x99: write(getOperandAddress(AddressMode::AbsoluteY), A); cycles = 0x05; break; // Absolute,Y
        case 0x81: write(getOperandAddress(AddressMode::IndirectX), A); cycles = 0x06; break; // (Indirect,X)
        case 0x91: write(getOperandAddress(AddressMode::IndirectY), A); cycles = 0x06; break; // (Indirect),Y

        // STX - Store X Register
        case 0x86: write(getOperandAddress(AddressMode::ZeroPage), X); cycles = 0x03; break; // Zero Page
        case 0x96: write(getOperandAddress(AddressMode::ZeroPageY), X); cycles = 0x04; break; // Zero Page,Y
        case 0x8E: write(getOperandAddress(AddressMode::Absolute), X); cycles = 0x04; break; // Absolute

        // STY - Store Y Register
        case 0x84: write(getOperandAddress(AddressMode::ZeroPage), Y); cycles = 0x03; break; // Zero Page
        case 0x94: write(getOperandAddress(AddressMode::ZeroPageX), Y); cycles = 0x04; break; // Zero Page,X
        case 0x8C: write(getOperandAddress(AddressMode::Absolute), Y); cycles = 0x04; break; // Absolute

        // TAX - Transfer Accumulator to X
        case 0xAA: X = A; updateZeroAndNegativeFlags(X); cycles = 0x02; break; // Implied

        // TAY - Transfer Accumulator to Y
        case 0xA8: Y = A; updateZeroAndNegativeFlags(Y); cycles = 0x02; break; // Implied

        // TSX - Transfer Stack Pointer to X
        case 0xBA: X = S; updateZeroAndNegativeFlags(X); cycles = 0x02; break; // Implied

        // TXA - Transfer X to Accumulator
        case 0x8A: A = X; updateZeroAndNegativeFlags(A); cycles = 0x02; break; // Implied

        // TXS - Transfer X to Stack Pointer
        case 0x9A: S = X; cycles = 0x02; break; // Implied

        // TYA - Transfer Y to Accumulator
        case 0x98: A = Y; updateZeroAndNegativeFlags(A); cycles = 0x02; break; // Implied

        default:
            cycles = 0x02;
            break;
    }
}
