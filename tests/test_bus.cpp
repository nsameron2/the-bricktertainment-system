#include <cstdio>
#include "Bus.h"

int main() {
    Bus bus;

    // Test bus functions
    bus.write(0x0000, 0x42);
    printf("read back: 0x%02X\n", bus.read(0x0000));

    return 0;
}
