// front.cpp - version 6 front-end regressions that compile (test 17): built by cppparse --compile6 and by cl,
// the two programs must print the same lines
#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
    conv::runtimeD(); conv::runtimeF(); conv::consts(); conv::toFloating();
    std::printf("hiding after using namespace std: %d\n", localHidesFileUsing(5));
    // ::printf, found directly and through std's "using ::printf", is one function, not an ambiguity
    printf("unqualified printf after using namespace std: %d %s\n", 5, "ok");
    // library results are typed by the headers: a double from strtod, a size_t from string::size
    std::string s("abc"); double d = std::strtod("2.5", 0); std::size_t n = s.size(); long long q = n * 3;
    printf("library results: %f %d %lld\n", d * 2, (int)n, q);
    return 0;
}
