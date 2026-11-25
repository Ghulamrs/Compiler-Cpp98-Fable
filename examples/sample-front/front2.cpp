// front2.cpp - more front-end regressions against cl (test 17 builds it with cl and with cppparse at -O0 and -O2
// and compares the output), kept apart from front.cpp. The units that must be rejected are named with each group.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <typeinfo>
#include <vector>

namespace explorder {   // R5: an explicit specialisation is declared before the first use that would instantiate the
                        // template (14.7.3/6); naming the class before ("Box<char>*", a typedef, a parameter) is no
                        // use, and the types made from the name mean the specialisation. explspec-used.cpp is rejected
template <class T> struct Box { int k() const { return 1; } T v; };
Box<char>* gp;                               // names Box<char>: no instantiation in C++
typedef Box<char> CharBox;
struct Holder { Box<char>* p; Box<long>* q; };
int take(Box<char>* p);
Box<char>& ref();
template <> struct Box<char> { int k() const { return 2; } char v; int extra; };
int take(Box<char>* p) { return p->k() * 10 + p->extra; }
Box<char>& ref() { static Box<char> b; b.extra = 7; return b; }

template <class T> struct Fwd { int k() const { return 1; } };
template <> struct Fwd<int>;                 // declared, used as an incomplete type, defined later
int viaFwd(Fwd<int>* p);
template <> struct Fwd<int> { int k() const { return 3; } };
int viaFwd(Fwd<int>* p) { return p->k(); }

template <class T> struct Mem { int f() const; static int s; static const int c; };
template <class T> int Mem<T>::f() const { return 1; }
template <class T> int Mem<T>::s = 1;
template <class T> const int Mem<T>::c = 1;
template <> int Mem<int>::f() const { return 2; }
template <> int Mem<int>::s = 5;             // the static data member of Mem<int> alone
template <> const int Mem<char>::c = 9;
int members() { Mem<int> a; Mem<long> b; return a.f() * 1000 + b.f() * 100 + Mem<int>::s * 10 + Mem<long>::s + Mem<char>::c * 10000 + Mem<int>::c * 100000; }

template <class T> int fn(T) { return 1; }
int fn(int) { return 3; }                    // the non-template is chosen below: fn<int> is not instantiated
int plain() { return fn(1); }
template <> int fn<int>(int) { return 2; }
template <class T> int viaTemplate(T t) { return fn<T>(t); }   // instantiated after the specialisation below
template <> int fn<long>(long);              // declared before its use, defined after it
int early() { return fn(1L); }
template <> int fn<long>(long) { return 4; }

// a class named before whose members need the specialised one (a member of its type, a member typedef read
// through it) is declared with the specialisation: its layout and its member types follow it
template <class T> struct Part { int k() const { return 1; } T v; static int s; };
template <class T> int Part<T>::s = 10;
template <class T> struct Whole { Part<T> part; int n; int get() const { return part.k(); } };
Whole<char>* pw;
typedef Whole<char> WholeChar;
template <class T> struct Traits { static int id() { return 1; } typedef T type; };
template <class T, class X = Traits<T> > struct Str { typename X::type v; int id() const { return X::id(); } };
typedef Str<char> CharStr;
template <> struct Part<char> { int k() const { return 2; } char v[24]; static int s; };
int Part<char>::s = 20;
template <> struct Traits<char> { static int id() { return 2; } typedef long type; };
Whole<char> whole;
struct More : Whole<char> { int more; };

void show()
{
    WholeChar x; pw = &x; More d; d.more = 1; CharStr str;
    std::printf("explorder named %d %d %d %d %d %d %d %d\n", static_cast<int>(sizeof(Whole<char>)), static_cast<int>(sizeof(More)), pw->get(), whole.get(), Part<char>::s, Part<int>::s,
                str.id(), static_cast<int>(sizeof(str.v)));
    CharBox a; a.extra = 4; gp = &a;
    Holder h; h.p = &a; h.q = 0;
    Fwd<int> f; Fwd<long> g;
    std::printf("explorder %d %d %d %d %d %d %d\n", take(h.p), ref().extra + gp->k(), static_cast<int>(sizeof(CharBox)), viaFwd(&f) * 10 + g.k(), members(),
                plain() * 100 + fn<int>(1) * 10 + viaTemplate(1), early());
}
}

namespace friends {     // M12: friend templates and qualified friends are friends (11.4, 14.5.3), and a protected member
                        // is reached from a derived class through that class or one derived from it (11.5).
                        // access-refused.cpp is rejected
struct P;
namespace N { struct Q; void nf(Q& q); struct Y; }
struct Other { int om(P& p); };
template <class T> struct TC;
template <class T> int tf(T t);
template <class T> int tspec(T t);
struct P {
private:
    int secret;
    template <class U> friend struct TC;            // every TC<U>
    template <class U> friend int tf(U u);          // every tf<U>
    template <class U> friend struct Later;         // not declared yet
    friend int Other::om(P& p);                     // a member function of another class
    friend int tspec<>(int);                        // a specialisation of a function template
public:
    P() : secret(7) {}
};
struct N::Q { private: int qs; friend void N::nf(Q& q); friend struct N::Y; public: Q() : qs(1) {} };
void N::nf(Q& q) { q.qs = 2; }
struct N::Y { int y(Q& q) { return q.qs; } };
template <class T> struct TC { int get(P& p) { return p.secret; } };
template <class T> int tf(T t) { P p; return p.secret + static_cast<int>(t); }
template <class U> struct Later { int get(P& p) { return p.secret * 2; } };
int Other::om(P& p) { return p.secret * 3; }
template <class T> int tspec(T t) { P p; return p.secret + static_cast<int>(t); }
template <class T> struct Box {
private:
    T v;
    friend struct Box<int>;                         // that specialisation
    template <class U> friend struct Box;           // every Box<U>
    template <class U> friend U peek(Box<U>& b);
public:
    Box() : v(5) {}
    template <class U> int from(Box<U>& o) { return static_cast<int>(o.v) + 1; }
};
template <class U> U peek(Box<U>& b) { return b.v; }

struct B { protected: int x; int f() { return 1; } static int s; static int sf() { return 2; } enum { E = 3 }; typedef int T; B() : x(4) {} };
int B::s = 5;
struct D : B {
    int ok1() { return x + f() + s + sf() + E; }
    int ok2(D& d, D* p) { return d.x + p->x + d.f() + p->f() + d.B::x; }
    int ok3(B& b) { return b.s + b.sf() + B::s + B::sf() + B::x + this->B::x + static_cast<int>(sizeof(T)); }
    int ok4() { int D::*pm = &D::x; int (D::*pf)() = &D::f; return this->*pm + (this->*pf)(); }
    friend int fr(D& d);
    struct In { int ok(D& d) { return d.x; } };
};
struct DD : D { int ok(DD& a) { return a.x + a.f(); } };
int fr(D& d) { return d.x + d.f(); }                // a friend of the derived class, through a D

void show()
{
    P p; TC<int> a; TC<char> b; Later<long> l; Other o; N::Q q; N::Y y; Box<int> bi; Box<long> bl;
    N::nf(q);
    std::printf("friends %d %d %d %d %d %d %d %d %d %d %d\n", a.get(p), b.get(p), tf(1), tf(2L), l.get(p), o.om(p), y.y(q), tspec(1), bi.from(bl), peek(bi), static_cast<int>(peek(bl)));
    D d; DD dd; D::In in;
    std::printf("protected %d %d %d %d %d %d %d\n", d.ok1(), d.ok2(d, &d), d.ok3(d), d.ok4(), dd.ok(dd), fr(d), in.ok(d));
}
}

