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

// virtual bases: built, converted, copied and destroyed by cppparse's code
long g_vbLog = 0;
VD::VD() : d(1) {}
VD::~VD() { g_vbLog = g_vbLog * 10 + 5; }
int  vbReadV(VL* p) { return p->v; }
int  vbReadVR(const VR& r) { return r.v * 10 + r.r; }
int  vbReadVM(VM* p) { return (int)(p->q * 2) + p->v + p->m; }
int  vbReadVN(const VN& n) { return n.v * 10 + n.n; }
VB*  vbToVB(VX* p) { return p; }
VX*  vbMakeVX() { return new VX; }
void vbDelete(VX* p) { delete p; }
long vbOnStack() { g_vbLog = 0; { VX x; if (x.v != 7 || vbReadV(&x) != 7) return -1; } return g_vbLog; }
int  vbCopy(const VX& x) { VX c(x); return c.v * 1000 + c.l * 100 + c.r * 10 + c.x; }
long vbArray(int n)
{
    g_vbLog = 0;
    VX* a = new VX[n];
    long s = 0;
    for (int i = 0; i < n; ++i) s += a[i].v + a[i].l;
    long ok = g_vbLog == 12341234 && s == 18;
    g_vbLog = 0;
    delete[] a;
    return ok * 10000 + g_vbLog;
}
int  vbSumVY(const VY& y) { return y.v + y.l + y.r + y.x + y.y; }
int  vbSumVZ(const VZ& z) { const VR& r = z; return z.v * 1000 + r.r * 100 + z.z * 10 + z.x; }
int  vbNestedF(const VNF& f) { const VNE& e = f; const VNB& b = e; return f.a * 10000 + b.b * 1000 + f.c * 100 + e.e * 10 + f.f; }
int  vbNestedG(VNG* g) { VNE* e = g; VNC* c = g; VNB* b = c; return g->a * 10000 + b->b * 1000 + c->c * 100 + e->e * 10 + g->g; }
VNA* vbToVNA(VNG* g) { VNC* c = g; return c; }
long vbNestedLife() { g_vbLog = 0; long ok = 0; { VNF f; VNG g; ok = f.a + g.a == 2 && g_vbLog == 12351235; g_vbLog = 0; } return ok * 10000 + g_vbLog; }

long g_pvLog = 0;
PV::PV() : v(7) { g_pvLog = g_pvLog * 10 + (probe() == 701 ? 1 : 0); }
PV::~PV() { g_pvLog = g_pvLog * 10 + (probe() == 701 ? 9 : 0); }
int PV::f() const { return v; }
int PV::g() const { return 1; }
int PV::probe() const { return f() * 100 + g(); }
PVL::PVL() : l(2) { g_pvLog = g_pvLog * 10 + (probe() == 1201 ? 2 : 0); }
PVL::~PVL() { g_pvLog = g_pvLog * 10 + (probe() == 1201 ? 6 : 0); }
int PVL::f() const { return l + 10; }
PVR::PVR() : r(3) { g_pvLog = g_pvLog * 10 + (probe() == 701 ? 3 : 0); }
PVD::PVD() : d(4) { g_pvLog = g_pvLog * 10 + (probe() == 1224 ? 4 : 0); }
PVD::~PVD() { g_pvLog = g_pvLog * 10 + (probe() == 1224 ? 5 : 0); }
int PVD::g() const { return d + 20; }
PNB::PNB() : nb(4) {}
int PNB::nf() const { return nb; }
PVX::PVX() : x(6) { g_pvLog = g_pvLog * 10 + (probe() == 4601 ? 4 : 0); }
int PVX::f() const { return x + 40; }
int PVX::nf() const { return x + 1; }
PVN::PVN() : n(5) {}
int PVN::h() const { return 3; }
PVN2::PVN2() : n2(8) {}
int PVN2::h() const { return n2; }
int PVN2::g() const { return n2 + 1; }
int  pvCall(const PV* p) { return p->f() * 100 + p->g(); }
int  pvCallL(const PVL* p) { return p->f() * 100 + p->g(); }
int  pvCallD(const PVD* p)
{
    const PVL* l = p; const PVR* r = p; const PV* v = p;
    int a = p->f() * 100 + p->g();
    return a == l->f() * 100 + l->g() && a == r->f() * 100 + r->g() && a == v->f() * 100 + v->g() ? a : 0;
}
int  pvCallX(const PVX* p) { return p->f() * 100 + p->nf(); }
int  pvCallN(const PVN* p) { return p->f() * 100 + p->g() + p->h() * 10000; }
PV*  pvMakeD() { return new PVD; }
PV*  pvMakeX() { return new PVX; }
PVN* pvMakeN2() { return new PVN2; }
void pvDelete(PV* p) { delete p; }
long pvLife() { g_pvLog = 0; long built; { PVD d; built = g_pvLog; g_pvLog = 0; } return built * 1000 + g_pvLog; }
int  pvCopy(const PVD& d) { PVD c(d); return c.probe(); }
int  pvMemptr(const PVD& d, int PVL::* pm, int (PV::*pf)() const) { return d.*pm * 10000 + (d.*pf)(); }

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

