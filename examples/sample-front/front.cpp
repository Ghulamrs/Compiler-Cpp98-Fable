// front.cpp - version 6 front-end regressions that compile (test 17): built by cppparse --compile6 and by cl,
// the two programs must print the same lines
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// sizeof(T&) is sizeof(T) (C++98 5.3.3/2), not the size of a pointer
struct Forty { char a[40]; };
template <typename T> int sizeOfRef(const T&) { return (int)sizeof(T&); }

void sizes()
{
    std::printf("sizeof refs: %d %d %d %d\n", (int)sizeof(Forty&), (int)sizeof(char&), (int)sizeof(double&), (int)sizeof(const short&));
    std::printf("sizeof refs in a template: %d %d\n", sizeOfRef(Forty()), sizeOfRef('c'));
    Forty f; Forty& rf = f;
    std::printf("sizeof of a reference expression: %d\n", (int)sizeof(rf));
}

// a local variable or a parameter hides a function template of the same name: "count < n" is a comparison
template <typename T> int count(T a, T b) { return a < b ? 1 : 2; }
int localHidesGlobal(int n) { int count = 0; while (count < n + 1) ++count; return count; }
int paramHidesGlobal(int count) { return count < 3 ? 10 : count > 5 ? 20 : 30; }
int forHidesGlobal(int n) { int s = 0; for (int count = 0; count < n; ++count) s += count; return s; }
int templateStillSeen() { return count<int>(1, 2) * 10 + count(5, 4); }

namespace std {
// inside namespace std after <algorithm>, where std::count is a template too
int localHidesStd(int n) { int count = 0; while (count < n + 1) ++count; return count; }
}

namespace other {
using namespace std;
int localHidesUsing(int n) { int count = 0, min = n; while (count < min * 2) ++count; return count; }
}

void hiding()
{
    std::printf("hiding: %d %d %d %d %d %d %d\n", localHidesGlobal(4), paramHidesGlobal(2), paramHidesGlobal(7), forHidesGlobal(5),
                templateStillSeen(), std::localHidesStd(6), other::localHidesUsing(3));
}

// inside Grid<T, R, C> the name Grid with arguments names a specialisation: Grid<T, C, R> is another class
template <typename T, int R, int C> struct Grid {
    T v[R * C];
    static int cells() { return R * C * 100 + R * 10 + C; }
    Grid<T, C, R> transposed() const { Grid<T, C, R> t; for (int i = 0; i < R; ++i) for (int j = 0; j < C; ++j) t.v[j * R + i] = v[i * C + j]; return t; }
    int otherCells() const { return Grid<T, C, R>::cells(); }
};

void injected()
{
    Grid<int, 2, 3> g;
    for (int i = 0; i < 6; ++i) g.v[i] = i + 1;
    Grid<int, 3, 2> t = g.transposed();
    std::printf("injected name with arguments: %d %d %d %d %d\n", t.v[1], t.v[4], (int)sizeof(t), g.otherCells(), t.otherCells());
}

// a function template declared and then defined is one template, not two candidates; a default argument given
// by the declaration holds for the definition
template <typename T> T twice(T x);
template <typename T> T scaled(T x, int k = 3);
template <typename T> T twice(T x) { return x + x; }
template <typename T> T scaled(T x, int k) { return x * k; }

void redeclared()
{
    std::printf("redeclared templates: %d %g %d %d\n", twice(21), twice(1.25), scaled(7), scaled(2, 10));
}

// member templates of a class template see the enclosing parameters, in their declaration (deduction against
// Mat<T, R, K> with R bound, or against a literal argument) and in their body
template <typename T, int R, int C> struct Mat {
    T v[R * C];
    template <int K> int widthOf(const Mat<T, R, K>&) const { return K * 10 + R; }
    template <int K> int fixedRows(const Mat<T, 2, K>&) const { return R * 100 + K; }
    template <int K> Mat<T, R, K> times(const Mat<T, C, K>& b) const {
        Mat<T, R, K> r;
        for (int i = 0; i < R; ++i)
            for (int j = 0; j < K; ++j) { T s = T(); for (int c = 0; c < C; ++c) s += v[i * C + c] * b.v[c * K + j]; r.v[i * K + j] = s; }
        return r;
    }
};

