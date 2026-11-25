// ehx.h - exceptions across compilers (sandbox\scripts\ehx.ps1, run by eh.ps1): the types both sides share.
// Each source of a group (cross.cpp, throw.cpp, unwind.cpp, catch.cpp, state.cpp) is compiled twice, as side A and as side B (the macro SIDE), by cl or by
// cppparse; main.cpp (always cl's) drives them. Row M7, docs\design-cl-exceptions.md. Strict C++98.
#ifndef EHX_H
#define EHX_H

#define EHX_CAT2(a, b) a##_##b
#define EHX_CAT(a, b) EHX_CAT2(a, b)
#define FN(name) EHX_CAT(SIDE, name)

extern int g_copies, g_dtors, g_guards, g_log, g_value;

struct Plain { int a, b; };
struct Err {
    int code;
    explicit Err(int c) : code(c) {}
    Err(const Err& o) : code(o.code) { ++g_copies; }
    virtual ~Err() { ++g_dtors; }
    virtual int kind() const { return 1; }
};
struct Other { int o; Other() : o(77) {} virtual ~Other() {} virtual int other() const { return o; } };
struct Derived : Other, Err { explicit Derived(int c) : Err(c) {} virtual int kind() const { return 2; } };
struct Single : Err { int s; Single() : Err(61), s(3) {} virtual int kind() const { return 6; } };
struct VD : virtual Err { int v; VD() : Err(31), v(5) {} virtual int kind() const { return 3; } };
struct DD : VD { int d; DD() : Err(32), d(6) {} virtual int kind() const { return 4; } };
struct Hidden : private Err { Hidden() : Err(41) {} };              // the base is not accessible: no handler of Err takes it
struct Left : Err { Left() : Err(51) {} };
struct Right : Err { Right() : Err(52) {} };
struct Twice : Left, Right { int t; Twice() : t(9) {} };            // two Err subobjects: no handler of Err takes it
enum Colour { Red, Green = 7 };

// an object whose destructor adds its id to g_guards (g_log: the order, base 10)
struct Guard {
    int id;
    explicit Guard(int i) : id(i) {}
    ~Guard();
};

#endif
