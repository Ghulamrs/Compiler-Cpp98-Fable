// driver-eh.cpp - compiled by cl, linked with cppparse's probe.obj, entry.obj (ml64) and the runtime
// (sandbox\scripts\eh.ps1).
// Unwind tables: probe_entry fills the callee-saved registers with patterns and calls probeBig, which recurses
// through probeDeep; at the bottom, probe_report walks the stack back to probe_entry with RtlVirtualUnwind. Every
// cppparse frame on the way must be found, and the registers restored in probe_entry's frame must be the patterns.
#include <stdio.h>

extern "C" int probe_entry(int n, int (*fn)(int));
extern "C" int cppparse_walk_frames(const void* stop, const void* counted, unsigned long long* ints, unsigned long long* xmms);
int probeDeep(int n, double x);
int probeBig(int n);

static int g_walked = -2;
static unsigned long long g_ints[8], g_xmms[10];

extern "C" int probe_report(int)
{
    g_walked = cppparse_walk_frames((const void*)&probe_entry, (const void*)&probeDeep, g_ints, g_xmms);
    return 1000;
}

// the same computation as probe.cpp, compiled by cl
static int refDeep(int n, double x)
{
    int a = n * 3, b = n + 7, c = n ^ 5, d = n * n, e = n - 2;
    double y = x * 1.5, z = x + 2.25, w = x * x;
    int r = n == 0 ? 1000 : refDeep(n - 1, x + 1.0);
    return r + a + b + c + d + e + static_cast<int>(y + z + w);
}

static int failures = 0;
static void check(const char* what, unsigned long long got, unsigned long long expected)
{
    if (got == expected) return;
    printf("FAIL %s: got %016llx, expected %016llx\n", what, got, expected);
    ++failures;
}

int main()
{
    const int depth = 12;
    const int result = probe_entry(depth, &probeBig);
    check("result of probeBig", (unsigned long long)result, (unsigned long long)(refDeep(depth, 0.5) + depth % 64 + 5999 % 64));
    check("probeDeep frames found by the walk", (unsigned long long)g_walked, depth + 1);
    static const char* const intNames[8] = { "rbx", "rbp", "rsi", "rdi", "r12", "r13", "r14", "r15" };
    static const char* const xmmNames[10] = { "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" };
    static const unsigned long long intPatterns[8] = { 0x1111111111111111ULL, 0x2222222222222222ULL, 0x3333333333333333ULL,
        0x4444444444444444ULL, 0x5555555555555555ULL, 0x6666666666666666ULL, 0x7777777777777777ULL, 0xA8A8A8A8A8A8A8A8ULL };
    for (int i = 0; i < 8; ++i) check(intNames[i], g_ints[i], intPatterns[i]);
    for (int i = 0; i < 10; ++i) {
        unsigned long long b = 0xB6 + i;
        check(xmmNames[i], g_xmms[i], b * 0x0101010101010101ULL);
    }
    printf("unwind walk: %d probeDeep frames, %d registers checked\n", g_walked, 18);
    printf(failures ? "FAILED\n" : "ALL OK\n");
    return failures ? 1 : 0;
}
