// driver-abi.cpp - compiled by cl and linked with cppparse's abi.obj: cl calls cppparse's functions with
// small and large classes as arguments and results, and cppparse calls cl's (c_callsK).
#include "abi.hpp"

#include <stdio.h>

#define ABI_DEFINE(T) \
    T k_mk##T(int s) { T r; fill(r, s); return r; } \
    int k_use##T(int k, T a, T b) { return val(a) * k + val(b); }
ABI_TYPES(ABI_DEFINE)

int k_many(S1 a, S2 b, S4 c, S8 d, F2 e, D1 f, S3 g, C4 h)
{
    return val(a) + val(b) * 2 + val(c) * 3 + val(d) * 4 + val(e) * 5 + val(f) * 6 + val(g) * 7 + val(h) * 8;
}

S4 N::get(int s) const { S4 r; r.a = base + s; return r; }
S8 N::pair(S4 a) const { S8 r; r.a = base; r.b = a.a; return r; }
S8 N::make(int s) { S8 r; r.a = s; r.b = s + 1; return r; }

static int failures = 0;
static void expect(const char* what, int got, int want)
{
    if (got == want) printf("  ok   %-24s %d\n", what, got);
    else { printf("  FAIL %-24s %d, expected %d\n", what, got, want); ++failures; }
}

#define ABI_CALL(T) \
    { T x = c_mk##T(5); T y; fill(y, 5); expect("c_mk" #T, val(x), val(y)); \
      T a; fill(a, 3); T b; fill(b, 4); expect("c_use" #T, c_use##T(9, a, b), val(a) * 9 + val(b)); \
      expect("c_use" #T " nested", c_use##T(2, c_mk##T(6), k_mk##T(7)), val(k_mk##T(6)) * 2 + val(c_mk##T(7))); }

int main()
{
    ABI_TYPES(ABI_CALL)
    S1 a; fill(a, 1); S2 b; fill(b, 2); S4 c; fill(c, 3); S8 d; fill(d, 4); F2 e; fill(e, 5); D1 f; fill(f, 6); S3 g; fill(g, 7); C4 h; fill(h, 8);
    expect("c_many", c_many(a, b, c, d, e, f, g, h), k_many(a, b, c, d, e, f, g, h));
    M m; m.base = 40;
    expect("M::get", m.get(2).a, 42);
    S4 q; q.a = 9;
    S8 p = m.pair(q); expect("M::pair", p.a * 100 + p.b, 4009);
    S8 s = M::make(7); expect("M::make", s.a * 100 + s.b, 708);
    expect("c_callsK failures", c_callsK(), 0);
    if (failures == 0) printf("ALL OK\n");
    return failures == 0 ? 0 : 1;
}
