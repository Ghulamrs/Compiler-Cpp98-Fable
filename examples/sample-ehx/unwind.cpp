// unwind.cpp - side SIDE's frames that own objects and have no handler: an exception thrown below them (by f, a
// function of main.cpp) must destroy what they own, in order, on its way to a handler above (see ehx.h, main.cpp
// "unwind").
#include "ehx.h"
#include <stddef.h>
#include <stdlib.h>

void note(int v);                // main.cpp: appends a digit to g_log
double scale(double v);          // main.cpp: v * 1.5, never inlined here

namespace {

struct Tmp {
    int id;
    explicit Tmp(int i) : id(i) {}
    Tmp(const Tmp& o) : id(o.id) {}
    ~Tmp() { note(id); }
    int call(void (*f)(int)) const { f(0); return id; }
    bool yes() const { return true; }
};

struct Elem {
    static int built;
    int id;
    Elem() : id(++built) {}
    ~Elem() { note(id); }
};
int Elem::built = 0;

struct Fussy {
    static int built;
    static void (*thrower)(int);
    int id;
    Fussy() : id(++built) { if (id == 3) thrower(0); }
    ~Fussy() { note(id); }
};
int Fussy::built = 0;
void (*Fussy::thrower)(int) = 0;

int g_freed = 0;
struct Heap {
    Guard g;
    explicit Heap(void (*f)(int)) : g(6) { f(0); }
    void* operator new(size_t n) { return malloc(n); }
    void operator delete(void* p) { ++g_freed; free(p); }
};

struct Base { Guard gb; Base() : gb(1) {} };
struct Holder : Base {
    Guard a;
    Guard b;
    explicit Holder(void (*f)(int)) : a(2), b(3) { f(0); }
};

struct Once {
    int v;
    explicit Once(void (*f)(int)) : v(7) { if (f) f(0); }
};

// a destructor with floating-point work around a call: inlined into a cleanup, its values need xmm registers that
// are the frame's to keep
struct Weigh {
    double w;
    explicit Weigh(double v) : w(v) {}
    ~Weigh() { double a = w * 2.0, b = w + 0.25; double c = scale(a); note(static_cast<int>(a + b + c) % 10); }
};

}

// 0: plain objects, destroyed in reverse order
int FN(unwindPlain)(void (*f)(int), int k)
{
    Guard a(1);
    Guard b(2);
    f(k);
    Guard c(3);
    return a.id + b.id + c.id;
}

// 1: temporaries: one bound to a reference, one of each arm of ?:, one behind &&
int FN(unwindTemps)(void (*f)(int), int k)
{
    const Tmp& kept = Tmp(4);
    if (k == 0) return (kept.id > 0 ? Tmp(5) : Tmp(6)).call(f);
    if (k == 1) return (kept.id < 0 ? Tmp(5) : Tmp(6)).call(f);
    if (k == 2) return Tmp(7).yes() && Tmp(8).call(f) ? 1 : 0;
    const int first = Tmp(9).call(f);   // (two statements: the order of the operands of + is unspecified)
    return first + Tmp(1).id;
}

// 2: an array partly built (the third constructor throws: two elements are destroyed), and one fully built
int FN(unwindArrays)(void (*f)(int), int k)
{
    Elem::built = 0;
    Fussy::built = 0;
    Fussy::thrower = f;
    if (k == 0) { Fussy part[5]; return part[0].id; }
    Elem whole[3];
    f(0);
    return whole[0].id;
}

// 3: a constructor that throws in a new-expression (its member is destroyed, the storage freed by the class's
// operator delete), and one that throws after its base and members are built
int FN(unwindCtors)(void (*f)(int), int k)
{
    g_freed = 0;
    if (k == 0) { Heap* h = new Heap(f); delete h; return 0; }
    Holder h(f);
    return h.a.id;
}
int FN(unwindFreed)() { return g_freed; }

// 4: a static local whose initialiser throws is not built: the next call tries again
int FN(unwindStatic)(void (*f)(int))
{
    Guard g(8);
    static Once once(f);
    return once.v;
}

// 5: a cleanup with floating-point values in registers; the frame's own doubles must survive in the caller
double FN(unwindDoubles)(void (*f)(int), double x)
{
    double p = scale(x) + 1.0, q = scale(x + 2.0);
    Weigh w(p);
    f(0);
    return p + q + w.w;
}

// 6: an object of a loop's body, and a class parameter passed by value (the callee's to destroy)
int FN(unwindLoop)(void (*f)(int), Guard byValue, int n)
{
    int sum = byValue.id;
    for (int i = 0; i < n; ++i) {
        Guard g(i + 1);
        if (i == n - 1) f(0);
        sum += g.id;
    }
    return sum;
}