namespace tiarray {     // B1: the type descriptor of an array type is spelled as cl spells it ("$$BY" + rank, dimensions,
                        // element; a const or volatile element behind "$$CB", "$$CC", "$$CD"), and so is the array a
                        // pointer or a reference refers to ("PEAY02H"); typeid-refused.cpp is rejected
struct S { int a; };
enum E { e0 };
union U { int a; };
class C { };
typedef int A3[3];
int byPtr(int (*p)[3]) { return (*p)[1]; }             // ?byPtr@tiarray@@YAHPEAY02H@Z, as cl decorates it
int byRef(const int (&r)[2]) { return r[1]; }       // ?byRef@tiarray@@YAHAEAY01$$CBH@Z
void show()
{
    int a[3]; const int ca[2] = { 1, 2 }; int m[2][5]; S sa[4]; char str[6]; int* pi[3]; const char* const cc[2] = { 0, 0 }; double (*pm)[7] = 0; volatile long vl[2]; unsigned char big[70000];
    a[0] = 0; m[0][0] = 0; sa[0].a = 0; str[0] = 0; pi[0] = 0; vl[0] = 0; big[0] = 0;
    std::printf("tiarray %s|%s|%s|%s|%s\n", typeid(int[3]).name(), typeid(a).name(), typeid(A3).name(), typeid(const int[2]).name(), typeid(ca).name());
    std::printf("tiarray %s|%s|%s|%s|%s\n", typeid(m).name(), typeid(m[0]).name(), typeid(sa).name(), typeid(str).name(), typeid("abc").name());
    std::printf("tiarray %s|%s|%s|%s|%s\n", typeid(L"ab").name(), typeid(pi).name(), typeid(cc).name(), typeid(pm).name(), typeid(*pm).name());
    std::printf("tiarray %s|%s|%s|%s\n", typeid(vl).name(), typeid(int[2][3][4]).name(), typeid(big).name(), typeid(int(*)[3]).name());
    std::printf("tiarray %s|%s|%s|%s|%s\n", typeid(int[3]).raw_name(), typeid(m).raw_name(), typeid(const int[2]).raw_name(), typeid(sa).raw_name(), typeid(cc).raw_name());
    std::printf("tiarray %s|%s|%s|%s\n", typeid(*pm).raw_name(), typeid(vl).raw_name(), typeid(big).raw_name(), typeid(const S[2]).raw_name());
    std::printf("tiarray %s|%s|%s|%s\n", typeid(const volatile int[2]).raw_name(), typeid(int* const[2]).raw_name(), typeid(const S*[2]).raw_name(), typeid(E[2]).raw_name());
    std::printf("tiarray %s|%s|%s|%s\n", typeid(const E[2]).raw_name(), typeid(bool[1]).raw_name(), typeid(volatile S[3]).raw_name(), typeid(int(*[2])(int)).raw_name());
    std::printf("tiarray %s|%s|%s|%s\n", typeid(int S::*[2]).raw_name(), typeid(const int[2][3]).raw_name(), typeid(int* volatile[2]).raw_name(), typeid(U[10]).raw_name());
    std::printf("tiarray %s|%s|%s\n", typeid(C[11]).raw_name(), typeid(const C[16]).raw_name(), typeid(const char* const volatile[1]).raw_name());
    std::printf("tiarray %s|%s|%s|%s\n", typeid(const int(*)[3]).raw_name(), typeid(int(*)[2][3]).raw_name(), typeid(S(*const)[2]).raw_name(), typeid(const S(*)[2]).raw_name());
    std::printf("tiarray %s|%s|%s\n", typeid(int(**)[3]).raw_name(), typeid(void(*)(int(*)[3])).raw_name(), typeid(void(int(&)[3])).raw_name());
    a[1] = 6;
    std::printf("tiarray %d %d\n", byPtr(&a), byRef(ca));
    std::printf("tiarray %d %d %d %d %d\n", typeid(a) == typeid(int[3]), typeid(a) == typeid(int*), typeid(ca) == typeid(int[2]), typeid(m[1]) == typeid(int[5]), typeid(a) == typeid(int[4]));
}
}

namespace newarr {      // rows 21, B6: a multidimensional array is allocated and deleted as one array of its innermost
                        // elements (their number in the cookie, the class's own operator new[] and delete[], cl's
                        // sizes), new A3 for a typedef of an array, and the placement forms: an operator new[] with
                        // arguments (a cookie as the plain one, the matching operator delete[] when a constructor
                        // throws), a class's own operator new(size_t, void*), a null result that builds nothing.
                        // new-refused.cpp is rejected
struct Arena { char buf[512]; int used; int freed; Arena() : used(0), freed(0) {} };
struct Pool { int tag; };
}
void* operator new[](size_t n, newarr::Arena& a) { std::printf("newarr g new[] %d\n", static_cast<int>(n)); void* p = a.buf + a.used; a.used += static_cast<int>(n); return p; }
void operator delete[](void* p, newarr::Arena& a) { std::printf("newarr g delete[] placement %d\n", static_cast<int>(static_cast<char*>(p) - a.buf)); a.freed++; }
void* operator new[](size_t n, int tag, int tag2) { std::printf("newarr g new[] tag %d %d %d\n", static_cast<int>(n), tag, tag2); return std::malloc(n); }
void* operator new(size_t n, newarr::Pool* pool) { std::printf("newarr g new pool %d %d\n", static_cast<int>(n), pool->tag); return std::malloc(n); }
void* operator new(size_t n, newarr::Arena& a, int z) throw() { std::printf("newarr g new %d %d\n", static_cast<int>(n), z); if (z) return 0; void* p = a.buf + a.used; a.used += static_cast<int>(n); return p; }
void operator delete(void* p, newarr::Arena& a, int z) { std::printf("newarr g delete placement %d %d\n", static_cast<int>(static_cast<char*>(p) - a.buf), z); }
void* operator new[](size_t n, const char* why) throw() { std::printf("newarr null new[] %d %s\n", static_cast<int>(n), why); return 0; }
void* operator new(size_t n, const char* why) throw() { std::printf("newarr null new %d %s\n", static_cast<int>(n), why); return 0; }
namespace newarr {
static int live = 0, boom = -1;
struct S { int id; S() : id(live) { if (live == boom) throw 5; ++live; } ~S() { --live; std::printf("newarr ~S %d\n", id); }
    static void* operator new[](size_t n) { std::printf("newarr S new[] %d\n", static_cast<int>(n)); return std::malloc(n); }
    static void operator delete[](void* p) { std::printf("newarr S delete[]\n"); std::free(p); } };
struct T { int id; T() : id(1) {}
    static void* operator new[](size_t n) { std::printf("newarr T new[] %d\n", static_cast<int>(n)); return std::malloc(n); }
    static void operator delete[](void* p, size_t n) { std::printf("newarr T delete[] %d\n", static_cast<int>(n)); std::free(p); } };
struct U { int id; U() : id(live) { if (live == boom) throw 4; ++live; } ~U() { --live; }
    static void* operator new[](size_t n) { std::printf("newarr U new[] %d\n", static_cast<int>(n)); return std::malloc(n); }
    static void operator delete[](void* p, size_t n) { std::printf("newarr U delete[] %d\n", static_cast<int>(n)); std::free(p); } };
struct G { int id; G() : id(live) { if (live == boom) throw 6; ++live; } ~G() { --live; std::printf("newarr ~G %d\n", id); } };
struct V { int id; V() : id(1) {} virtual ~V() {}
    static void* operator new[](size_t n) { std::printf("newarr V new[] %d\n", static_cast<int>(n)); return std::malloc(n); }
    static void operator delete[](void* p, size_t n) { std::printf("newarr V delete[] %d\n", static_cast<int>(n)); std::free(p); } };
struct Q { int a; Q() : a(2) {} };
struct C { int id; C() : id(live) { if (live == boom) throw 8; ++live; } ~C() { --live; std::printf("newarr ~C %d\n", id); }
    static void* operator new[](size_t n, Arena& a, int k) { std::printf("newarr C new[] %d %d\n", static_cast<int>(n), k); void* p = a.buf + a.used; a.used += static_cast<int>(n); return p; }
    static void operator delete[](void* p, Arena& a, int k) { std::printf("newarr C delete[] placement %d %d\n", static_cast<int>(static_cast<char*>(p) - a.buf), k); }
    static void operator delete[](void*) { std::printf("newarr C delete[]\n"); } };
struct M { int id; M() : id(4) {}
    static void* operator new(size_t n, void* p) { std::printf("newarr M new %d\n", static_cast<int>(n)); return p; }
    static void* operator new[](size_t n, void* p) { std::printf("newarr M new[] %d\n", static_cast<int>(n)); return p; } };
struct N { int id; N() : id(5) { std::printf("newarr N\n"); } ~N() {} };
typedef G G3[3];
typedef G G23[2][3];
int cookie(void* p) { return static_cast<int>(static_cast<long*>(p)[-2]); }
void dims(int n)
{
    S (*s)[3] = new S[n][3]; std::printf("newarr %d %d\n", s[1][2].id, cookie(s)); delete[] s;
    T (*t)[3] = new T[n][3]; std::printf("newarr %d\n", t[1][2].id); delete[] t;
    U (*u)[3][2] = new U[n][3][2]; std::printf("newarr %d %d\n", u[1][2][1].id, cookie(u)); delete[] u;
    G (*g)[3] = new G[n][3]; std::printf("newarr %d %d\n", g[1][2].id, cookie(g)); delete[] g;
    V (*v)[3] = new V[n][3]; std::printf("newarr %d %d\n", v[1][2].id, cookie(v)); delete[] v;
    V (*v0)[3] = new V[n - 2][3]; std::printf("newarr %d\n", cookie(v0)); delete[] v0;
    G* g3 = new G3; std::printf("newarr %d %d\n", cookie(g3), g3[2].id); delete[] g3;
    G (*g23)[3] = new G23; std::printf("newarr %d %d\n", cookie(g23), g23[1][2].id); delete[] g23;
    Q (*q)[4] = new Q[n][4]; std::printf("newarr %d\n", q[1][3].a); delete[] q;
    int (*ip)[5] = new int[n + 1][5]; ip[2][4] = 9; std::printf("newarr %d\n", ip[2][4]); delete[] ip;
    int (*iz)[5] = new int[n + 1][5](); std::printf("newarr %d\n", iz[2][4]); delete[] iz;
    S (*s0)[2] = 0; delete[] s0;
    S (*sg)[2] = ::new S[n][2]; std::printf("newarr %d\n", sg[1][1].id); ::delete[] sg;
    const S (*cs)[2] = new const S[1][2]; delete[] cs;
    boom = 3; live = 0;
    try { S (*x)[2] = new S[n][2]; std::printf("newarr no %d\n", x != 0); } catch (int e) { std::printf("newarr caught %d live %d\n", e, live); }
    boom = 4; live = 0;
    try { U (*x)[3] = new U[n][3]; std::printf("newarr no %d\n", x != 0); } catch (int e) { std::printf("newarr caught %d live %d\n", e, live); }
    try { G (*x)[3] = new G[n][3]; std::printf("newarr no %d\n", x != 0); } catch (int e) { std::printf("newarr caught %d live %d\n", e, live); }
    boom = -1; live = 0;
}
void placed()
{
    Arena a;
    G* d = new (a) G[3]; std::printf("newarr %d %d %d\n", static_cast<int>(reinterpret_cast<char*>(d) - a.buf), a.used, cookie(d)); for (int i = 2; i >= 0; --i) d[i].~G();
    Q* p = new (a) Q[2]; std::printf("newarr %d %d %d\n", static_cast<int>(reinterpret_cast<char*>(p) - a.buf), a.used, p[1].a);
    Q (*p2)[2] = new (a) Q[2][2]; std::printf("newarr %d %d %d\n", static_cast<int>(reinterpret_cast<char*>(p2) - a.buf), a.used, p2[1][1].a);
    G (*d2)[2] = new (a) G[2][2]; std::printf("newarr %d %d %d %d\n", static_cast<int>(reinterpret_cast<char*>(d2) - a.buf), a.used, d2[1][1].id, cookie(d2)); for (int i = 3; i >= 0; --i) d2[0][i].~G();
    int* q = new (4, 5) int[6]; q[5] = 1; std::free(q);
    boom = 2; live = 0;
    try { G* e = new (a) G[4]; std::printf("newarr no %d\n", e != 0); } catch (int x) { std::printf("newarr caught %d live %d freed %d\n", x, live, a.freed); }
    boom = 5; live = 0;
    try { G (*e)[3] = new (a) G[2][3]; std::printf("newarr no %d\n", e != 0); } catch (int x) { std::printf("newarr caught %d live %d freed %d\n", x, live, a.freed); }
    boom = 1; live = 0;
    try { C* e = new (a, 9) C[4]; std::printf("newarr no %d\n", e != 0); } catch (int x) { std::printf("newarr caught %d live %d\n", x, live); }
    boom = -1; live = 0;
    C* c = new (a, 11) C[2]; std::printf("newarr %d\n", c[1].id); delete[] c;
    N* n = new ("why") N[2]; std::printf("newarr %d\n", n == 0);
    N* n1 = new ("one") N; std::printf("newarr %d\n", n1 == 0);
    G (*nt)[3] = new (std::nothrow) G[2][3]; std::printf("newarr %d %d\n", nt[1][2].id, cookie(nt)); delete[] nt;
    long buf[32];
    G (*pb)[3] = new (buf) G[2][3]; std::printf("newarr %d %d\n", pb[1][2].id, reinterpret_cast<long*>(pb) == buf); for (int i = 5; i >= 0; --i) pb[0][i].~G();
    M* m = new (buf) M; std::printf("newarr %d %d\n", m->id, reinterpret_cast<long*>(m) == buf);
    M* m2 = new (buf) M[2]; std::printf("newarr %d %d\n", m2[1].id, reinterpret_cast<long*>(m2) == buf);
    M* m3 = ::new (buf) M; std::printf("newarr %d\n", m3->id);
    Pool pool; pool.tag = 12;
    Q* pq = new (&pool) Q; std::printf("newarr %d\n", pq->a); std::free(pq);
    boom = 1; live = 0;
    try {
        G* t = new (a, 0) G; std::printf("newarr ok %d\n", t->id); t->~G();
        G* u = new (a, 1) G; std::printf("newarr null %d\n", u == 0);
        live = 1;
        G* v = new (a, 0) G; std::printf("newarr no %d\n", v != 0);
    } catch (int x) { std::printf("newarr caught %d live %d\n", x, live); }
    boom = -1; live = 0;
}
void show() { dims(2); placed(); std::printf("newarr live %d\n", live); }
}

