// driver-statics.cpp - compiled by cl /O2 and linked with cppparse's statics.obj: sharedA is reached by cl first,
// sharedB by cppparse first; every object must be constructed once and be the same object on both sides.
#define STATICS_INLINE __forceinline   // cl's code for konstK is inlined here: it reads the statics as data
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
    // constant-initialised statics: cl (inlined) first, then cppparse, then cl, on the same objects
    for (int k = 0; k < 3; ++k) {
        KonstSeen s;
        if (k == 1) cpp_konstK(s); else konstK(s);
        float g = (float)(1.0/3) + 1;
        for (int i = 0; i < k; ++i) g += 1;
        char name[48];
        sprintf(name, "konstK u (%s)", k == 1 ? "cppparse" : "cl"); expect(name, s.u, k);
        sprintf(name, "konstK d == %d.25", k); expect(name, s.d == k + 0.25, 1);
        sprintf(name, "konstK f == %d", 3 + k); expect(name, s.f == 3.0f + k, 1);
        sprintf(name, "konstK g float exact"); expect(name, s.g == g, 1);
        sprintf(name, "konstK z == -%d", k); expect(name, s.z == -k, 1);
        sprintf(name, "konstK c"); expect(name, s.c, 'b' + k);
        sprintf(name, "konstK b"); expect(name, s.b, k % 2 == 0);
        sprintf(name, "konstK t, t2"); expect(name, s.t * 100 + s.t2, 405);
    }
    expect("constructions after konstK", g_ctorCalls, 5);
    // statics in nested blocks: cl first for p, s, x, y, then cppparse; cppparse first for q, then cl
    NestedSeen n1, n2, n3, n4;
    nestedN(1, n1);
    cpp_nestedN(1, n2);
    expect("nested p same object", n1.p == n2.p && n1.p != 0, 1);
    expect("nested s same object", n1.s == n2.s && n1.s != 0, 1);
    expect("nested x same object", n1.x == n2.x && n1.x != 0, 1);
    expect("nested y same object", n1.y == n2.y && n1.y != 0, 1);
    expect("nested w cl, cppparse", n1.w * 10 + n2.w, 78);
    expect("constructions after nested", g_ctorCalls, 9);
    cpp_nestedN(-1, n3);
    nestedN(-1, n4);
    expect("nested q same object", n3.p == n4.p && n3.p != 0 && n3.p != n1.p, 1);
    expect("nested s again", n3.s == n1.s && n4.s == n1.s, 1);
    expect("nested w cppparse, cl", n3.w * 10 + n4.w, 9 * 10 + 10);
    expect("constructions after nested q", g_ctorCalls, 10);
    // folded constants: cl first (inlined), then cppparse; the values cl gives the same expressions
    for (int k = 0; k < 2; ++k) {
        KonstFold s;
        if (k == 0) konstF(s); else cpp_konstF(s);
        volatile float big = 1.0e38f;
        float inf = big * 10;
        expect("konstF 16777217 + 1.0f", s.f3 == 16777216.0f, 1);
        expect("konstF 0.1f * 3", s.d17 == (double)(0.1f * 3), 1);
        expect("konstF (float)(1.0/3) + 1", s.d16 == (double)((float)(1.0 / 3) + 1), 1);
        expect("konstF 1.0e38f * 10 / 10", s.f6 == inf, 1);
        expect("konstF 3000000000.0", s.u == 3000000000u, 1);
        expect("konstF 3.4028235e38f", s.fm == 3.4028235e38f, 1);
        expect("konstF sizeof, 5LL", s.ds == 4 && s.dl == 5, 1);
        expect("konstF 1.00000005960464477550f", s.fh == 1.00000005960464477550f && s.fh != 1.0f, 1);
        expect("konstF (unsigned char)300.0 + 0.5", s.dv == 300.5, 1);
        expect("konstF (int)-2147483649.0", s.ni == 2147483647, 1);
        expect("konstF (long long)-9.3e18", s.nl == 0x7EEFC34604DE0000LL, 1);
        expect("konstF (int)3e9 as a double", s.dc == 3000000000.0, 1);
        expect("konstF \"\\x41\" \"b\" \"\\0\" \"c\"", s.cs[0] == 'A' && s.cs[1] == 'b' && s.cs[2] == 0 && s.cs[3] == 'c', 1);
        expect("konstF t", s.t, 11);
    }
    expect("constructions after konstF", g_ctorCalls, 11);
    const Counted* tr1 = tryN(1);
    expect("tryN s1 shared", tr1 == cpp_tryN(1) && tr1 != 0, 1);
    const Counted* tr0 = cpp_tryN(0);
    expect("tryN s3 shared", tr0 == tryN(0) && tr0 != 0 && tr0 != tr1, 1);
    expect("constructions after tryN", g_ctorCalls, 13);
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
