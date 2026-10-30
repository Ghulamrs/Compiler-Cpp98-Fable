// covariant-refused.cpp - must be rejected by --compile6 (S131): covariant overrides whose slot cannot get cl's
// return-adjusting thunk yet: one in a vftable inside a virtual base whose overrider is held by another virtual base
// (cl's vtordispex thunk would lead to the return thunk), and a variadic one (the thunk cannot stage the unnamed
// arguments again). A return conversion through a virtual base, a slot that also moves this, and a slot in a vftable
// inside a virtual base (through cl's vtordisp thunk) are generated as cl's (B11, front.cpp covvb:: and covvbt::)
struct SA { virtual ~SA() {} int a; };
struct X { int x; };
struct Y : SA, X { int y; };
struct VB { virtual X* get() { return 0; } virtual int f() { return 1; } int vb; };
struct VL : virtual VB { Y yy; VL() {} Y* get() { return &yy; } };
struct WL : virtual VB, virtual VL { WL() {} int f() { return 2; } };
struct SB2 { virtual X* v(int, ...) { return 0; } int b; };
struct CD2 : SB2 { Y yy; Y* v(int, ...) { return &yy; } };

int main() { WL w; CD2 d; return (static_cast<VB&>(w).get() != 0) + (d.v(1) != 0); }
