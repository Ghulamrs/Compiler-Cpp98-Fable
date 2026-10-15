// front.cpp - version 6 front-end regressions that compile (test 17): built by cppparse --compile6 and by cl,
// the two programs must print the same lines
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <typeinfo>
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
// a class named T: test 2 also reads this file with the old pipeline, whose template instantiator took "T" and "C - 1"
// in Matrix.hpp's submatrix for concrete arguments and nested Matrix<T, C - 1 - 1, ...> until the stack overflowed
namespace tclass {
struct T { int t; };
int useT() { T x; x.t = 2; return x.t; }
}

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

// value-initialisation T() as cl does it (C++03 8.5/5): a class without a user-declared constructor is zeroed,
// then its implicit constructor runs; the empty implicit constructor alone left stack and heap garbage (row S1)
namespace valinit {
struct Q { int* f; int x; };
struct R { std::string s; int n; };
struct U { U() : a(7) {} int a; int b; };
struct PB { int p; int q; };
struct HasPod { Q m; int k; PB arr[2]; int sc[3]; HasPod() : m(), k(3), arr(), sc() {} };
struct Der : PB { int d; Der() : PB(), d(4) {} };
struct Agg { Q qs[2]; int z; };
struct PV { virtual int f() { return x + y; } int x; int y; };
struct VB { int v; };
struct VD : virtual VB { int d; };
struct E { };
struct EB : E { int w; EB() : E(), w(5) {} };
template <class T> T makeT(T*) { return T(); }
template <class T> struct Box { T v; Box() : v() {} };
Q gq = Q();
void dirty()
{
    volatile char buf[1024];
    for (int i = 0; i < 1024; ++i) buf[i] = (char)0xA5;
    void* m[4];
    for (int i = 0; i < 4; ++i) { m[i] = std::malloc(16 + 32 * i); std::memset(m[i], 0xA5, 16 + 32 * i); }
    for (int i = 0; i < 4; ++i) std::free(m[i]);
}
Q make() { return Q(); }
int takeQ(Q q = Q()) { return q.x + (q.f != 0); }
int v1() { Q q = Q(); return q.x + (q.f != 0); }
int v2() { Q q((Q())); return q.x + (q.f != 0); }
int v3() { Q q; q.x = 5; q.f = &q.x; q = Q(); return q.x + (q.f != 0); }
int v4() { Q q = make(); return q.x + (q.f != 0) + takeQ(); }
int v5() { Q* p = new Q(); int r = p->x + (p->f != 0); delete p; return r; }
int v6() { int i = int(); double d = double(); double* pd = new double(); int r = i + (int)d + (int)*pd; delete pd; return r; }
int v7() { HasPod h; return h.m.x + (h.m.f != 0) + h.k + h.arr[0].p + h.arr[1].q + h.sc[2]; }
int v8() { Der d; EB e; return d.p + d.q + d.d + e.w; }
int v9() { R r = R(); R* p = new R(); R* a = new R[3](); int n = r.n + (int)r.s.size() + p->n + a[2].n; delete p; delete[] a; return n; }
int v10() { Q q = makeT((Q*)0); int i = makeT((int*)0); Box<Q> b; Box<int> c; return q.x + (q.f != 0) + i + b.v.x + (b.v.f != 0) + c.v; }
int v11() { U u = U(); Agg a = Agg(); return u.a + a.qs[0].x + a.qs[1].x + (a.qs[1].f != 0) + a.z; }
int v12() { Q* p = new Q[2](); int r = p[0].x + p[1].x + (p[1].f != 0); delete[] p; return r + gq.x; }
int v13() { PV p = PV(); VD x = VD(); return p.f() + x.v + x.d; }
int v14() { try { throw Q(); } catch (Q q) { return q.x + (q.f != 0); } return 99; }
void show()
{
    int (*fs[])() = { v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14 };
    std::printf("value-initialised:");
    for (int i = 0; i < (int)(sizeof(fs) / sizeof(fs[0])); ++i) { dirty(); std::printf(" %d", fs[i]()); }
    std::printf("\n");
}
}

// arguments matching "..." get the default argument promotions (C++98 5.2.2/7): a float travelled as its own bits
// (5.19e-315 at -O0, a double-rounded 0.333333312 at -O2), and va_arg reads the promoted types (row S2)
namespace varargs {
enum Col { Red = 3, Green };
float third() { volatile float a = 1.0f; return a / 3.0f; }
float named(float a, float b) { return a + b; }
double sumv(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    double s = 0;
    for (int i = 0; i < n; ++i) s += va_arg(ap, double);
    va_end(ap);
    return s;
}
int digits(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    int s = 0;
    for (int i = 0; i < n; ++i) s = s * 10 + va_arg(ap, int);
    va_end(ap);
    return s;
}
void show()
{
    float f = third();
    const float cf = 0.1f;
    double (*pv)(int, ...) = sumv;
    std::printf("varargs %.9g %.9g %.9g %.17g\n", f, 1.0f / 3.0f, f * 2.0f, cf);
    std::printf("varargs through a pointer %.9g %.9g\n", pv(2, f, -f), (*pv)(1, f * 3.0f));
    char c = 'A'; signed char sc = -3; unsigned char uc = 200; short s = -7; unsigned short us = 65000; bool b = true; Col col = Green;
    std::printf("varargs %.9g %d %d %d %d %d %d %d %d %.9g\n", sumv(3, f, 2.5f, 1.0f / 3.0f), digits(4, (char)1, (short)2, true, Red), c, sc, uc, s, us, b, col, (double)named(f, f));
}
}

// the size of a pointer to member of a class completed later in the unit: cl decides the inheritance model where
// it first needs the size (sizeof, also in a function or an initializer before the definition, a member function
// defined in a class before it) and keeps the unspecified one; naming the type decides nothing (row S10)
namespace mplater {
struct A { int a; };
struct B { int b; };
struct Inc;
int ga = sizeof(int Inc::*);
int gb = sizeof(void (Inc::*)());
struct Inc { int x; };
struct M;
typedef void (M::*MF)();
struct M : A, B { int m; };
struct Fb;
int fb() { return (int)sizeof(int Fb::*) + 100 * (int)sizeof(void (Fb::*)()); }
struct Fb : A, B { int q; };
struct Later;
struct Host { int f() { return (int)sizeof(int Later::*); } };
struct Later { int z; };
struct Host2 { int f() { return (int)sizeof(int N::*); } struct N { int z; }; };
struct Late;
void takes(int Late::*);
struct Late { int z; };
void show()
{
    Host h; Host2 h2;
    std::printf("member pointer sizes, completed later: %d %d %d %d %d %d %d %d %d %d\n", ga, gb, (int)sizeof(int Inc::*), (int)sizeof(MF), (int)sizeof(int M::*),
        fb(), (int)sizeof(void (Fb::*)()), h.f(), h2.f(), (int)sizeof(int Late::*));
}
}

