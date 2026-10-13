// layout.hpp - class layouts shared by the unit cppparse compiles (layout.cpp) and the driver cl compiles
// (sample-layout-driver): MSVC's placement of empty bases. Both sides evaluate the same
// expressions (LAYOUT_VALUES); the driver compares cppparse's results with its own.
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
    X(sizeof(EVal)) X(LAYOUT_FIELD(EVal, v))

// the value of the k-th expression of LAYOUT_VALUES as cppparse computes it; -1 past the end
int layoutValue(int k);

// objects built by cppparse, read by the driver
extern EM g_em;
extern NE g_ne;
int  emSum(const EM* p);                   // w read through cppparse's view of EM
int  evalGet(EVal& v, int set);            // stores set, assigns to the E subobject, reads v back through EVal*

#endif
