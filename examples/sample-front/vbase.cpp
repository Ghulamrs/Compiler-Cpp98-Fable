// vbase.cpp - a virtual base (VL) that overrides a virtual function of another virtual base (V) of the class. In
// WL's copy of V's vftable cl fills V::f's slot with a vtordispex thunk, which reads the vbtable to find VL;
// --compile6 refused such classes (S130) until it generated that thunk too (B11, front.cpp vtordex:: runs these
// shapes against cl). Test 17 checks that --sema6 analyses the unit without errors.
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
