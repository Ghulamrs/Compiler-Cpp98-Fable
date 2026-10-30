// driver-align.cpp - compiled by cl and linked with cppparse's align.obj: cl's sizes and offsets for align.hpp
// must equal the ones cppparse computed, the globals cppparse laid out must read back right through cl's view
// of the types, and every global must sit at the alignment its declaration asks for.
#include "align.hpp"

#include <stddef.h>
#include <stdio.h>

static int failures = 0;

static void expect(const char* what, long got, long want)
{
    if (got == want) printf("  ok   %-36s %ld\n", what, got);
    else { printf("  FAIL %-36s %ld, expected %ld\n", what, got, want); ++failures; }
}

int main()
{
    const long cl[] = {
        (long)sizeof(Pack1), (long)offsetof(Pack1, d), (long)offsetof(Pack1, i), (long)sizeof(Pack2), (long)offsetof(Pack2, d),
        (long)sizeof(Pack4), (long)offsetof(Pack4, d), (long)sizeof(Natural), (long)sizeof(Align32),
        (long)offsetof(HoldsAlign32, x), (long)sizeof(HoldsAlign32), (long)offsetof(PackedHoldsAlign32, x),
        (long)sizeof(PackedHoldsAlign32), (long)offsetof(MemberAligned, x), (long)sizeof(MemberAligned),
        (long)offsetof(AfterNamedPop, d), (long)sizeof(AfterNamedPop)
    };
    const char* const names[] = {
        "sizeof(Pack1)", "Pack1::d", "Pack1::i", "sizeof(Pack2)", "Pack2::d", "sizeof(Pack4)", "Pack4::d",
        "sizeof(Natural)", "sizeof(Align32)", "HoldsAlign32::x", "sizeof(HoldsAlign32)", "PackedHoldsAlign32::x",
        "sizeof(PackedHoldsAlign32)", "MemberAligned::x", "sizeof(MemberAligned)", "AfterNamedPop::d", "sizeof(AfterNamedPop)"
    };
    printf("layouts: cppparse's value, compared with cl's\n");
    for (int k = 0; k < (int)(sizeof(cl) / sizeof(cl[0])); ++k) expect(names[k], layoutValue(k), cl[k]);
    expect("layoutValue past the end", layoutValue(99), -1);

    printf("globals defined by cppparse, read by cl\n");
    expect("g_pack1.c", g_pack1.c, 'p');
    expect("g_pack1.d * 4", (long)(g_pack1.d * 4), 10);
    expect("g_pack1.i", g_pack1.i, 7);
    expect("g_align64 * 4", (long)(g_align64 * 4), 25);
    expect("g_align32.a", g_align32.a, 32);
    expect("g_char", g_char, 'c');
    expect("g_int", g_int, 41);

    printf("alignment of the globals\n");
    expect("&g_align64 % 64", (long)((size_t)&g_align64 % 64), 0);
    expect("&g_align32 % 32", (long)((size_t)&g_align32 % 32), 0);
    expect("&g_pack1 % 8 (size 13)", (long)((size_t)&g_pack1 % 8), 0);
    expect("&g_int % 4", (long)((size_t)&g_int % 4), 0);

    if (failures) { printf("%d FAILED\n", failures); return 1; }
    printf("ALL OK\n");
    return 0;
}
