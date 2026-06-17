#include <cstdio>
#include "CPU.h"
#include "Bus.h"

int main() {
    CPU cpu;
    Bus bus;

    // Test CPU functions
    cpu.connectBus(&bus);

    return 0;
}
