// ambiguous.cpp - must be rejected: a conversion to a base held more than once (cl C2594: S143 for casts, S105
// for implicit conversions) and an unqualified member of such a base (cl C2385: S144); the qualified and
// unambiguous forms after them are valid (front.cpp ambig:: runs them)
struct A { int a; int f() { return a; } };
struct B : A {};
struct C : A {};
struct D : B, C {};
struct V2 { int v; };
struct E1 : virtual V2 {};
struct E2 : virtual V2 {};
struct G : E1, E2 {};
A* p1 = (D*)0;
A* p2 = (A*)(D*)0;
A* p3 = static_cast<A*>((D*)0);
int m1(D& d) { return d.a; }
int m2(D& d) { return d.f(); }
A& r1(D& d) { return d; }
int ok1(D& d) { return d.B::a + d.C::a; }
A* ok2(D* d) { return (B*)d; }
int ok3(G& g) { V2* v = &g; return g.v + v->v; }
// a virtual function of a virtual base overridden in two direct bases has no final overrider (cl C2250: S145),
// unless the class overrides it itself (XE)
struct XV { virtual int f() const { return 0; } };
struct XL : virtual XV { int f() const { return 1; } };
struct XR : virtual XV { int f() const { return 2; } };
struct XD : XL, XR {};
struct XE : XL, XR { int f() const { return 3; } };
// 10.2/2: a name declared in a derived class on one path and in its base on another (cl C2385: S144), unqualified
// inside a member function, qualified with the class and in a pointer to member too; a declaration in a virtual
// base is hidden by one in a class derived from it (front.cpp ambig::), unless the base is also held non-virtually
struct YA { int a; int f() { return a; } static int s; };
struct YB : YA { int a; int f() { return 1; } static int s; };
struct YC : YA {};
struct YD : YB, YC { int m1() { return a; } int m2() { return f(); } int m3() { return YD::a; } int m4() { return s; } };
int y1(YD& d) { return d.a; }
int y2(YD& d) { return d.f(); }
int YD::* ypm = &YD::a;
struct D2 : B, C { int m() { return a + f(); } };
struct YV { int v; };
struct YL : virtual YV { int v; };
struct YW : YV {};
struct YY : YL, YW { int m() { return v; } };