void memberTemplates()
{
    Mat<int, 2, 3> a; Mat<int, 3, 4> b; Mat<int, 2, 5> w;
    for (int i = 0; i < 6; ++i) a.v[i] = i + 1;
    for (int i = 0; i < 12; ++i) b.v[i] = i % 5 - 2;
    Mat<int, 2, 4> p = a.times(b);
    std::printf("member templates: %d %d %d %d %d %d\n", a.widthOf(w), b.fixedRows(w), p.v[0], p.v[5], p.v[7], (int)sizeof(p));
}

// a call through a function-pointer data member is an indirect call through its value, not a call to a
// function named after the member; also through ->, in parentheses, a static member and a local pointer
static double doubled(double x) { return 2 * x; }
static double halved(double x) { return x / 2; }
struct Scaler { double (*scale)(double); int k; static double (*shared)(double); };
double (*Scaler::shared)(double) = halved;

void functionPointerMembers()
{
    Scaler e; e.scale = doubled; e.k = 1;
    Scaler* p = &e;
    double (*lp)(double) = halved;
    std::printf("function pointer members: %g %g %g %g %g %g %g %g %g %g\n", e.scale(3.0), p->scale(4.0), (e.scale)(5.0),
                Scaler::shared(6.0), e.shared(8.0), lp(10.0), (*lp)(12.0), (*p->scale)(1.5), (*doubled)(0.25), (halved)(0.5));
}

// a data member of class type called as a function: operator() on the member itself, not on the enclosing
// object; also through ->, in parentheses, a member inherited from a base and a member whose class inherits
// operator() from its base
struct Adder { int k; int operator()(int x) { return x + k; } };
struct TaggedAdder : Adder { int tag; };
struct Holder { int pad; Adder f; };
struct Other { int a, b; };
struct Derived : Other, Holder { int more; TaggedAdder g; };

void functorMembers()
{
    Holder h; h.pad = 1000; h.f.k = 7;
    Holder* p = &h;
    Derived d; d.a = 1; d.b = 2; d.pad = 2000; d.f.k = 40; d.more = 3000; d.g.k = 500; d.g.tag = 9;
    Derived* q = &d;
    std::printf("functor members: %d %d %d %d %d %d %d %d\n", (h.f)(1), h.f(2), p->f(3), (p->f)(4), d.f(5), q->f(6), d.g(7), (q->g)(8));
}

// with a using-directive at file scope: std::count, std::min and std::max are visible unqualified
using namespace std;
int localHidesFileUsing(int n) { int count = 0, max = 2; while (count < n + max) ++count; return count; }

// the macros cl defines for a /MT /EHsc build, which cppparse predefines too (not _MSC_VER: MSVC's extensions)
void predefined()
{
#if defined(_MT) && defined(_CPPUNWIND) && defined(_WIN64) && defined(_M_X64)
    std::printf("predefined: _MT %d _CPPUNWIND %d _M_X64 %d\n", _MT, _CPPUNWIND, _M_X64);
#else
    std::printf("predefined: missing\n");
#endif
}

// a declaration as the condition of while, if, for and switch: re-initialised from the condition on every
// iteration, destroyed at the end of each iteration (and when the condition is false or the loop is left)
struct Gauge {
    static int built, destroyed;
    int v;
    Gauge(int x) : v(x) { ++built; }
    Gauge(const Gauge& o) : v(o.v) { ++built; }
    ~Gauge() { ++destroyed; }
    operator int() const { return v; }
};
int Gauge::built = 0;
int Gauge::destroyed = 0;

