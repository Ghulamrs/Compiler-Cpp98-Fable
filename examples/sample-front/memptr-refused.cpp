// memptr-refused.cpp - must be rejected by --compile6 (S132): conversions of pointers to members through a virtual
// base, which C++98 makes ill-formed (4.11/2: "If B is an inaccessible, ambiguous or virtual base class of D, a program
// that necessitates this conversion is ill-formed") and cl accepts as an extension: between two classes with virtual
// bases, and between two classes of the unspecified model (sizes of their member pointers taken while they were
// incomplete). The simpler ones of them are generated as cl's (front.cpp memptr::, mpdownt:: fromS). Every
// conversion along non-virtual bases, up and down, between any two models is generated since row 18 was finished
// (front.cpp mpremap::, mpdownt::, front2.cpp mpdown::)
struct S { int s; };
struct V1 : virtual S { int v1; int f1() { return v1; } };
struct DV : virtual V1 { int dv; };
struct I1; struct I2;
int sz = sizeof(int I1::*) + sizeof(int I2::*);
struct I1 { int a; };
struct I2 : virtual I1 { int b; };

int DV::* upData(int V1::* m) { return m; }
int (DV::*upFunction(int (V1::*f)()))() { return f; }
int I2::* up(int I1::* m) { return m; }

int main() { return 0; }
