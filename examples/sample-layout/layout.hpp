// layout.hpp - class layouts shared by the unit cppparse compiles (layout.cpp) and the driver cl compiles
// (sample-layout-driver): MSVC's placement of empty bases, bit-fields, anonymous unions and structs, and
// polymorphic bases. Both sides evaluate the same expressions (LAYOUT_VALUES); the driver compares cppparse's
// results with its own.
// Strict C++98.
#ifndef SAMPLE_LAYOUT_HPP_INCLUDED
#define SAMPLE_LAYOUT_HPP_INCLUDED

#include <stddef.h>

// the offset of base B in an object of class D, and of a field reached through D (without offsetof, which
// is not defined for non-POD classes)
#define LAYOUT_BASE(D, B)  ((int)((char*)static_cast<B*>((D*)0x1000) - (char*)0x1000))
#define LAYOUT_FIELD(D, f) ((int)((char*)&((D*)0x1000)->f - (char*)0x1000))

// ---- empty bases (MSVC without __declspec(empty_bases)) -------------------------------------------------
// An empty class has size 1 but takes no space as a base. Between two bases, the first ending with a
// zero-sized subobject (an empty base, or a member or base that ends with one) and the second leading with
// one, a byte of padding keeps the two apart. An empty base that ends up at the very end of the class is
// placed at its last byte.
struct E  {};
struct E2 {};
struct E3 {};
struct N  { int n; };
struct C1 { char c; };
struct D8 { double x; };
struct EB  : E { int w; };                 // size 4, w at 0
struct EM  : E, E2 { int w; };             // E at 0, E2 at 1, w at 4, size 8
struct EE  : E {};                         // size 1
struct E3b : E, E2, E3 { char c; };        // E2 at 1, E3 at 2, c at 2, size 3
struct NE  : N, E { int w; };              // E at 4, w at 4, size 8
struct EN  : E, N { int w; };              // N at 0, w at 4, size 8
struct DE  : D8, E {};                     // E at 7 (its last byte), size 8
struct EEE : EE { int w; };                // w at 0, size 4
struct EEx : EE, E2 { int w; };            // E2 at 1, w at 4, size 8
struct FE  : N { E e; };                   // e at 4, size 8; ends with a zero-sized object
struct FEb : FE, E2 { char d; };           // E2 at 9, d at 9, size 12
struct LE  : E, N {};                      // leads with a zero-sized base
struct ELE : E2, LE { char c; };           // LE at 4, c at 8, size 12
struct CLE : C1, LE { char c; };           // LE at 4, c at 8, size 12
struct EF  : E { E2 x; char d; };          // x at 0, d at 1, size 2
struct EVal : E { int v; EVal() : v(0) {} int get() const { return v; } };

// ---- bit-fields ---------------------------------------------------------------------------------------
// A bit-field takes an allocation unit of its declared type's size; the next bit-field shares it when its
// type has the same size and its bits still fit, from the low bit up. A zero-width bit-field closes the
// unit (ignored unless it follows a bit-field). In a union, bit-fields do not raise the alignment.
enum Colour { Red = -1, Green = 1, Blue = 2 };
struct BF  { unsigned a : 3; unsigned b : 5; unsigned c : 24; char d; };    // size 8, d at 4
struct BF2 { char a : 3; int b : 4; char c : 2; };                         // units at 0, 4, 8: size 12
struct BF3 { int a : 3; int : 0; int b : 2; };                             // b in a new unit: size 8
struct BF4 { char x; int : 0; char y; };                                   // the :0 is ignored: y at 1, size 2
struct BF5 { int a : 30; int b : 4; };                                     // b does not fit: size 8
struct BF6 { short a : 3; unsigned short b : 9; int c : 1; int s : 5; };   // a, b share a short; c, s an int
union  BU  { int a : 3; char c; };                                         // size 4, alignment 1
struct BUa { char c; BU u; };                                              // u at 1
struct BF7 { long long a : 40; int b : 3; long long c : 30; };             // size 24
struct BF8 { bool f : 1; bool g : 1; char h; Colour k : 3; };              // f, g share a byte; k at 4
struct BF9 { int a : 4; int : 0; int : 0; char c; };                       // the second :0 is ignored: c at 4
struct BF10 { int a : 1; long b : 2; };                                    // int and long share: size 4
#pragma pack(push, 1)
struct BFP { char c; int a : 4; int b : 30; };                             // units at 1 and 5: size 9
#pragma pack(pop)
struct BFAgg { int x; unsigned y : 4; int : 3; int z : 5; };               // the unnamed bit-field takes no initialiser