int whileInt(int k) { int r = 0; while (int y = k--) { r += y; } return r; }
int whileGauge(int k) { int r = 0; while (Gauge g = k--) { r += g.v; if (Gauge::built - Gauge::destroyed != 1) return -1; } return r; }
int whileBreak(int k) { int r = 0; while (Gauge g = k--) { if (g.v == 2) break; if (g.v == 4) continue; r += g.v; } return r; }
int ifInt(int k) { if (int y = k * 2) return y; else return y - 1; }
int ifGauge(int k) { if (Gauge g = k) return g.v * 10; else return g.v - 1; }
int forInt(int k) { int r = 0; for (int i = 0; int y = k - i; ++i) r += y; return r; }
int forGauge(int k) { int r = 0; for (int i = 0; Gauge g = k - i; ++i) { if (i == 1) continue; r += g.v; } return r; }
int switchInt(int k) { switch (int y = k + 1) { case 2: return y * 100; default: return y; } }
int switchGauge(int k) { switch (Gauge g = k + 1) { case 2: return g.v * 100; default: break; } return -5; }

void conditionDeclarations()
{
    std::printf("while decl: %d %d %d\n", whileInt(4), whileInt(0), whileInt(1));
    Gauge::built = Gauge::destroyed = 0;
    int a = whileGauge(4);
    std::printf("while Gauge: %d built %d destroyed %d\n", a, Gauge::built, Gauge::destroyed);
    Gauge::built = Gauge::destroyed = 0;
    int b = whileBreak(6);
    std::printf("while break/continue: %d built %d destroyed %d\n", b, Gauge::built, Gauge::destroyed);
    std::printf("if decl: %d %d\n", ifInt(3), ifInt(0));
    Gauge::built = Gauge::destroyed = 0;
    int c = ifGauge(3), d = ifGauge(0);
    std::printf("if Gauge: %d %d built %d destroyed %d\n", c, d, Gauge::built, Gauge::destroyed);
    std::printf("for decl: %d %d\n", forInt(4), forInt(0));
    Gauge::built = Gauge::destroyed = 0;
    int e = forGauge(4);
    std::printf("for Gauge: %d built %d destroyed %d\n", e, Gauge::built, Gauge::destroyed);
    std::printf("switch decl: %d %d\n", switchInt(1), switchInt(5));
    Gauge::built = Gauge::destroyed = 0;
    int f = switchGauge(1), g = switchGauge(4);
    std::printf("switch Gauge: %d %d built %d destroyed %d\n", f, g, Gauge::built, Gauge::destroyed);
}

// globals initialised by constants that need a conversion: static data with the bytes cl gives them
namespace folded {
double d1 = 0xFFFFFFFFu;
float f1 = 0x80000000;
unsigned u1 = 3000000000.0;
unsigned char uc1 = 255.9;
wchar_t w1 = 65.7;
bool b1 = 0.5;
short s1 = -1.9;
double d4 = 2147483647 + 0.5f;
double d16 = (float)(1.0 / 3) + 1;
double d17 = 0.1f * 3;
float f3 = 16777217 + 1.0f;
double d18 = 'a' + 0.25f;
}
void showBytes(const char* name, const void* p, int n)
{
    std::printf("%s", name);
    for (int i = 0; i < n; ++i) std::printf(" %02x", ((const unsigned char*)p)[i]);
    std::printf("\n");
}
void foldedGlobals()
{
    showBytes("d1", &folded::d1, 8); showBytes("f1", &folded::f1, 4); showBytes("u1", &folded::u1, 4);
    showBytes("uc1", &folded::uc1, 1); showBytes("w1", &folded::w1, 2); showBytes("b1", &folded::b1, 1);
    showBytes("s1", &folded::s1, 2); showBytes("d4", &folded::d4, 8); showBytes("d16", &folded::d16, 8);
    showBytes("d17", &folded::d17, 8); showBytes("f3", &folded::f3, 4); showBytes("d18", &folded::d18, 8);
}

