#pragma once

class Benchmark {
public:
    int runTimed(const char* romPath, float runTime);
    int runInput(const char* romPath, const char* moviePath);
};
