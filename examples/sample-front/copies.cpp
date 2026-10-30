// copies.cpp - copies and temporaries (test 17): built by cppparse --compile6 and by cl, the two programs must
// print the same log: how many copies are made, and when each object (temporaries included) is destroyed. cl
// /Od copies a returned named local, /O2 builds it in the result (NRVO): -O0 is compared with the first, -O2 with
// the second. By-value parameters are destroyed left to right, as cl does
#include <cstdio>

static int nextId = 0;

struct V {
    int id; int val;
    V(int v) : id(++nextId), val(v) { std::printf("  V%d(%d)\n", id, val); }
    V(const V& o) : id(++nextId), val(o.val) { std::printf("  V%d copy of V%d\n", id, o.id); }
    ~V() { std::printf("  ~V%d\n", id); }
};

struct D : V {
    int extra;
    D(int v) : V(v), extra(v * 2) { std::printf("  D on V%d\n", id); }
    D(const D& o) : V(o), extra(o.extra) { std::printf("  D copy on V%d\n", id); }
    ~D() { std::printf("  ~D on V%d\n", id); }
};

struct W { int w; W(int x) : w(x) { std::printf("  W(%d)\n", w); } W(const W& o) : w(o.w) { std::printf("  W copy\n"); } ~W() { std::printf("  ~W(%d)\n", w); } };

struct DV : virtual V {
    int extra;
    DV(int v) : V(v), extra(v + 1) { std::printf("  DV on V%d\n", id); }
    DV(const DV& o) : V(o), extra(o.extra) { std::printf("  DV copy on V%d\n", id); }
    ~DV() { std::printf("  ~DV on V%d\n", id); }
};

D mkD(int v) { return D(v); }
DV mkDV(int v) { return DV(v); }
V mkV(int v) { return V(v); }

int takeV(V v) { std::printf("  takeV(V%d %d)\n", v.id, v.val); return v.val; }
int takeD(D d) { std::printf("  takeD(V%d %d)\n", d.id, d.extra); return d.extra; }
int takeRef(const V& v) { std::printf("  takeRef(V%d)\n", v.id); return v.val; }
int takeTwo(V a, const V& b) { std::printf("  takeTwo(V%d, V%d)\n", a.id, b.id); return a.val + b.val; }

// built without a log line (the order in which arguments are evaluated is unspecified), destroyed with one: the
// callee destroys its by-value parameters left to right
struct Q { int q; Q(int x) : q(x) {} Q(const Q& o) : q(o.q + 10) {} ~Q() { std::printf("  ~Q%d\n", q); } };
int takeQ(Q a, Q b, Q c) { std::printf("  takeQ\n"); return a.q + b.q + c.q; }

// named return value: one local returned on every path
D nrvo(int v)
{
    D d(v);
    d.extra += 1;
    if (v > 100) return d;
    std::printf("  nrvo body\n");
    return d;
}

// a returned prvalue
V rvo(int v) { return V(v + 1); }

// two different locals returned: one of them must be copied
V twoLocals(int v)
{
    V a(v), b(v + 1);
    if (v & 1) return a;
    return b;
}

// the named local is destroyed when an exception leaves before the return
V nrvoThrow(int v)
{
    V x(v);
    if (v == 13) throw 13;
    return x;
}

// NRVO of a class with a virtual base, in a member function, in a function template
DV nrvoDV(int v) { DV d(v); d.extra = 5; return d; }
struct Maker {
    int base;
    D make(int v) const { D d(v + base); if (v < 0) return d; d.extra = 1; return (d); }
};
template <class T> T tmake(const T& p) { T t(p.val + 1); std::printf("  tmake\n"); return t; }

// an NRVO local named in its own initializer: at -O2 the result object itself (cl /O2: self == &r)
struct Self { int x; int pad[4]; Self* self; Self(Self* p) : x(7), self(p) {} Self(const Self& o) : x(o.x), self(o.self) { std::printf("  Self copy\n"); } ~Self() { std::printf("  ~Self\n"); } };
Self selfRef() { Self a = &a; return a; }

// block-scope extern declarations name the namespace's entities: no local is built
struct GA { int v; GA(int x) : v(x) {} GA(const GA& o) : v(o.v + 1) { std::printf("  GA copy\n"); } };
GA gA(100);
int gCount = 5;
int helperF() { return 3; }
GA useExtern() { extern GA gA; return gA; }
int useExternInt() { extern int gCount; extern int helperF(); gCount += 10; return gCount + helperF(); }
int lateUse() { extern int gLate; return gLate; }
int gLate = 42;