// a constructor initializer calls the constructor overload resolution chooses, not the first one taking as many
// arguments: B(o) with o a const C& is B's copy constructor, not B(int)
struct InitB { int v; InitB(int x) : v(x * 100) {} InitB(const InitB& o) : v(o.v + 1) {} };
struct InitM { int m; InitM(const char*) : m(7) {} InitM(int x) : m(x) {} };
struct InitC : InitB { InitM mem; InitC(int x) : InitB(x), mem(x) {} InitC(const InitC& o) : InitB(o), mem("copy") {} };

void memInitOverloads()
{
    InitC a(3);
    InitC b(a);
    std::printf("constructor initializers: %d %d %d %d\n", a.v, a.mem.m, b.v, b.mem.m);
}

// floating point as cl computes it, results printed as their bits
namespace fp {
unsigned long long bitsD(double d) { unsigned long long u; std::memcpy(&u, &d, 8); return u; }
unsigned int bitsF(float f) { unsigned int u; std::memcpy(&u, &f, 4); return u; }
volatile double vzero = 0.0;

// long double is MSVC's 8-byte double: double <-> long double converts nothing, float <-> long double as double
long double d2ld(double x) { return x; }
double ld2d(long double x) { return x; }
long double f2ld(float x) { return x; }
float ld2f(long double x) { return x; }
long double ldAdd(long double a, double b) { return a + b; }
void longDouble()
{
    std::printf("long double: %016llx %016llx %016llx %08x %016llx %d\n", bitsD(d2ld(1.1)), bitsD(ld2d(2.2L)), bitsD(f2ld(0.1f)), bitsF(ld2f(0.1L)),
                bitsD(ldAdd(1.5L, 0.1)), (int)sizeof(long double));
    std::printf("long double through varargs: %Lf %Lf\n", d2ld(3.25), (long double)1.5);
}
}

