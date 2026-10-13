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

int main()
{
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
    std::printf("hiding after using namespace std: %d\n", localHidesFileUsing(5));
    // ::printf, found directly and through std's "using ::printf", is one function, not an ambiguity
    printf("unqualified printf after using namespace std: %d %s\n", 5, "ok");
    // library results are typed by the headers: a double from strtod, a size_t from string::size
    std::string s("abc"); double d = std::strtod("2.5", 0); std::size_t n = s.size(); long long q = n * 3;
    printf("library results: %f %d %lld\n", d * 2, (int)n, q);
    return 0;
}
