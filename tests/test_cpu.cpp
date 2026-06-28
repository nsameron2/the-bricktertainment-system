#include <cstdio>
#include "CPU.h"
#include "Bus.h"

int main() {
    CPU cpu;
    Bus bus;

    cpu.connectBus(&bus);

    std::printf("test_cpu passed\n");

    return 0;
}