namespace fp {
// comparisons: ucomiss for floats, ucomisd for doubles, and the parity flag: NaN is unequal to everything (itself
// included) and every ordered comparison with it is false; as a condition -0.0 is false and NaN true
bool fBool(float x) { return x; }
bool dBool(double x) { return x; }
bool fNot(float x) { return !x; }
bool fEq(float a, float b) { return a == b; }
bool fNe(float a, float b) { return a != b; }
bool fLt(float a, float b) { return a < b; }
bool fLe(float a, float b) { return a <= b; }
bool fGt(float a, float b) { return a > b; }
bool fGe(float a, float b) { return a >= b; }
bool dEq(double a, double b) { return a == b; }
bool dNe(double a, double b) { return a != b; }
bool dLt(double a, double b) { return a < b; }
bool dGe(double a, double b) { return a >= b; }
int fIf(float x) { if (x) return 1; return 2; }
int dIf(double x) { if (x) return 1; return 2; }
int dEqIf(double a, double b) { if (a == b) return 1; return 2; }
int dNeIf(double a, double b) { if (a != b) return 1; return 2; }
int fEqIf(float a, float b) { if (a == b) return 1; return 2; }
int fNotNeIf(float a, float b) { if (!(a != b)) return 1; return 2; }
int fAnd(float a, float b) { return a && b ? 3 : 4; }
int dCond(double a) { return a ? 5 : 6; }
void comparisons()
{
    double nan = vzero / vzero, nz = -vzero; float fnan = (float)nan, fnz = (float)nz;
    std::printf("fp to bool: %d %d %d %d %d %d %d\n", fBool(fnz), fBool(fnan), fBool(1.0f), dBool(nz), dBool(nan), fNot(fnz), fNot(fnan));
    std::printf("fp float compares: %d%d%d%d%d%d %d%d%d%d%d%d %d%d%d%d%d%d\n", fEq(fnan, fnan), fNe(fnan, fnan), fLt(fnan, 1), fLe(fnan, 1), fGt(fnan, 1), fGe(fnan, 1),
                fEq(1, 1), fNe(1, 2), fLt(1, 2), fLe(2, 2), fGt(3, 2), fGe(2, 3), fEq(fnz, 0), fNe(fnz, 0), fLt(fnz, 0), fLe(-1, fnan), fGt(1, fnan), fGe(1, fnan));
    std::printf("fp double compares: %d%d%d%d %d%d%d%d\n", dEq(nan, nan), dNe(nan, nan), dLt(nan, 1), dGe(nan, 1), dEq(nan, 1), dNe(1, 1), dLt(1, 2), dGe(nz, 0));
    std::printf("fp conditions: %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", fIf(fnz), fIf(fnan), dIf(nz), dIf(nan), dEqIf(nan, nan), dNeIf(nan, nan), fEqIf(fnan, fnan),
                fNotNeIf(fnan, fnan), fAnd(fnan, 1), fAnd(fnz, 1), dCond(nan), dCond(nz), dEqIf(2, 2), dNeIf(2, 2));
    std::printf("fp folded conditions: %d %d %d %d\n", (bool)-0.0, -0.0 == 0.0, (bool)-0.0f, fIf(-0.0f) + dCond(-0.0));
}

// algebraic identities kept only where exact under IEEE: x * 1, x / 1, x - 0.0 and x + (-0.0); x + 0.0 of -0.0 is
// +0.0, x * 0 is -0.0 for a negative x, x - x and x == x are not for NaN. Constants are values, not their
// 6-digit spelling (0.1 and 0.1000001 differ) and -0.0 is not 0.0
double addZero(double x) { return x + 0.0; }
double subZero(double x) { return x - 0.0; }
double mulOne(double x) { return x * 1.0; }
double mulZero(double x) { return x * 0.0; }
double zeroSub(double x) { return 0.0 - x; }
double divOne(double x) { return x / 1.0; }
double selfSub(double x) { return x - x; }
bool selfEq(double x) { return x == x; }
bool selfNe(double x) { return x != x; }
float fAddZero(float x) { return x + 0.0f; }
double nearConstants(double x) { double a = x + 0.1; double b = x + 0.1000001; return b - a; }
double signedZeros(bool c) { double r; if (c) r = 0.0; else r = -0.0; return r; }
void identities()
{
    double nan = vzero / vzero, nz = -vzero;
    std::printf("fp identities: %016llx %016llx %016llx %016llx %016llx %016llx %d %d %d %08x\n", bitsD(addZero(nz)), bitsD(subZero(nz)), bitsD(mulOne(nz)),
                bitsD(mulZero(-3.0)), bitsD(zeroSub(0.0)), bitsD(divOne(nz)), selfSub(nan) != selfSub(nan), selfEq(nan), selfNe(nan), bitsF(fAddZero(-0.0f)));
    std::printf("fp folded identities: %016llx %016llx %016llx %016llx %016llx\n", bitsD(addZero(-0.0)), bitsD(-0.0 + 0.0), bitsD(nearConstants(1.0)),
                bitsD(signedZeros(true)), bitsD(signedZeros(false)));
}

// float arithmetic is single precision (float op float is a float, C++98 5/9), and so is -f
double floatSum(float f) { double r = f + 0.1f; return r; }
double floatMul(float a, float b) { return a * b; }
double floatDiv(float a, float b) { return a / b; }
float floatChain(float a, float b, float c) { return a * b + c; }
float floatCompound(float f) { f += 0.1f; f *= 3.0f; f -= 0.2f; return f; }
double floatConstants() { float a = 0.1f, b = 0.2f; return a + b; }
double floatRoundTrip() { return (double)(float)0.1; }
int floatSizes(float f) { return (int)sizeof(-f) * 10 + (int)sizeof(f + f); }
float floatNeg(float f) { return -f; }
void floatArithmetic()
{
    std::printf("fp float arithmetic: %016llx %016llx %016llx %08x %08x\n", bitsD(floatSum(3.0f)), bitsD(floatMul(0.1f, 0.3f)), bitsD(floatDiv(1.0f, 3.0f)),
                bitsF(floatChain(0.1f, 0.2f, 0.3f)), bitsF(floatCompound(1.0f)));
    std::printf("fp float constants: %016llx %016llx %d %08x %08x\n", bitsD(floatConstants()), bitsD(floatRoundTrip()), floatSizes(1.0f), bitsF(floatNeg(0.0f)),
                bitsF(floatNeg(1.5f)));
}
}