// literals: each piece of a concatenation has its escapes decoded on its own ("\x41" "b" was one character 0x1B
// "\0" "1" one \01), wide literals are 16-bit units (L"ab" was emitted as the bytes of its text), and a
// multicharacter constant is an int with cl's value ('ab' was 97; cl shifts each character in from the right
// until the second escape sequence, then puts each above the ones before)
namespace strlit {
const char* gp = "\x41" "b";
char ga[] = "\x41\x42" "c";
const wchar_t* gw = L"\x41" L"b";
wchar_t gwa[4] = L"\x41" L"b";
void units(const char* name, const void* p, int n, int size)
{
    std::printf("%s", name);
    for (int i = 0; i < n; ++i) std::printf(" %x", size == 1 ? ((const unsigned char*)p)[i] : (unsigned)((const wchar_t*)p)[i]);
    std::printf("\n");
}
const char* local() { static char s[] = "\101" "7" "\0" "1"; return s; }
#if 'ab' == 24930 && '\x12\x34' == 0x3412 && '\xff' < 0
const int ppOk = 1;
#else
const int ppOk = 0;
#endif
void show()
{
    const char* p = "\x41" "b";
    char a[] = "\x41\x42" "c";
    char b[8] = "\101" "7" "\0" "1";
    const wchar_t* w = L"\x41" L"b" L"\x263A";
    wchar_t wa[] = L"\x41" L"b";
    std::printf("strlit sizes %d %d %d %d %d %d %d\n", (int)std::strlen(p), (int)sizeof("\x41" "b"), (int)sizeof(a), (int)sizeof("\1" "2"),
        (int)sizeof(L"\x41" L"b"), (int)sizeof(wa), (int)sizeof(ga));
    units("strlit p", p, 3, 1); units("strlit a", a, 4, 1); units("strlit b", b, 8, 1); units("strlit gp", gp, 3, 1);
    units("strlit ga", ga, 4, 1); units("strlit local", local(), 5, 1); units("strlit cat", "\x4" "1" "\08" "\x2" "f", 6, 1);
    units("strlit w", w, 4, 2); units("strlit wa", wa, 3, 2); units("strlit gw", gw, 3, 2); units("strlit gwa", gwa, 4, 2);
    std::printf("strlit chars %d %d %d %d %d %d %d\n", 'ab', 'abcd', (int)sizeof('ab'), 'a\x62', '\0a', (int)sizeof('a'), ppOk);
    std::printf("strlit escapes %x %x %x %x %x %x %x %x\n", '\xff\xfe', '\x12\x34', 'a\x80', '\200a', 'a\n\tb', '\x11\x22\x33\x44', 'a\0b\0', '\"a\?');
    std::printf("strlit wide %x %x %d\n", (unsigned)L'\xffff', (unsigned)L'\x41', (int)sizeof(L'a'));
}
}

// floating constants converted to integers as cl folds them: modulo 2^N for any constant operand (a negation,
// arithmetic, a cast through float, a const double, ?:; only a bare literal was folded, the rest gave 80000000h),
// in code, globals and array bounds ((int)(unsigned char)300.0 + 1 was S114); converted straight on to a floating
// type the folded value is the untruncated one ((unsigned char)300.0 + 0.5 is 300.5, it was 44.5); a float
// literal rounded once from its decimal (1.00000005960464477550f is 3F800001h, through a double 3F800000h)
namespace cfold {
const double cd = 3e9;
const float hf = 1.00000005960464477550f;
double g1 = (unsigned char)300.0 + 0.5;
int g2 = (int)-2147483649.0;
long long g3 = (long long)-9.3e18;
unsigned long long g4 = (unsigned long long)-1.0;
int g5 = (int)cd;
double g6 = (int)3e9;
float g7 = 1.00000005960464477550f;
float g8[2] = { 1.00000005960464477530f, 16777217.000000001f };
int bound[(int)(unsigned char)300.0 + 1];
int bound2[(int)cd / 1000000000 + (int)(2.0 * 1.5)];
int take(int x) { return x; }
double takeD(double x) { return x; }
void show()
{
    const double lcd = 3e9;
    std::printf("cfold a %x %x %x %x %x\n", (int)-2147483649.0, (int)+3e9, (int)(2.0 * 1.5e9), (int)(3e9 + 1.0), (int)(float)1e10);
    std::printf("cfold b %llx %x %x %llx %x\n", (long long)-9.3e18, (int)cd, (int)lcd, (unsigned long long)-1e19, (int)1e20);
    std::printf("cfold c %.17g %.17g %.17g %.17g %.17g\n", (unsigned char)300.0 + 0.5, (double)(int)3e9, -(int)3e9 * 1.0, (double)(unsigned)-1.0, (short)1e5 * 2.0);
    int i = 3e9; double d = (int)3e9;
    std::printf("cfold d %x %.17g %x %.17g %x %llx\n", i, d, take(3e9), takeD((unsigned char)300.0), (int)(3e9 > 1 ? 3e9 : 1.0), (unsigned long long)(unsigned)-1.0);
    std::printf("cfold e %.17g %d %llx %llx %x %.17g %.17g\n", g1, g2, g3, g4, g5, g6, (double)(long long)(int)3e9);
    std::printf("cfold f %08x %08x %08x %08x %08x\n", fp::bitsF(g7), fp::bitsF(hf), fp::bitsF(1.00000005960464477550f), fp::bitsF(g8[0]), fp::bitsF(g8[1]));
    std::printf("cfold g %d %d\n", (int)(sizeof(bound) / sizeof(bound[0])), (int)(sizeof(bound2) / sizeof(bound2[0])));
}
}

