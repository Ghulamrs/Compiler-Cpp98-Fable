// driver-eh.cpp - compiled by cl, linked with the probe objects (cppparse's, or cl's for the reference), entry.obj
// (ml64) and the runtime (sandbox\scripts\eh.ps1).
// Unwind tables: probe_entry fills the callee-saved registers with patterns and calls probeBig, which recurses
// through probeDeep; at the bottom, probe_report walks the stack back to probe_entry with RtlVirtualUnwind. Every
// probeDeep frame on the way must be found, and the registers restored in probe_entry's frame must be the patterns.
// Exceptions: the eh* functions print their results; the output must equal the cl build's.
#include <stdio.h>

extern "C" int probe_entry(int n, int (*fn)(int));
extern "C" int cppparse_walk_frames(const void* stop, const void* counted, unsigned long long* ints, unsigned long long* xmms);
extern "C" int cppparse_frame_depth();
extern "C" int cppparse_caught_depth();
int probeDeep(int n, double x);
int probeBig(int n);
int ehBasic(int n);
int ehDeep(int n);
int ehLocals(int n);
int ehNested(int n);
int ehFromHandler(int n);
int ehBig(int n);
int ehLoop(int n);
int ehMixed(int n);
void ehCatchLoop(int n);
void ehNestedLoops(int n);
extern int g_destroyed;

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

// called by the catch-in-a-loop tests of probe.cpp with string literals the caller keeps in registers
static unsigned g_notes = 0;
extern "C" void eh_note(const char* s, int v)
{
    unsigned h = static_cast<unsigned>(v);
    while (*s) h = h * 31 + static_cast<unsigned char>(*s++);
    g_notes = g_notes * 7 + h;
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
    setvbuf(stdout, 0, _IONBF, 0);   // a crash keeps what was printed before it
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

    for (int n = -1; n <= 5; ++n) printf("ehBasic(%d) = %d\n", n, ehBasic(n));
    printf("ehDeep(10) = %d\n", ehDeep(10));
    printf("ehLocals(20) = %d\n", ehLocals(20));
    for (int n = 0; n <= 5; ++n) printf("ehNested(%d) = %d\n", n, ehNested(n));
    printf("ehFromHandler(41) = %d\n", ehFromHandler(41));
    printf("ehBig(0) = %d, ehBig(99) = %d\n", ehBig(0), ehBig(99));
    printf("ehLoop(1000) = %d\n", ehLoop(1000));
    printf("ehMixed(0) = %d, ehMixed(21) = %d\n", ehMixed(0), ehMixed(21));
    ehCatchLoop(9);
    printf("ehCatchLoop(9): notes %08x\n", g_notes);
    g_notes = 0;
    ehNestedLoops(5);
    printf("ehNestedLoops(5): notes %08x\n", g_notes);
    printf("destructors: %d\n", g_destroyed);
    check("try frames left", (unsigned long long)cppparse_frame_depth(), 0);
    check("caught exceptions left", (unsigned long long)cppparse_caught_depth(), 0);
    printf(failures ? "FAILED\n" : "ALL OK\n");
    return failures ? 1 : 0;
}
