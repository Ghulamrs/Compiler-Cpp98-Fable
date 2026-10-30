// complexf.cpp - <complex>'s specialisations (26.2.3): complex<float>, complex<double> and complex<long double> are
// classes of their own, whose conversions widen implicitly and narrow only explicitly, and whose values and
// transcendentals are computed in their own precision. Built by cl with its own headers (the reference) and by
// cppparse --compile6 with include/, test 15 compares what the programs print at -O0 and -O2.
#include <complex>
#include <cstdio>
#include <sstream>

typedef std::complex<float>       F;
typedef std::complex<double>      D;
typedef std::complex<long double> L;

static void showF(const char* what, const F& c) { std::printf("%-10s (%.6g, %.6g)\n", what, static_cast<double>(c.real()), static_cast<double>(c.imag())); }
static void showD(const char* what, const D& c) { std::printf("%-10s (%.15g, %.15g)\n", what, c.real(), c.imag()); }
static void showL(const char* what, const L& c) { std::printf("%-10s (%.15g, %.15g)\n", what, static_cast<double>(c.real()), static_cast<double>(c.imag())); }

// a complex<double> argument: complex<long double> takes it implicitly, complex<float> only explicitly, so this
// overload set is not ambiguous (it would be if every conversion were implicit)
static int widenOnly(const F&) { return 1; }
static int widenOnly(const L&) { return 2; }
// a complex<float> argument: both wider types take it implicitly, the exact match wins
static int exactFirst(const F&) { return 1; }
static int exactFirst(const D&) { return 2; }

int main()
{
    std::printf("sizes %d %d %d\n", static_cast<int>(sizeof(F)), static_cast<int>(sizeof(D)), static_cast<int>(sizeof(L)));
    std::printf("value_type sizes %d %d %d\n", static_cast<int>(sizeof(F::value_type)), static_cast<int>(sizeof(D::value_type)), static_cast<int>(sizeof(L::value_type)));
    F f(1.5f, -2.25f), g(0.1f, 0.2f);
    D d = f;            // float to double: implicit
    L l = d;            // double to long double: implicit
    L l2 = f;           // float to long double: implicit
    F back(d);          // double to float: explicit
    F back2 = F(l);     // long double to float: explicit
    D fromL(l2);        // long double to double: explicit
    showD("d = f", d); showL("l = d", l); showL("l2 = f", l2);
    showF("F(d)", back); showF("F(l)", back2); showD("D(l)", fromL);
    std::printf("overloads %d %d\n", widenOnly(D(1, 2)), exactFirst(f));

    // arithmetic in float: 0.1f + 0.2f is not 0.3f in float, nor what double gives
    F s = g + F(0.2f, 0.1f);
    showF("g + h", s);
    showF("f * g", f * g);
    showF("f / g", f / g);
    F c(f);
    c += 1.0f; c -= F(0.5f, 0.5f); c *= 2.0f; c /= F(1.0f, 1.0f);
    c += d;             // a complex of another type, converted to float
    showF("compound", c);
    std::printf("== %d %d %d\n", f == F(1.5f, -2.25f) ? 1 : 0, F(3.0f) == 3.0f ? 1 : 0, f != g ? 1 : 0);

    // the values and transcendentals of each type, each in its own precision
    std::printf("abs %.6g arg %.6g norm %.6g\n", static_cast<double>(std::abs(f)), static_cast<double>(std::arg(f)), static_cast<double>(std::norm(f)));
    showF("conj", std::conj(f));
    showF("polar", std::polar(2.0f, 0.5f));
    showF("exp", std::exp(F(0.5f, 1.0f)));
    showF("log", std::log(F(3.0f, 4.0f)));
    showF("log10", std::log10(F(100.0f, 0.0f)));
    showF("sqrt", std::sqrt(F(-4.0f, 0.0f)));
    showF("sqrt2", std::sqrt(F(3.0f, -4.0f)));
    showF("sin", std::sin(F(1.0f, 1.0f)));
    showF("cos", std::cos(F(1.0f, 1.0f)));
    showF("tan", std::tan(F(0.5f, 0.25f)));
    showF("sinh", std::sinh(F(1.0f, 0.5f)));
    showF("cosh", std::cosh(F(1.0f, 0.5f)));
    showF("tanh", std::tanh(F(0.5f, 0.5f)));
    showF("pow c", std::pow(F(3.0f, 4.0f), F(0.5f, 0.25f)));
    showF("pow f", std::pow(F(3.0f, 4.0f), 2.5f));
    showF("pow f c", std::pow(2.0f, F(1.0f, 1.0f)));
    showL("L exp", std::exp(L(0.5L, 1.0L)));
    showL("L sqrt", std::sqrt(L(3.0L, -4.0L)));
    showL("L pow", std::pow(L(3.0L, 4.0L), L(0.5L, 0.25L)));
    std::printf("L abs %.15g\n", static_cast<double>(std::abs(L(3.0L, 4.0L))));

    // the stream operators of each type
    std::ostringstream o;
    o << f << ' ' << l << ' ' << D(0.25, -8);
    std::printf("<< [%s]\n", o.str().c_str());
    std::istringstream in("(1.25,-0.5) (2) 7");
    F r1, r2;
    L r3;
    in >> r1 >> r2 >> r3;
    showF(">> pair", r1); showF(">> (re)", r2); showL(">> re", r3);
    return 0;
}