// pointers to members in cl's representations (row 18): a data member an int offset with null -1, under virtual
// inheritance with the vbtable offset of the base's entry (null {0, -1}), in the unspecified model (the class
// incomplete when the size was first needed) with the vbptr's offset as well; a member function a code pointer
// (cl's vcall thunk for a virtual one), with an int this adjustment under multiple inheritance and the vbtable
// offset under virtual inheritance. Conversions add the base's offset (data: unless null), .* and ->* apply them,
// == compares every field (a function's beyond the pointer only when it is not null), static storage holds the
// null values cl writes (a class's own members, arrays, not nested classes), value-initialisation zeroes, a brace
// list's omitted members are null. The words of the representations are printed, except code pointers and padding
namespace memptr {
struct P { int a; int b; virtual int f() { return 1; } virtual int g(int x) { return x + 2; } int h() { return a; } P() : a(0), b(0) {} };
struct M { int m; int mf() { return m + 100; } virtual int mv() { return 3; } M() : m(0) {} };
struct Q : M, P { int q; Q() : q(0) {} };
struct A { virtual int fa() { return 1; } int x; A() : x(0) {} };
struct B { virtual int fb() { return 2; } virtual int fb2() { return 22; } int y; B() : y(0) {} };
struct AB : A, B { int fb() { return 5; } int z; AB() : z(0) {} };
struct S { int s; int t; int sf() { return s + 1000; } S() : s(0), t(0) {} };
struct V : virtual S { int v; V() : v(0) {} };
struct VD : V { int vd; VD() : vd(0) {} };
struct N { int n; int nf() { return n + 200; } N() : n(0) {} };
struct W : N, virtual S { int w; W() : w(0) {} };
struct Holder { int P::* field; int x; };
template <class T> struct H { int T::* f; int x; };
struct Outer { H<P> h; int y; };
struct Mem { H<P> h; int (Q::*qf)(); Mem() : h(), qf() {} };
struct Two { int P::* a[2]; int (Q::*q)(); };
struct Inc;
int incSize = sizeof(int Inc::*) + 100 * sizeof(int (Inc::*)());
struct Inc { int x; int y; int g() { return y; } Inc() : x(0), y(0) {} };
struct Inc2;
int inc2Size = sizeof(int Inc2::*) + 100 * sizeof(int (Inc2::*)());
struct Inc2 : N, virtual S { int i2; int g2() { return i2; } Inc2() : i2(0) {} };
int P::* pm = &P::b;
int P::* pmnull = 0;
int (P::*pf)() = &P::f;
int (P::*pg)(int) = &P::g;
int (P::*ph)() = &P::h;
int Q::* qm = &P::b;
int Q::* qmm = &M::m;
int Q::* qmnull = 0;
int (Q::*qf)() = &P::f;
int (Q::*qh)() = &P::h;
int (Q::*qmv)() = &M::mv;
int (Q::*qfnull)() = 0;
int (AB::*abfb)() = &B::fb;
int (AB::*abfb2)() = &AB::fb;
int (AB::*abfa)() = &A::fa;
int S::* sm = &S::t;
int V::* vm = &S::t;
int V::* vmnull = 0;
int (V::*vf)() = &S::sf;
int W::* wm = &S::t;
int W::* ww = &W::w;
int W::* wmm = &N::n;
int (W::*wf)() = &S::sf;
int (W::*wfnull)() = 0;
int Inc::* ix = &Inc::y;
int (Inc::*ig)() = &Inc::g;
int Inc::* incNull = 0;
int (Inc::*incFNull)() = 0;
int Inc2::* i2a = &Inc2::i2;
int Inc2::* i2s = &S::t;
int (Inc2::*i2g)() = &Inc2::g2;
int (Inc2::*i2sf)() = &S::sf;
int P::* arr[2] = { &P::a, 0 };
int P::* arr2[3];
Holder gh;
Outer go[2];
Outer go1;
Two gtwo;
struct SS { static int P::* smp; static Holder sho; };
int P::* SS::smp = 0;
Holder SS::sho;
int readP(P* p, int P::* m) { return p->*m; }
int readQ(Q& q, int Q::* m) { return q.*m; }
int callP(P* p, int (P::*f)()) { return (p->*f)(); }
int callPg(P* p, int (P::*f)(int), int x) { return (p->*f)(x); }
int callQ(Q& q, int (Q::*f)()) { return (q.*f)(); }
int callAB(AB* p, int (AB::*f)()) { return (p->*f)(); }
int readV(V* v, int V::* m) { return v->*m; }
int callV(V* v, int (V::*f)()) { return (v->*f)(); }
int readW(W* w, int W::* m) { return w->*m; }
int callW(W* w, int (W::*f)()) { return (w->*f)(); }
int readInc(Inc* p, int Inc::* m) { return p->*m; }
int callInc(Inc* p, int (Inc::*f)()) { return (p->*f)(); }
int readInc2(Inc2* p, int Inc2::* m) { return p->*m; }
int callInc2(Inc2* p, int (Inc2::*f)()) { return (p->*f)(); }
bool isnullP(int P::* m) { return m == 0; }
bool isnullQf(int (Q::*f)()) { return f == 0; }
bool eqQf(int (Q::*f)(), int (Q::*g)()) { return f == g; }
bool eqQm(int Q::* f, int Q::* g) { return f == g; }
bool isnullV(int V::* m) { return 0 == m; }
bool isnullWf(int (W::*f)()) { return !f; }
bool neWm(int W::* f, int W::* g) { return f != g; }
bool isNullInc(int Inc::* m) { return m == 0; }
bool isNullIncF(int (Inc::*f)()) { return f == 0; }
int Q::* convPQ(int P::* m) { return m; }
int Q::* convMQ(int M::* m) { return m; }
int (Q::*convPQf(int (P::*f)()))() { return f; }
int (Q::*convMQf(int (M::*f)()))() { return f; }
int V::* convSV(int S::* m) { return m; }
int VD::* convVVD(int V::* m) { return m; }
int (V::*convSVf(int (S::*f)()))() { return f; }
int W::* convSW(int S::* m) { return m; }
int (W::*convSWf(int (S::*f)()))() { return f; }
int Inc::* convInc(int Inc::* m) { return m; }
int P::* castQP(int Q::* m) { return static_cast<int P::*>(m); }
int M::* castQM(int Q::* m) { return static_cast<int M::*>(m); }
int (M::*castQMf(int (Q::*f)()))() { return static_cast<int (M::*)()>(f); }
bool toBoolP(int P::* m) { return m; }
bool toBoolW(int W::* m) { return m; }
int (P::*gf())() { return &P::g == pg ? &P::f : 0; }
bool cmpPQ(int P::* p, int Q::* q) { return p == q; }
bool cmpPQf(int (P::*p)(), int (Q::*q)()) { return p == q; }
struct Ret { int V::* rv(int S::* m) { return m; } int (Q::*rq(int (P::*f)()))() { return f; } int P::* rp() { return &P::a; } };
void dump(const char* name, const void* p, int n)
{
    std::printf("memptr %s:", name);
    for (int i = 0; i < n; i += 4) std::printf(" %x", *(const unsigned*)((const char*)p + i));
    std::printf("\n");
}
#define MP_DUMP(x) dump(#x, &x, (int)sizeof(x))
void show()
{
    Q q; q.a = 1; q.b = 2; q.m = 3; q.q = 4;
    P p; p.a = 5; p.b = 6;
    AB ab; ab.x = 7; ab.y = 8;
    V v; v.s = 9; v.t = 10; v.v = 11;
    VD vd; vd.s = 12; vd.t = 13; vd.v = 14; vd.vd = 15;
    W w; w.n = 16; w.s = 17; w.t = 18; w.w = 19;
    Inc i; i.x = 20; i.y = 21;
    Inc2 j; j.n = 22; j.s = 23; j.t = 24; j.i2 = 25;
    std::printf("memptr sizes %d %d %d %d %d %d %d %d %d %d\n", (int)sizeof(pm), (int)sizeof(pf), (int)sizeof(qm), (int)sizeof(qf), (int)sizeof(vm), (int)sizeof(vf), (int)sizeof(wm), (int)sizeof(wf), incSize, inc2Size);
    MP_DUMP(pm); MP_DUMP(pmnull); MP_DUMP(qm); MP_DUMP(qmm); MP_DUMP(qmnull); dump("qfnull", &qfnull, 12); MP_DUMP(sm); MP_DUMP(vm); MP_DUMP(vmnull);
    MP_DUMP(wm); MP_DUMP(ww); MP_DUMP(wmm); MP_DUMP(wfnull); MP_DUMP(arr); MP_DUMP(arr2); MP_DUMP(gh); MP_DUMP(go); MP_DUMP(go1); MP_DUMP(gtwo); MP_DUMP(SS::smp); MP_DUMP(SS::sho);
    MP_DUMP(ix); MP_DUMP(incNull); dump("ig+8", (char*)&ig + 8, 12); dump("incFNull", &incFNull, 24); MP_DUMP(i2a); MP_DUMP(i2s); dump("i2g+8", (char*)&i2g + 8, 12); dump("i2sf+8", (char*)&i2sf + 8, 12);
    std::printf("memptr adj %d %d %d %d %d %d %d %d %d %d\n", ((int*)&qf)[2], ((int*)&qh)[2], ((int*)&qmv)[2], ((int*)&abfb)[2], ((int*)&abfb2)[2], ((int*)&abfa)[2], ((int*)&vf)[2], ((int*)&vf)[3], ((int*)&wf)[2], ((int*)&wf)[3]);
    std::printf("memptr same %d %d %d %d\n", pf == &P::f, pg == &P::g, ph == &P::h, *(void**)&qf == *(void**)&pf);
    std::printf("memptr reads %d %d %d %d %d %d %d %d %d %d %d %d\n", readP(&p, pm), readQ(q, qm), readQ(q, qmm), readQ(q, &P::a), readV(&v, vm), readV(&vd, vm), readW(&w, wm), readW(&w, ww), readW(&w, wmm), p.*pm, q.*qm, (&q)->*qmm);
    std::printf("memptr reads2 %d %d %d %d %d %d\n", w.*ww, readInc(&i, ix), readInc(&i, &Inc::x), readInc2(&j, i2a), readInc2(&j, i2s), j.*i2s);
    std::printf("memptr calls %d %d %d %d %d %d %d %d\n", callP(&p, pf), callPg(&p, pg, 40), callP(&p, ph), callQ(q, qf), callQ(q, qh), callQ(q, qmv), callAB(&ab, abfb), callAB(&ab, abfb2));
    std::printf("memptr calls2 %d %d %d %d %d %d %d\n", callAB(&ab, abfa), callV(&v, vf), callV(&vd, vf), callW(&w, wf), (p.*pf)(), (q.*pf)(), (ab.*abfb2)());
    std::printf("memptr calls3 %d %d %d %d\n", callInc(&i, ig), callInc2(&j, i2g), callInc2(&j, i2sf), (j.*i2sf)());
    std::printf("memptr nulls %d %d %d %d %d %d %d %d %d %d\n", isnullP(pm), isnullP(pmnull), isnullQf(qf), isnullQf(qfnull), isnullV(vm), isnullV(vmnull), isnullWf(wf), isnullWf(wfnull), toBoolP(pm), toBoolP(pmnull));
    std::printf("memptr nulls2 %d %d %d %d %d %d %d %d %d %d\n", toBoolW(wm), toBoolW(ww), toBoolW(0), neWm(wm, ww), neWm(wm, convSW(sm)), isNullInc(ix), isNullInc(incNull), isNullIncF(ig), isNullIncF(incFNull), isNullIncF(0));
    std::printf("memptr eq %d %d %d %d %d %d %d %d %d %d %d %d\n", eqQf(qf, qf), eqQf(qf, qh), eqQf(qfnull, 0), eqQm(qm, qm), eqQm(qm, qmm), eqQm(qmnull, 0), cmpPQ(pm, qm), cmpPQf(pf, qf), cmpPQf(ph, qh), cmpPQf(0, qfnull), eqQf(convMQf(&M::mf), convMQf(&M::mf)), eqQf(convMQf(0), convMQf(0)));
    int Q::* c1 = convPQ(pm); int Q::* c2 = convMQ(&M::m); int Q::* c3 = convMQ(0); int Q::* c4 = convPQ(0);
    int (Q::*c5)() = convPQf(ph); int (Q::*c6)() = convMQf(&M::mf); int (Q::*c7)() = convMQf(0);
    int V::* c8 = convSV(sm); int V::* c9 = convSV(0); int VD::* c10 = convVVD(vm); int (V::*c11)() = convSVf(&S::sf);
    int W::* c12 = convSW(sm); int W::* c13 = convSW(0); int (W::*c14)() = convSWf(&S::sf); int Inc::* c15 = convInc(ix);
    MP_DUMP(c1); MP_DUMP(c2); MP_DUMP(c3); MP_DUMP(c4); dump("c7", &c7, 12); MP_DUMP(c8); MP_DUMP(c9); MP_DUMP(c10); MP_DUMP(c12); MP_DUMP(c13); MP_DUMP(c15);
    std::printf("memptr conv %d %d %d %d %d %d %d %d\n", ((int*)&c5)[2], ((int*)&c6)[2], ((int*)&c11)[2], ((int*)&c11)[3], ((int*)&c14)[2], ((int*)&c14)[3], c5 == qh, c15 == ix);
    std::printf("memptr convcalls %d %d %d %d %d %d\n", callQ(q, c5), callQ(q, c6), callV(&v, c11), callW(&w, c14), readQ(q, c1), readQ(q, c2));
    int P::* d1 = castQP(qm); int M::* d2 = castQM(qmm); int M::* d3 = castQM(0); int (M::*d4)() = castQMf(c6);
    std::printf("memptr down %d %d %d %d %d %d %d\n", d1 == pm, d2 == &M::m, d3 == 0, d4 == &M::mf, (q.*d4)(), gf() == pf, gf() != 0);
    H<P> h1 = H<P>(); Holder h2 = Holder(); Outer o1 = { }; H<P> h3 = { }; Mem mm; Holder h4 = { &P::a }; Two t1 = { }; Outer o2 = { { } };
    int P::* a1[2] = { &P::a }; int P::* a2[2] = { };
    MP_DUMP(h1); MP_DUMP(h2); MP_DUMP(o1); MP_DUMP(h3); dump("mm", &mm, 20); MP_DUMP(h4); dump("t1", &t1, 20); MP_DUMP(o2); MP_DUMP(a1); MP_DUMP(a2);
    Outer* no = new Outer(); dump("new Outer()", no, 12); delete no;
    Outer* na = new Outer[2](); dump("new Outer[2]()", &na[1], 12); delete[] na;
    { static Holder sh; static int P::* sp; static int (Q::*sq)(); MP_DUMP(sh); MP_DUMP(sp); dump("sq", &sq, 12); }
    Ret r;
    int V::* r1 = r.rv(&S::t); int (Q::*r2)() = r.rq(&P::f); int P::* r3 = r.rp();
    MP_DUMP(r1); std::printf("memptr ret %d %d %d %d\n", ((int*)&r2)[2], r2 == qf, r3 == &P::a, callQ(q, r2));
    int P::* local = 0; int (Q::*localq)() = 0; int W::* localw = 0; int (W::*localwf)() = 0; int Inc::* ln = 0; int (Inc::*lfn)() = 0;
    MP_DUMP(local); dump("localq", &localq, 12); MP_DUMP(localw); MP_DUMP(localwf); MP_DUMP(ln); dump("lfn+8", (char*)&lfn + 8, 12);
    local = pm; localq = qf; localw = ww; localwf = wf;
    std::printf("memptr assign %d %d %d %d %d %d\n", local == pm, localq == qf, localw == ww, localwf == wf, ix ? 1 : 0, lfn ? 1 : 0);
    int P::* ternary = p.a > 3 ? pm : 0; int (Q::*ternq)() = p.a > 3 ? qf : 0;
    const P* cp = &p; const int P::* cpm = &P::b;
    std::printf("memptr ternary %d %d %d %d %d %d\n", ternary == pm, ternq == qf, ternary ? 1 : 0, ternq ? 1 : 0, cp->*cpm, cp->*pm);
}
}

