// friend-refused.cpp - must be rejected (row G14, the rest): a friend declared with a qualified name names a function
// declared before (8.3/1, 7.3.1.2/3); one that names nothing, or a function with another signature, is an error
// (cl C2039, clang "no function named"), and its class is checked like any other. A member function template named
// as a friend makes its specialisations friends, and nobody else. front2.cpp accessrest:: has the valid forms
namespace N { int f(int); template <class T> int tf(T); }
struct B { void g(); template <class T> void mt(T); };
class C1 { int s; friend int N::nothing(int); };            // line 7: N declares no nothing
int bad1(C1& c) { return c.s; }                             // line 8: C1 is checked
class C3 { int s; friend int N::f(long); };                 // line 9: N::f takes an int
int bad3(C3& c) { return c.s; }
class C8 { int s; friend int ::nothing8(int); };            // line 11: nor does the global namespace
int bad8(C8& c) { return c.s; }
class C5 { int s; template <class T> friend void B::mt(T); };
int bad5(C5& c) { return c.s; }                             // line 14: only B::mt's specialisations are friends
class C4 { int s; friend int N::f(int); friend void B::g(); };
int bad4(C4& c) { return c.s; }                             // line 16
int main() { return 0; }
