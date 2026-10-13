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

    if (failures) { printf("%d FAILED\n", failures); return 1; }
    printf("ALL OK\n");
    return 0;
}