// covariant returns through a secondary base (row 19), as cl lays them out: the overrider gets a slot of its own at
// the end of the vftable that holds the overridden function (in the primary table in declaration order among the
// class's new virtual functions), and every slot it overrides holds a return-adjusting thunk that calls it and
// moves a non-null pointer (a reference always) by the base's offset; an override whose conversions move nothing
// (PD : SB) stays in place. The driver of test 13 calls them across the compilers; here every call goes through
// every base at -O0 and -O2, with stack arguments staged again by the thunk (six)
namespace covariant {
struct SA { virtual ~SA() {} int a; SA() : a(1) {} };
struct SB { virtual ~SB() {} virtual SB* self() { return this; } virtual SB& selfRef() { return *this; } virtual SB* six(int p, int q, int r, int s, double t, int u) { return p + q + r + s + (int)t + u == 21 ? this : 0; } int b; SB() : b(2) {} };
struct CD : SA, SB { CD* self() { return this; } CD& selfRef() { return *this; } CD* six(int p, int q, int r, int s, double t, int u) { return p + q + r + s + (int)t + u == 21 ? this : 0; } virtual int cd() { return 5; } int c; CD() : c(3) {} };
struct CD2 : SA, SB { virtual int cd2() { return 6; } CD2* self() { return this; } virtual int cd3() { return 7; } };
struct E : CD { E* self() { return this; } int e; E() : e(4) {} };
struct PD : SB { PD* self() { return this; } };
struct PE : PD { virtual int more() { return 1; } PE* self() { return this; } };
struct X { virtual X* make() { return this; } int x; };
struct Y { virtual int yf() { return 1; } int y; };
struct Z : Y, X { Z* make() { return this; } int z; };
struct K { virtual SB* k() { return 0; } virtual int kk() { return 2; } int q; };
struct L : K { virtual int ln() { return 3; } CD* k() { return &held; } virtual int lm() { return 4; } CD held; };
struct NullCD : SA, SB { CD* self2() { return 0; } };
struct NB { virtual ~NB() {} virtual CD* get() { return 0; } };
struct ND : SA, NB { E* get() { return 0; } };
SB* viaSB(SB* p) { return p->self(); }
CD* viaCD(CD* p) { return p->self(); }
SB& viaSBref(SB& p) { return p.selfRef(); }
CD& viaCDref(CD& p) { return p.selfRef(); }
SB* viaSix(SB* p, int u) { return p->six(1, 2, 3, 4, 5.0, u); }
CD* viaSixCD(CD* p, int u) { return p->six(1, 2, 3, 4, 5.0, u); }
X* viaX(X* p) { return p->make(); }
SB* viaK(K* k) { return k->k(); }
CD* viaL(L* l) { return l->k(); }
void show()
{
    CD c; E e; Z z; L l; PD pd; PE pe; CD2 c2; ND nd;
    std::printf("covariant self %d %d %d %d %d %d %d\n", viaSB(&c) == &c, viaCD(&c) == &c, viaSB(&e) == &e, viaCD(&e) == &e, static_cast<SB*>(&e)->self() == &e, e.self() == &e, viaSB(&c2) == &c2);
    std::printf("covariant ref %d %d %d %d\n", &viaSBref(c) == &c, &viaCDref(c) == &c, &viaSBref(e) == &e, &c.selfRef() == &c);
    std::printf("covariant six %d %d %d %d %d\n", viaSix(&c, 6) == &c, viaSix(&c, 7) == 0, viaSixCD(&c, 6) == &c, viaSixCD(&c, 7) == 0, viaSix(&e, 6) == &e);
    std::printf("covariant primary %d %d %d %d %d %d %d\n", viaX(&z) == &z, z.make() == &z, viaK(&l) == &l.held, viaL(&l) == &l.held, l.ln() + l.lm() + l.kk(), viaSB(&pd) == &pd, viaSB(&pe) == &pe && pe.more() == 1);
    std::printf("covariant sizes %d %d %d\n", (int)sizeof(CD), (int)sizeof(E), (int)sizeof(L));
    SB* nullSb = 0;
    std::printf("covariant null %d %d %d\n", static_cast<NB*>(&nd)->get() == 0, nd.get() == 0, nullSb == 0);
}
}

