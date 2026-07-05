#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#define private public
#include "CPU.h"
#undef private

#include "CPUBus.h"

namespace {

constexpr uint8_t FLAG_C = 1 << 0;
constexpr uint8_t FLAG_Z = 1 << 1;
constexpr uint8_t FLAG_I = 1 << 2;
constexpr uint8_t FLAG_D = 1 << 3;
constexpr uint8_t FLAG_B = 1 << 4;
constexpr uint8_t FLAG_U = 1 << 5;
constexpr uint8_t FLAG_V = 1 << 6;
constexpr uint8_t FLAG_N = 1 << 7;

void expectEqual8(uint8_t actual, uint8_t expected, const char* message) {
    if(actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%02X, got 0x%02X)\n",
                     message,
                     expected,
                     actual);
        std::exit(EXIT_FAILURE);
    }
}

void expectEqual16(uint16_t actual, uint16_t expected, const char* message) {
    if(actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%04X, got 0x%04X)\n",
                     message,
                     expected,
                     actual);
        std::exit(EXIT_FAILURE);
    }
}

void expectFlag(const CPU& cpu, uint8_t flag, bool expected, const char* message) {
    bool actual = (cpu.P & flag) != 0x00;
    if(actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected %s, got %s; P=0x%02X)\n",
                     message,
                     expected ? "set" : "clear",
                     actual ? "set" : "clear",
                     cpu.P);
        std::exit(EXIT_FAILURE);
    }
}

void loadProgram(CPUBus& bus, std::initializer_list<uint8_t> bytes, uint16_t start = 0x0000) {
    uint16_t address = start;
    for(uint8_t byte : bytes) {
        bus.write(address, byte);
        address++;
    }
}

uint8_t runInstruction(CPU& cpu) {
    uint8_t elapsedCycles = 0x00;
    do {
        cpu.clock();
        elapsedCycles++;
    } while(cpu.cycles != 0x00);

    return elapsedCycles;
}

void connectAndPowerOn(CPU& cpu, CPUBus& bus) {
    cpu.connectBus(&bus);
    cpu.powerOn();
}

void testPowerOnInitializesStatus() {
    CPU cpu;
    CPUBus bus;
    connectAndPowerOn(cpu, bus);

    expectEqual8(cpu.A, 0x00, "powerOn clears A");
    expectEqual8(cpu.X, 0x00, "powerOn clears X");
    expectEqual8(cpu.Y, 0x00, "powerOn clears Y");
    expectEqual8(cpu.S, 0xFD, "powerOn initializes stack pointer to 0xFD");
    expectFlag(cpu, FLAG_I, true, "powerOn sets interrupt disable flag");
    expectFlag(cpu, FLAG_U, true, "powerOn sets unused status flag");
    expectFlag(cpu, FLAG_C, false, "powerOn clears carry flag");
    expectFlag(cpu, FLAG_Z, false, "powerOn clears zero flag");
    expectFlag(cpu, FLAG_D, false, "powerOn clears decimal flag");
    expectFlag(cpu, FLAG_B, false, "powerOn clears break flag");
    expectFlag(cpu, FLAG_V, false, "powerOn clears overflow flag");
    expectFlag(cpu, FLAG_N, false, "powerOn clears negative flag");
}

void testLoadAddStoreAndFlags() {
    CPU cpu;
    CPUBus bus;
    connectAndPowerOn(cpu, bus);
    loadProgram(bus, {
        0xA9, 0x7F,       // LDA #0x7F
        0x69, 0x01,       // ADC #0x01
        0x85, 0x80,       // STA 0x0080
    });

    expectEqual8(runInstruction(cpu), 0x02, "LDA immediate consumes 0x02 cycles");
    expectEqual8(cpu.A, 0x7F, "LDA immediate loads A");
    expectFlag(cpu, FLAG_Z, false, "LDA immediate clears Z for non-zero value");
    expectFlag(cpu, FLAG_N, false, "LDA immediate clears N for bit 7 clear");

    expectEqual8(runInstruction(cpu), 0x02, "ADC immediate consumes 0x02 cycles");
    expectEqual8(cpu.A, 0x80, "ADC immediate stores result in A");
    expectFlag(cpu, FLAG_C, false, "ADC immediate leaves C clear without unsigned overflow");
    expectFlag(cpu, FLAG_V, true, "ADC immediate sets V on signed overflow");
    expectFlag(cpu, FLAG_N, true, "ADC immediate sets N when bit 7 is set");

    expectEqual8(runInstruction(cpu), 0x03, "STA zero page consumes 0x03 cycles");
    expectEqual8(bus.read(0x0080), 0x80, "STA zero page writes A to memory");
}