// ---- anonymous unions and structs ----------------------------------------------------------------------
// The members of an anonymous union or struct are members of the enclosing class, at the anonymous
// object's offset; union members share it.
struct AU  { int k; union { int i; double dd; }; char z; };                // dd at 8, z at 16, size 24
union  US  { struct { int a, b; }; long long q; };                         // b at 4, size 8
struct AN  { char t; union { struct { short lo, hi; }; int all; }; unsigned f : 4; union { unsigned g : 3; char gc; }; };
struct AUC {
    int tag;
    union { int iv; float fv; };
    AUC() : tag(1), iv(0) {}
    int twice() const { return iv * 2; }                                    // an anonymous member named in a member function
};

// ---- a polymorphic base that is not first ------------------------------------------------------------
// MSVC lays out the polymorphic bases first: the first one is at offset 0, the class shares its vptr and
// extends its vtable; the other bases follow in declaration order.
struct NP { int x; };
struct PB { PB(); virtual int f() const; int y; };
struct DP : NP, PB { DP(); int f() const; int z; };                        // PB at 0, NP at 16, z at 20, size 24
struct DQ : NP, E, PB { DQ(); virtual int g() const; char q; };            // PB at 0, NP at 16, E and q at 20
struct DR : DQ { DR(); int g() const; int f() const; char r; };            // r at 24, size 32

// ---- a second polymorphic base: secondary vftables and adjustor thunks ----------------------------------
// Every polymorphic base subobject but the one at offset 0 keeps its own vfptr and gets a table of its own
// (??_7SD@@6BSB@@@). A function first held by such a table expects this at that vfptr (SD::g, SD::b: MSVC's
// this adjustor 16); one also held by the primary table (SD::f overrides SA::f and SB::f) is reached from the
// secondary table through a thunk (?f@SD@@WBA@EBAHXZ: this -= 16); the destructor always takes the complete
// object, so its secondary slot is a thunk too.
struct SA { SA(); virtual int a() const; virtual int f() const; int sa; };
struct SB { SB(); virtual int b() const; virtual int f() const; virtual int g() const; virtual ~SB(); int sb; };
struct SD : SA, SB { SD(); int f() const; int g() const; int b() const; virtual int h() const; ~SD(); int sd; };   // SB at 16, sd at 32, size 40
struct SE : NP, SD { SE(); int g() const; int a() const; ~SE(); char se; };     // SD at 0, NP at 40, se at 44, size 48
// a class-specific operator delete, found by lookup in the complete class and given its size, also when the
// deleting destructor is reached through a secondary base (KD : SA, KB)
struct KB { KB(); virtual ~KB(); static void operator delete(void* p, size_t n); int k; };
struct KD : SA, KB { KD(); ~KD(); int kd; };
// two vfptrs introduced by the same class (GP through GQ1 and GQ2): the tables are named after GQ1 and GQ2, as
// cl names them (??_7GG@@6BGQ1@@@, ??_7GG@@6BGQ2@@@), not twice after GP
struct GP { GP(); virtual ~GP(); virtual int fp() const; int p; };
struct GQ1 : GP { };
struct GQ2 : GP { virtual int fq() const; };
struct GG : GQ1, GQ2 { int fp() const; };