// goto out of scopes destroys their objects, innermost first, and leaves try blocks; backwards over a
// declaration destroys the object too
struct L { int v; L(int x) : v(x) { std::printf("  L%d\n", v); } ~L() { std::printf("  ~L%d\n", v); } };
void gotoOut(int n) { { L a(20); if (n) goto out; L b(21); } out: std::printf("  out\n"); }
void gotoBack(int n) { int i = 0; again: L c(30 + i); if (++i < n) goto again; std::printf("  done\n"); }
void gotoTry(int n) { try { L d(40); { L e(41); if (n) goto past; } throw 1; } catch (int) { std::printf("  caught\n"); } past: std::printf("  past\n"); try { throw 2; } catch (int) { std::printf("  caught again\n"); } }
void gotoNested(int n) { for (int i = 0; i < 3; ++i) { L f(50 + i); for (int j = 0; j < 3; ++j) { L g(60 + j); if (i == n && j == 1) goto done; } } done: std::printf("  done2\n"); }

struct Holder {
    V m; W w;
    Holder(int v) : m(mkV(v)), w(W(v + 1)) { std::printf("  Holder\n"); }
    Holder(const V& v, int x) : m(v), w(x) { std::printf("  Holder from V%d\n", v.id); }
    ~Holder() { std::printf("  ~Holder\n"); }
};

static void section(const char* s) { std::printf("%s\n", s); }

int main()
{
    section("1 takeV(mkD())");
    { int r = takeV(mkD(1)); std::printf("  r=%d\n", r); }
    section("1b takeV(mkDV())");
    { int r = takeV(mkDV(2)); std::printf("  r=%d\n", r); }
    section("1c takeV(D(3))");
    { int r = takeV(D(3)); std::printf("  r=%d\n", r); }
    section("2 take(mkV()) / take(V()) / take(lvalue)");
    { takeV(mkV(4)); takeV(V(5)); V l(6); takeV(l); D d(7); takeV(d); takeD(d); takeD(mkD(8)); }
    section("2b nested calls");
    { takeV(mkV(takeV(V(9)))); }
    section("2c two arguments");
    { V l(10); takeTwo(mkV(11), l); Q q(3); takeQ(Q(1), Q(2), q); }
    section("3 D d = mkD()");
    { D d = mkD(12); std::printf("  have V%d\n", d.id); }
    section("3b nrvo / rvo");
    { D d = nrvo(13); std::printf("  have V%d %d\n", d.id, d.extra); V v = rvo(14); std::printf("  have V%d\n", v.id); }
    section("3f nrvo: virtual base, member function, template");
    { DV a = nrvoDV(40); Maker m; m.base = 1; D b = m.make(41); D c = m.make(-50); V t = tmake(V(42)); takeV(tmake(V(43)));
      std::printf("  have V%d V%d V%d V%d %d %d\n", a.id, b.id, c.id, t.id, b.extra, c.extra); }
    section("3c two locals");
    { V a = twoLocals(15); V b = twoLocals(16); std::printf("  have V%d V%d\n", a.id, b.id); }
    section("3d nrvo with exception");
    { try { V a = nrvoThrow(13); std::printf("  not reached V%d\n", a.id); } catch (int e) { std::printf("  caught %d\n", e); }
      V b = nrvoThrow(17); std::printf("  have V%d\n", b.id); }
    section("3e assignment from a returned temporary");
    { V a(18); a = mkV(19); std::printf("  a=V%d %d\n", a.id, a.val); }
    section("4 V v = D()");
    { V v = D(20); std::printf("  have V%d\n", v.id); }
    section("4b const V& r = D()");
    { const V& r = D(21); std::printf("  bound V%d\n", r.id); }
    section("4c const V& r = mkD()");
    { const V& r = mkD(22); std::printf("  bound V%d\n", r.id); }
    section("4d const D& r = mkD()");
    { const D& r = mkD(23); std::printf("  bound V%d\n", r.id); }
    section("5 conditional operator");
    { bool t = nextId > 0; V v = t ? mkV(24) : V(25); std::printf("  have V%d\n", v.id); takeV(t ? V(26) : mkV(27)); }
    section("5b takeRef of temporaries");
    { takeRef(D(28)); takeRef(mkV(29)); int s = takeRef(V(30)); s += takeRef(V(31)); std::printf("  sum %d\n", s); }
    section("6 member initialisers and constructor arguments");
    { Holder h(32); std::printf("  holder V%d\n", h.m.id); }
    { Holder h(D(33), 34); std::printf("  holder V%d\n", h.m.id); }
    { int k = takeV(V(36)); Holder h(mkV(35), k); std::printf("  holder V%d\n", h.m.id); }
    section("7 self-referring NRVO local, block-scope extern");
    { Self r = selfRef(); std::printf("  self %d %d\n", r.x, (int)(r.self == &r)); }
    { GA a = useExtern(); int i = useExternInt(); std::printf("  extern %d %d %d %d\n", a.v, i, gCount, lateUse()); }
    section("8 goto");
    gotoOut(1); gotoOut(0); gotoBack(3); gotoTry(1); gotoTry(0); gotoNested(1);
    section("end");
    return 0;
}