// floating -> integer conversions as cl compiles them at run time and folds them as constants: signed and narrow
// targets through 32 bits (out of range: 80000000h, then truncated), unsigned int through 64 bits, unsigned long long
// by its two ranges; unsigned long long -> floating with the top bit set; -O2 once folded (unsigned)3e9 to 2147483648
// and value-numbered 1e19 and 1e19f as one constant
namespace conv {
static const double dv[] = { 0.0, 1.5, -1.5, 127.9, 200.7, 255.9, -128.9, 32767.5, 40000.25, 65535.75, -32768.5,
    2147483647.0, 2147483648.0, 3000000000.0, 4294967295.0, -2147483648.0, 9007199254740993.0,
    9223372036854775807.0, 1e19, 1.8446744073709550e19, -9223372036854775808.0, 0.999 };
static const float fv[] = { 0.0f, 1.5f, -1.5f, 127.9f, 200.7f, 255.9f, -128.9f, 32767.5f, 40000.25f, 65535.75f,
    2147483520.0f, 2147483648.0f, 3000000000.0f, 4294967040.0f, -2147483648.0f, 9.2233715e18f, 1e19f, 1.8446743e19f, 0.999f };
void runtimeD()
{
    for (unsigned i = 0; i < sizeof(dv) / sizeof(dv[0]); ++i) {
        volatile double x = dv[i];
        double v = x;
        std::printf("d %u: sc %d uc %u ss %d us %u", i, (int)(signed char)v, (unsigned)(unsigned char)v, (int)(short)v, (unsigned)(unsigned short)v);
        std::printf(" i %d u %u ll %lld ull %llu\n", (int)v, (unsigned)v, (long long)v, (unsigned long long)v);
    }
}
void runtimeF()
{
    for (unsigned i = 0; i < sizeof(fv) / sizeof(fv[0]); ++i) {
        volatile float x = fv[i];
        float v = x;
        std::printf("f %u: sc %d uc %u ss %d us %u", i, (int)(signed char)v, (unsigned)(unsigned char)v, (int)(short)v, (unsigned)(unsigned short)v);
        std::printf(" i %d u %u ll %lld ull %llu\n", (int)v, (unsigned)v, (long long)v, (unsigned long long)v);
    }
}
void consts()
{
    std::printf("c u %u %u %u %u\n", (unsigned)3000000000.0, (unsigned)4294967295.0, (unsigned)2147483648.0, (unsigned)3000000000.0f);
    std::printf("c ull %llu %llu %llu\n", (unsigned long long)1e19, (unsigned long long)9223372036854775808.0, (unsigned long long)1e19f);
    std::printf("c i %d %d %lld %lld\n", (int)-2147483648.0, (int)-1.5, (long long)-1.5, (long long)9007199254740993.0);
    std::printf("c s %d %u %d %u\n", (int)(short)32767.5, (unsigned)(unsigned short)65535.75, (int)(signed char)-128.9, (unsigned)(unsigned char)255.9f);
    double d = 3000000000.0; unsigned u = (unsigned)d; unsigned long long q = (unsigned long long)(d * 4.0);
    std::printf("c local %u %llu\n", u, q);
    float f = 3e9f; std::printf("c localf %u %llu\n", (unsigned)f, (unsigned long long)(f * 4.0f));
}
void toFloating()
{
    const unsigned long long top = 1ULL << 63;
    const unsigned long long uq[] = { 0ULL, 1ULL, (1ULL << 32) - 1, top - 1, top, top + 2048, top + 2049, ~0ULL, top + (1ULL << 61) + 12345, top + 1024, top + 3072 };
    for (unsigned i = 0; i < sizeof(uq) / sizeof(uq[0]); ++i) {
        volatile unsigned long long x = uq[i];
        unsigned long long v = x; long long s = (long long)v; unsigned u = (unsigned)v;
        std::printf("tofp %u: %.17g %.9g %.17g %.17g %.9g\n", i, (double)v, (double)(float)v, (double)s, (double)u, (double)(float)u);
    }
    std::printf("tofp c %.17g %.17g %.9g %.17g\n", (double)3000000000u, (double)~0ULL, (double)(float)4294967295u, (double)(1ULL << 63));
    unsigned k = 3000000000u; unsigned long long q = ~0ULL;
    std::printf("tofp local %.17g %.17g %.9g\n", (double)k, (double)q, (double)(float)q);
    float f19 = 1e19f; std::printf("f19 %.17g %llu\n", (double)f19, (unsigned long long)f19);
}
}

