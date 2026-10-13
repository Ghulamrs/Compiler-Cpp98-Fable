// driver-layout.cpp - compiled by cl and linked with cppparse's layout.obj: cl's value of every expression in
// layout.hpp's LAYOUT_VALUES must equal cppparse's, and objects laid out by cppparse must read back right
// through cl's view of the classes.
#include "layout.hpp"

#include <stdio.h>

static int failures = 0;

static void expect(const char* what, long got, long want)
{
    if (got == want) printf("  ok   %-40s %ld\n", what, got);
    else { printf("  FAIL %-40s %ld, expected %ld\n", what, got, want); ++failures; }
}

int main()
{
    const long cl[] = {
#define LAYOUT_ENTRY(e) (long)(e),
        LAYOUT_VALUES(LAYOUT_ENTRY)
#undef LAYOUT_ENTRY
    };
    const char* const names[] = {
#define LAYOUT_ENTRY(e) #e,
        LAYOUT_VALUES(LAYOUT_ENTRY)
#undef LAYOUT_ENTRY
    };
    printf("layouts: cppparse's value, compared with cl's\n");
    for (int k = 0; k < (int)(sizeof(cl) / sizeof(cl[0])); ++k) expect(names[k], layoutValue(k), cl[k]);
    expect("layoutValue past the end", layoutValue((int)(sizeof(cl) / sizeof(cl[0]))), -1);

    printf("objects shared with cppparse\n");
    expect("g_em.w", g_em.w, 41);
    expect("g_ne.n", g_ne.n, 7);
    expect("g_ne.w", g_ne.w, 9);
    EM em; em.w = 5;
    expect("emSum(&em)", emSum(&em), 6);
    EVal v;
    expect("evalGet(v, 12)", evalGet(v, 12), 12);
    expect("v.v after evalGet", v.v, 12);

    printf("bit-fields and anonymous members\n");
    expect("g_bf.a", g_bf.a, 5);
    expect("g_bf.b", g_bf.b, 17);
    expect("g_bf.c", g_bf.c, 0xABCDEF);
    expect("g_bf.d", g_bf.d, 'd');
    expect("g_bf6.a", g_bf6.a, -3);
    expect("g_bf6.b", g_bf6.b, 300);
    expect("g_bf6.c", g_bf6.c, -1);
    expect("g_bf6.s", g_bf6.s, -16);
    expect("g_bf7.a == -2", g_bf7.a == -2, 1);
    expect("g_bf7.b", g_bf7.b, 3);
    expect("g_bf7.c", (long)g_bf7.c, 0x2345678);
    expect("g_bf8.f", g_bf8.f, 1);
    expect("g_bf8.g", g_bf8.g, 0);
    expect("g_bf8.h", g_bf8.h, 'h');
    expect("g_bf8.k", g_bf8.k, Red);
    expect("g_bfp.c", g_bfp.c, 'p');
    expect("g_bfp.a", g_bfp.a, 7);
    expect("g_bfp.b", g_bfp.b, -100000);
    expect("g_agg.x", g_agg.x, 11);
    expect("g_agg.y", g_agg.y, 9);
    expect("g_agg.z", g_agg.z, -7);
    expect("g_au.k", g_au.k, 1);
    expect("g_au.dd * 2", (long)(g_au.dd * 2), 5);
    expect("g_au.z", g_au.z, 'z');
    expect("g_an.t", g_an.t, 't');
    expect("g_an.all", g_an.all, 0x56781234);
    expect("g_an.f", g_an.f, 9);
    expect("g_an.gc & 7", g_an.gc & 7, 5);
    BF2 b2;
    fillBits(&b2, -1, 5, 1);
    expect("BF2 a written by cppparse", b2.a, -1);
    expect("BF2 b written by cppparse", b2.b, 5);
    expect("BF2 c written by cppparse", b2.c, 1);
    b2.a = 3; b2.b = -8; b2.c = -2;
    expect("BF2 a read by cppparse", readBits(&b2, 0), 3);
    expect("BF2 b read by cppparse", readBits(&b2, 1), -8);
    expect("BF2 c read by cppparse", readBits(&b2, 2), -2);
    BF6 b6; b6.a = -3; b6.b = 300; b6.c = -1; b6.s = -16;
    expect("bitOps", bitOps(&b6), 1111111);
    expect("b6.a after bitOps", b6.a, -3);
    expect("b6.b after bitOps", b6.b, 511);
    expect("b6.s after bitOps", b6.s, -15);
    BF7 b7; b7.a = -2; b7.b = -4; b7.c = -5;
    expect("BF7 a read by cppparse", readBf7(&b7, 0), 1);
    expect("BF7 b read by cppparse", readBf7(&b7, 1), -4);
    expect("BF7 c read by cppparse", readBf7(&b7, 2), -5);

    printf("polymorphic bases\n");
    DP dp; DQ dq; DR dr;
    const PB& pdp = dp; const PB& pdq = dq; const PB& pdr = dr;
    expect("dp.x, dp.y, dp.z", dp.x * 100 + dp.y * 10 + dp.z, 123);
    expect("dp f() through cl's PB&", pdp.f(), 123);
    expect("dq f() through cl's PB&", pdq.f(), 5);
    expect("dr f() through cl's PB&", pdr.f(), 2004);
    expect("dq g() called by cl", dq.g(), 4 + 5 + 'q');
    expect("dr g() through cl's DQ&", static_cast<const DQ&>(dr).g(), 1007);
    expect("polyCall(&dp)", polyCall(&dp), 123);
    expect("polyCall(&dr)", polyCall(&dr), 2004);
    expect("polyCallG(dr)", polyCallG(dr), 1007);
    expect("polyX(&dp)", polyX(&dp), 1);
    expect("polyX(&dr)", polyX(&dr), 4);
    expect("dr.r", dr.r, 7);

    if (failures) { printf("%d FAILED\n", failures); return 1; }
    printf("ALL OK\n");
    return 0;
}