// polymorphic virtual bases, as cl lays them out: the vfptr stays in the virtual base subobject and a class gets a
// vfptr of its own only for new virtual functions (VN); the virtual base's vftable has a copy per class
// (??_7VL@@6B@, ??_7VN@@6BV@@@) whose entries from outside the base are adjustor thunks, or cl's $4 vtordisp thunks
// when the class has a user-declared constructor or destructor and overrides a function of the base (the vtordisp,
// 4 bytes before the base, holds how far the base moved while a base constructor runs). Every call goes through
// every base, during construction and destruction too, through delete, and through pointers to members
namespace pvbase {
struct V  { int v; V() : v(7) { std::printf("V() %d ", probe()); } virtual int f() const { return v; } virtual int g() const { return 1; } virtual ~V() { std::printf("~V %d ", probe()); } int probe() const { return f() * 100 + g(); } };
struct VL : virtual V { int l; VL() : l(2) { std::printf("VL() %d ", probe()); } ~VL() { std::printf("~VL %d ", probe()); } int f() const { return l + 10; } };
struct VR : virtual V { int r; VR() : r(3) { std::printf("VR() %d ", probe()); } };
struct VD : VL, VR { int d; VD() : d(4) { std::printf("VD() %d ", probe()); } ~VD() { std::printf("~VD %d ", probe()); } int g() const { return d + 20; } };
struct VN : virtual V { int n; VN() : n(5) {} virtual int h() const { return 3; } };
struct VN2 : VN { int n2; VN2() : n2(8) {} int h() const { return n2; } int g() const { return n2 + 1; } };
struct VO : virtual V { int o; int f() const { return o + 30; } };
struct VO2 : VO { int o2; };
struct VL2 : VL { int l2; VL2() : l2(5) { std::printf("VL2() %d ", probe()); } };
struct NB { int nb; NB() : nb(4) {} virtual int nf() const { return nb; } };
struct MX : NB, virtual V { int mx; MX() : mx(6) { std::printf("MX() %d ", probe()); } int f() const { return mx + 40; } int nf() const { return mx + 1; } };
struct MY : NB, virtual V { int my; MY() : my(2) {} virtual int h() const { return 9; } int f() const { return my; } };
struct VV { int vv; VV() : vv(5) {} virtual ~VV() { std::printf("~VV "); } virtual int k() const { return vv; } };
struct VY : virtual VV { int y; VY() : y(6) {} ~VY() { std::printf("~VY "); } int k() const { return y + 50; } };
struct VY2 : VY { int y2; VY2() : y2(9) {} ~VY2() { std::printf("~VY2 "); } };
struct VP : NB, virtual V { int p; VP() : p(8) {} int f() const { return p; } };
int callF(const V* p) { return p->f() * 100 + p->g(); }
int callVL(const VL* p) { return p->f() * 100 + p->g(); }
int callVD(const VD* p) { return p->f() * 100 + p->g(); }
int callVN(const VN* p) { return p->f() * 100 + p->g() + p->h() * 10000; }
int callMX(const MX* p) { return p->f() * 100 + p->nf(); }
int callMY(const MY* p) { return p->f() * 100 + p->nf() + p->h() * 10000; }
int callK(const VV* p) { return p->k(); }
void destroyV(V* p) { delete p; }
void destroyVV(VV* p) { delete p; }
void show()
{
    std::printf("pvbase build: "); VD d; std::printf("\n");
    VL a; VR b; VN n; VN2 n2; VO o; o.o = 1; VO2 o2; o2.o = 2; VL2 l2; MX mx; MY my; VY y; VY2 y2; VP vp;
    std::printf("\npvbase calls %d %d %d %d %d %d %d %d %d %d %d\n", callF(&a), callF(&b), callF(&d), callF(&n), callF(&n2), callF(&o), callF(&o2), callF(&l2), callF(&mx), callF(&my), callK(&y));
    std::printf("pvbase calls2 %d %d %d %d %d %d %d %d %d\n", callVL(&a), callVL(&d), callVD(&d), callVN(&n), callVN(&n2), callMX(&mx), callMY(&my), callK(&y2), callF(&vp));
    std::printf("pvbase direct %d %d %d %d %d %d %d\n", d.f(), d.g(), d.VL::f(), d.V::f(), n2.h(), y2.k(), vp.f());
    std::printf("pvbase sizes %d %d %d %d %d %d %d %d %d\n", (int)sizeof(VL), (int)sizeof(VD), (int)sizeof(VN), (int)sizeof(VN2), (int)sizeof(VO2), (int)sizeof(MX), (int)sizeof(MY), (int)sizeof(VY2), (int)sizeof(VP));
    VD* hd = new VD; std::printf("\npvbase heap %d ", callF(hd)); destroyV(hd); std::printf("\n");
    VL2* hl = new VL2; std::printf("\npvbase heap2 %d ", callF(hl)); destroyV(hl); std::printf("\n");
    VY2* hy = new VY2; std::printf("pvbase heap3 %d ", callK(hy)); destroyVV(hy); std::printf("\n");
    VD c(d); std::printf("pvbase copy %d %d\n", callF(&c), c.d);
    VD* arr = new VD[2]; std::printf("\npvbase array %d %d ", callF(&arr[0]), callF(&arr[1])); delete[] arr; std::printf("\n");
    int VP::* pm = &VP::p; int (VP::*pf)() const = &VP::f; int (VP::*pg)() const = &V::g; int VD::* pv = &V::v; int (VD::*pdg)() const = &V::g;
    std::printf("pvbase memptr %d %d %d %d %d %d %d\n", vp.*pm, (vp.*pf)(), (vp.*pg)(), (my.*static_cast<int (MY::*)() const>(&V::f))(), d.*pv, (d.*pdg)(), (hd = &d)->*pv);
    std::printf("pvbase end: ");
}
}