namespace mpdown {      // row 18: pointers to members converted along non-virtual bases between any two inheritance models,
                        // down (static_cast, 5.2.9/9) and up, as cl converts them: a member function's pointer down
                        // to a base of multiple inheritance, data and functions down between two classes with virtual
                        // bases (the vbtable entry as the base's own table has it), and to and from the unspecified
                        // model (sizes taken while the class was incomplete). memptr-refused.cpp keeps the
                        // conversions through a virtual base, which 4.11/2 makes ill-formed
// the words of a representation from word 'from' on (a function's code pointer and the padding are left out)
template <class T> void words(const char* tag, T v, int from, int n)
{
    int w[8]; std::memset(w, 0, sizeof w); std::memcpy(w, &v, sizeof v);
    std::printf(" %s[%d]", tag, static_cast<int>(sizeof v));
    for (int i = from; i < static_cast<int>(sizeof v / 4) && i < from + n; ++i) std::printf(" %d", w[i]);
}
struct S { int s; S() : s(70) {} int fs() { return s + 1; } };
struct S2 { int s2; S2() : s2(71) {} int fs2() { return s2 + 1; } };
struct A { int a; A() : a(1) {} virtual int fa() { return a; } };
struct B { int b; B() : b(2) {} int fb() { return b; } virtual int vb() { return b + 100; } };
struct M : A, B { int m; M() : m(3) {} int fm() { return m; } };
struct P { int p; P() : p(4) {} virtual int vp() { return p; } };
struct D : P, M { int d; D() : d(5) {} int fd() { return d; } };
struct VM : M, virtual S { int vm; VM() : vm(6) {} int fvm() { return vm; } };
struct VB1 : virtual S { int v1; VB1() : v1(7) {} int f1() { return v1; } };
struct Pad { int pad; Pad() : pad(4) {} };
struct VD : Pad, VB1, virtual S2 { int vd; VD() : vd(8) {} };
struct VS : VB1 { int vs; VS() : vs(9) {} };                                       // shares VB1's vbptr
struct W : virtual S2, virtual S { int w; W() : w(10) {} int fw() { return w; } };
struct VW : Pad, virtual S, W { int vw; VW() : vw(11) {} };
struct VW2 : VB1, W { int vw; VW2() : vw(12) {} };                                 // VB1's vbptr: S, S2; W's own table: S2, S
struct I1; struct I2; struct J1; struct K2;
int sizes = sizeof(int I1::*) + sizeof(int I2::*) + sizeof(int J1::*) + sizeof(int K2::*);
struct I1 { int a; I1() : a(30) {} int fa() { return a + 4; } };
struct Q { int q[3]; Q() { q[0] = 1; } };
struct Q2 { int q2[5]; Q2() { q2[0] = 1; } };
struct Q3 { int q3[7]; Q3() { q3[0] = 1; } };
struct I2 : Q, I1 { int b; I2() : b(40) {} int fb() { return b + 4; } };            // unspecified, no virtual bases
struct N1 { int n; N1() : n(60) {} int fn() { return n + 4; } };
struct N2 : Q, N1 { int n2; N2() : n2(61) {} int fn2() { return n2 + 4; } };
struct J1 : Q2, N2 { int j; J1() : j(62) {} };                                     // unspecified, its base is not
struct J2 : Q2, N2, virtual S { int j; J2() : j(64) {} };
struct K1 : Q3, J1 { int k; K1() : k(63) {} };                                     // its base is unspecified
struct NV : Q, virtual S { int nv; NV() : nv(65) {} int fnv() { return nv + 4; } };
struct K2 : Q2, NV { int k; K2() : k(66) {} };                                     // unspecified, a virtual base below

int (M::*downDM(int (D::*f)()))() { return static_cast<int (M::*)()>(f); }
int (M::*downVM(int (VM::*f)()))() { return static_cast<int (M::*)()>(f); }
int (VB1::*downVD(int (VD::*f)()))() { return static_cast<int (VB1::*)()>(f); }
int (VB1::*downVS(int (VS::*f)()))() { return static_cast<int (VB1::*)()>(f); }
int (W::*downVW(int (VW::*f)()))() { return static_cast<int (W::*)()>(f); }
int (W::*downVW2(int (VW2::*f)()))() { return static_cast<int (W::*)()>(f); }
int M::* ddownDM(int D::* m) { return static_cast<int M::*>(m); }
int VB1::* ddownVD(int VD::* m) { return static_cast<int VB1::*>(m); }
int VB1::* ddownVS(int VS::* m) { return static_cast<int VB1::*>(m); }
int W::* ddownVW(int VW::* m) { return static_cast<int W::*>(m); }
int W::* ddownVW2(int VW2::* m) { return static_cast<int W::*>(m); }
int I1::* i21(int I2::* m) { return static_cast<int I1::*>(m); }
int N2::* j1n(int J1::* m) { return static_cast<int N2::*>(m); }                    // unspecified down to multiple inheritance
int (N2::*fj1n(int (J1::*m)()))() { return static_cast<int (N2::*)()>(m); }
int (N1::*fj1n1(int (J1::*m)()))() { return static_cast<int (N1::*)()>(m); }
int J1::* kj(int K1::* m) { return static_cast<int J1::*>(m); }                     // multiple inheritance down to unspecified
int (J1::*fkj(int (K1::*m)()))() { return static_cast<int (J1::*)()>(m); }
int K1::* jk(int J1::* m) { return m; }
int (K1::*fjk(int (J1::*m)()))() { return m; }
int K2::* nk2(int NV::* m) { return m; }                                            // virtual bases up to unspecified
int (K2::*fnk2(int (NV::*m)()))() { return m; }
int NV::* k2n(int K2::* m) { return static_cast<int NV::*>(m); }                    // unspecified down to virtual bases
int (NV::*fk2n(int (K2::*m)()))() { return static_cast<int (NV::*)()>(m); }
int J2::* n2j2(int N2::* m) { return m; }
void show()
{
    D d; VM vm; VD vd; VS vs; VW vw; VW2 vw2; I2 i2; J1 j1; K1 k1; K2 k2;
    M& dm = d; M& vmm = vm; VB1& vd1 = vd; VB1& vs1 = vs; W& vww = vw; W& vww2 = vw2; I1& i21r = i2; N2& j1n2 = j1; N1& j1n1 = j1; J1& k1j = k1; NV& k2nv = k2;
    std::printf("mpdown DM");
    words("fm", downDM(&D::fm), 2, 1); words("fb", downDM(&D::fb), 2, 1); words("vb", downDM(&D::vb), 2, 1); words("fa", downDM(&D::fa), 2, 1);
    words("m", ddownDM(&D::m), 0, 1); words("b", ddownDM(&D::b), 0, 1); words("0", ddownDM(0), 0, 1);
    std::printf(" | %d %d %d %d %d %d %d\n", (dm.*downDM(&D::fm))(), (dm.*downDM(&D::fb))(), (dm.*downDM(&D::vb))(), (dm.*downDM(&D::fa))(), downDM(0) == 0, dm.*ddownDM(&D::m), dm.*ddownDM(&D::b));
    std::printf("mpdown VM");
    words("fm", downVM(&VM::fm), 2, 1); words("fb", downVM(&VM::fb), 2, 1); words("vb", downVM(&VM::vb), 2, 1);
    std::printf(" | %d %d %d %d\n", (vmm.*downVM(&VM::fm))(), (vmm.*downVM(&VM::fb))(), (vmm.*downVM(&VM::vb))(), downVM(0) == 0);
    std::printf("mpdown VD");
    words("f1", downVD(&VD::f1), 2, 2); words("fs", downVD(&VD::fs), 2, 2); words("v1", ddownVD(&VD::v1), 0, 2); words("s", ddownVD(&VD::s), 0, 2); words("0", ddownVD(0), 0, 2);
    std::printf(" | %d %d %d %d %d %d\n", (vd1.*downVD(&VD::f1))(), (vd1.*downVD(&VD::fs))(), downVD(0) == 0, vd1.*ddownVD(&VD::v1), vd1.*ddownVD(&VD::s), ddownVD(0) == 0);
    std::printf("mpdown VS");
    words("f1", downVS(&VS::f1), 2, 2); words("fs", downVS(&VS::fs), 2, 2); words("v1", ddownVS(&VS::v1), 0, 2); words("s", ddownVS(&VS::s), 0, 2); words("0", ddownVS(0), 0, 2);
    std::printf(" | %d %d %d %d\n", (vs1.*downVS(&VS::f1))(), (vs1.*downVS(&VS::fs))(), vs1.*ddownVS(&VS::v1), vs1.*ddownVS(&VS::s));
    std::printf("mpdown VW");
    words("fw", downVW(&VW::fw), 2, 2); words("fs", downVW(&VW::fs), 2, 2); words("w", ddownVW(&VW::w), 0, 2); words("s", ddownVW(&VW::s), 0, 2); words("s2", ddownVW(&VW::s2), 0, 2); words("0", ddownVW(0), 0, 2);
    std::printf(" | %d %d %d %d %d\n", (vww.*downVW(&VW::fw))(), (vww.*downVW(&VW::fs))(), vww.*ddownVW(&VW::w), vww.*ddownVW(&VW::s), vww.*ddownVW(&VW::s2));
    std::printf("mpdown VW2");
    words("fw", downVW2(&VW2::fw), 2, 2); words("fs", downVW2(&VW2::fs), 2, 2); words("fs2", downVW2(&VW2::fs2), 2, 2);
    words("w", ddownVW2(&VW2::w), 0, 2); words("s", ddownVW2(&VW2::s), 0, 2); words("s2", ddownVW2(&VW2::s2), 0, 2); words("0", ddownVW2(0), 0, 2);
    std::printf(" | %d %d %d %d %d %d\n", (vww2.*downVW2(&VW2::fw))(), (vww2.*downVW2(&VW2::fs))(), (vww2.*downVW2(&VW2::fs2))(), vww2.*ddownVW2(&VW2::w), vww2.*ddownVW2(&VW2::s), vww2.*ddownVW2(&VW2::s2));
    std::printf("mpdown I");
    words("a", i21(&I2::a), 0, 3); words("0", i21(0), 0, 3);
    std::printf(" | %d\n", i21r.*i21(&I2::a));
    std::printf("mpdown J");
    words("n2", j1n(&J1::n2), 0, 1); words("n", j1n(&J1::n), 0, 1); words("0", j1n(0), 0, 1); words("fn2", fj1n(&J1::fn2), 2, 1); words("fn", fj1n(&J1::fn), 2, 1);
    std::printf(" | %d %d %d %d %d\n", j1n2.*j1n(&J1::n2), j1n2.*j1n(&J1::n), (j1n2.*fj1n(&J1::fn2))(), (j1n2.*fj1n(&J1::fn))(), (j1n1.*fj1n1(&J1::fn))());
    std::printf("mpdown K");
    words("j", kj(&K1::j), 0, 3); words("n", kj(&K1::n), 0, 3); words("0", kj(0), 0, 3); words("fn", fkj(&K1::fn), 2, 3); words("fn2", fkj(&K1::fn2), 2, 3);
    words("j", jk(&J1::j), 0, 1); words("n", jk(&J1::n), 0, 1); words("0", jk(0), 0, 1); words("fn", fjk(&J1::fn), 2, 1);
    std::printf(" | %d %d %d %d %d %d %d\n", k1j.*kj(&K1::j), k1j.*kj(&K1::n), (k1j.*fkj(&K1::fn))(), (k1j.*fkj(&K1::fn2))(), k1.*jk(&J1::j), k1.*jk(&J1::n), (k1.*fjk(&J1::fn))());
    std::printf("mpdown K2");
    words("nv", nk2(&NV::nv), 0, 3); words("s", nk2(&NV::s), 0, 3); words("0", nk2(0), 0, 3); words("fnv", fnk2(&NV::fnv), 2, 3); words("fs", fnk2(&NV::fs), 2, 3);
    words("nv", k2n(&K2::nv), 0, 2); words("s", k2n(&K2::s), 0, 2); words("0", k2n(0), 0, 2); words("fnv", fk2n(&K2::fnv), 2, 2); words("fs", fk2n(&K2::fs), 2, 2);
    std::printf(" | %d %d %d %d %d %d %d %d\n", k2.*nk2(&NV::nv), k2.*nk2(&NV::s), (k2.*fnk2(&NV::fnv))(), (k2.*fnk2(&NV::fs))(), k2nv.*k2n(&K2::nv), k2nv.*k2n(&K2::s), (k2nv.*fk2n(&K2::fnv))(), (k2nv.*fk2n(&K2::fs))());
    std::printf("mpdown J2");
    words("n2", n2j2(&N2::n2), 0, 2); words("0", n2j2(0), 0, 2);
    J2 j2;
    std::printf(" | %d %d\n", j2.*n2j2(&N2::n2), sizes);
}
}

