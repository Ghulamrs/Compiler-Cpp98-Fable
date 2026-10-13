// layout.cpp - compiled by cppparse (--compile6): the values of layout.hpp's expressions, and objects the
// cl-compiled driver reads through its own view of the classes.
#include "layout.hpp"

EM g_em;
NE g_ne;
BF    g_bf;
BF6   g_bf6;
BF7   g_bf7;
BF8   g_bf8;
BFP   g_bfp;
BFAgg g_agg;
AU    g_au;
AN    g_an;

namespace {

const int s_values[] = {
#define LAYOUT_ENTRY(e) (int)(e),
    LAYOUT_VALUES(LAYOUT_ENTRY)
#undef LAYOUT_ENTRY
    0
};

struct Init {
    Init()
    {
        g_em.w = 41; g_ne.n = 7; g_ne.w = 9;
        g_bf.a = 5; g_bf.b = 17; g_bf.c = 0xABCDEF; g_bf.d = 'd';
        g_bf6.a = -3; g_bf6.b = 300; g_bf6.c = -1; g_bf6.s = -16;
        g_bf7.a = -2; g_bf7.b = 3; g_bf7.c = 0x2345678;
        g_bf8.f = true; g_bf8.g = false; g_bf8.h = 'h'; g_bf8.k = Red;
        g_bfp.c = 'p'; g_bfp.a = 7; g_bfp.b = -100000;
        BFAgg agg = { 11, 9, -7 };
        g_agg = agg;
        g_au.k = 1; g_au.dd = 2.5; g_au.z = 'z';
        g_an.t = 't'; g_an.lo = 0x1234; g_an.hi = 0x5678; g_an.f = 9; g_an.g = 5;
    }
};
Init s_init;

} // namespace

int layoutValue(int k)
{
    if (k < 0 || k >= (int)(sizeof(s_values) / sizeof(s_values[0])) - 1) return -1;
    return s_values[k];
}

int emSum(const EM* p)
{
    return p->w + 1;
}

int evalGet(EVal& v, int set)
{
    v.v = set;
    E* e = &v;
    E other;
    *e = other;                            // copies nothing: E's byte is the low byte of v
    EVal* back = static_cast<EVal*>(e);
    return back->get();
}

void fillBits(BF2* p, int a, int b, int c)
{
    p->a = (char)a;
    p->b = b;
    p->c = (char)c;
}

long readBits(const BF2* p, int which)
{
    switch (which) {
    case 0: return p->a;
    case 1: return p->b;
    case 2: return p->c;
    }
    return -99;
}

long bitOps(BF6* p)
{
    long r = 0;
    p->a += 2;                             // -3 + 2 = -1
    r = r * 10 + (p->a == -1);
    p->b++;                                // 301
    r = r * 10 + (p->b == 301);
    ++p->s;                                // -15
    r = r * 10 + (p->s == -15);
    int v = (p->a = 5);                    // 5 does not fit in 3 signed bits: -3
    r = r * 10 + (v == -3);
    p->b -= 302;                           // 9 unsigned bits wrap: 511
    r = r * 10 + (p->b == 511);
    p->c = 0;
    r = r * 10 + (p->c == 0 && p->s == -15 && p->a == -3);
    AUC u;
    u.fv = 1.0f;                           // 0x3F800000 seen through the int member
    r = r * 10 + (u.twice() == 0x7F000000 && u.tag == 1);
    return r;
}

long readBf7(const BF7* p, int which)
{
    switch (which) {
    case 0: return p->a == -2 ? 1 : 0;
    case 1: return p->b;
    case 2: return (long)p->c;
    }
    return -99;
}

PB::PB() : y(2) {}
int PB::f() const { return y; }
DP::DP() : z(3) { x = 1; }
int DP::f() const { return x * 100 + y * 10 + z; }
DQ::DQ() : q('q') { x = 4; y = 5; }
int DQ::g() const { return x + y + q; }
DR::DR() : r(7) {}
int DR::g() const { return 1000 + r; }
int DR::f() const { return 2000 + x; }

int polyCall(const PB* p)
{
    return p->f();
}

int polyCallG(const DQ& q)
{
    return q.g();
}

int polyX(const NP* p)
{
    return p->x;
}
