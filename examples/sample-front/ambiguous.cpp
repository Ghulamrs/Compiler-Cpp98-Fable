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