namespace elab {        // G4: "struct P" first named in a parameter or a member declaration of a class declares P in the
                        // enclosing namespace or block scope, not in the class (3.3.1/5, 3.4.4)
struct Other { int om(struct P& p); struct Q* q; int on(struct R* r) { return r != 0; } };
struct P { int v; };
struct Q { int w; };
struct R { int z; };
int Other::om(P& p) { return p.v; }
struct Outer { struct In { int f(struct Late* l); }; };
struct Late { int k; };
int Outer::In::f(Late* l) { return l->k; }
void show()
{
    P p; p.v = 4; Q q; q.w = 5; Other o; o.q = &q; R r; r.z = 0;
    Late l; l.k = 6; Outer::In in;
    std::printf("elab %d %d %d %d\n", o.om(p), o.q->w, o.on(&r), in.f(&l));
}
}

namespace deducecv {    // G5: a template parameter deduced through a reference, a pointer or an array keeps the
                        // qualifiers of the argument that the parameter does not spell (14.8.2.1, 14.8.2.4): T& from
                        // a const int is const int, T (&)[N] from "abc" is const char with N 4
template <class T, int N> int len(T (&)[N]) { return N; }
template <class T, unsigned N> int ulen(const T (&a)[N]) { return static_cast<int>(N) + a[0]; }
template <class T, int N, int M> int len2(T (&)[N][M]) { return N * 10 + M; }
template <class T> int byRef(T& t) { return static_cast<int>(sizeof t); }
template <class T> int byPtr(T* t) { return static_cast<int>(sizeof *t); }
template <class T> T* same(T* t) { return t; }
template <class T> T& sameR(T& t) { return t; }
template <class T> int isConst(T&) { return 0; }
template <class T> int isConst(const T&) { return 1; }
template <class T> int pointee(T*) { return 0; }
template <class T> int pointee(const T*) { return 1; }
template <class T> T* first(T (&a)[2]) { return a; }
void show()
{
    int a[7]; const int c[3] = { 1, 2, 3 }; int m[2][5]; a[0] = 0; m[0][0] = 0;
    const int ci = 1; int i = 2; const char* s = "ab"; char buf[2] = { 'x', 0 }; const short cs[2] = { 1, 2 }; volatile int vi = 3;
    const char* r = same(s); const int& rc = sameR(ci); volatile int& rv = sameR(vi); const short* f = first(cs);
    std::printf("deducecv %d %d %d %d %d %d\n", len("abc"), len(a), len(c), ulen(c), len2(m), len(L"ab"));
    std::printf("deducecv %d %d %d %d %d %d %d\n", byRef(ci), byPtr(s), byPtr(&ci), r[0] == 'a', rc + byRef(cs), rv, f[1]);
    std::printf("deducecv %d %d %d %d %d\n", isConst(ci), isConst(i), pointee(s), pointee(buf), pointee(&ci));
}
}

