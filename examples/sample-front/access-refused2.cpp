// access-refused2.cpp - must be rejected (row G14): member types and destructors are members like the others (11/4).
// A protected or private typedef, nested class or enumeration named by what is neither a member nor a friend, an
// object destroyed where its class's destructor cannot be reached, and (line 26) a class with a friend template defined
// in it, which is checked like any other; cl C2248 and clang on the same lines. front2.cpp has the valid forms
struct T4 { T4() {} protected: typedef int T; struct In { int v; }; enum E { e0 }; private: typedef long PT; };
T4::T mt1;                                          // line 6: a protected typedef named outside
T4::In mt2;
T4::PT mt3;
T4::E mt4;
struct V4 : T4 { PT bad; };                         // line 10: private in T4, for a derived class too
int mt5() { return sizeof(T4::In); }
void mt6(T4::In* p);                                // line 12: a parameter type
T4::T mt7() { return 0; }                           // line 13: a return type
typedef T4::In mt8;                                 // line 14
struct P2 { static void kill(P2* p) { delete p; } P2() {} private: ~P2() {} };
struct Q2 { Q2() {} protected: ~Q2() {} };
void dt1() { P2 p; }                                // line 17: an automatic object
void dt2(P2* p) { delete p; }
void dt3() { Q2 q; }                                // line 19: a protected destructor
void dt4() { P2(); }                                // line 20: a temporary
P2 dt5;                                             // line 21: an object of namespace scope
struct R2 : Q2 { void bad() { Q2 q; } };            // line 22: a complete Q2 in a member of a derived class
void dt6() { static P2 s; }                         // line 23: a static local
void dt7(P2* p) { delete[] p; }                     // line 24
struct F5 { F5() : secret(1) {} private: int secret; template <class T> friend int peekT(F5& f, T t) { return f.secret + static_cast<int>(t); } };
int ft1(F5& f) { return f.secret; }                 // line 26: ft1 is no friend of F5
int ft2(F5& f) { return peekT(f, 2); }              // fine: the friend template is found by argument-dependent lookup
int main() { return 0; }
