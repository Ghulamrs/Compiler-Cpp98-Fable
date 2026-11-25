// access-refused.cpp - must be rejected: private and protected members reached by what is no friend, protected
// members reached from a derived class through another class (11.5), and from line 46 private operators, conversion
// functions and operator new (row G14); cl C2248 and clang on the same lines. front2.cpp friends:: and opaccess:: valid
struct P;
struct Other { int om(P& p); int om2(P& p); };
template <class T> int tf(T t);
template <class T> int other(T t);
struct P {
private:
    int secret;
    template <class U> friend int tf(U u);
    friend int Other::om(P& p);
public:
    P() : secret(7) {}
};
int Other::om(P& p) { return p.secret; }
int Other::om2(P& p) { return p.secret; }           // line 17: om is the friend, not om2
struct B { protected: int x; int f() { return 1; } B() : x(0) {} B(int v) : x(v) {} };
struct D : B {
    int bad1(B& b) { return b.x; }                  // line 20: through a B
    int bad2(B* p) { return p->x; }
    int bad3(B& b) { return b.f(); }
    int bad4() { int B::*pm = &B::x; return pm != 0; }
    int bad5() { int (B::*pf)() = &B::f; return pf != 0; }
    int bad6() { B b; return 0; }                   // line 25: a complete B object
    int bad7() { B* p = new B(1); return p != 0; }
    friend int fr(D& d, B& b);
    struct In { int bad8(B& b) { return b.x; } };   // line 28
};
struct DD : D { int bad9(D& d) { return d.x; } };   // line 30: through a D, in a member of DD
int fr(D& d, B& b) { return d.x + b.x; }            // line 31: b.x
struct O {                                          // operators and conversion functions are members like the others
    O() : v(0) {}
    int v;
private:
    O operator+(const O&) const { return *this; }
    bool operator==(const O&) const { return true; }
    int operator[](int) const { return 0; }
    O& operator++() { return *this; }
    O operator++(int) { return *this; }
    O* operator->() { return this; }
    bool operator!() const { return false; }
    operator int() const { return 0; }
    static void* operator new(unsigned long long n);
};
void op1(O& a, O& b) { a + b; }                     // line 46
bool op2(O& a, O& b) { return a == b; }
int op3(O& a) { return a[0]; }
void op4(O& a) { ++a; }
void op5(O& a) { a++; }
int op6(O& a) { return a->v; }
bool op7(O& a) { return !a; }
int op8(O& a) { int i = a; return i; }              // line 53: an implicit conversion
int op9(O& a) { return static_cast<int>(a); }       // an explicit one
int op10(O& a) { if (a) return 1; return 0; }       // a condition
O* op11() { return new O; }                         // line 56: the class's operator new
int main() { return 0; }