namespace arrparm {     // G6: a parameter declared as an array is a pointer that cl decorates as a const one (Q), also
                        // through a typedef, in back-references, members and templates; the first declaration
                        // decides (test 17 looks for cl's names in the listing)
typedef int A3[3];
int f1(int a[3]) { return a[0]; }
int f2(int a[], int b[]) { return a[0] + b[0]; }
int f3(A3 a, int* p, A3 b) { return a[0] + *p + b[1]; }
int f4(const char* argv[], const int c[2]) { return argv[0][0] + c[1]; }
int f5(int m[2][3], int n[][3]) { return m[0][0] + n[1][2]; }
struct S { int g(int a[4]) const; static int h(char s[]); };
int S::g(int a[4]) const { return a[3]; }
int S::h(char s[]) { return s[0]; }
template <class T> int t1(T a[2]) { return static_cast<int>(a[1]); }
int decl(int a[]);
int decl(int* a) { return a[0]; }
int decl2(int* a);
int decl2(int a[]) { return a[0]; }
void show() { int a[4] = { 1, 2, 3, 4 }; int m[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } }; const char* av[1] = { "x" }; const int c[2] = { 7, 8 }; char s[2] = { 'a', 0 }; S o; double d[2] = { 1.5, 2.5 };
    std::printf("arrparm %d\n", f1(a) + f2(a, a) + f3(a, a, a) + f4(av, c) + f5(m, m) + o.g(a) + S::h(s) + t1(a) + t1(d) + decl(a) + decl2(a));
}
}

namespace qd_ns { template <class T> struct Box { T t; }; template <class T, int N> struct Arr { T a[N]; }; }
namespace qd_other { template <class T> struct Box { T t; }; }
namespace qd_a { namespace b { template <class T> struct Box { T t; }; } }
struct QdOuter { template <class T> struct In { T v; }; };
namespace qualdeduce {  // G8: a template-id written with a qualified name deduces from its last component; only the
                        // nested-name-specifier is a non-deduced context (14.8.2.4/4). The qualifier picks the
                        // template: qd_ns::Box and qd_other::Box are two templates, so each overload takes its own
template <class T> int byRef(const qd_ns::Box<T>& b) { return static_cast<int>(b.t) + 1; }
template <class T> int byVal(qd_ns::Box<T> b) { return static_cast<int>(b.t) + 2; }
template <class T> int byPtr(qd_ns::Box<T>* b) { return static_cast<int>(sizeof b->t); }
template <class T> int global(const ::qd_ns::Box<T>& b) { return static_cast<int>(b.t) * 2; }
template <class T, int N> int count(const qd_ns::Arr<T, N>& a) { return N * 10 + static_cast<int>(sizeof a.a[0]); }
template <class T> int which(const qd_ns::Box<T>&) { return 1; }
template <class T> int which(const qd_other::Box<T>&) { return 2; }
template <class T> int deep(const qd_a::b::Box<T>& b) { return static_cast<int>(b.t) + 100; }
template <class T> int member(const QdOuter::In<T>& i) { return static_cast<int>(i.v) + 200; }
template <class T> int vec(const std::vector<T>& v) { return static_cast<int>(v.size() * sizeof(T)); }
template <class T> T sum(const std::vector<T>* v) { T s = T(); for (size_t i = 0; i < v->size(); ++i) s += (*v)[i]; return s; }
void show()
{
    qd_ns::Box<int> bi; bi.t = 10; qd_ns::Box<double> bd; bd.t = 2.5; qd_other::Box<char> oc; oc.t = 'x';
    qd_ns::Arr<short, 7> ar; qd_a::b::Box<long> ab; ab.t = 5; QdOuter::In<int> in; in.v = 3;
    std::vector<int> v; v.push_back(4); v.push_back(5); std::vector<double> vd; vd.push_back(1.5); vd.push_back(2.0);
    std::printf("qualdeduce %d %d %d %d %d %d\n", byRef(bi), byRef(bd), byVal(bi), byPtr(&bd), global(bi), count(ar));
    std::printf("qualdeduce %d %d %d %d %d %d %d %g\n", which(bi), which(oc), deep(ab), member(in), vec(v), vec(vd), sum(&v), sum(&vd));
}
}

namespace protstatic {  // G10: a protected static member, static member function or enumerator is open to a friend of any
                        // class derived from its class, whichever class names it (11.2/4 of C++98: "a member or friend
                        // of a class P derived from N"; 11.5 restricts only non-static members). cl agrees but refuses
                        // an enumerator named through the base from a friend of D (B::E, C2248), which is left out
struct B { protected: static int s; static int sf() { return 7; } enum { E = 3 }; public: B() {} };
int B::s = 5;
struct D : B { friend int peek(); friend struct Pal; };
struct D2 : private B { friend int peek2(); };
struct DD : D {};
int peek() { return D::s + B::s + D::sf() + B::sf() + D::E + DD::s; }   // a friend of D
struct Pal { int get() { return B::s * 10 + D::sf() + DD::sf(); } };    // a friend class of D
int peek2() { return B::s + D2::sf() + D2::E; }                          // a friend of a class deriving privately
void show()
{
    Pal p;
    std::printf("protstatic %d %d %d\n", peek(), p.get(), peek2());
}
}

