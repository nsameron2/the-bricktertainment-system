#include <cstdio>
#include <cstdlib>
#include "Bus.h"

namespace {

void expectEqual(uint8_t actual, uint8_t expected, const char* message) {
    if(actual != expected) {
        std::fprintf(stderr,
                     "FAIL: %s (expected 0x%02X, got 0x%02X)\n",
                     message,
                     expected,
                     actual);
        std::exit(EXIT_FAILURE);
    }
}

}

int main() {
    Bus bus;

    // MIRRORING TEST
    expectEqual(bus.read(0x0000), 0x00, "RAM initializes to 0x00 at 0x0000");
    expectEqual(bus.read(0x07FF), 0x00, "RAM initializes to 0x00 at 0x07FF");

    bus.write(0x0000, 0x42);
    expectEqual(bus.read(0x0000), 0x42, "write/read at 0x0000");

    bus.write(0x07FF, 0x99);
    expectEqual(bus.read(0x07FF), 0x99, "write/read at 0x07FF");

    bus.write(0x0800, 0x11);
    expectEqual(bus.read(0x0000), 0x11, "0x0800 mirrors 0x0000");
    expectEqual(bus.read(0x1000), 0x11, "0x1000 mirrors 0x0000");
    expectEqual(bus.read(0x1800), 0x11, "0x1800 mirrors 0x0000");

    bus.write(0x1FFF, 0x7E);
    expectEqual(bus.read(0x07FF), 0x7E, "0x1FFF mirrors 0x07FF");

    bus.write(0x2000, 0x55);
    expectEqual(bus.read(0x2000), 0x00, "unmapped address 0x2000 reads as 0x00");
    expectEqual(bus.read(0x0000), 0x11, "unmapped write at 0x2000 does not alter RAM");

    std::printf("test_bus passed\n");

    return 0;
}