// sizes of pointers to members by cl's inheritance models (data 4, 8 with virtual bases; functions 8, 16 with
// multiple or virtual inheritance); they were all 8
namespace mpsize {
struct P { int a; int f(); };
struct M { int m; };
struct Q : M, P { int q; };
struct R : Q { };
struct V { int v; };
struct W : virtual V { int w; int g(); };
struct S1 : P { };
void show()
{
    std::printf("member pointer sizes: %d %d %d %d %d %d %d %d %d\n", (int)sizeof(int P::*), (int)sizeof(int (P::*)()), (int)sizeof(int Q::*),
        (int)sizeof(int (Q::*)()), (int)sizeof(int R::*), (int)sizeof(int (R::*)()), (int)sizeof(int W::*), (int)sizeof(int (W::*)()),
        (int)sizeof(int (S1::*)()));
}
}

// 64-bit integer literals: types and values as cl (a value from 2^31 to 2^64-1, decimal, hexadecimal, octal,
// suffixed or not) at run time, in globals, statics, arrays and as converted case labels; 4294967295ULL was
// sign-extended to 2^64-1 and wider literals kept their low 32 bits
namespace lit {
static const unsigned long long a[] = { 4294967295ULL, 9223372036854775808ULL, 12345678901234567891ULL, 0xFFFFFFFFULL, 0x80000000ULL, 2147483648LL, 040000000000LL };
unsigned long long b = 4294967295ULL;
static unsigned long long c[] = { 4294967296ULL, 1ULL << 40, 0x100000000 };
long long d[] = { -5000000000LL, 5000000000LL, -2147483648LL, 2147483648LL, 9223372036854775807LL, 0x7FFFFFFFFFFFFFFF };
unsigned long long sq() { static unsigned long long s = 18446744073709551615ULL; return s; }
const unsigned long long kc = 4294967295ULL;
void show()
{
    volatile int i = 0;
    std::printf("lit a %llu %llu %llu %llu %llu %llu %llu\n", a[i], a[i+1], a[i+2], a[i+3], a[i+4], a[i+5], a[i+6]);
    std::printf("lit b %llu c %llu %llu %llu\n", b, c[i], c[i+1], c[i+2]);
    std::printf("lit d %lld %lld %lld %lld %lld %lld\n", d[i], d[i+1], d[i+2], d[i+3], d[i+4], d[i+5]);
    unsigned long long e[] = { 4294967295ULL, 9223372036854775808ULL, 3000000000ULL };
    std::printf("lit e %llu %llu %llu sq %llu kc %llu\n", e[i], e[i+1], e[i+2], sq(), kc);
    long long x = 3000000000LL; unsigned long long y = 0xFFFFFFFFull + 1; long long z = -3000000000LL;
    std::printf("lit run %lld %llu %lld %llu %lld\n", x, y, z, 4294967295ULL * 2, 5000000000 / 3);
    // types: signed or not, and size
    std::printf("lit types %d %d %d %d %d %d %d %d\n", (int)(2147483648 * 0 - 1 < 0), (int)sizeof(2147483648), (int)(0x80000000 * 0 - 1 < 0),
        (int)sizeof(4294967296), (int)(0x100000000 * 0 - 1 < 0), (int)(9223372036854775808 * 0 - 1 < 0), (int)sizeof(5000000000L), (int)(0xFFFFFFFFLL * 0 - 1 < 0));
    unsigned long long sw = b; int r = 0;
    switch (sw) { case 4294967295U: r = 1; break; case 1ULL: r = 2; break; default: r = 3; }
    std::printf("lit switch %d\n", r);
}
}