namespace specconst {   // G12: an explicitly specialised static const member of a class template is a constant from its
                        // definition on (5.19/1), as an array bound and as a template argument
template <class T> struct A { static const int s; static const long w; };
template <> const int A<int>::s = 2;
template <> const long A<char>::w = 3L * 4;
int arr[A<int>::s];
char buf[A<char>::w + 1];
template <int N> struct Tag { enum { value = N * 10 }; };
void show()
{
    std::printf("specconst %d %d %d %d\n", static_cast<int>(sizeof arr / sizeof arr[0]), static_cast<int>(sizeof buf), static_cast<int>(Tag<A<int>::s>::value), A<int>::s);
}
}

namespace newhide {     // G13: a class's operator new hides the global one for a plain new, but only under the name it
                        // declares: a class with a placement operator new alone still gets the global operator new[]
                        // for an array and the global operator new through ::new. new-refused.cpp has the plain new
                        // of such a class, which is ill-formed
struct Pool { char buf[64]; int used; Pool() : used(0) {} };
struct OnlyPlaced {
    int v;
    OnlyPlaced() : v(7) {}
    static void* operator new(size_t n, Pool& p) { p.used += static_cast<int>(n); return p.buf; }
    static void operator delete(void*, Pool&) {}
};
void show()
{
    Pool pool;
    OnlyPlaced* a = new (pool) OnlyPlaced;      // the class's own form
    OnlyPlaced* b = ::new OnlyPlaced;           // the global operator new
    OnlyPlaced* c = new OnlyPlaced[3];          // the class declares no operator new[]: the global one
    std::printf("newhide %d %d %d %d\n", a->v, b->v, c[2].v, pool.used);
    ::delete b;
    delete[] c;
}
}

namespace opaccess {    // G14: operators and conversion functions are members like the others (11/4): private ones are
                        // open to the class's members and friends, protected ones to a derived class through its own
                        // objects (11.5). access-refused.cpp has the uses that are refused
struct O {
    O(int x) : v(x) {}
    int v;
    int members() { O t(v); ++t; t++; return (t + *this).v + (t == *this) + t[2] + t->v + !t + static_cast<int>(t); }
    friend int viaFriend(O& a, O& b);
private:
    O operator+(const O& o) const { return O(v + o.v); }
    bool operator==(const O& o) const { return v == o.v; }
    int operator[](int i) const { return v * i; }
    O& operator++() { ++v; return *this; }
    O operator++(int) { O old(v); v += 10; return old; }
    O* operator->() { return this; }
    bool operator!() const { return v == 0; }
    operator int() const { return v + 100; }
};
int viaFriend(O& a, O& b) { int i = a; if (b) ++i; return i + (a + b).v + a[3] + (a == b) + a->v; }
struct PB { PB(int x) : v(x) {} int v; protected: PB operator-() const { return PB(-v); } operator long() const { return v * 2L; } };
struct PD : PB { PD(int x) : PB(x) {} int neg() { return (-*this).v; } long twice() { long l = *this; return l; } };
void show()
{
    O a(1), b(2); PD d(21);
    std::printf("opaccess %d %d %d %ld\n", a.members(), viaFriend(a, b), d.neg(), d.twice());
}
}

namespace typeaccess {  // G14: member types (typedefs, nested classes, enumerations) are members like the others. Valid:
                        // the class itself, a derived class for the protected ones, a friend in its body and in its own
                        // declaration, and a member's definition outside its class, whose return type is named before
                        // the member is known (11/5). access-refused.cpp has the uses that are refused
struct A {
    A() : n(3) {}
    int twice();
    friend int viaFriend(A& a);
protected:
    typedef int T;
    struct In { int v; };
    enum E { e0 = 5 };
private:
    typedef long PT;
    PT priv();
    static In make(int v);
    static PT sp;
    int n;
};
A::PT A::priv() { PT x = n; return x * 2; }
A::In A::make(int v) { In i; i.v = v; return i; }
A::PT A::sp = 4;
int A::twice() { return static_cast<int>(priv()) + make(1).v + static_cast<int>(sp); }
struct D : A { T t; In i; E e; D() : t(1) { i.v = 2; e = e0; } int sum() { return t + i.v + e; } };
int viaFriend(A& a) { A::PT p = a.priv(); A::In i = A::make(7); A::T t = 1; return static_cast<int>(p) + i.v + t + A::e0; }
struct G { G() {} private: typedef int T; friend T fr(T); };
G::T fr(G::T x) { return x + 1; }
void show()
{
    A a; D d;
    std::printf("typeaccess %d %d %d %d\n", a.twice(), d.sum(), viaFriend(a), fr(41));
}
}

namespace dtoraccess {  // G14: a destructor is a member like the others: a private one is open to the class's members and
                        // friends, a protected one to the destructor of a derived class. access-refused.cpp has the rest
static int g_dead = 0;
struct P { P() {} static void kill(P* p) { delete p; } static int local() { P p; return 1; } friend void viaFriend(); private: ~P() { ++g_dead; } };
void viaFriend() { P p; P* q = new P; delete q; }
struct Q { Q() {} protected: ~Q() { g_dead += 10; } };
struct R : Q {};                                    // R's implicit destructor reaches Q's protected one
struct S : Q { ~S() { g_dead += 100; } };
void show()
{
    P* p = new P; P::kill(p);
    const int l = P::local();
    viaFriend();
    { R r; S s; }
    R* pr = new R; delete pr;
    std::printf("dtoraccess %d %d\n", l, g_dead);
}
}

namespace friendtmpl {  // G14: a friend function template defined in its class is a template of the enclosing namespace,
                        // found by argument-dependent lookup (11.4/5, 7.3.1.2/3); its body is in the lexical scope of
                        // the class (bonus() is found unqualified). It was not declared at all: a call was S103
struct F {
    F() : secret(4) {}
private:
    int secret;
    static int bonus() { return 30; }
    template <class T> friend int peek(F& f, T t) { return f.secret + static_cast<int>(t) + bonus(); }   // defined in the class
    template <class T> friend int twice(const F& f, T t);                                                // declared only
};
template <class T> int twice(const F& f, T t) { return 2 * (f.secret + static_cast<int>(t)); }
struct W { W(int x) : v(x) {} template <class U> friend bool operator==(const W& a, const U& b) { return a.v == static_cast<int>(b); } private: int v; };
void show()
{
    F f; W w(7);
    std::printf("friendtmpl %d %d %d %d %d\n", peek(f, 1), peek(f, 2.5), twice(f, 3L), w == 7, w == 8.0);
}
}

namespace baseaccess {  // G14: the access of base classes (11.2). Valid: the members and friends of a class reach its
                        // private base's members and convert to that base; a protected base stays reachable in the
                        // classes derived further; a using-declaration gives a member the access of the declaration; a
                        // public base changes nothing. access-refused3.cpp has the uses that are refused
struct B { B() : x(2) {} int x; int f() { return 3; } static int s; enum { E = 4 }; typedef int T; int kept; void pub() {} };
int B::s = 5;
struct Priv : private B {
    Priv() { kept = 7; }
    int own() { return x + f() + s + E; }
    B* self() { return this; }
    friend int viaFriend(Priv& p);
    using B::kept;
    using B::pub;
};
int viaFriend(Priv& p) { B& b = p; return p.x + p.f() + b.x; }
struct Prot : protected B { int own() { return x + f(); } };
struct More : Prot { int deeper() { return x + f() + s + E; } B* base() { return this; } };
struct Pub : B {};
int viaPublic(Pub& p) { B* b = &p; return p.x + b->f() + Pub::s; }
void show()
{
    Priv p; Prot q; More m; Pub u;
    p.pub();
    std::printf("baseaccess %d %d %d %d %d %d %d\n", p.own(), p.self()->x, viaFriend(p), p.kept, q.own(), m.deeper() + m.base()->x, viaPublic(u));
}
}