// the review of 2026-10-15 (silent miscompiles B3, B4, M4, M5, M6, M8, M9): each case against cl at -O0 and -O2
namespace convop {   // B3: casts through conversion functions
struct Ci { int re; Ci(int r) : re(r) {} operator int() const { return re; } };
struct Cd { double re; Cd(double r) : re(r) {} operator double() const { return re; } };
struct Cp { const char* s; Cp(const char* p) : s(p) {} operator const char*() const { return s; } };
struct Der : Ci { Der(int x) : Ci(x) {} };
struct Ref { int w; Ref(int x) : w(x) {} operator const int&() const { return w; } };
struct Str { const char* s; Str(const char* p) : s(p) {} };
struct ToStr { operator Str() const { return Str("conv"); } };
struct Both { operator int() const { return 3; } operator double() const { return 2.5; } };
struct Vp { int v; Vp(int x) : v(x) {} operator void*() const { return v ? const_cast<Vp*>(this) : 0; } };
Ci mki() { return Ci(4); }
Cd mkd() { return Cd(2.5); }
int uc(int) { return 1; } int uc(double) { return 2; } int uc(long) { return 3; }
void show()
{
    Ci a(7); Cd b(1.5); Cp c("str"); Der d(9); Ref r(11); ToStr ts; Both bo; Vp yes(1), no(0);
    std::printf("convop int %d %d %d %d | %d %d %d %d\n", (int)a, int(a), static_cast<int>(a), a, (int)mki(), int(mki()), static_cast<int>(mki()), mki());
    std::printf("convop double %g %g %g | %g %g %g\n", (double)b, double(b), static_cast<double>(b), (double)mkd(), static_cast<double>(mkd()), b * 2);
    std::printf("convop pointer %s %s %s\n", (const char*)c, static_cast<const char*>(c), c);
    std::printf("convop wider %ld %g %hd %d | %d %ld %d\n", (long)a, (double)a, (short)a, (unsigned char)d, (int)r, (long)r, static_cast<int>(r));
    Str s1 = (Str)ts; Str s2 = static_cast<Str>(ts); Str s3 = Str(ts); Str s4 = (Str)"lit";
    std::printf("convop class %s %s %s %s\n", s1.s, s2.s, s3.s, s4.s);
    std::printf("convop best %d %g | %d %d %d\n", (int)bo, (double)bo, uc(a), uc(d), uc(Ci(2)));
    std::printf("convop bool %d %d %d %d\n", (int)(bool)yes, (int)static_cast<bool>(no), yes ? 1 : 0, (void*)no == 0);
}
}

namespace reftemp {   // B4: references bound to temporaries; M9: the lifetime of the other temporaries of the initializer
struct L { int v; L(int x) : v(x) { std::printf("L%d ", v); } L(const L& o) : v(o.v + 100) { std::printf("Lc%d ", v); } ~L() { std::printf("~L%d ", v); } };
struct ToL { int k; ToL(int x) : k(x) {} operator L() const { return L(k); } };
const int& gr = 5;
const double& gd = 3;
const L& gl = 7;
static const L& gsl = 8;
int sum(const int (&a)[3]) { return a[0] + a[1] + a[2]; }
long twice(const long& x) { return x * 2; }
double dbl(const double& d) { return d * 2; }
int byRef(const int& x) { return x + 1; }
const L& pass(const L& l) { return l; }
L mk(int x) { return L(x); }
int use(const L& l) { return l.v; }
void show()
{
    std::printf("reftemp globals %d %g %d %d\n", gr, gd, gl.v, gsl.v);
    { const L& r = 9; std::printf("| %d ", r.v); } std::printf("| scope\n");
    { ToL t(4); const L& r = t; std::printf("| %d ", r.v); } std::printf("| scope\n");
    { short s = 6; int i = 3; const long& lr = s; const int& ir = s + 1; const double& cd = i; const int& cr = 5; std::printf("reftemp scalars %ld %d %g %d %ld %g %d\n", lr, ir, cd, cr, twice(s), dbl(i), byRef(41)); }
    { int a[3] = { 1, 2, 3 }; std::printf("reftemp array %d\n", sum(a)); }
    { static const int& si = 11; static const L& sl = 12; std::printf("reftemp statics %d %d\n", si, sl.v); }
    { const char* const& p = "lit"; char buf[4] = "abc"; const char* const& p2 = buf; std::printf("reftemp ptrs %s %s\n", p, p2); }
    { const L& r = pass(L(20)); std::printf("| after full expr %d ", r.v == r.v); } std::printf("| scope\n");
    { const L& r = L(21); std::printf("| bound "); } std::printf("| scope\n");
    { const L& r = mk(22); std::printf("| bound "); } std::printf("| scope\n");
    { int v = use(mk(23)) + use(L(24)); std::printf("| %d ", v); } std::printf("| scope\n");
    { L a(25); const L& r = true ? L(26) : a; std::printf("| ?: %d ", r.v); } std::printf("| scope\n");
}
}

