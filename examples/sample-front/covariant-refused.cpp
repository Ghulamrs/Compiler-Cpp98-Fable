// covariant-refused.cpp - must be rejected by --compile6 (S131): covariant overrides whose slot cannot get cl's
// return-adjusting thunk yet: one whose return conversion goes through a virtual base (cl's thunk reads the vbtable),
// and one whose slot needs a this adjustment as well as the return adjustment (the overridden function sits in a
// secondary vftable while the overrider's own slot is in the primary one: cl's $R thunks)
struct V { int v; };
struct VD : virtual V { int d; };
struct SB { virtual ~SB() {} virtual V* get() { return 0; } int b; };
struct CD : SB { VD* get() { return 0; } };
struct SA { virtual ~SA() {} int a; };
struct X { int x; };
struct Y : SA, X { int y; };
struct A { virtual Y* g() { return 0; } };
struct B { virtual X* g() { return 0; } };
struct D : A, B { Y* g() { return 0; } };

int main() { CD c; D d; return (c.get() == 0) + (d.g() == 0); }
