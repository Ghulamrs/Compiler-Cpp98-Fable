// abi.cpp - compiled by cppparse: callees for cl's driver (c_*, M) and a caller of cl's functions (k_*, N).
#include "abi.hpp"

#define ABI_DEFINE(T) \
    T c_mk##T(int s) { T r; fill(r, s); return r; } \
    int c_use##T(int k, T a, T b) { return val(a) * k + val(b); }
ABI_TYPES(ABI_DEFINE)
ABI_MORE_DEFINE(c)

static Mk8 pick(Mk8 f) { return f; }

int c_viaPointers(Mk8 f8, S12 (*f12)(int), S24 (*f24)(int), C4 (*fc)(int), int (*fuse)(int, S8, S8))
{
    int bad = 0;
    S8 e8; fill(e8, 3); S8 a = f8(3); if (val(a) != val(e8)) bad |= 1;
    Mk8 lp = f8; S8 b = (*lp)(4); S8 e4; fill(e4, 4); if (val(b) != val(e4)) bad |= 2;
    S12 c = (*f12)(5); S12 e5; fill(e5, 5); if (val(c) != val(e5)) bad |= 4;
    S24 d = f24(6); if (d.a != 6 || d.b != 12 || d.c != 18) bad |= 8;
    C4 e = fc(7); C4 e7; fill(e7, 7); if (val(e) != val(e7)) bad |= 16;
    if (fuse(2, a, b) != val(a) * 2 + val(b)) bad |= 32;
    S8 g = pick(f8)(8); S8 e88; fill(e88, 8); if (val(g) != val(e88)) bad |= 64;
    return bad;
}

int c_many(S1 a, S2 b, S4 c, S8 d, F2 e, D1 f, S3 g, C4 h)
{
    return val(a) + val(b) * 2 + val(c) * 3 + val(d) * 4 + val(e) * 5 + val(f) * 6 + val(g) * 7 + val(h) * 8;
}

S4 M::get(int s) const { S4 r; r.a = base + s; return r; }
S8 M::pair(S4 a) const { S8 r; r.a = base; r.b = a.a; return r; }
S8 M::make(int s) { S8 r; r.a = s; r.b = s + 1; return r; }

static int g_failed = 0;
static void check(bool ok) { if (!ok) ++g_failed; }

#define ABI_CALL(T) \
    { T x = k_mk##T(5); T y; fill(y, 5); check(val(x) == val(y)); \
      T a; fill(a, 3); T b; fill(b, 4); check(k_use##T(9, a, b) == val(a) * 9 + val(b)); \
      check(k_use##T(2, k_mk##T(6), c_mk##T(7)) == val(c_mk##T(6)) * 2 + val(k_mk##T(7))); }

int c_callsK()
{
    g_failed = 0;
    ABI_TYPES(ABI_CALL)
    ABI_MORE_CHECK(k, check)
    check(c_viaPointers(k_mkS8, k_mkS12, k_mk24, k_mkC4, k_useS8) == 0);
    check(c_viaPointers(c_mkS8, c_mkS12, c_mk24, c_mkC4, c_useS8) == 0);
    S1 a; fill(a, 1); S2 b; fill(b, 2); S4 c; fill(c, 3); S8 d; fill(d, 4); F2 e; fill(e, 5); D1 f; fill(f, 6); S3 g; fill(g, 7); C4 h; fill(h, 8);
    check(k_many(a, b, c, d, e, f, g, h) == c_many(a, b, c, d, e, f, g, h));
    N n; n.base = 40;
    check(n.get(2).a == 42);
    S4 q; q.a = 9;
    S8 p = n.pair(q); check(p.a == 40 && p.b == 9);
    S8 m = N::make(7); check(m.a == 7 && m.b == 8);
    return g_failed;
}