#define LAYOUT_VALUES(X) \
    X(sizeof(EB)) X(LAYOUT_FIELD(EB, w)) X(LAYOUT_BASE(EB, E)) \
    X(sizeof(EM)) X(LAYOUT_FIELD(EM, w)) X(LAYOUT_BASE(EM, E2)) \
    X(sizeof(EE)) \
    X(sizeof(E3b)) X(LAYOUT_BASE(E3b, E2)) X(LAYOUT_BASE(E3b, E3)) X(LAYOUT_FIELD(E3b, c)) \
    X(sizeof(NE)) X(LAYOUT_BASE(NE, E)) X(LAYOUT_FIELD(NE, w)) \
    X(sizeof(EN)) X(LAYOUT_BASE(EN, N)) X(LAYOUT_FIELD(EN, w)) \
    X(sizeof(DE)) X(LAYOUT_BASE(DE, E)) \
    X(sizeof(EEE)) X(LAYOUT_FIELD(EEE, w)) \
    X(sizeof(EEx)) X(LAYOUT_BASE(EEx, E2)) X(LAYOUT_FIELD(EEx, w)) \
    X(sizeof(FE)) X(LAYOUT_FIELD(FE, e)) \
    X(sizeof(FEb)) X(LAYOUT_BASE(FEb, E2)) X(LAYOUT_FIELD(FEb, d)) \
    X(sizeof(ELE)) X(LAYOUT_BASE(ELE, LE)) X(LAYOUT_FIELD(ELE, c)) \
    X(sizeof(CLE)) X(LAYOUT_BASE(CLE, LE)) X(LAYOUT_FIELD(CLE, c)) \
    X(sizeof(EF)) X(LAYOUT_FIELD(EF, x)) X(LAYOUT_FIELD(EF, d)) \
    X(sizeof(EVal)) X(LAYOUT_FIELD(EVal, v)) \
    X(sizeof(BF)) X(offsetof(BF, d)) X(sizeof(BF2)) X(sizeof(BF3)) X(sizeof(BF4)) X(offsetof(BF4, y)) X(sizeof(BF5)) \
    X(sizeof(BF6)) X(sizeof(BU)) X(offsetof(BUa, u)) X(sizeof(BF7)) X(sizeof(BF8)) X(offsetof(BF8, h)) X(sizeof(BF9)) \
    X(offsetof(BF9, c)) X(sizeof(BF10)) X(sizeof(BFP)) X(sizeof(BFAgg)) \
    X(sizeof(AU)) X(offsetof(AU, i)) X(offsetof(AU, dd)) X(offsetof(AU, z)) X(sizeof(US)) X(offsetof(US, b)) \
    X(offsetof(US, q)) X(sizeof(AN)) X(offsetof(AN, lo)) X(offsetof(AN, hi)) X(offsetof(AN, all)) X(offsetof(AN, gc)) \
    X(sizeof(AUC)) X(LAYOUT_FIELD(AUC, iv)) X(LAYOUT_FIELD(AUC, fv)) \
    X(sizeof(DP)) X(LAYOUT_BASE(DP, NP)) X(LAYOUT_BASE(DP, PB)) X(LAYOUT_FIELD(DP, z)) \
    X(sizeof(DQ)) X(LAYOUT_BASE(DQ, NP)) X(LAYOUT_BASE(DQ, E)) X(LAYOUT_BASE(DQ, PB)) X(LAYOUT_FIELD(DQ, q)) \
    X(sizeof(DR)) X(LAYOUT_BASE(DR, NP)) X(LAYOUT_BASE(DR, PB)) X(LAYOUT_FIELD(DR, r)) \
    X(sizeof(SD)) X(LAYOUT_BASE(SD, SA)) X(LAYOUT_BASE(SD, SB)) X(LAYOUT_FIELD(SD, sd)) \
    X(sizeof(SE)) X(LAYOUT_BASE(SE, SB)) X(LAYOUT_BASE(SE, NP)) X(LAYOUT_FIELD(SE, se)) \
    X(sizeof(VB)) X(sizeof(VD)) X(LAYOUT_FIELD(VD, d)) X(sizeof(VL)) X(sizeof(VX)) X(LAYOUT_BASE(VX, VR)) \
    X(LAYOUT_FIELD(VX, x)) X(sizeof(VN)) X(LAYOUT_FIELD(VN, n)) X(sizeof(VM)) X(LAYOUT_FIELD(VM, m)) X(sizeof(VY)) \
    X(LAYOUT_FIELD(VY, y)) X(sizeof(VZ)) X(LAYOUT_BASE(VZ, VX)) X(LAYOUT_FIELD(VZ, z)) \
    X(sizeof(VNB)) X(sizeof(VNC)) X(sizeof(VNE)) X(sizeof(VNF)) X(LAYOUT_BASE(VNF, VNE)) X(LAYOUT_FIELD(VNF, f)) X(sizeof(VNG))

