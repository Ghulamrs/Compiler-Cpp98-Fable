// memptr-refused.cpp - must be rejected by --compile6 (S132): conversions of pointers to members that cppparse does
// not generate as cl does yet: a member function's pointer down from a class with virtual bases to a base of
// multiple inheritance, and one between two classes of the unspecified model (sizes of their member pointers taken
// while they were incomplete) through a virtual base. The others are generated as cl's since B11: into a class with
// virtual bases that does not share the base's vbptr (front.cpp mpremap::), down to a base without virtual bases,
// and between classes of the unspecified model along non-virtual bases (front.cpp mpdownt::)
struct S { int s; };
struct A { int a; virtual int fa() { return a; } };
struct B { int b; int fb() { return b; } };
struct M : A, B { int m; int fm() { return m; } };
struct VM : M, virtual S { int vm; };
struct I1; struct I2;
int sz = sizeof(int I1::*) + sizeof(int I2::*);
struct I1 { int a; };
struct I2 : virtual I1 { int b; };

int (M::*down(int (VM::*f)()))() { return static_cast<int (M::*)()>(f); }
int I2::* up(int I1::* m) { return m; }

int main() { return 0; }
