// layout.cpp - compiled by cppparse (--compile6): the values of layout.hpp's expressions, and objects the
// cl-compiled driver reads through its own view of the classes.
#include "layout.hpp"

#include <new>
#include <stdlib.h>

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

int g_dtorLog = 0;
int g_sbCtorSaw = 0, g_sbDtorSaw = 0, g_sdDtorSaw = 0;
SA::SA() : sa(10) {}
int SA::a() const { return 1; }
int SA::f() const { return 2; }
SB::SB() : sb(20) { g_sbCtorSaw = g(); }      // SB::g: the object is an SB while SB is built
int SB::b() const { return 3; }
int SB::f() const { return 4; }
int SB::g() const { return 5; }
SB::~SB() { g_dtorLog = g_dtorLog * 10 + 2; g_sbDtorSaw = g(); }   // SB::g again while SB is destroyed
SD::SD() : sd(30) {}
int SD::f() const { return 100 + sd; }
int SD::g() const { return 200 + sa + sb + sd; }   // this arrives at the SB subobject and is moved back to SD
int SD::b() const { return 300 + sa; }
int SD::h() const { return 400 + g(); }            // a call through this: moved to SB again
SD::~SD() { g_dtorLog = g_dtorLog * 10 + 1; g_sdDtorSaw = h(); }   // SD::h, SD::g, not SE::g
SE::SE() : se('e') { x = 7; }
int SE::g() const { return 500 + se; }
int SE::a() const { return 600 + x; }
SE::~SE() { g_dtorLog = g_dtorLog * 10 + 3; }

int saCall(const SA* p, int which)
{
    return which == 0 ? p->a() : p->f();
}

int sbCall(const SB* p, int which)
{
    switch (which) {
    case 0: return p->b();
    case 1: return p->f();
    case 2: return p->g();
    }
    return -1;
}

int sdCall(const SD* p, int which)
{
    switch (which) {
    case 0: return p->f();
    case 1: return p->g();
    case 2: return p->b();
    case 3: return p->h();
    case 4: return p->a();
    case 5: return p->SD::g();
    }
    return -1;
}

long destroyThroughSB(int derived)
{
    static double storage[8];
    g_dtorLog = 0;
    SB* b = derived ? static_cast<SB*>(new (storage) SE) : static_cast<SB*>(new (storage) SD);
    b->~SB();                              // the destructor slot of SB's table: a thunk to the complete object's
    return g_dtorLog;
}

long deleteThroughSB(int derived)
{
    SB* b = derived ? static_cast<SB*>(new SE) : static_cast<SB*>(new SD);
    g_dtorLog = 0;
    delete b;                              // the deleting destructor frees the complete object, not the SB part
    return g_dtorLog;
}

int g_kDeleted = 0;
KB::KB() : k(5) {}
KB::~KB() {}
void KB::operator delete(void* p, size_t n) { g_kDeleted = (int)n; free(p); }
KD::KD() : kd(6) {}
KD::~KD() {}
KB* makeKDasKB() { return new KD; }
void deleteKB(KB* p) { delete p; }

GP::GP() : p(8) {}
GP::~GP() {}
int GP::fp() const { return 80 + p; }
int GQ2::fq() const { return 90 + p; }
int GG::fp() const { return 10 + GQ1::p; }
int ggCall(const GQ2* p) { return p->fp() * 100 + p->fq(); }
GG* makeGG() { return new GG; }

int g_ovLog = 0;
OV::OV() : o(0) {}
int OV::a() const { return 1 + o; }
int OV::f(int) const { return 2 + o; }
int OV::b() const { return 3 + o; }
int OV::f(double) const { return 4 + o; }
int OV::c() const { return 5 + o; }
OV::~OV() { g_ovLog = g_ovLog * 10 + 1; }
int OV::f(char) const { return 6 + o; }
OW::OW() { o = 10; }
int OW::f(double) const { return 104 + o; }
int OW::g() const { return 107 + o; }
int OW::f(long) const { return 108 + o; }
int OW::a() const { return 101 + o; }
int OW::g(int) const { return 109 + o; }
int OW::f(short) const { return 110 + o; }
OW::~OW() { g_ovLog = g_ovLog * 10 + 2; }

int ovCall(const OV* p, int which)
{
    switch (which) {
    case 0: return p->a();
    case 1: return p->f(1);
    case 2: return p->b();
    case 3: return p->f(1.0);
    case 4: return p->c();
    case 5: return p->f('x');
    }
    return -1;
}

int owCall(const OW* p, int which)
{
    switch (which) {
    case 6: return p->g();
    case 7: return p->f(1L);
    case 8: return p->g(1);
    case 9: return p->f((short)1);
    }
    return ovCall(p, which);
}

OV* makeOW() { return new OW; }
void deleteOV(OV* p) { delete p; }

long g_avLog = 0;
int  g_avNext = 0;
long g_avFreed = 0;
AV::AV() : a(++g_avNext) {}
AV::~AV() { g_avLog = g_avLog * 10 + a; }
int AV::v() const { return a; }
void AV::operator delete[](void* p, size_t n) { g_avFreed = (long)n; free(p); }
AW::AW() : w(a + 5) {}
AW::~AW() { g_avLog = g_avLog * 10 + w % 10; }
int AW::v() const { return w; }
AV* makeAVArray(int n) { return new AV[n]; }
AW* makeAWArray(int n) { return new AW[n]; }
void deleteAVArray(AV* p) { delete[] p; }
void deleteAWArray(AW* p) { delete[] p; }

long g_tnNew = 0, g_tnDel = 0, g_tnLog = 0;
int  g_tnNext = 0;
TN::TN() : t(++g_tnNext) {}
TN::~TN() { g_tnLog = g_tnLog * 10 + t; }
void* TN::operator new(size_t n) { g_tnNew = (long)n; return malloc(n); }
void* TN::operator new[](size_t n) { g_tnNew = (long)n; return malloc(n); }
void TN::operator delete(void* p, size_t n) { g_tnDel = (long)n; free(p); }
void TN::operator delete[](void* p, size_t n) { g_tnDel = (long)n; free(p); }
TM::TM() : m(t + 5) {}
TM::~TM() { g_tnLog = g_tnLog * 10 + m % 10; }
TN* makeTN() { return new TN; }
TN* makeTNArray(int n) { return new TN[n]; }
TM* makeTMArray(int n) { return new TM[n]; }
void deleteTN(TN* p) { delete p; }
void deleteTNArray(TN* p) { delete[] p; }
void deleteTMArray(TM* p) { delete[] p; }
TN* makeTNGlobal() { return ::new TN; }
void deleteTNGlobal(TN* p) { ::delete p; }