// covariant-vbase.cpp - a covariant override in a vftable inside a virtual base whose overrider is held by another
// virtual base of the class (rows 19, B11): WL : virtual VB, virtual VL with VL : virtual VB overriding VB::get
// (X*) by Y* get(). The slot of VB's table in WL holds a vtordispex thunk (this moved by the vtordisp, then through
// the vbtable to VL) that leads to the return-adjusting thunk, so a call through VB gets an X*, as 10.3/5 requires.
// Test 17 compares the output with the lines below and with the build of Visual Studio's clang-cl (22.1.3 prints
// them at /Od and /O2), not with cl's build: cl 19.50 puts the vtordispex thunk straight
// to VL::get there and the caller gets the Y* unadjusted (this program prints garbage for p->x with cl).
// Expected output:
//   covvbx 2 3 2 1 5
//   covvbx 2 5 1 15
//   covvbx 1 1 2 3
//   covvbx 2 3 7 1
#include <cstdio>
struct SA { virtual ~SA() {} int a; SA() : a(1) {} };
struct X { int x; X() : x(2) {} };
struct Y : SA, X { int y; Y() : y(3) {} };
struct VB {
    virtual X* get() { return 0; }
    virtual int f() { return 1; }
    virtual X& ref(int, int, int, int, int k) { static X sx; sx.x = k; return sx; }
    virtual X* none() { return 0; }
    int vb;
    VB() : vb(4) {}
};
struct VL : virtual VB {
    Y yy;
    VL() {}
    Y* get() { return &yy; }
    Y& ref(int a, int b, int c, int d, int k) { yy.y = a + b + c + d + k; return yy; }
    Y* none() { return 0; }
};
struct WL : virtual VB, virtual VL { WL() {} int f() { return 2; } };
struct XL : WL { int xl; XL() : xl(5) {} int f() { return 3; } };
struct ZL : virtual VL, virtual VB { int zl; ZL() : zl(7) {} int f() { return zl; } };
// calls during construction: the vtordisp is not 0 while VB's constructor-time table of WL is in place inside XL
struct Probe { static int seen; };
int Probe::seen = 0;
struct VC : virtual VB, virtual VL { VC() { VB* b = this; X* p = b->get(); Probe::seen = p ? p->x : -1; } int f() { return 9; } };
struct DC : VC { int dc; DC() : dc(8) {} };

int main()
{
    WL w; XL x; ZL z; DC dcObj;
    VB& vb = w; VL& vl = w; VB& xb = x; VB& zb = z; VB& db = dcObj;
    X* p = vb.get(); Y* q = vl.get(); X* r = xb.get();
    std::printf("covvbx %d %d %d %d %d\n", p->x, q->y, r->x, p == static_cast<X*>(q), vb.f() + xb.f());
    X& rr = vb.ref(1, 2, 3, 4, 5);
    const int first = static_cast<Y&>(rr).y;
    Y& ry = vl.ref(1, 1, 1, 1, 1);
    std::printf("covvbx %d %d %d %d\n", rr.x, ry.y, &rr == static_cast<X*>(&ry), first);
    std::printf("covvbx %d %d %d %d\n", vb.none() == 0, vl.none() == 0, w.get()->x, x.get()->y);
    std::printf("covvbx %d %d %d %d\n", zb.get()->x, static_cast<Y*>(zb.get())->y, zb.f(), Probe::seen == 2 && db.get()->x == 2);
    return 0;
}
