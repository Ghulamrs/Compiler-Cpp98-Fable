// memptr-refused.cpp - must be rejected by --compile6 (S132): conversions of pointers to members that cppparse does
// not generate as cl does yet: into a class with virtual bases that does not share the base's vbptr (the vbtable
// entry would have to be remapped), down from a class with virtual bases to a base, and between two classes whose
// inheritance model is unspecified (sizes of their member pointers taken while they were incomplete)
struct S { int s; };
struct X { int x; };
struct V : virtual S { int v; };
struct W : virtual S { int w; };
struct VW : V, W { int vw; };
struct VX : X, virtual S { int vx; };
struct I1; struct I2;
int sz = sizeof(int I1::*) + sizeof(int I2::*);
struct I1 : virtual S { int a; };
struct I2 : I1 { int b; };

int VW::* a(int W::* m) { return m; }
int X::* b(int VX::* m) { return static_cast<int X::*>(m); }
int I2::* c(int I1::* m) { return m; }

int main() { return 0; }