void testIndexedAndIndirectAddressing() {
    CPU cpu;
    CPUBus bus;
    connectAndPowerOn(cpu, bus);
    bus.write(0x007F, 0xAA);
    bus.write(0x0020, 0xFF);
    bus.write(0x0021, 0x00);
    bus.write(0x0100, 0x44);

    loadProgram(bus, {
        0xA2, 0xFF,       // LDX #0xFF
        0xB5, 0x80,       // LDA 0x80,X -> wraps to 0x007F
        0xA0, 0x01,       // LDY #0x01
        0xB1, 0x20,       // LDA (0x20),Y -> 0x00FF + 0x01 = 0x0100
    });

    expectEqual8(runInstruction(cpu), 0x02, "LDX immediate consumes 0x02 cycles");
    expectEqual8(cpu.X, 0xFF, "LDX immediate loads X");
    expectFlag(cpu, FLAG_N, true, "LDX immediate sets N for 0xFF");

    expectEqual8(runInstruction(cpu), 0x04, "LDA zero page,X consumes 0x04 cycles");
    expectEqual8(cpu.A, 0xAA, "LDA zero page,X wraps within zero page");
    expectFlag(cpu, FLAG_N, true, "LDA zero page,X sets N for 0xAA");

    expectEqual8(runInstruction(cpu), 0x02, "LDY immediate consumes 0x02 cycles");
    expectEqual8(cpu.Y, 0x01, "LDY immediate loads Y");

    expectEqual8(runInstruction(cpu), 0x06, "LDA (indirect),Y adds page-cross cycle");
    expectEqual8(cpu.A, 0x44, "LDA (indirect),Y reads from computed address");
}

void testBranchCycleCounts() {
    CPU cpu;
    CPUBus bus;
    connectAndPowerOn(cpu, bus);
    loadProgram(bus, {
        0xA9, 0x00,       // LDA #0x00
        0xF0, 0x02,       // BEQ +0x02
        0xA9, 0xFF,       // LDA #0xFF, skipped
        0xA9, 0x42,       // LDA #0x42
    });

    expectEqual8(runInstruction(cpu), 0x02, "LDA immediate consumes 0x02 cycles");
    expectFlag(cpu, FLAG_Z, true, "LDA immediate sets Z for 0x00");

    expectEqual8(runInstruction(cpu), 0x03, "BEQ taken on same page consumes 0x03 cycles");
    expectEqual16(cpu.PC, 0x0006, "BEQ taken updates PC by signed offset");

    expectEqual8(runInstruction(cpu), 0x02, "LDA after branch consumes 0x02 cycles");
    expectEqual8(cpu.A, 0x42, "BEQ skips over untaken instruction bytes");

    CPU pageCrossCpu;
    CPUBus pageCrossBus;
    connectAndPowerOn(pageCrossCpu, pageCrossBus);
    pageCrossCpu.PC = 0x00FC;
    pageCrossCpu.P |= FLAG_Z;
    loadProgram(pageCrossBus, {
        0xF0, 0x02,       // BEQ +0x02 from 0x00FE to 0x0100
    }, 0x00FC);

    expectEqual8(runInstruction(pageCrossCpu), 0x04, "BEQ taken across page consumes 0x04 cycles");
    expectEqual16(pageCrossCpu.PC, 0x0100, "BEQ taken across page lands at 0x0100");
}

void testStackAndSubroutineOpcodes() {
    CPU cpu;
    CPUBus bus;
    connectAndPowerOn(cpu, bus);
    loadProgram(bus, {
        0x20, 0x06, 0x00, // JSR 0x0006
        0xA9, 0x42,       // LDA #0x42 after RTS
        0xEA,             // NOP padding
        0xA9, 0x99,       // LDA #0x99 in subroutine
        0x60,             // RTS
    });

    expectEqual8(runInstruction(cpu), 0x06, "JSR absolute consumes 0x06 cycles");
    expectEqual16(cpu.PC, 0x0006, "JSR jumps to subroutine address");
    expectEqual8(cpu.S, 0xFB, "JSR pushes return address on stack");
    expectEqual8(bus.read(0x01FC), 0x02, "JSR pushes return low byte");
    expectEqual8(bus.read(0x01FD), 0x00, "JSR pushes return high byte");

    expectEqual8(runInstruction(cpu), 0x02, "subroutine LDA consumes 0x02 cycles");
    expectEqual8(cpu.A, 0x99, "subroutine LDA executes before RTS");

    expectEqual8(runInstruction(cpu), 0x06, "RTS consumes 0x06 cycles");
    expectEqual16(cpu.PC, 0x0003, "RTS returns to instruction after JSR");
    expectEqual8(cpu.S, 0xFD, "RTS restores stack pointer");

    expectEqual8(runInstruction(cpu), 0x02, "post-RTS LDA consumes 0x02 cycles");
    expectEqual8(cpu.A, 0x42, "post-RTS LDA executes after return");
}