// a base held twice (D : B, C with B : A, C : A): reached through a named base or an unambiguous path; d.C::a
// used to be B's A (the first path), and the ambiguous forms compiled silently (now S143/S144, ambiguous.cpp)
namespace ambig {
struct A { int a; int f() const { return a * 10; } };
struct B : A { int b; };
struct C : A { int c; };
struct D : B, C { int d; };
struct V2 { int v; };
struct E1 : virtual V2 { };
struct E2 : virtual V2 { };
struct G : E1, E2 { };
int viaB(A* p) { return p->a; }
void show()
{
    D d; d.B::a = 1; d.C::a = 2; d.b = 3; d.c = 4; d.d = 5;
    A* pb = (B*)&d; A* pc = static_cast<C*>(&d);
    B& rb = d; C* cp = &d;
    std::printf("ambig %d %d %d %d %d %d %d %d\n", d.B::a, d.C::a, pb->a, pc->a, rb.a, cp->a, d.B::f(), d.C::f() + viaB(cp));
    G g; g.v = 7; V2* v = &g; E1* e1 = &g;
    std::printf("ambig virtual %d %d %d\n", g.v, v->v, e1->v);
}
}

// review of the member-pointer and conversion commits: cl's sizes for an incomplete class (unspecified model) and
// for a vfptr added in front of a base without one (multiple model); a class template with a member pointer
// only named through a pointer is accepted; out-of-range literal casts folded modulo 2^N as cl folds them
namespace review {
struct Inc;
struct A1 { int a; };
struct D1 : A1 { virtual void h() {} };
struct P { int a; };
template <class T> struct Holder { int T::* f; int x; };
void show()
{
    std::printf("review sizes %d %d %d %d %d\n", (int)sizeof(void (Inc::*)()), (int)sizeof(int Inc::*), (int)sizeof(void (D1::*)()), (int)sizeof(int D1::*), (int)sizeof(Holder<P>*));
    std::printf("review folds %x %llx %x %x %llx %d %x\n", (int)3e9, (long long)9.3e18, (unsigned)1e19, (unsigned)(unsigned short)1e10,
        (unsigned long long)2e19, (int)(signed char)300.0, (unsigned)static_cast<unsigned char>(-1.5));
}
}

int main()
{
    memInitOverloads();
    predefined();
    sizes();
    hiding();
    injected();
    redeclared();
    memberTemplates();
    functionPointerMembers();
    functorMembers();
    conditionDeclarations();
    foldedGlobals();
    fp::longDouble();
    fp::comparisons();
    fp::identities();
    fp::floatArithmetic();
    conv::runtimeD(); conv::runtimeF(); conv::consts(); conv::toFloating();
    mpsize::show();
    lit::show();
    ambig::show();
    review::show();
    std::printf("hiding after using namespace std: %d\n", localHidesFileUsing(5));
    // ::printf, found directly and through std's "using ::printf", is one function, not an ambiguity
    printf("unqualified printf after using namespace std: %d %s\n", 5, "ok");
    // library results are typed by the headers: a double from strtod, a size_t from string::size
    std::string s("abc"); double d = std::strtod("2.5", 0); std::size_t n = s.size(); long long q = n * 3;
    printf("library results: %f %d %lld\n", d * 2, (int)n, q);
    return 0;
}
