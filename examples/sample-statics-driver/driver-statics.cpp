// driver-statics.cpp - compiled by cl /O2 and linked with cppparse's statics.obj: sharedA is reached by cl first,
// sharedB by cppparse first; every object must be constructed once and be the same object on both sides.
#include "statics.hpp"

#include <stdio.h>

int g_ctorCalls = 0;
Counted::Counted() : id(++g_ctorCalls) {}

static int failures = 0;
static void expect(const char* what, long got, long want)
{
    if (got == want) printf("  ok   %-28s %ld\n", what, got);
    else { printf("  FAIL %-28s %ld, expected %ld\n", what, got, want); ++failures; }
}

int main()
{
    int a1 = sharedA().id;        // cl first
    int a2 = cpp_touchA();
    expect("sharedA cl then cppparse", a2, a1);
    expect("constructions after A", g_ctorCalls, 1);
    int b1 = cpp_touchB();        // cppparse first
    int b2 = sharedB().id;
    expect("sharedB cppparse then cl", b2, b1);
    expect("constructions after B", g_ctorCalls, 2);
    expect("same object A", cpp_addrA() == &sharedA(), 1);
    expect("same object B", cpp_addrB() == &sharedB(), 1);
    int c1 = cpp_touchC();        // two guarded statics in one function: $TSS0 and $TSS1
    int c2 = sharedC();
    expect("sharedC cppparse then cl", c2, c1);
    expect("sharedC value", c1, 11);
    expect("constructions after C", g_ctorCalls, 3);
    if (failures == 0) printf("ALL OK\n");
    return failures == 0 ? 0 : 1;
}