namespace fnarr {       // G15: an array parameter of a function type inside a declared function's parameter is a const
                        // pointer in cl's decorated name too ("int (*cb)(int[3])": P6AHQEAH@Z); the second use of a
                        // function type is a back-reference however it is written; a reference to a function is A6A
                        // (test 17 reads the names in the listing). G19: a function that returns a pointer to a
                        // function (pick) has the parameters of its own function part only
int one(int a[3]) { return a[0]; }
int fp(int (*cb)(int[3]), int a[3]) { return cb(a); }
int two(int (*a)(int[3]), int (*b)(int*)) { int v[3] = {1, 2, 3}; return a(v) + b(v); }
int two2(int (*b)(int*), int (*a)(int[3])) { int v[3] = {4, 5, 6}; return a(v) + b(v); }
int mix(int (*cb)(int*), int a[3]) { return cb(a); }
int mix2(int (*cb)(int[3]), int* p) { return cb(p); }
int ref(int (&)(char* argv[]), char** v) { return v != 0; }
int inner(int (*x)(int[2]), int a[4]) { return x(a) + a[1]; }
int deep(int (*outer)(int (*x)(int[2]), int[4])) { int v[4] = {7, 8, 9, 10}; return outer(one, v); }
int rep(int (*cb)(int (*x)(int[2]), int (*y)(int[2]), int[5])) { return cb != 0; }
int (*pick(char c[2]))(int[3]) { return c[0] ? one : 0; }
int ret(int (*(*cb)(char[2]))(int[3])) { char c[2] = {1, 0}; int v[3] = {11, 12, 13}; return cb(c)(v); }
int direct() { char c[2] = {1, 0}; int v[3] = {31, 32, 33}; return pick(c)(v); }
struct S { int m(void (*cb)(int[5])) { return cb != 0; } static int sm(int (*cb)(const char[4]), const char s[4]) { return cb != 0 && s != 0; } };
template <class T> int tp(T (*cb)(T[2]), T x) { T v[2] = { x, x }; return static_cast<int>(cb(v)); }
void show()
{
    S s; int v[3] = {4, 5, 6};
    std::printf("fnarr %d %d %d %d %d %d %d %d %d %d %d\n", fp(one, v), two(one, one), two2(one, one), mix(one, v), mix2(one, v), deep(inner), ret(pick), rep(0), s.m(0) + S::sm(0, 0), tp<int>(one, 9), direct());
}
}

namespace fnref {       // G18: a reference to a function is called as the function it refers to (5.2.2/1; it was S118)
                        // and binds to a function, which is an lvalue (3.10/2): as a parameter, as a local and at
                        // namespace scope, to a static member function, and handed on to another reference
int twice(int x) { return 2 * x; }
int count(char* argv[]) { int n = 0; while (argv[n]) ++n; return n; }
int apply(int (&f)(int), int x) { return f(x) + (&f != 0); }
int words(int (&cb)(char* argv[]), char** v) { return cb(v); }
struct K { static int inc(int x) { return x + 1; } };
int (&g_ref)(int) = twice;
int through(int (&f)(int), int (&g)(int), int x) { return f(g(x)); }
void show()
{
    char a[] = "a", b[] = "b"; char* v[] = { a, b, 0 };
    int (&r)(int) = twice;
    int (&k)(int) = K::inc;
    std::printf("fnref %d %d %d %d %d %d\n", apply(twice, 20), words(count, v), r(4), k(1), g_ref(5), through(r, k, 6));
}
}

namespace accessrest {  // G14, the rest. Valid: a private operator delete used by the class and its friend; static_cast to
                        // a private base by the class itself, and a C-style cast from anywhere (5.4/7); member types
                        // through a protected base in a class derived further; a friend naming a private nested
                        // class's members; a friend that destroys the temporaries of calls; the implicit destructor
                        // of a friend class and of a class derived from one with a protected destructor; a throw
                        // that copies with a public copy constructor; a friend function template defined in a class
                        // template, one template for each instantiation; a parenthesised name that ordinary lookup
                        // finds. access-refused4.cpp has the uses that are refused
class OD {
    void operator delete(void* p) { ++freed; ::operator delete(p); }
    void operator delete[](void* p) { freed += 10; ::operator delete[](p); }
public:
    static int freed;
    static void own() { OD* a = new OD; delete a; OD* b = new OD[2]; delete[] b; }
    friend void odFriend();
};
int OD::freed = 0;
void odFriend() { delete new OD; }
struct B2 { int x; B2() : x(4) {} };
struct D2 : private B2 {
    int y;
    D2() : y(5) {}
    B2* up() { return static_cast<B2*>(this); }
    static D2* down(B2* b) { return static_cast<D2*>(b); }
    static B2& ref(D2& d) { return static_cast<B2&>(d); }
};
int cstyle(D2* d) { B2* b = (B2*)d; D2* back = (D2*)b; return b->x + back->y; }
struct B3 { typedef int T; struct N { int v; }; };
struct D3p : protected B3 {};
struct D3q : D3p { T a; D3p::T b; D3p::N n; D3q() : a(1), b(2) { n.v = 3; } };
class A4 {
    class Priv { public: struct Inner { int v; }; static int value; typedef int T; };
    friend int a4friend();
public:
    static int own() { Priv::Inner i; i.v = A4::Priv::value; A4::Priv::T t = 2; return i.v + t; }
};
int A4::Priv::value = 6;
int a4friend() { A4::Priv::Inner i; i.v = A4::Priv::value; return i.v + 1; }
class P6 {
    ~P6() { ++gone; }
public:
    static int gone;
    int v;
    P6() : v(8) {}
    P6(const P6& o) : v(o.v + 1) {}
    friend P6 make6();
    friend int take6(P6 p);
    friend int ok6();
};
int P6::gone = 0;
P6 make6() { return P6(); }
int take6(P6 p) { return p.v; }
int ok6() { P6* p = new P6; int r = take6(*p) + make6().v; delete p; return r; }
class M7 { ~M7() { ++gone; } public: static int gone; M7() {} friend struct F7; };
int M7::gone = 0;
struct F7 { M7 m; M7 two[2]; };
class PB7 { protected: ~PB7() { ++gone; } public: static int gone; };
int PB7::gone = 0;
struct PD7 : PB7 { int k; PD7() : k(1) {} };
struct Thrown { int v; Thrown() : v(9) {} Thrown(const Thrown& o) : v(o.v + 1) {} };
int thrower() { Thrown t; try { throw t; } catch (Thrown& c) { return c.v; } }
template <class U> class CT8 {
    U secret;
    static int bonus() { return 100; }
public:
    CT8(U s) : secret(s) {}
    template <class T> friend int peek8(CT8& c, T t) { return static_cast<int>(c.secret) + static_cast<int>(t) + bonus(); }
    template <class T> friend bool operator==(const CT8& a, const T& t) { return a.secret == static_cast<U>(t); }
    friend int plain8(CT8& c) { return static_cast<int>(c.secret); }
};
int twice(int x) { return 2 * x; }
void show()
{
    OD::own(); odFriend();
    D2 d; D3q q;
    std::printf("accessrest %d %d %d %d %d %d %d\n", OD::freed, d.up()->x + D2::down(d.up())->y + D2::ref(d).x, cstyle(&d), q.a + q.b + q.n.v, A4::own(), a4friend(), ok6());
    { F7 f; PD7 pd; (void)f; (void)pd; }
    CT8<int> a(3); CT8<long> b(40L); CT8<int> a2(5);
    std::printf("accessrest %d %d %d %d %d %d %d %d %d %d %d\n", P6::gone, M7::gone, PB7::gone, thrower(), peek8(a, 1), peek8(a, 'a'), peek8(b, 2), peek8(a2, 2.5), a == 3, (b == 41) + plain8(a) + plain8(b), (twice)(21));
}
}

namespace fnarr2 {      // G15, the rest: an array parameter of a function type is cl's const pointer (Q) in the type a
                        // declared function returns, in a function type named through a typedef (the typedef's
                        // spelling goes with its name: CB is Q, CBP is P, and the first of the two in a name is the
                        // one written) and in a variable's type, a static member's too (test 17 reads the names)
int one(int a[3]) { return a[0]; }
int (*pick(char c[2]))(int[3]) { return c[0] ? one : 0; }
int (*(*pick3(int))(char[2]))(int[3]) { return pick; }
typedef int CB(int[3]);
typedef int CBP(int*);
typedef int (*PCB)(int[3]);
int t1(CB* cb) { int v[3] = {1, 2, 3}; return cb(v); }
int t2(CBP* cb) { int v[3] = {4, 5, 6}; return cb(v); }
int t3(PCB cb) { int v[3] = {7, 8, 9}; return cb(v); }
int t4(CB* a, CBP* b) { return a == b; }
int t5(CBP* a, CB* b) { return a == b; }
int t6(CB& r) { int v[3] = {10, 11, 12}; return r(v); }
int (*gp)(int[3]) = one;
int (*gp2)(int*) = one;
CB* gp3 = one;
PCB gp4 = one;
int (*garr[2])(int[3]) = { one, one };
int (*(*gpp)(char[2]))(int[3]) = pick;
struct S { static int (*sm)(int[3]); static CB* sm2; };
int (*S::sm)(int[3]) = one;
CB* S::sm2 = one;
void show()
{
    char c[2] = {1, 0}; int v[3] = {20, 21, 22};
    std::printf("fnarr2 %d %d %d %d %d %d %d %d %d\n", pick(c)(v), pick3(0)(c)(v), t1(one), t2(one), t3(one), t4(one, one) + t5(one, one), t6(one),
                gp(v) + gp2(v) + gp3(v) + gp4(v) + garr[1](v), gpp(c)(v) + S::sm(v) + S::sm2(v));
}
}