namespace indirect {   // M5: argument conversions of a call through a function pointer
struct Base { int flags; Base() : flags(0) {} };
struct Der : Base { virtual ~Der() {} int extra; Der() : extra(0) {}
    Der& apply(Base& (*m)(Base&)) { m(*this); return *this; }
    Der& applyPtr(Base* (*m)(Base*)) { m(this); return *this; } };
Base& setHex(Base& b) { b.flags |= 8; return b; }
Base* setOct(Base* b) { b->flags |= 16; return b; }
Base& direct(Der& d, Base& (*m)(Base&)) { return m(d); }
double half(const double& d) { return d / 2; }
long widen(long x, double y, const Base& b) { return x * 1000 + (long)y + b.flags; }
void show()
{
    Der d, e, f, g;
    d.apply(setHex); e.applyPtr(setOct); direct(f, setHex); setHex(g);
    double (*ph)(const double&) = half; long (*pw)(long, double, const Base&) = widen;
    int i = 7; short s = 2; Der h; h.flags = 3;
    std::printf("indirect %d %d %d %d %d | %g %ld\n", d.flags, e.flags, f.flags, g.flags, d.extra, ph(i), pw(s, 1.5f, h));
}
}

namespace narrow {   // M4: ++ and -- on narrow types
void show()
{
    unsigned char ov = 255; ov++; short sh = 32767; sh++; signed char sc = 127; ++sc; unsigned short us = 65535; us++;
    unsigned char ov2 = 0; ov2--; short sh2 = -32768; --sh2; bool b = false; b++; char c = 127; c++;
    unsigned char arr[2] = { 255, 0 }; arr[0]++; ++arr[1]; float fl = 1.5f; fl++; --fl;
    std::printf("narrow %d %d %d %d | %d %d %d %d | %d %d %g\n", ov, sh, sc, us, ov2, sh2, b, c, arr[0], arr[1], fl);
    unsigned char x = 255; int y = x++; int z = ++x; std::printf("narrow values %d %d %d\n", x, y, z);
}
}

namespace leafctor {   // M8: an implicit default constructor first needed by a global's initialiser
struct Base { int v; Base() : v(7) {} };
struct Leaf : Base {};
struct Leaf2 : Base { int w; };
Leaf g;
Leaf2 g2[2];
void show() { Leaf2 l; std::printf("leafctor %d %d %d %d\n", g.v, g2[0].v, g2[1].v, l.v); }
}

namespace ranking {   // M6: overload ranking of conversions to a base over void* and bool
struct Base {}; struct Der : Base {}; struct Der2 : Der {};
int db(Base*) { return 1; } int db(void*) { return 2; }
int dr(const Base&) { return 1; } int dr(bool) { return 2; }
int bl(bool) { return 1; } int bl(Base*) { return 2; }
int cv(const Der*) { return 1; } int cv(void*) { return 2; }
void show()
{
    Der d; Der2 d2; Base b;
    std::printf("ranking %d %d %d %d %d %d %d\n", db(&d), db(&d2), db(&b), dr(d), bl(&d), cv(&d2), cv((Base*)&d));
}
}

namespace adl {   // B2: argument-dependent lookup on ordinary calls, in-class friend definitions
namespace N {
    struct S { int v; S(int x) : v(x) {} };
    int dbl2(S s) { return s.v * 2; }
    int dbl2(S* p) { return p->v * 3; }
    enum Color { Red, Green };
    int paint(Color c) { return c == Red ? 1 : 2; }
    namespace Inner { struct T : S { T(int x) : S(x) {} }; int thrice(const T& t) { return t.v * 3; } }
    template <class X> struct Box { X x; Box(X a) : x(a) {} };
    template <class X> int unbox(const Box<X>& b) { return b.x; }
}
namespace M { struct U { int u; }; int viaArg(const N::Box<U>& b) { return b.x.u; } }
struct V {
    int x; V(int a) : x(a) {}
    friend V operator+(const V& a, const V& b) { return V(a.x + b.x); }
    friend int norm(const V& v) { return v.x < 0 ? -v.x : v.x; }
    friend void swap(V& a, V& b) { int t = a.x; a.x = b.x; b.x = t; }
    friend int later(const V& v);
    friend bool operator==(const V& a, const V& b) { return a.x == b.x; }
};
int later(const V& v) { return v.x + 1000; }
struct W : V { W(int a) : V(a) {} };
template <class T> struct Tmpl { T t; Tmpl(T v) : t(v) {} friend int peek(const Tmpl& b) { return (int)b.t * 10; } };
template <class T> int callDbl2(T t) { return dbl2(t); }        // found by ADL at the instantiation
int local(int) { return -1; }
namespace Q { struct R {}; int local(R) { return 7; } int shadow(R) { return 1; } }
int shadow(int) { return 2; }
void show()
{
    N::S s(21); N::Inner::T t(5); M::U u0 = { 9 }; N::Box<M::U> bu(u0); N::Box<int> bi(4);
    std::printf("adl %d %d %d %d %d %d %d %d\n", dbl2(s), dbl2(&s), paint(N::Green), thrice(t), dbl2(t), viaArg(bu), unbox(bi), callDbl2(s));
    V a(-3), b(5); W w(8);
    std::printf("adl friends %d %d %d %d %d\n", (a + b).x, norm(a), later(b), norm(w), (int)(a == b));
    swap(a, b); std::printf("adl swap %d %d\n", a.x, b.x);
    Tmpl<double> td(2.5); Tmpl<int> ti(3);
    std::printf("adl template friends %d %d\n", peek(td), peek(ti));
    Q::R r; std::printf("adl rules %d %d %d\n", local(r), shadow(r), shadow(1));
}
}

namespace rtti {   // B1: typeid and dynamic_cast through cl's RTTI data and vcruntime

struct B { virtual ~B() {} int b; };
struct C { virtual ~C() {} int c; };
struct D : B, C { int d; };
struct E : virtual B { int e; };
struct F : virtual B { int f; };
struct G : E, F { int g; };
struct NB { int nb; };
struct V : NB { virtual int f() { return 1; } };
struct W : virtual V { int w; };
struct X : W { int x; };
struct Plain { int p; };

void show()
{
    D d; B* pb = &d; C* pc = &d;
    std::printf("rtti down: %d\n", dynamic_cast<D*>(pb) == &d);
    std::printf("rtti down from C: %d\n", dynamic_cast<D*>(pc) == &d);
    std::printf("rtti cross: %d %d\n", dynamic_cast<C*>(pb) == pc, dynamic_cast<B*>(pc) == pb);
    B lone; B* pl = &lone;
    std::printf("rtti fail is null: %d\n", dynamic_cast<D*>(pl) == 0);
    std::printf("rtti fail cross null: %d\n", dynamic_cast<C*>(pl) == 0);
    G g; B* gb = &g; E* ge = &g; F* gf = &g;
    std::printf("rtti vbase down: %d %d %d\n", dynamic_cast<G*>(gb) == &g, dynamic_cast<G*>(ge) == &g, dynamic_cast<G*>(gf) == &g);
    std::printf("rtti vbase to E/F: %d %d %d\n", dynamic_cast<E*>(gb) == static_cast<E*>(&g), dynamic_cast<F*>(gb) == static_cast<F*>(&g), dynamic_cast<F*>(ge) == static_cast<F*>(&g));
    std::printf("rtti void*: %d %d %d\n", dynamic_cast<void*>(pc) == (void*)&d, dynamic_cast<void*>(gf) == (void*)&g, dynamic_cast<void*>(pl) == (void*)&lone);
    X xo; V* xv = &xo; W* xw = &xo;
    std::printf("rtti vbase chain: %d %d %d\n", dynamic_cast<X*>(xv) == &xo, dynamic_cast<W*>(xv) == xw, dynamic_cast<NB*>(xw) == static_cast<NB*>(&xo));
    std::printf("rtti null in: %d %d\n", dynamic_cast<D*>((B*)0) == 0, dynamic_cast<void*>((C*)0) == 0);
    std::printf("rtti typeid same: %d %d %d\n", typeid(*pb) == typeid(D), typeid(*pl) == typeid(D), typeid(*pl) == typeid(B));
    std::printf("rtti typeid static: %d %d %d %d\n", typeid(int) == typeid(int), typeid(int) == typeid(long), typeid(d) == typeid(D), typeid(Plain) != typeid(D));
    std::printf("rtti typeid refs: %d %d\n", typeid(*gb) == typeid(G), typeid(*xv) == typeid(X));
    const std::type_info& t = typeid(*ge);
    std::printf("rtti typeid through vbase: %d %d\n", t == typeid(G), typeid(*xw) == typeid(X));
    std::printf("rtti names: %s %s %s %s\n", typeid(d).name(), typeid(int).name(), typeid(const char*).name(), typeid(*pl).name());
    std::printf("rtti raw names: %s %s\n", typeid(D).raw_name(), typeid(double).raw_name());
    std::printf("rtti before: %d %d\n", typeid(B).before(typeid(C)) != typeid(C).before(typeid(B)), typeid(B).before(typeid(B)));
    try { D& r = dynamic_cast<D&>(*pb); std::printf("rtti ref ok %d\n", &r == &d); } catch (std::bad_cast&) { std::printf("rtti bad_cast?\n"); }
    try { dynamic_cast<D&>(*pl); std::printf("rtti no throw\n"); } catch (std::bad_cast& e) { std::printf("rtti bad_cast %s\n", e.what()); }
    try { dynamic_cast<C&>(*pl); std::printf("rtti no throw\n"); } catch (std::exception& e) { std::printf("rtti as exception %s\n", e.what()); }
    B* nullp = 0;
    try { typeid(*nullp); std::printf("rtti no throw\n"); } catch (std::bad_typeid& e) { std::printf("rtti bad_typeid %s\n", e.what()); }
    std::printf("rtti typeid of null expression not dereferenced: %d\n", typeid(nullp) == typeid(B*));
    std::printf("rtti sizes %d %d\n", (int)sizeof(std::type_info) > 8, (int)sizeof(std::bad_cast) > 0);
}
}

