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

    // runtime hook used by generated code for "throw"
    void cppparse_throw(void* object)
    {
        std::printf("cppparse_throw(%p): exceptions are not implemented on the x86-64 target\n", object);
        std::abort();
    }
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
    if (g_failures) { std::printf("FAILED: %d check(s)\n", g_failures); return 1; }
    std::printf("ALL OK\n");
    return 0;
}
