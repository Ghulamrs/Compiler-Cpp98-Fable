// vbase-first.cpp - a class that names a virtual base before the polymorphic base whose function it overrides
// (row G45): J2 : virtual VR, P1 { int r(); } overrides P1::r in the table at offset 0, which is P1's, as well as
// VR::r in the table inside the virtual base, and is reached with this at the object (cl's this adjustor 0); the
// slot of VR's table holds an adjustor thunk. Before the row the slot of P1's table was left to P1::r whenever
// the function was found first in a base named before the primary base: a call through P1* reached P1::r.
// Test 17 compares the output with cl's build of this file, at -O0 and -O2 in the three exception models, and
// every vftable of the -O0 listing, slot for slot, with the tables of cl's listing (those of namespace covm
// apart, see there).
// The groups: s1 to s10 the shapes of the classes (several virtual bases, a virtual base without virtual
// functions, the function in one base only or in a base of a base, further derived classes, a base whose only
// vfptr is its virtual base's, two polymorphic bases, new virtual functions, overloads and const); r1 to r5 pure
// virtual functions, a plain base beside, the shape at two levels, a diamond, class templates; x1 functions of
// the primary base that have the name and are not overridden (they stay in its table); d1 to d5
// destructors and the calls made while an object is built and destroyed (the tables of the partially built
// object, the vtordisp); c1, c2 and covm covariant returns; m1 pointers to member functions; t1 dynamic_cast and
// typeid; e1 a throw that crosses the call.
// Strict C++98.
#include <cstdio>
#include <typeinfo>

int g_log[64]; int g_n = 0;
void note(int v) { if (g_n < 64) g_log[g_n++] = v; }
void dump(const char* what) { std::printf("%s", what); for (int i = 0; i < g_n; ++i) std::printf(" %d", g_log[i]); std::printf("\n"); g_n = 0; }