void testShiftRotateAndMemoryOpcodes() {
    CPU cpu;
    CPUBus bus;
    connectAndPowerOn(cpu, bus);
    loadProgram(bus, {
        0xA9, 0x81,       // LDA #0x81
        0x0A,             // ASL A
        0x6A,             // ROR A
        0x85, 0x90,       // STA 0x0090
        0xE6, 0x90,       // INC 0x0090
        0xC6, 0x90,       // DEC 0x0090
        0x46, 0x90,       // LSR 0x0090
    });

    runInstruction(cpu);

    expectEqual8(runInstruction(cpu), 0x02, "ASL accumulator consumes 0x02 cycles");
    expectEqual8(cpu.A, 0x02, "ASL accumulator shifts A left");
    expectFlag(cpu, FLAG_C, true, "ASL accumulator stores bit 7 in C");
    expectFlag(cpu, FLAG_N, false, "ASL accumulator clears N for 0x02");

    expectEqual8(runInstruction(cpu), 0x02, "ROR accumulator consumes 0x02 cycles");
    expectEqual8(cpu.A, 0x81, "ROR accumulator rotates carry into bit 7");
    expectFlag(cpu, FLAG_C, false, "ROR accumulator stores bit 0 in C");
    expectFlag(cpu, FLAG_N, true, "ROR accumulator sets N for 0x81");

    runInstruction(cpu);
    expectEqual8(bus.read(0x0090), 0x81, "STA stores rotated A before memory ops");

    expectEqual8(runInstruction(cpu), 0x05, "INC zero page consumes 0x05 cycles");
    expectEqual8(bus.read(0x0090), 0x82, "INC zero page increments memory");
    expectFlag(cpu, FLAG_N, true, "INC zero page updates N from memory result");

    expectEqual8(runInstruction(cpu), 0x05, "DEC zero page consumes 0x05 cycles");
    expectEqual8(bus.read(0x0090), 0x81, "DEC zero page decrements memory");

    expectEqual8(runInstruction(cpu), 0x05, "LSR zero page consumes 0x05 cycles");
    expectEqual8(bus.read(0x0090), 0x40, "LSR zero page shifts memory right");
    expectFlag(cpu, FLAG_C, true, "LSR zero page stores bit 0 in C");
    expectFlag(cpu, FLAG_N, false, "LSR zero page clears N");
}

void testCompareBitAndSbcOpcodes() {
    CPU cpu;
    CPUBus bus;
    connectAndPowerOn(cpu, bus);
    bus.write(0x00A0, 0xC0);
    loadProgram(bus, {
        0xA9, 0x40,       // LDA #0x40
        0x24, 0xA0,       // BIT 0x00A0
        0xC9, 0x40,       // CMP #0x40
        0x38,             // SEC
        0xE9, 0x01,       // SBC #0x01
    });

    runInstruction(cpu);

    expectEqual8(runInstruction(cpu), 0x03, "BIT zero page consumes 0x03 cycles");
    expectFlag(cpu, FLAG_Z, false, "BIT clears Z when A and memory overlap");
    expectFlag(cpu, FLAG_V, true, "BIT copies bit 6 into V");
    expectFlag(cpu, FLAG_N, true, "BIT copies bit 7 into N");

    expectEqual8(runInstruction(cpu), 0x02, "CMP immediate consumes 0x02 cycles");
    expectFlag(cpu, FLAG_C, true, "CMP sets C when A >= operand");
    expectFlag(cpu, FLAG_Z, true, "CMP sets Z when A == operand");
    expectFlag(cpu, FLAG_N, false, "CMP clears N for zero result");

    expectEqual8(runInstruction(cpu), 0x02, "SEC consumes 0x02 cycles");
    expectFlag(cpu, FLAG_C, true, "SEC sets C before SBC");

    expectEqual8(runInstruction(cpu), 0x02, "SBC immediate consumes 0x02 cycles");
    expectEqual8(cpu.A, 0x3F, "SBC immediate subtracts operand when C is set");
    expectFlag(cpu, FLAG_C, true, "SBC keeps C set when no borrow occurs");
    expectFlag(cpu, FLAG_Z, false, "SBC clears Z for non-zero result");
    expectFlag(cpu, FLAG_N, false, "SBC clears N for 0x3F");
}

}

int main() {
    testPowerOnInitializesStatus();
    testLoadAddStoreAndFlags();
    testIndexedAndIndirectAddressing();
    testBranchCycleCounts();
    testStackAndSubroutineOpcodes();
    testShiftRotateAndMemoryOpcodes();
    testCompareBitAndSbcOpcodes();

    std::printf("test_cpu passed\n");

    return 0;
}
