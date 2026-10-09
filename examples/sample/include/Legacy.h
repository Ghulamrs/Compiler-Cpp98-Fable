// Legacy.h - NOT listed in Sample.vcxproj. It is reached only through
// "#include" from main.cpp, so the report lists it as a discovered file.
#ifndef LEGACY_H
#define LEGACY_H

struct LegacyCounter {
    int ticks;
    LegacyCounter() : ticks(0) {}
    void tick() { ++ticks; }
};

typedef struct {
    int  code;
    char text[32];
} LegacyStatus;

#endif