long g_trNew = 0, g_trDel = 0;
void* TR::operator new[](size_t n) { g_trNew = (long)n; return malloc(n); }
void TR::operator delete[](void* p) { g_trDel = 1; free(p); }
void TQ::operator delete[](void* p, size_t n) { g_trDel = (long)n; free(p); }
void TS::operator delete(void* p) { g_trDel = 1; free(p); }
TR* makeTRArray(int n) { return new TR[n]; }
TQ* makeTQArray(int n) { return new TQ[n]; }
void deleteTRArray(TR* p) { delete[] p; }
void deleteTQArray(TQ* p) { delete[] p; }
void deleteTS(TS* p) { delete p; }

long g_gNewA = 0, g_gDelA = 0;
void* operator new[](size_t n) { g_gNewA += (long)n; return malloc(n); }
void operator delete[](void* p) { if (p) ++g_gDelA; free(p); }
GQ::~GQ() {}
GC::GC() : c(1) {}
GC::~GC() {}
int* makeInt(int v) { return new int(v); }
int* makeInts(int n) { return new int[n]; }
void deleteInt(int* p) { delete p; }
void deleteInts(int* p) { delete[] p; }
GQ* makeGQ() { return new GQ; }
void deleteGQ(GQ* p) { delete p; }
GC* makeGCArray(int n) { return new GC[n]; }
void deleteGCArray(GC* p) { delete[] p; }
int ZG::f(char) const { return 4; }
int ZG::g() const { return 1; }
int ZG::f(int) const { return 2; }
int ZG::f(double) const { return 3; }
int zgCall(const ZG* p, int which)
{
    switch (which) {
    case 0: return p->g();
    case 1: return p->f(1);
    case 2: return p->f(1.0);
    case 3: return p->f('c');
    }
    return -1;
}
int YA::f(int) const { return 11; }
int YA::f(double) const { return 12; }
int YB::f(double) const { return 22; }
int YC::f(int) const { return 31; }
int YC::k() const { return 33; }
int yaCall(const YA* p, int which) { return which == 0 ? p->f(1) : p->f(1.0); }
int ycCall(const YC* p, int which) { return which == 0 ? p->f(1) : p->k(); }
YA* makeYC() { return new YC; }
int g_dnForm = 0;
DN::DN() : d(1) {}
DN::~DN() {}
void DN::operator delete[](void* p) { g_dnForm = 1; free(p); }
void DN::operator delete[](void* p, size_t) { g_dnForm = 2; free(p); }
DN* makeDNArray(int n) { return new DN[n]; }
void deleteDNArray(DN* p) { delete[] p; }

RA::RA() : ra(1) {}
RA::~RA() {}
RB::RB() : rb(2) {}
RB::~RB() {}
RB* RB::self() { return this; }
RB& RB::selfRef() { return *this; }
RB* RB::six(int p, int q, int r, int s, double t, int u) { return p + q + r + s + (int)t + u == 21 ? this : 0; }
RC::RC() : c(3) {}
RC* RC::self() { return this; }
RC& RC::selfRef() { return *this; }
RC* RC::six(int p, int q, int r, int s, double t, int u) { return p + q + r + s + (int)t + u == 21 ? this : 0; }
int RC::rc() const { return c; }
RB* rbSelf(RB* p) { return p->self(); }
RC* rcSelf(RC* p) { return p->self(); }
RB& rbSelfRef(RB& p) { return p.selfRef(); }
RB* rbSix(RB* p, int u) { return p->six(1, 2, 3, 4, 5.0, u); }
RC* rcSix(RC* p, int u) { return p->six(1, 2, 3, 4, 5.0, u); }

MP::MP() : a(1), b(2) {}
int MP::f() const { return a * 10; }
int MP::h() const { return b * 10; }
MM::MM() : m(3) {}
int MM::mf() const { return m * 100; }
MQ::MQ() : q(4) {}
int MQ::f() const { return q * 10; }
int MP::* g_mpB = &MP::b;
int MP::* g_mpNull = 0;
int (MP::*g_mpF)() const = &MP::f;
int MQ::* g_mqM = &MM::m;
int (MQ::*g_mqF)() const = &MP::f;
int (MQ::*g_mqMf)() const = &MM::mf;
int VL::* g_vlV = &VB::v;
int (VL::*g_vlGet)() const = &VB::get;
int MP::* g_mpArr[3] = { &MP::a, 0 };
MH g_mh;
int mpRead(const MP* p, int MP::* m) { return p->*m; }
int mpCall(const MP* p, int (MP::*f)() const) { return (p->*f)(); }
int mpReadQ(const MQ& q, int MQ::* m) { return q.*m; }
int mpCallQ(const MQ& q, int (MQ::*f)() const) { return (q.*f)(); }
int mpReadVL(const VL* p, int VL::* m) { return p->*m; }
int mpCallVL(const VL* p, int (VL::*f)() const) { return (p->*f)(); }
int MP::* mpOfB() { return &MP::b; }
int (MQ::*mqOfMf())() const { return &MM::mf; }
int VL::* vlOfV() { return &VB::v; }
int (VL::*vlOfGet())() const { return &VB::get; }
int mpIsNull(int MP::* m, int (MQ::*f)() const, int VL::* v, int (VL::*g)() const) { return (m == 0 ? 1 : 0) + (f == 0 ? 2 : 0) + (!v ? 4 : 0) + (g ? 0 : 8); }
int mpEqual(int (MQ::*f)() const, int (MQ::*g)() const, int VL::* v, int VL::* w) { return (f == g ? 1 : 0) + (v == w ? 2 : 0); }
int mpViaCl(const MP* p, const MQ& q) { return clMpRead(p, &MP::b) + clMpCallQ(q, &MM::mf); }
