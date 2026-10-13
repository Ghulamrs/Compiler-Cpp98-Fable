// driver-statics.cpp - compiled by cl /O2 and linked with cppparse's statics.obj: sharedA is reached by cl first,
// sharedB by cppparse first; every object must be constructed once and be the same object on both sides.
#include "statics.hpp"

#include <stdio.h>

int g_ctorCalls = 0;
Counted::Counted() : id(++g_ctorCalls) {}
int g_flakyAttempts[2] = { 0, 0 };
int g_flakyBuilt[2] = { 0, 0 };
FlakyQ::FlakyQ(int w) : which(w) { if (++g_flakyAttempts[w] == 1) throw 7; ++g_flakyBuilt[w]; }
static int clFlakyY() { try { return flakyY().which; } catch (int) { return -1; } }
static int clFlakyX() { try { return flakyX().which; } catch (int) { return -1; } }

static int failures = 0;
static void expect(const char* what, long got, long want)
{
    if (got == want) printf("  ok   %-28s %ld\n", what, got);
    else { printf("  FAIL %-28s %ld, expected %ld\n", what, got, want); ++failures; }
}

int main(int argc, char**)
{
    setvbuf(stdout, 0, _IONBF, 0);   // the lines so far when a check hangs
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
    // a throwing initializer: cppparse first (throws), then cl constructs; and the reverse. Only when
    // cppparse's copies of the inline functions are linked (an argument says they are not): through cl's
    // copy, cppparse's exception would have to unwind a cl frame, which the two runtimes do not support
    if (argc < 2) {
    expect("flakyX cppparse throws", cpp_flakyX(), -1);
    expect("flakyX cl constructs", clFlakyX(), 0);
    expect("flakyX cppparse later", cpp_flakyX(), 0);
    expect("flakyX cl later", clFlakyX(), 0);
    expect("flakyX attempts", g_flakyAttempts[0], 2);
    expect("flakyX built", g_flakyBuilt[0], 1);
    expect("flakyY cl throws", clFlakyY(), -1);
    expect("flakyY cppparse constructs", cpp_flakyY(), 1);
    expect("flakyY cl later", clFlakyY(), 1);
    expect("flakyY cppparse later", cpp_flakyY(), 1);
    expect("flakyY attempts", g_flakyAttempts[1], 2);
    expect("flakyY built", g_flakyBuilt[1], 1);
    }
    if (failures == 0) printf("ALL OK\n");
    return failures == 0 ? 0 : 1;
}
