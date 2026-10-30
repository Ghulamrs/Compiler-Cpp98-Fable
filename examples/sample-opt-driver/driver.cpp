// driver.cpp - links against the x86-64 code cppparse generated for opt.cpp
// and checks every function's result. Built with MSVC:
//   cl /EHsc driver.cpp opt.obj
// The symbols use cppparse's mangling (see Lower.cpp: mangle()).
#include <cstdio>
#include <cstdlib>

extern "C" {
    int      square_int_x(int x);
    int      constants();
    int      deadcode_int_x(int x);
    int      cse_int_a_int_b(int a, int b);
    int      loopInvariant_int_n_int_k(int n, int k);
    int      unroll();
    int      strength_int_x(int x);
    double   mix_double_d_int_i(double d, int i);
    int      usePoint();
    int      branches_int_v(int v);
    unsigned pick_unsigned_flags(unsigned flags);
    int      useTemplates();
    // version 5: exceptions (the runtime runtime/cppparse_rt.cpp is linked as well)
    int      throwCatchInt_int_v(int v);
    int      catchAll_int_v(int v);
    int      throwThrough_int_v(int v);
    int      rethrown_int_v(int v);
    int      leaveTry_int_n(int n);
    int      cppparse_frame_depth();
    int      cppparse_caught_depth();
}

static int g_failures = 0;

static void check(const char* what, long got, long expected)
{
    if (got == expected) std::printf("  ok   %-28s = %ld\n", what, got);
    else { std::printf("  FAIL %-28s = %ld, expected %ld\n", what, got, expected); ++g_failures; }
}

int main()
{
    check("square(5)", square_int_x(5), 25);
    check("square(-7)", square_int_x(-7), 49);
    check("constants()", constants(), 14);
    check("deadcode(7)", deadcode_int_x(7), 7);
    check("cse(3, 4)", cse_int_a_int_b(3, 4), 24);
    check("loopInvariant(3, 5)", loopInvariant_int_n_int_k(3, 5), 30);
    check("loopInvariant(0, 5)", loopInvariant_int_n_int_k(0, 5), 0);
    check("unroll()", unroll(), 6);
    check("strength(2)", strength_int_x(2), 18);
    check("strength(-3)", strength_int_x(-3), -27);
    check("mix(1.5, 2) * 10", static_cast<long>(mix_double_d_int_i(1.5, 2) * 10.0), 30);
    check("usePoint()", usePoint(), 12);
    check("branches(5)", branches_int_v(5), 5);
    check("branches(-3)", branches_int_v(-3), 3);
    check("branches(20)", branches_int_v(20), 0);
    check("pick(1)", pick_unsigned_flags(1), 20);
    check("pick(6)", pick_unsigned_flags(6), 30);
    check("pick(4)", pick_unsigned_flags(4), 10);
    check("useTemplates()", useTemplates(), 14);
    check("throwCatchInt(4)", throwCatchInt_int_v(4), 8);
    check("throwCatchInt(-6)", throwCatchInt_int_v(-6), 6);
    check("catchAll(0)", catchAll_int_v(0), 1);
    check("catchAll(1)", catchAll_int_v(1), 2);
    check("throwThrough(5)", throwThrough_int_v(5), 1);
    check("throwThrough(12)", throwThrough_int_v(12), 1012);
    check("throwThrough(-3)", throwThrough_int_v(-3), -3);
    check("rethrown(0)", rethrown_int_v(0), 0);
    check("rethrown(7)", rethrown_int_v(7), 14);
    check("leaveTry(10)", leaveTry_int_n(10), 103);
    check("leaveTry(2)", leaveTry_int_n(2), 101);
    check("frames left open", cppparse_frame_depth(), 0);
    check("handlers left open", cppparse_caught_depth(), 0);
    if (g_failures) { std::printf("FAILED: %d check(s)\n", g_failures); return 1; }
    std::printf("ALL OK\n");
    return 0;
}
