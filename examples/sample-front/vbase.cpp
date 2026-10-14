// vbase.cpp - must be rejected by --compile6 (S130): a virtual base (VL) that overrides a virtual function of
// another virtual base (V) of the class. In WL's copy of V's vftable cl fills V::f's slot with a vtordispex thunk,
// which reads the vbtable to find VL, and --compile6 does not generate that yet; it refuses every class with such
// a slot, with or without a constructor of its own. --sema6 still analyses the unit without errors; every other
// class with virtual bases is compiled (front.cpp pvbase, sample-layout).
#include <stdio.h>

struct V  { int v; V() : v(7) {} virtual int f() const { return v; } };
struct VL : virtual V { int l; VL() : l(2) {} int f() const { return l; } };
struct WL : virtual VL { int w; WL() : w(3) {} };
struct WL2 : virtual VL { int w; };

int call(const V* p) { return p->f(); }

int main()
{
    WL a;
    WL2 b;
    printf("%d\n", call(&a) + call(&b));
    return 0;
}
