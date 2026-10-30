// diagnostics.cpp - must be rejected: the ill-formed lines cl rejects and cppparse accepted (the 2026-10-15 review's
// diag.cpp and syntax.cpp, M12): private and protected members, abstract classes, an implicit copy assignment with
// a reference member, and the parser's messages for a missing ')' and a broken initializer
class Priv { int secret; public: Priv() : secret(1) {} int get() const { return secret; } };
struct Ab { virtual int f() = 0; };
struct RefMem { int& r; RefMem(int& x) : r(x) {} };
struct Holder { RefMem m; Holder(int& x) : m(x) {} };
struct Base { protected: int prot; Base() : prot(2) {} private: int priv; };
struct Der : Base { int get() { return prot; } int bad() { return priv; } };
class Hidden { Hidden() {} public: static Hidden* make() { return new Hidden; } };
class Friendly { int secret; friend int peek(const Friendly&); friend struct Peeker; public: Friendly() : secret(4) {} };
int peek(const Friendly& f) { return f.secret; }
struct Peeker { int look(const Friendly& f) { return f.secret; } };
int syntax(int a { return a; }
int main()
{
    Priv p; int s = p.secret;
    Ab ab;
    Ab* pa = new Ab;
    int x; RefMem a(x), b(x); a = b;
    Holder h1(x), h2(x); h1 = h2;
    Der d; d.prot = 3;
    Hidden h;
    int v = 3 +;
    Friendly f;
    return s + d.get() + peek(f) + Peeker().look(f);
}