namespace s1 {   // the row's shape
struct VR { virtual int r(); int vr; };
struct P1 { virtual int r(); int p1; };
struct J2 : virtual VR, P1 { int r(); int j; };
int VR::r() { return 1; }
int P1::r() { return 2; }
int J2::r() { return 3; }
void run() { J2 a; P1* p = &a; VR* v = &a; J2* j = &a; P1& pr = a; VR& vr = a; J2& jr = a;
    std::printf("vbf s1 %d %d %d %d %d %d %d\n", p->r(), v->r(), j->r(), pr.r(), vr.r(), jr.r(), (int)sizeof(J2)); }
}
namespace s2 {   // several virtual bases first, the function in all of them
struct VA { virtual int r(); int va; };
struct VB { virtual int r(); virtual int q(); int vb; };
struct P1 { virtual int p(); virtual int r(); int p1; };
struct K : virtual VA, virtual VB, P1 { int r(); int k; };
int VA::r() { return 1; }
int VB::r() { return 2; }
int VB::q() { return 20; }
int P1::p() { return 30; }
int P1::r() { return 3; }
int K::r() { return 4; }
void run() { K a; P1* p = &a; VA* va = &a; VB* vb = &a; K* k = &a;
    std::printf("vbf s2 %d %d %d %d %d %d %d\n", p->r(), va->r(), vb->r(), k->r(), vb->q(), p->p(), (int)sizeof(K)); }
}
namespace s3 {   // a virtual base without virtual functions first
struct NV { int nv; };
struct P1 { virtual int r(); int p1; };
struct L : virtual NV, P1 { int r(); int l; };
int P1::r() { return 2; }
int L::r() { return 3; }
void run() { L a; P1* p = &a; L* l = &a; NV* n = &a; n->nv = 7;
    std::printf("vbf s3 %d %d %d %d\n", p->r(), l->r(), a.nv, (int)sizeof(L)); }
}
namespace s4 {   // the function only in the non-virtual base, only in the virtual one, one of each
struct VQ { virtual int q(); int vq; };
struct P1 { virtual int r(); int p1; };
struct M : virtual VQ, P1 { int r(); int m; };
struct N : virtual VQ, P1 { int q(); int n; };
struct MN : virtual VQ, P1 { int q(); int r(); int n; };
int VQ::q() { return 1; }
int P1::r() { return 2; }
int M::r() { return 3; }
int N::q() { return 4; }
int MN::q() { return 5; }
int MN::r() { return 6; }
void run() { M a; N b; MN c; P1* p = &a; VQ* v = &a; P1* p2 = &b; VQ* v2 = &b; P1* p3 = &c; VQ* v3 = &c;
    std::printf("vbf s4 %d %d %d %d %d %d %d %d %d %d\n", p->r(), v->q(), a.r(), a.q(), p2->r(), v2->q(), b.q(), b.r(), p3->r(), v3->q()); }
}
namespace s5 {   // the function in a base of the non-virtual base; the non-virtual base an overrider itself
struct VR { virtual int r(); int vr; };
struct PB0 { virtual int r(); int pb0; };
struct P2 : PB0 { int p2; };
struct P3 : PB0 { int r(); int p3; };
struct O : virtual VR, P2 { int r(); int o; };
struct O3 : virtual VR, P3 { int r(); int o; };
int VR::r() { return 1; }
int PB0::r() { return 2; }
int P3::r() { return 3; }
int O::r() { return 4; }
int O3::r() { return 5; }
void run() { O a; O3 b; PB0* p0 = &a; P2* p2 = &a; VR* v = &a; PB0* q0 = &b; P3* q3 = &b; VR* w = &b;
    std::printf("vbf s5 %d %d %d %d %d %d %d %d\n", p0->r(), p2->r(), v->r(), a.r(), q0->r(), q3->r(), w->r(), b.r()); }
}
namespace s6 {   // the function in a base of the virtual base; the virtual base an overrider itself
struct VR0 { virtual int r(); int vr0; };
struct VR2 : VR0 { int vr2; };
struct VR3 : VR0 { int r(); int vr3; };
struct P1 { virtual int r(); int p1; };
struct Q : virtual VR2, P1 { int r(); int q; };
struct Q3 : virtual VR3, P1 { int r(); int q; };
int VR0::r() { return 1; }
int VR3::r() { return 2; }
int P1::r() { return 3; }
int Q::r() { return 4; }
int Q3::r() { return 5; }
void run() { Q a; Q3 b; P1* p = &a; VR0* v0 = &a; VR2* v2 = &a; P1* p3 = &b; VR0* w0 = &b; VR3* w3 = &b;
    std::printf("vbf s6 %d %d %d %d %d %d %d %d\n", p->r(), v0->r(), v2->r(), a.r(), p3->r(), w0->r(), w3->r(), b.r()); }
}
namespace s7 {   // further derived classes: overriding again, not overriding, naming the bases again, virtually
struct VR { virtual int r(); int vr; };
struct P1 { virtual int r(); int p1; };
struct J2 : virtual VR, P1 { int r(); int j; };
struct JJ : J2 { int r(); int jj; };
struct JN : J2 { int jn; };
struct JK : virtual VR, J2 { int r(); int jk; };
struct P9 { virtual int r(); int p9; };
struct JM : P9, J2 { int r(); int jm; };
struct JV : virtual J2 { int r(); int jv; };
struct JW : virtual VR, virtual J2, P9 { int r(); int jw; };
int VR::r() { return 1; }
int P1::r() { return 2; }
int J2::r() { return 3; }
int JJ::r() { return 4; }
int JK::r() { return 5; }
int P9::r() { return 6; }
int JM::r() { return 7; }
int JV::r() { return 8; }
int JW::r() { return 9; }
template <class T> void show(const char* n, T& a) { P1* p = &a; VR* v = &a; J2* j = &a; std::printf("vbf s7 %s %d %d %d %d %d\n", n, p->r(), v->r(), j->r(), a.r(), (int)sizeof(T)); }
void run() { JJ a; JN b; JK c; JM d; JV e; JW f; show("JJ", a); show("JN", b); show("JK", c); show("JM", d); show("JV", e); show("JW", f);
    P9* p9 = &d; P9* q9 = &f; std::printf("vbf s7 %d %d\n", p9->r(), q9->r()); }
}
namespace s8 {   // a non-virtual base whose only vfptr lies in its virtual base, named before the polymorphic base
struct VR { virtual int r(); int vr; };
struct VL : virtual VR { int l; };
struct VLo : virtual VR { int r(); int l; };
struct P1 { virtual int r(); int p1; };
struct W : VL, P1 { int r(); int w; };
struct Wo : VLo, P1 { int r(); int w; };
int VR::r() { return 1; }
int VLo::r() { return 2; }
int P1::r() { return 3; }
int W::r() { return 4; }
int Wo::r() { return 5; }
void run() { W a; Wo b; P1* p = &a; VR* v = &a; VL* l = &a; P1* p2 = &b; VR* v2 = &b; VLo* l2 = &b;
    std::printf("vbf s8 %d %d %d %d %d %d %d %d %d %d\n", p->r(), v->r(), l->r(), a.r(), p2->r(), v2->r(), l2->r(), b.r(), (int)sizeof(W), (int)sizeof(Wo)); }
}
namespace s9 {   // two polymorphic non-virtual bases after the virtual one; new virtual functions of the class
struct VR { virtual int r(); int vr; };
struct P1 { virtual int r(); int p1; };
struct P2 { virtual int z(); virtual int r(); int p2; };
struct X : virtual VR, P1, P2 { int r(); int x; };
struct Y : virtual VR, P1 { virtual int y(); int r(); virtual int y2(); int yy; };
struct Z : virtual VR, P1, P2 { virtual int y(); int z(); int r(); int zz; };
int VR::r() { return 1; }
int P1::r() { return 2; }
int P2::z() { return 30; }
int P2::r() { return 3; }
int X::r() { return 4; }
int Y::y() { return 50; }
int Y::r() { return 5; }
int Y::y2() { return 51; }
int Z::y() { return 60; }
int Z::z() { return 61; }
int Z::r() { return 6; }
void run() { X a; Y b; Z c; P1* p = &a; P2* p2 = &a; VR* v = &a; P1* q = &b; VR* w = &b; P1* t = &c; P2* t2 = &c; VR* tv = &c;
    std::printf("vbf s9 %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", p->r(), p2->r(), v->r(), a.r(), p2->z(), q->r(), w->r(), b.r(), b.y(), b.y2(), t->r(), t2->r(), tv->r(), t2->z(), c.y()); }
}
namespace s10 {   // overloads and const: only the function of the same signature is overridden, in both tables
struct VR { virtual int r() const; virtual int r(int); int vr; };
struct P1 { virtual int r(); virtual int r(int); virtual int r() const; int p1; };
struct A : virtual VR, P1 { int r(int); int a; };
struct B : virtual VR, P1 { int r() const; int b; };
struct C : virtual VR, P1 { int r(); int c; };
int VR::r() const { return 1; }
int VR::r(int) { return 2; }
int P1::r() { return 3; }
int P1::r(int) { return 4; }
int P1::r() const { return 5; }
int A::r(int) { return 6; }
int B::r() const { return 7; }
int C::r() { return 8; }
void run() { A a; B b; C c; P1* p = &a; VR* v = &a; const P1* cp = &a; const VR* cv = &a;
    std::printf("vbf s10 %d %d %d %d %d %d\n", p->r(), p->r(1), cp->r(), v->r(1), cv->r(), a.r(1));
    p = &b; v = &b; cp = &b; cv = &b; std::printf("vbf s10 %d %d %d %d %d\n", p->r(), p->r(1), cp->r(), v->r(1), cv->r());
    p = &c; v = &c; cp = &c; cv = &c; std::printf("vbf s10 %d %d %d %d %d %d\n", p->r(), p->r(1), cp->r(), v->r(1), cv->r(), c.r()); }
}
namespace r1 {   // pure in the bases: the class that overrides is not abstract (it was refused, S151)
struct VR { virtual int r() = 0; int vr; };
struct P1 { virtual int r() = 0; int p1; };
struct VC { virtual int r(); int vr; };
struct J2 : virtual VR, P1 { int r(); int j; };
struct J3 : virtual VC, P1 { int r(); int j; };
struct J4 : virtual VR, P1 { int j; };          // abstract
struct J5 : J4 { int r(); };
int VC::r() { return 1; }
int J2::r() { return 3; }
int J3::r() { return 4; }
int J5::r() { return 5; }
void run() { J2 a; J3 b; J5 c; P1* p = &a; VR* v = &a; P1* q = &b; VC* w = &b; P1* t = &c; VR* u = &c; J4* f = &c;
    std::printf("vbf r1 %d %d %d %d %d %d %d\n", p->r(), v->r(), q->r(), w->r(), t->r(), u->r(), static_cast<P1*>(f)->r()); }
}
namespace r2 {   // a base without virtual functions beside the polymorphic one
struct NP { int np; };
struct VR { virtual int r(); int vr; };
struct P1 { virtual int r(); int p1; };
struct D : virtual VR, P1, NP { int r(); int d; };
struct D3 : virtual VR, P1, NP { virtual int n(); int r(); int d; };
int VR::r() { return 1; }
int P1::r() { return 2; }
int D::r() { return 3; }
int D3::n() { return 50; }
int D3::r() { return 5; }
void run() { D a; D3 c; a.np = 7; P1* p = &a; VR* v = &a; NP* n = &a; P1* t = &c; VR* u = &c;
    std::printf("vbf r2 %d %d %d %d %d %d %d %d\n", p->r(), v->r(), n->np, t->r(), u->r(), c.n(), (int)sizeof(D), (int)sizeof(D3)); }
}
namespace r3 {   // the shape at two levels
struct VR { virtual int r(); int vr; };
struct VS { virtual int r(); virtual int s(); int vs; };
struct P1 { virtual int r(); int p1; };
struct A : virtual VR, P1 { int r(); int a; };
struct B : virtual VS, A { int r(); int b; };
struct B2 : virtual VS, A { int s(); int b; };
struct C : virtual VS, virtual VR, B { int r(); int s(); int c; };
int VR::r() { return 1; }
int VS::r() { return 2; }
int VS::s() { return 20; }
int P1::r() { return 3; }
int A::r() { return 4; }
int B::r() { return 5; }
int B2::s() { return 60; }
int C::r() { return 7; }
int C::s() { return 70; }
void run() { B b; B2 b2; C c; P1* p = &b; VR* v = &b; VS* s = &b; A* a = &b;
    std::printf("vbf r3 %d %d %d %d %d %d\n", p->r(), v->r(), s->r(), a->r(), s->s(), (int)sizeof(B));
    p = &b2; v = &b2; a = &b2; VS* s2 = &b2; std::printf("vbf r3 %d %d %d %d %d %d\n", p->r(), v->r(), s2->r(), a->r(), s2->s(), (int)sizeof(B2));
    p = &c; v = &c; s = &c; a = &c; B* bb = &c; std::printf("vbf r3 %d %d %d %d %d %d %d\n", p->r(), v->r(), s->r(), a->r(), s->s(), bb->r(), (int)sizeof(C)); }
}
namespace r4 {   // a diamond of two such classes
struct VR { virtual int r(); int vr; };
struct P1 { virtual int r(); int p1; };
struct P2 { virtual int r(); int p2; };
struct L : virtual VR, P1 { int r(); int l; };
struct R : virtual VR, P2 { int r(); int rr; };
struct D : L, R { int r(); int d; };
struct E : R, L { int r(); int e; };
int VR::r() { return 1; }
int P1::r() { return 2; }
int P2::r() { return 3; }
int L::r() { return 4; }
int R::r() { return 5; }
int D::r() { return 6; }
int E::r() { return 7; }
void run() { D d; E e; P1* p = &d; P2* q = &d; VR* v = &d; L* l = &d; R* r = &d;
    std::printf("vbf r4 %d %d %d %d %d %d\n", p->r(), q->r(), v->r(), l->r(), r->r(), (int)sizeof(D));
    p = &e; q = &e; v = &e; l = &e; r = &e; std::printf("vbf r4 %d %d %d %d %d %d\n", p->r(), q->r(), v->r(), l->r(), r->r(), (int)sizeof(E)); }
}
namespace r5 {   // class templates
struct VR { virtual int r(); int vr; };
struct P1 { virtual int r(); int p1; };
template <class V, class P> struct TJ : virtual V, P { int r() { return 3 + k; } int k; TJ() : k(0) {} };
template <class T> struct TB { virtual T r() { return 8; } T t; };
template <class T> struct TD : virtual VR, TB<T> { T r() { return 9; } };
int VR::r() { return 1; }
int P1::r() { return 2; }
void run() { TJ<VR, P1> a; P1* p = &a; VR* v = &a; TD<int> d; TB<int>* tb = &d; VR* dv = &d;
    std::printf("vbf r5 %d %d %d %d %d\n", p->r(), v->r(), a.r(), tb->r(), dv->r()); }
}
namespace x1 {   // not overridden: a function of the primary base that takes another enumeration or another class
                 // by value, a top-level const on either, and a conversion function to another type that is written
                 // with the same name (the override is the virtual base's function alone; P1's and PT's tables keep theirs)
enum E1 { e1 }; enum E2 { e2 };
struct CA { int a; }; struct CB { int b; };
struct VR { virtual int g(const E1) { return 1; } virtual int f(CA) { return 10; } int vr; };
struct P1 { virtual int g(const E2) { return 2; } virtual int f(const CB) { return 20; } int p1; };
struct J : virtual VR, P1 { int g(const E1) { return 3; } int f(const CA) { return 30; } int j; };
struct VT { typedef int T; virtual operator T() { return 1; } int vt; };
struct PT { typedef long T; virtual operator T() { return 2; } int pt; };
struct JT : virtual VT, PT { typedef int T; operator T() { return 3; } int jt; };
void run() { J a; VR* v = &a; P1* p = &a; CA ca; CB cb; ca.a = 0; cb.b = 0;
    JT t; VT* vt = &t; PT* pt = &t;
    std::printf("vbf x1 %d %d %d %d %d %d\n", v->g(e1), p->g(e2), v->f(ca), p->f(cb), (int)*vt, (int)(long)*pt); }
}
namespace d1 {   // virtual destructors in both bases, an object deleted through each
struct VR { VR() : vr(1) { note(10); } virtual ~VR() { note(11); } virtual int r() { return 1; } int vr; };
struct P1 { P1() : p1(2) { note(20); } virtual ~P1() { note(21); } virtual int r() { return 2; } int p1; };
struct J2 : virtual VR, P1 { J2() : j(3) { note(30); } ~J2() { note(31); } int r() { return 3; } int j; };
struct JJ : J2 { JJ() : jj(4) { note(40); } ~JJ() { note(41); } int r() { return 4; } int jj; };
void run() {
    { P1* p = new J2; note(p->r()); delete p; } dump("vbf d1 a");
    { VR* v = new J2; note(v->r()); delete v; } dump("vbf d1 b");
    { J2* j = new J2; note(j->r()); delete j; } dump("vbf d1 c");
    { P1* p = new JJ; note(p->r()); delete p; } dump("vbf d1 d");
    { VR* v = new JJ; note(v->r()); delete v; } dump("vbf d1 e");
    { J2* j = new JJ; note(j->r()); delete j; } dump("vbf d1 f");
    { J2 a; JJ b; P1& p = a; P1& q = b; note(p.r() * 10 + q.r()); } dump("vbf d1 g");
    std::printf("vbf d1 %d %d\n", (int)sizeof(J2), (int)sizeof(JJ));
}
}
namespace d2 {   // a virtual destructor in one of the bases only, or none declared in the class
struct VR { virtual ~VR() { note(11); } virtual int r() { return 1; } int vr; };
struct P1 { virtual int r() { return 2; } int p1; };
struct J2 : virtual VR, P1 { ~J2() { note(31); } int r() { return 3; } int j; };
struct VS { virtual int r() { return 1; } int vr; };
struct P2 { virtual ~P2() { note(21); } virtual int r() { return 2; } int p1; };
struct K2 : virtual VS, P2 { ~K2() { note(32); } int r() { return 3; } int j; };
struct K3 : virtual VS, P2 { int r() { return 5; } int j; };
void run() {
    { VR* v = new J2; note(v->r()); delete v; } dump("vbf d2 a");
    { J2* j = new J2; P1* p = j; note(p->r()); delete j; } dump("vbf d2 b");
    { P2* p = new K2; note(p->r()); delete p; } dump("vbf d2 c");
    { K2* k = new K2; VS* v = k; note(v->r()); delete k; } dump("vbf d2 d");
    { P2* p = new K3; note(p->r()); delete p; } dump("vbf d2 e");
    std::printf("vbf d2 %d %d %d\n", (int)sizeof(J2), (int)sizeof(K2), (int)sizeof(K3));
}
}
namespace d3 {   // calls while the object is built and destroyed: every constructor and destructor, through each base
struct VR; struct P1;
void viaV(VR* v); void viaP(P1* p);
struct VR { VR() { viaV(this); } virtual ~VR() { viaV(this); } virtual int r() { return 1; } int vr; };
struct P1 { P1() { viaP(this); } virtual ~P1() { viaP(this); } virtual int r() { return 2; } int p1; };
void viaV(VR* v) { note(v->r()); }
void viaP(P1* p) { note(p->r()); }
struct J2 : virtual VR, P1 { J2() : j(3) { viaV(this); viaP(this); note(r()); } ~J2() { viaV(this); viaP(this); note(r()); } int r() { return 3; } int j; };
struct JJ : J2 { JJ() : jj(4) { viaV(this); viaP(this); } ~JJ() { viaV(this); viaP(this); } int r() { return 4; } int jj; };
struct JN : J2 { JN() : jn(5) { viaV(this); viaP(this); } ~JN() { viaV(this); viaP(this); } int jn; };
struct JE : virtual VR, J2 { double e; JE() : e(6) { viaV(this); viaP(this); } ~JE() { viaV(this); viaP(this); } int r() { return 6; } };
struct OV { OV() : o(8) {} virtual ~OV() {} virtual int o1() { return 81; } int o; };
struct JO : OV, JN { JO() { viaV(this); viaP(this); } ~JO() { viaV(this); viaP(this); } int r() { return 9; } };
void run() {
    { J2 a; } dump("vbf d3 J2");
    { JJ a; } dump("vbf d3 JJ");
    { JN a; } dump("vbf d3 JN");
    { JE a; } dump("vbf d3 JE");
    { JO a; } dump("vbf d3 JO");
    { P1* p = new JJ; delete p; } dump("vbf d3 pJJ");
    { VR* v = new JO; delete v; } dump("vbf d3 vJO");
    std::printf("vbf d3 %d %d %d %d %d\n", (int)sizeof(J2), (int)sizeof(JJ), (int)sizeof(JN), (int)sizeof(JE), (int)sizeof(JO));
}
}
namespace d4 {   // no constructor in the class of the shape (no vtordisp), one in the classes derived from it
struct VR { virtual int r() { return 1; } int vr; };
struct P1 { virtual int r() { return 2; } int p1; };
void viaV(VR* v) { note(v->r()); }
void viaP(P1* p) { note(p->r()); }
struct J2 : virtual VR, P1 { int r() { return 3; } int j; };
struct JC : J2 { JC() { viaV(this); viaP(this); } ~JC() { viaV(this); viaP(this); } int r() { return 4; } int c; };
struct JD : JC { JD() { viaV(this); viaP(this); } int d; };
void run() {
    { JC a; } dump("vbf d4 JC");
    { JD a; } dump("vbf d4 JD");
    std::printf("vbf d4 %d %d %d\n", (int)sizeof(J2), (int)sizeof(JC), (int)sizeof(JD));
}
}
namespace d5 {   // the base whose only vfptr is its virtual base's, with constructors and destructors
struct VR { VR() { note(r()); } virtual ~VR() { note(r()); } virtual int r() { return 1; } int vr; };
struct VL : virtual VR { VL() { note(r()); } ~VL() { note(r()); } int r() { return 2; } int l; };
struct P1 { P1() { note(r()); } virtual ~P1() { note(r()); } virtual int r() { return 3; } int p1; };
struct W : VL, P1 { W() { VR* v = this; P1* p = this; VL* l = this; note(v->r()); note(p->r()); note(l->r()); } ~W() { VR* v = this; P1* p = this; note(v->r()); note(p->r()); } int r() { return 4; } int w; };
struct WW : W { WW() { VR* v = this; P1* p = this; note(v->r()); note(p->r()); } int r() { return 5; } int ww; };
void run() {
    { W a; } dump("vbf d5 W");
    { WW a; } dump("vbf d5 WW");
    { P1* p = new WW; delete p; } dump("vbf d5 pWW");
    { VR* v = new WW; delete v; } dump("vbf d5 vWW");
    { VL* l = new W; delete l; } dump("vbf d5 lW");
    std::printf("vbf d5 %d %d\n", (int)sizeof(W), (int)sizeof(WW));
}
}
namespace c1 {   // a covariant return whose result does not move (X is where Y starts): overridden in place
struct X0 { int x0; X0() : x0(1) {} };
struct X { int x; X() : x(2) {} virtual ~X() {} };
struct Y : X0, X { int y; Y() : y(3) {} };
struct VR { virtual X* get(); virtual int r(); int vr; };
struct P1 { virtual X* get(); virtual int r(); int p1; };
struct J2 : virtual VR, P1 { Y yy; Y* get(); int r(); int j; };
struct JJ : J2 { Y zz; Y* get(); int jj; };
X gx;
X* VR::get() { return &gx; }
int VR::r() { return 1; }
X* P1::get() { return &gx; }
int P1::r() { return 2; }
Y* J2::get() { return &yy; }
int J2::r() { return 3; }
Y* JJ::get() { zz.y = 9; return &zz; }
void run() {
    J2 a; P1* p = &a; VR* v = &a; J2* j = &a;
    X* xp = p->get(); X* xv = v->get(); Y* yj = j->get();
    std::printf("vbf c1 %d %d %d %d %d %d %d %d\n", xp->x, xv->x, yj->y, xp == static_cast<X*>(yj), xv == xp, p->r(), v->r(), j->r());
    JJ b; p = &b; v = &b; j = &b;
    xp = p->get(); xv = v->get(); yj = j->get();
    std::printf("vbf c1 %d %d %d %d %d %d %d %d\n", xp->x, xv->x, yj->y, xp == static_cast<X*>(yj), xv == xp, p->r(), v->r(), j->r());
}
}
namespace c2 {   // covariant to the classes themselves: J2* where P1* is returned, P1 at offset 0
struct P1 { virtual P1* self(); virtual int r(); int p1; };
struct NV { int nv; };
struct VQ { virtual int q(); int vq; };
struct J2 : virtual VQ, P1 { J2* self(); int r(); int q(); int j; };
struct J3 : virtual NV, virtual VQ, P1 { J3* self(); int j; };
P1* P1::self() { return this; }
int P1::r() { return 2; }
int VQ::q() { return 1; }
J2* J2::self() { return this; }
int J2::r() { return 3; }
int J2::q() { return 4; }
J3* J3::self() { return this; }
void run() {
    J2 a; P1* p = &a; VQ* v = &a;
    std::printf("vbf c2 %d %d %d %d %d\n", p->self() == p, a.self() == &a, p->self()->r(), v->q(), a.self()->q());
    J3 b; p = &b; v = &b;
    std::printf("vbf c2 %d %d %d %d\n", p->self() == p, b.self() == &b, p->self()->r(), v->q());
}
}
namespace covm {   // a covariant return whose result moves (X at 4 in Y): return thunks in both tables. The output is
                   // compared; the tables are not: cl appends the overrider's own slot to the virtual base's table as
                   // well, cppparse to the table at 0 only, whatever the order of the bases (next-targets, row G47)
struct X0 { int x0; X0() : x0(1) {} };
struct X { int x; X() : x(2) {} };
struct Y : X0, X { int y; Y() : y(3) {} };
X gx; Y gy;
struct A { virtual X* g() { return &gx; } virtual int k() { return 1; } int a; };
struct VR { virtual X* g() { return &gx; } int vr; };
struct J : virtual VR, A { Y* g() { return &gy; } int j; };
void run() {
    J j; A* a = &j; VR* v = &j; X* x1 = a->g(); X* x2 = v->g(); J* jp = &j;
    std::printf("vbf covm %d %d %d %d %d %d\n", x1->x, x2->x, x1 == x2, x1 == static_cast<X*>(&gy), jp->g()->y, a->k());
}
}
namespace m1 {   // pointers to member functions
struct VR { virtual int r(); virtual int v2(); int vr; };
struct P1 { virtual int r(); virtual int p2(); int p1; };
struct J2 : virtual VR, P1 { int r(); int own(); virtual int nv(); int j; };
struct JJ : J2 { int r(); int p2(); int v2(); int jj; };
int VR::r() { return 1; }
int VR::v2() { return 10; }
int P1::r() { return 2; }
int P1::p2() { return 20; }
int J2::r() { return 3; }
int J2::own() { return 30 + j; }
int J2::nv() { return 31; }
int JJ::r() { return 4; }
int JJ::p2() { return 40; }
int JJ::v2() { return 41; }
int callP(P1* p, int (P1::*f)()) { return (p->*f)(); }
int callV(VR* p, int (VR::*f)()) { return (p->*f)(); }
int callJ(J2* p, int (J2::*f)()) { return (p->*f)(); }
int (J2::*g_pm)() = &J2::r;
void run() {
    J2 a; a.j = 5; JJ b; b.j = 6;
    int (J2::*jr)() = &J2::r; int (J2::*jp)() = &J2::p2; int (J2::*jv)() = &J2::v2; int (J2::*jo)() = &J2::own; int (J2::*jn)() = &J2::nv;
    int (J2::*up)() = &P1::r;                    // P1's member as a member of J2
    int (J2::*uv)() = static_cast<int (J2::*)()>(&P1::p2);
    std::printf("vbf m1 %d %d %d %d %d %d %d %d\n", callP(&a, &P1::r), callP(&a, &P1::p2), callV(&a, &VR::r), callV(&a, &VR::v2), callJ(&a, jr), callJ(&a, jp), callJ(&a, jv), callJ(&a, jo));
    std::printf("vbf m1 %d %d %d %d %d %d %d %d\n", callP(&b, &P1::r), callP(&b, &P1::p2), callV(&b, &VR::r), callV(&b, &VR::v2), callJ(&b, jr), callJ(&b, jp), callJ(&b, jv), callJ(&b, jo));
    std::printf("vbf m1 %d %d %d %d %d %d %d\n", callJ(&a, jn), callJ(&b, jn), callJ(&a, up), callJ(&b, up), callJ(&a, uv), callJ(&b, uv), callJ(&b, g_pm));
    std::printf("vbf m1 %d %d %d\n", (int)sizeof(jr), (int)sizeof(int (P1::*)()), (int)sizeof(int (VR::*)()));
    std::printf("vbf m1 %d %d %d %d\n", jr == &J2::r, jr == jp, up == jr, g_pm == jr);
}
}
namespace t1 {   // dynamic_cast and typeid through each base
struct VR { virtual int r() { return 1; } int vr; };
struct P1 { virtual int r() { return 2; } int p1; };
struct Other { virtual int o() { return 0; } int ot; };
struct J2 : virtual VR, P1 { int r() { return 3; } int j; };
struct JJ : J2, Other { int r() { return 4; } int jj; };
void run() {
    J2 a; JJ b; P1* p = &a; VR* v = &a; P1* q = &b; VR* w = &b;
    std::printf("vbf t1 %d %d %d %d %d\n", dynamic_cast<J2*>(p) == &a, dynamic_cast<VR*>(p) == v, dynamic_cast<P1*>(v) == p, dynamic_cast<J2*>(v) == &a, dynamic_cast<void*>(p) == static_cast<void*>(&a));
    std::printf("vbf t1 %d %d %d %d %d\n", dynamic_cast<JJ*>(p) == 0, dynamic_cast<Other*>(p) == 0, dynamic_cast<void*>(v) == static_cast<void*>(&a), typeid(*p) == typeid(J2), typeid(*v) == typeid(J2));
    std::printf("vbf t1 %d %d %d %d %d %d\n", dynamic_cast<JJ*>(q) == &b, dynamic_cast<Other*>(q) == static_cast<Other*>(&b), dynamic_cast<JJ*>(w) == &b, dynamic_cast<J2*>(w) == static_cast<J2*>(&b), typeid(*q) == typeid(JJ), typeid(*w) == typeid(JJ));
    std::printf("vbf t1 %s %s %s %s\n", typeid(*p).name(), typeid(*v).name(), typeid(*q).name(), typeid(*w).name());
    Other* o = &b;
    std::printf("vbf t1 %d %d %d %d\n", dynamic_cast<P1*>(o) == q, dynamic_cast<VR*>(o) == w, dynamic_cast<void*>(o) == static_cast<void*>(&b), dynamic_cast<J2*>(o)->r());
    try { P1& pr = a; JJ& bad = dynamic_cast<JJ&>(pr); std::printf("vbf t1 no throw %d\n", bad.jj); } catch (std::bad_cast&) { std::printf("vbf t1 bad_cast\n"); }
    try { VR& vr = b; J2& good = dynamic_cast<J2&>(vr); std::printf("vbf t1 good %d\n", good.r()); } catch (std::bad_cast&) { std::printf("vbf t1 bad_cast 2\n"); }
}
}
namespace e1 {   // a throw that crosses the call through each base, and one out of a constructor that makes it
struct Tr { int id; Tr(int i) : id(i) { note(id); } ~Tr() { note(-id); } };
struct Err { int code; Err(int c) : code(c) {} };
struct VR { VR() : vr(1) {} virtual ~VR() { note(-100); } virtual int r(int k) { return k; } int vr; };
struct P1 { P1() : p1(2) {} virtual ~P1() { note(-200); } virtual int r(int k) { return k + 1; } int p1; };
struct J2 : virtual VR, P1 { J2() : j(3) {} ~J2() { note(-300); } int r(int k); int j; };
struct JJ : J2 { JJ() : jj(4) {} ~JJ() { note(-400); } int r(int k); int jj; };
struct JT : J2 { Tr t; JT(int k); ~JT() { note(-500); } };
int J2::r(int k) { Tr t(7); if (k > 2) throw Err(k * 10 + j); return k + 2; }
int JJ::r(int k) { Tr t(8); if (k > 1) throw Err(k * 100 + jj); return k + 3; }
JT::JT(int k) : t(9) { P1* p = this; note(p->r(k)); }
int viaP(P1* p, int k) { Tr t(1); return p->r(k); }
int viaV(VR& v, int k) { Tr t(2); return v.r(k); }
int viaJ(J2* j, int k) { Tr t(3); return j->r(k); }
void run()
{
    {
        J2 a; JJ b;
        for (int k = 0; k < 5; ++k) {
            try { note(viaP(&a, k)); } catch (const Err& e) { note(e.code); }
            try { note(viaV(a, k)); } catch (const Err& e) { note(e.code); }
            try { note(viaJ(&a, k)); } catch (const Err& e) { note(e.code); }
            dump("vbf e1 J2");
            try { note(viaP(&b, k)); } catch (const Err& e) { note(e.code); }
            try { note(viaV(b, k)); } catch (const Err& e) { note(e.code); }
            try { note(viaJ(&b, k)); } catch (const Err& e) { note(e.code); }
            dump("vbf e1 JJ");
        }
    }
    dump("vbf e1 end");
    // out of the constructor of a derived class: the bases built so far are destroyed, the virtual one too
    try { JT t(1); note(1000); } catch (const Err& e) { note(e.code); } dump("vbf e1 JT1");
    try { JT t(4); note(1000); } catch (const Err& e) { note(e.code); } dump("vbf e1 JT4");
    try { P1* p = new JT(5); delete p; } catch (const Err& e) { note(e.code); } dump("vbf e1 JT5");
    try { P1* p = new JJ; Tr t(4); try { note(p->r(3)); } catch (...) { delete p; throw; } } catch (Err& e) { note(e.code); } dump("vbf e1 del");
}
}

int main()
{
    s1::run(); s2::run(); s3::run(); s4::run(); s5::run(); s6::run(); s7::run(); s8::run(); s9::run(); s10::run();
    r1::run(); r2::run(); r3::run(); r4::run(); r5::run(); x1::run();
    d1::run(); d2::run(); d3::run(); d4::run(); d5::run();
    c1::run(); c2::run(); covm::run(); m1::run(); t1::run(); e1::run();
    return 0;
}