// ---- virtual bases (MSVC: vbptr, vbtables, the "most derived" flag, the vbase destructor ??_D) -----------
// A class with virtual bases shares the vbptr of its first non-virtual base that has one or gets its own after
// the non-virtual bases; each virtual base is placed once, after the non-virtual part (a diamond shares it).
// The conversions to a virtual base go through the vbtable; the constructor called with "most derived" set
// stores the vbptrs and builds the virtual bases; ??_D destroys them after ~D. Both compilers build, convert,
// copy and destroy these objects; the constructors and destructors log their digits in g_vbLog.
extern long g_vbLog;
struct VB { int v; VB() : v(7) { g_vbLog = g_vbLog * 10 + 1; } VB(const VB& o) : v(o.v + 100) { g_vbLog = g_vbLog * 10 + 8; } ~VB() { g_vbLog = g_vbLog * 10 + 9; } };
struct VD : virtual VB { int d; VD(); ~VD(); };
struct VL : virtual VB { int l; VL() : l(2) { g_vbLog = g_vbLog * 10 + 2; } };
struct VR : virtual VB { int r; VR() : r(3) { g_vbLog = g_vbLog * 10 + 3; } ~VR() { g_vbLog = g_vbLog * 10 + 6; } };
struct VX : VL, VR { int x; VX() : x(4) { g_vbLog = g_vbLog * 10 + 4; } };   // the diamond: one VB, at 40
struct VN : C1, virtual VB { short n; VN() : n(9) {} };                         // vbptr at 8, n at 18
struct V2 { double q; V2() : q(2.5) {} };
struct VM : virtual VB, C1, virtual V2 { int m; VM() : m(5) {} };              // VB at 24, V2 at 32
struct VY : VX { int y; VY() : VB(), y(6) {} };                                // the most derived class names VB
struct VZ : C1, VX { int z; VZ() : z(8) {} };                                  // shares VX's vbptrs at 8 and 24
int  vbReadV(VL* p);                       // p->v
int  vbReadVR(const VR& r);                // r.v * 10 + r.r
int  vbReadVM(VM* p);                      // q * 2 + v + m
int  vbReadVN(const VN& n);                // v * 10 + n
VB*  vbToVB(VX* p);                        // null stays null
VX*  vbMakeVX();
void vbDelete(VX* p);                      // delete p (??_D, then operator delete)
long vbOnStack();                          // a VX on cppparse's stack: the log
int  vbCopy(const VX& x);                  // a copy's v l r x
long vbArray(int n);                       // new VX[n], delete[]: the destructors' log
int  vbSumVY(const VY& y);
int  vbSumVZ(const VZ& z);
// virtual bases with virtual bases of their own (their vbptrs in the virtual part): built once each
struct VNA { int a; VNA() : a(1) { g_vbLog = g_vbLog * 10 + 1; } ~VNA() { g_vbLog = g_vbLog * 10 + 9; } };
struct VNB : virtual VNA { int b; VNB() : b(2) { g_vbLog = g_vbLog * 10 + 2; } ~VNB() { g_vbLog = g_vbLog * 10 + 7; } };
struct VNC : virtual VNB { int c; VNC() : c(3) { g_vbLog = g_vbLog * 10 + 3; } };
struct VNE : virtual VNB, virtual VNA { int e; VNE() : e(5) { g_vbLog = g_vbLog * 10 + 5; } };
struct VNF : VNC, VNE { int f; VNF() : f(6) {} };                              // VNA at 40, VNB at 48
struct VNG : virtual VNC, virtual VNE { int g; VNG() : g(8) {} };              // four virtual bases
int  vbNestedF(const VNF& f);              // a b c e f, through the bases
int  vbNestedG(VNG* g);                    // a b c e g, through VNE* and VNC*
VNA* vbToVNA(VNG* g);                      // through VNC's VNB
long vbNestedLife();                       // a VNF and a VNG on cppparse's stack: built right, then the destructors' log

// the value of the k-th expression of LAYOUT_VALUES as cppparse computes it; -1 past the end
int layoutValue(int k);

// objects built by cppparse, read by the driver
extern EM g_em;
extern NE g_ne;
int  emSum(const EM* p);                   // w read through cppparse's view of EM
int  evalGet(EVal& v, int set);            // stores set, assigns to the E subobject, reads v back through EVal*

// bit-fields and anonymous members written and read by cppparse's code, checked by the driver through cl's
// view of the classes
extern BF    g_bf;
extern BF6   g_bf6;
extern BF7   g_bf7;
extern BF8   g_bf8;
extern BFP   g_bfp;
extern BFAgg g_agg;
extern AU    g_au;
extern AN    g_an;
void fillBits(BF2* p, int a, int b, int c);    // stores through the bit-fields (sign bits and all)
long readBits(const BF2* p, int which);        // reads them back: 0 a, 1 b, 2 c
long bitOps(BF6* p);                           // compound assignment, ++, -- and the value of an assignment
long readBf7(const BF7* p, int which);         // 0 a, 1 b, 2 c

// polymorphic classes: constructed and called by cppparse's code, called virtually by the driver
int  polyCall(const PB* p);                    // p->f()
int  polyCallG(const DQ& q);                   // q.g()
int  polyX(const NP* p);                       // p->x

// two polymorphic bases: virtual calls made by cppparse through each base, and the destructors' order
extern int g_dtorLog;                          // each destructor appends its digit: ~SE 3, ~SD 1, ~SB 2
extern int g_sbCtorSaw, g_sbDtorSaw, g_sdDtorSaw;   // what g() (h() in ~SD) returned in SB() / ~SB() / ~SD()
int  saCall(const SA* p, int which);           // 0 a(), 1 f()
int  sbCall(const SB* p, int which);           // 0 b(), 1 f(), 2 g()
int  sdCall(const SD* p, int which);           // 0 f(), 1 g(), 2 b(), 3 h(), 4 a(), 5 SD::g() (not virtual)
long destroyThroughSB(int derived);            // builds an SD (or an SE) in place, destroys it through SB*: the log
long deleteThroughSB(int derived);             // the same through new and delete: the log
extern int g_kDeleted;                         // the size KB::operator delete was given (0: not called)
KB*  makeKDasKB();
void deleteKB(KB* p);                          // delete p in cppparse's code
int  ggCall(const GQ2* p);                      // p->fp() * 100 + p->fq()
GG*  makeGG();

#endif
