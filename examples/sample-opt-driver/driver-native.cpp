// driver-native.cpp - links against the object cppparse generated for opt.cpp
// with --msvc-mangling: the functions are declared as plain C++ (no extern "C"),
// so MSVC's own decorated names must match cppparse's. Built with MSVC:
//   cl /EHsc driver-native.cpp opt.obj cppparse_rt.obj
#include <cstdio>

int      square(int x);
int      constants();
int      deadcode(int x);
int      cse(int a, int b);
int      loopInvariant(int n, int k);
int      unroll();
int      strength(int x);
double   mix(double d, int i);
int      usePoint();
int      branches(int v);
unsigned pick(unsigned flags);
int      useTemplates();
int      throwCatchInt(int v);
int      catchAll(int v);
int      throwThrough(int v);
int      rethrown(int v);
int      leaveTry(int n);

extern "C" int cppparse_frame_depth();
extern "C" int cppparse_caught_depth();

static int g_failures = 0;

static void check(const char* what, long got, long expected)
{
    if (got == expected) std::printf("  ok   %-28s = %ld\n", what, got);
    else { std::printf("  FAIL %-28s = %ld, expected %ld\n", what, got, expected); ++g_failures; }
}

int main()
{
    check("square(5)", square(5), 25);
    check("constants()", constants(), 14);
    check("deadcode(7)", deadcode(7), 7);
    check("cse(3, 4)", cse(3, 4), 24);
    check("loopInvariant(3, 5)", loopInvariant(3, 5), 30);
    check("unroll()", unroll(), 6);
    check("strength(-3)", strength(-3), -27);
    check("mix(1.5, 2) * 10", static_cast<long>(mix(1.5, 2) * 10.0), 30);
    check("usePoint()", usePoint(), 12);
    check("branches(20)", branches(20), 0);
    check("pick(6)", pick(6), 30);
    check("useTemplates()", useTemplates(), 14);
    check("throwCatchInt(-6)", throwCatchInt(-6), 6);
    check("catchAll(1)", catchAll(1), 2);
    check("throwThrough(12)", throwThrough(12), 1012);
    check("rethrown(7)", rethrown(7), 14);
    check("leaveTry(10)", leaveTry(10), 103);
    check("frames left open", cppparse_frame_depth(), 0);
    check("handlers left open", cppparse_caught_depth(), 0);
    if (g_failures) { std::printf("FAILED: %d check(s)\n", g_failures); return 1; }
    std::printf("ALL OK\n");
    return 0;
}