namespace partialexact {   // G31: a partial specialisation matches an argument list only when its pattern with the deduced
                           // arguments is that list (14.5.4.1): a type the pattern spells out must be the argument
                           // (X<T, yes> was taken for X<char, no>), a const in the pattern needs one in the argument
                           // (X<const T*, no> was taken for X<char*, no>) and one in the argument stays in what T is
struct yes {};
struct no {};
template <class T, class U> struct X { enum { which = 0 }; };
template <class T> struct X<T, int> { enum { which = 1 }; };
template <class T> struct X<T, yes> { enum { which = 2 }; };
template <class T> struct X<const T*, no> { enum { which = 3 }; };
template <class T> struct X<T*, no> { enum { which = 4 }; };
template <class T> struct X<T, T> { enum { which = 5 }; };
template <class T> struct X<T, T*> { enum { which = 6 }; };
template <class T> struct Q { enum { which = 0 }; };
template <class T> struct Q<const T> { enum { which = 1 }; };
template <class T> struct Q<T*> { enum { which = 2 }; };
template <class T> struct Q<T&> { enum { which = 3 }; };
template <class T> struct Q<T[4]> { enum { which = 4 }; };
template <class T> struct Strip { typedef T type; };
template <class T> struct Strip<const T> { typedef T type; };
template <class T> struct IsInt { enum { v = 0 }; };
template <> struct IsInt<int> { enum { v = 1 }; };
template <class T> struct IsInt<T*> { enum { v = 2 }; };
template <class T, int N> struct W { enum { which = 0 }; };
template <class T> struct W<T, 2> { enum { which = 1 }; };
template <int N> struct W<char, N> { enum { which = 2 }; };
// the dispatch the library's containers use (include/__iterator, __range_category): the second argument chooses
template <class It, class Integer> struct Cat2 { typedef typename It::category type; };
template <class It> struct Cat2<It, yes> { typedef yes type; };
template <class T> struct IsInteger { typedef no type; };
template <> struct IsInteger<int> { typedef yes type; };
template <class It> struct Cat { typedef typename Cat2<It, typename IsInteger<It>::type>::type type; };
struct Iter { typedef no category; };
int name(yes) { return 1; }
int name(no) { return 2; }
// a pattern over a template with value arguments: each instantiation has its own values (the library's
// __default_facet<moneypunct<charT, International> >)
template <class T, bool B> struct Flag {};
template <class T, int N> struct Arr {};
template <class F> struct Of { enum { which = 0 }; };
template <class C, bool I> struct Of<Flag<C, I> > { enum { which = 1 }; };
template <class T, int N> struct Of<Arr<T, N> > { enum { which = 2 }; };
void show()
{
    std::printf("partialexact X %d %d %d %d %d %d %d %d %d\n", static_cast<int>(X<char, long>::which), static_cast<int>(X<char, int>::which), static_cast<int>(X<char, yes>::which),
                static_cast<int>(X<char, no>::which), static_cast<int>(X<const char*, no>::which), static_cast<int>(X<char*, no>::which),
                static_cast<int>(X<long, long>::which), static_cast<int>(X<long, long*>::which), static_cast<int>(X<long, char*>::which));
    std::printf("partialexact Q %d %d %d %d %d %d %d\n", static_cast<int>(Q<int>::which), static_cast<int>(Q<const int>::which), static_cast<int>(Q<int*>::which),
                static_cast<int>(Q<const int*>::which), static_cast<int>(Q<int&>::which), static_cast<int>(Q<int[4]>::which), static_cast<int>(Q<int[5]>::which));
    std::printf("partialexact %d %d %d %d %d %d %d %d %d\n", static_cast<int>(IsInt<Strip<const int>::type>::v), static_cast<int>(IsInt<Strip<int>::type>::v), static_cast<int>(IsInt<Strip<int* const>::type>::v),
                static_cast<int>(W<int, 1>::which), static_cast<int>(W<int, 2>::which), static_cast<int>(W<char, 3>::which), static_cast<int>(W<long, 3>::which),
                name(Cat<int>::type()), name(Cat<Iter>::type()));
    std::printf("partialexact values %d %d %d %d %d %d %d\n", static_cast<int>(Of<Flag<char, true> >::which), static_cast<int>(Of<Flag<wchar_t, false> >::which),
                static_cast<int>(Of<Flag<char, false> >::which), static_cast<int>(Of<Arr<char, 0> >::which), static_cast<int>(Of<Arr<char, 1> >::which),
                static_cast<int>(Of<Arr<long, -1> >::which), static_cast<int>(Of<int>::which));
}
}

namespace baserank {    // G31: of two conversions of a class to two of its bases the one to the nearer base is better
                        // (13.3.3.2/4): the object, a reference bound to it, a pointer to it; the calls were ambiguous
                        // (S106). It is what tells f(forward_iterator_tag) from f(input_iterator_tag) for the tag of
                        // a bidirectional iterator
struct A {};
struct B : public A {};
struct C : public B {};
struct D : public C {};
int f(A) { return 1; }
int f(B) { return 2; }
int g(const A&) { return 1; }
int g(const B&) { return 2; }
int g(const C&) { return 3; }
int h(A*) { return 1; }
int h(B*) { return 2; }
int k(const A*) { return 1; }
int k(const C*) { return 3; }
int k(void*) { return 9; }
template <class T> int t(T, A) { return 1; }
template <class T> int t(T, B) { return 2; }
int two(A, B) { return 1; }
int two(B, B) { return 2; }
void show()
{
    C c; D d;
    const D cd = D();
    std::printf("baserank %d %d %d %d %d %d %d %d %d %d %d\n", f(c), f(d), g(c), g(d), g(cd), h(&c), h(&d), k(&d), k(&c), t(1, d), two(c, d));
}
}

namespace argdeduce {   // G31: a template argument is deduced as it is (14.8.2.4): Box<U> from a Box<const int> is
                        // U = const int, so the converting constructor template takes it (it was S107: U was int)
template <class T> struct Box {
    T v;
    Box(const T& x) : v(x) {}
    template <class U> Box(const Box<U>& o) : v(o.v) {}
};
template <class A, class B> struct Two {
    A a; B b;
    Two(const A& x, const B& y) : a(x), b(y) {}
    template <class U, class W> Two(const Two<U, W>& o) : a(o.a), b(o.b) {}
};
template <class T> int constness(const Box<T>&) { return 1; }
template <class T> int constness(const Box<const T>&) { return 2; }
template <class T, class It> T* build(void* at, It first) { return new (at) T(*first); }
void show()
{
    Box<const int> bc(5);
    Box<int> bi(bc);
    Box<long> bl(bi);
    Box<int> copy = bc;
    Two<const int, int> ka(1, 2);
    Two<int, int> kb(ka);
    Two<long, long> kc = kb;
    const Two<const int, int>* pa = &ka;
    char raw[sizeof(Two<int, int>)];
    Two<int, int>* made = build<Two<int, int>, const Two<const int, int>*>(raw, pa);
    std::printf("argdeduce %d %d %d %d %d %d %d %d %d %d\n", bi.v, static_cast<int>(bl.v), copy.v, kb.a, kb.b, static_cast<int>(kc.b), made->a, made->b, constness(bi), constness(bc));
}
}

int main()
{
    explorder::show();
    friends::show();
    tiarray::show();
    newarr::show();
    mpdown::show();
    elab::show(); deducecv::show();
    arrparm::show();
    qualdeduce::show();
    protstatic::show();
    specconst::show();
    newhide::show();
    opaccess::show();
    typeaccess::show();
    dtoraccess::show();
    friendtmpl::show();
    baseaccess::show();
    fnarr::show();
    fnref::show();
    fnarr2::show();
    accessrest::show();
    partialexact::show();
    baserank::show();
    argdeduce::show();
    return 0;
}
