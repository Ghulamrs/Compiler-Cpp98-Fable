// access-refused4.cpp - must be rejected (row G14, the rest): a class's operator delete, static_cast to and from an
// inaccessible base, member types named through such a base, the components of a qualifier, the destructors of the
// temporaries of calls and of what an implicit destructor destroys, the copy a throw makes, a friend function
// template defined in a class template, and a friend found without argument-dependent lookup. cl /permissive- and
// clang -std=c++98 refuse the same uses (clang names the member, not the use, for lines 44 to 46).
// front2.cpp accessrest:: has the valid forms
class OD { void operator delete(void*); void operator delete[](void*); public: OD(); ~OD(); };
class ODP { protected: void operator delete(void*); public: virtual ~ODP(); };
struct ODD : ODP { ~ODD(); };
void od1(OD* p) { delete p; }                       // line 10: the class's operator delete is private
void od2(OD* p) { delete[] p; }
OD* od3() { return new OD; }                        // line 12: it frees the storage when the constructor throws
OD* od4() { return new OD[2]; }
void od5(ODD* p) { delete p; }                      // line 14: protected in a base
struct B2 { int x; };
struct D2 : private B2 {};
B2* sc1(D2* d) { return static_cast<B2*>(d); }      // line 17: a static_cast to an inaccessible base
B2& sc2(D2& d) { return static_cast<B2&>(d); }
D2* sc3(B2* b) { return static_cast<D2*>(b); }      // line 19: and down from it (5.2.9/8)
D2& sc4(B2& b) { return static_cast<D2&>(b); }
struct B3 { typedef int T; struct N { int v; }; };
struct D3 : private B3 {};
struct D3p : protected B3 {};
D3::T mt1;                                          // line 24: a member type named through a private base
D3::N mt2;
int mt3() { D3::T v = 0; return v; }
int mt4() { return sizeof(D3::N); }
D3p::T mt5;                                         // line 28: through a protected base
class A4 { class Priv { public: struct Inner { int v; }; static int value; typedef int T; }; };
A4::Priv::Inner q1;                                 // line 30: Priv is private in A4
int q2() { return A4::Priv::value; }
int q3() { A4::Priv::T t = 1; return t; }
class P6 { ~P6(); public: P6(); P6(const P6&); int v; };
P6 make6();
void take6(P6);
void t6a() { make6(); }                             // line 36: the temporary a call returns is destroyed here
int  t6b() { return make6().v; }
void t6c(P6& p) { take6(p); }                       // line 38: and the parameter an argument is copied to
class M7 { ~M7(); public: M7(); };
struct H7 { M7 m; };
struct HB7 : M7 {};
class T7c { T7c(const T7c&); public: T7c(); ~T7c(); };
template <class U> class CT8 { U secret; public: CT8() : secret(3) {} template <class T> friend int peek8(CT8& c, T t) { return c.secret + t; } };
void t7a() { H7 h; }                                // line 44: H7's implicit destructor cannot destroy m
void t7b() { HB7 h; }                               // line 45: nor HB7's its base
void t7c(H7* p) { delete p; }
void t7e(T7c& r) { throw r; }                       // line 47: the exception object is a copy
int t8a(CT8<int>& c) { return c.secret; }           // line 48: the class template's instantiation is checked
class F10 { int s; public: F10() : s(1) {} template <class T> friend int peek10(F10& f, T t) { return f.s + t; } friend int plain10(F10& f) { return f.s; } };
int t10a(F10& f) { return (peek10)(f, 2); }         // line 50: a name in parentheses is not looked up by its arguments
int t10b(F10& f) { return (plain10)(f); }
int main() { return 0; }