namespace templ {   // M2, M1, M13: function templates (explicit arguments, specialisations, partial ordering, SFINAE, deduction from function types), template template parameters
template <class T> int kind(T) { return 1; }
template <> int kind<double>(double) { return 2; }
int kind(char) { return 3; }
template <class T> int kind2(T) { return 1; }
template <> int kind2(float) { return 4; }                    // the argument deduced from the parameter
template <class T> T make(int v) { return T(v); }
template <class R, class A> R conv2(A a) { return static_cast<R>(a); }
struct Cv { template <class U> U conv(double t) { return static_cast<U>(t); } int plain(int x) { return x + 1; } };
template <class T> int depcall(T& t) { return t.template conv<int>(2.7); }
template <class T> char probe(typename T::type*);
template <class T> long probe(...);
struct HasType { typedef int type; };
struct NoType {};
template <class T> int order(T) { return 1; }
template <class T> int order(T*) { return 2; }
template <class T> int order(const T*) { return 3; }
template <class T> int ref(T&) { return 1; }
template <class T> int ref(const T&) { return 2; }
template <class T> struct Box { T v; Box(T x) : v(x) {} int tag() const; };
template <class T> int Box<T>::tag() const { return 1; }
template <> int Box<char>::tag() const { return 2; }           // member specialisation
template class Box<long>;                                      // explicit instantiation
template int order<short>(short);
template <class R, class A> int arity(R (*)(A)) { return 1; }
template <class R, class A, class B> int arity(R (*)(A, B)) { return 2; }
template <class R> int retOf(R (*)()) { return sizeof(R); }
int one(int) { return 0; } int two(int, double) { return 0; } double three() { return 0; }
template <class T> struct Id { typedef T type; };
template <class T> int nondeduced(typename Id<T>::type x, T y) { return x + y; }
template <class T, int N> int bound(const T (&)[N]) { return N; }
template <class T> struct Counter { static int count; Counter() { ++count; } };
template <class T> int Counter<T>::count = 0;
template <class T> struct Outer { template <class U> struct Inner { U u; }; template <class U> U conv(T t) { return static_cast<U>(t); } };
template <class T> struct BoxT { T v; BoxT(T x) : v(x) {} T twice() const { return v * 2; } };
template <class T, template <class> class Holder> struct Wrap { Holder<T> h; Wrap(T x) : h(x) {} T get() const { return h.v; } T get2() const { return h.twice(); } int size() const { return (int)sizeof(h); } };
template <class T> struct Rec { static int depth() { return 1 + Rec<typename T::next>::depth(); } };
struct End {};
template <> struct Rec<End> { static int depth() { return 0; } };
struct L2 { typedef End next; }; struct L1 { typedef L2 next; };
void show()
{
    std::printf("templ spec %d %d %d %d | %d %d\n", kind(1), kind(2.0), kind('c'), kind<int>('c'), kind2(1.5f), kind2(2));
    std::printf("templ explicit %d %d %d %ld\n", make<int>(77), (int)make<double>(5), conv2<int>(3.9), conv2<long, double>(2.5));
    Cv c; Outer<double> od;
    std::printf("templ member explicit %d %d %d %d\n", c.conv<int>(3.9), c.plain(1), depcall(c), od.conv<int>(4.6));
    std::printf("templ sfinae %d %d\n", (int)(sizeof(probe<HasType>(0)) == 1), (int)(sizeof(probe<NoType>(0)) == 1));
    int x = 0; const int* cp = &x; const char arr[7] = "abcdef";
    std::printf("templ order %d %d %d | %d %d\n", order(x), order(&x), order(cp), ref(x), ref(5));
    std::printf("templ member spec %d %d %d\n", Box<int>(1).tag(), Box<char>('a').tag(), Box<long>(2L).tag());
    std::printf("templ fn types %d %d %d | nondeduced %d bound %d\n", arity(one), arity(two), retOf(three), nondeduced<int>(1, 2), bound(arr));
    Counter<int> a, b; Counter<double> cc;
    std::printf("templ static %d %d\n", Counter<int>::count, Counter<double>::count);
    Outer<double>::Inner<char> in; in.u = 'z';
    Wrap<int, BoxT> w(5);
    std::printf("templ tt %c %d %d %d %d\n", in.u, w.get(), w.get2(), w.size(), (int)sizeof(w));
    std::printf("templ rec %d\n", Rec<L1>::depth());
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
    valinit::show();
    varargs::show();
    mplater::show();
    strlit::show();
    cfold::show();
    std::printf("a class named T: %d\n", tclass::useT());
    memptr::show();
    covariant::show();
    pvbase::show();
    convop::show();
    reftemp::show();
    indirect::show();
    narrow::show();
    leafctor::show();
    ranking::show();
    adl::show();
    rtti::show();
    templ::show();
    std::printf("\nhiding after using namespace std: %d\n", localHidesFileUsing(5));
    // ::printf, found directly and through std's "using ::printf", is one function, not an ambiguity
    printf("unqualified printf after using namespace std: %d %s\n", 5, "ok");
    // library results are typed by the headers: a double from strtod, a size_t from string::size
    std::string s("abc"); double d = std::strtod("2.5", 0); std::size_t n = s.size(); long long q = n * 3;
    printf("library results: %f %d %lld\n", d * 2, (int)n, q);
    return 0;
}
