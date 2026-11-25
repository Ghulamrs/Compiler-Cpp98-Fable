// access-refused3.cpp - must be rejected (row G14): the access of base classes (11.2). A member reached through a
// private or protected base class is private or protected in the class it is named in, and a pointer, a reference
// or an object converts to a base class only where that base is accessible; cl C2247, C2243 and clang on the same
// lines. front2.cpp baseaccess:: has the valid forms
struct B1 { int x; void f() {} static int s; static int sf() { return 1; } typedef int T; B1() : x(0) {} };
int B1::s = 0;
struct D1 : private B1 { int own() { return x; } };
struct D1p : protected B1 {};
int ba1(D1& d) { return d.x; }                      // line 9: x is private in D1
void ba2(D1& d) { d.f(); }
B1* ba3(D1* d) { return d; }                        // line 11: a conversion to an inaccessible base
int ba4(D1p& d) { return d.x; }                     // line 12: a protected base
struct E1 : D1 { int g() { return x; } };           // line 13: B1's members are out of reach in E1
int ba5() { return D1::s; }                         // line 14: a static member named through D1
B1& ba6(D1& d) { return d; }                        // line 15: a reference bound to an inaccessible base
int ba7() { return D1::sf(); }                      // line 16
void ba8(D1& d) { B1 b(d); (void)b; }               // line 17: the base part copied
struct E1p : D1p { int g() { return x; } };         // fine: a protected base, in a member of a derived class
int main() { return 0; }
