// valarray.cpp - <valarray> (C3); built by cl with its own headers (the reference) and by cppparse --compile6 with
// include/, test 15 compares what the programs print at -O0 and -O2.
#include <cstdio>
#include <valarray>

static void show(const char* what, const std::valarray<double>& v)
{
    std::printf("%-12s", what);
    for (size_t i = 0; i < v.size(); ++i) std::printf(" %.4g", v[i]);
    std::printf(" (%d)\n", static_cast<int>(v.size()));
}
static void showInt(const char* what, const std::valarray<int>& v)
{
    std::printf("%-12s", what);
    for (size_t i = 0; i < v.size(); ++i) std::printf(" %d", v[i]);
    std::printf(" (%d)\n", static_cast<int>(v.size()));
}
static void showBool(const char* what, const std::valarray<bool>& v)
{
    std::printf("%-12s ", what);
    for (size_t i = 0; i < v.size(); ++i) std::printf("%d", v[i] ? 1 : 0);
    std::printf("\n");
}
static double twice(double x) { return 2 * x; }
static double square(const double& x) { return x * x; }

int main()
{
    const double init[] = { 1, 2, 3, 4, 5, 6 };
    std::valarray<double> a(init, 6), b(2.0, 6), e, z(3);
    show("a", a); show("b", b); show("empty", e); show("zeros", z);
    show("a+b", a + b); show("a-b", a - b); show("a*b", a * b); show("a/b", a / b);
    show("a+1", a + 1.0); show("10-a", 10.0 - a); show("a*a", a * a); show("-a", -a); show("+a", +a);
    std::valarray<double> c(a);
    c += b; c *= 3.0; c -= a; c /= 2.0;
    show("compound", c);
    std::printf("sum %.4g min %.4g max %.4g size %d\n", a.sum(), a.min(), a.max(), static_cast<int>(a.size()));
    show("shift 2", a.shift(2)); show("shift -1", a.shift(-1)); show("cshift 2", a.cshift(2)); show("cshift -1", a.cshift(-1));
    show("apply", a.apply(twice)); show("apply ref", a.apply(square));
    showBool("a>3", a > 3.0); showBool("a==b", a == b); showBool("2<=a", 2.0 <= a); showBool("!a", !(a - 1.0));
    showBool("a&&b", a && b); showBool("a||b-2", a || (b - 2.0)); showBool("a&&0", a && 0.0); showBool("0||a-1", 0.0 || (a - 1.0));
    show("abs", std::abs(10.0 - a * 3.0));
    show("sqrt", std::sqrt(a)); show("exp", std::exp(a / 6.0)); show("log", std::log(a)); show("log10", std::log10(a * 10.0));
    show("sin", std::sin(a)); show("cos", std::cos(a)); show("tan", std::tan(a / 10.0));
    show("asin", std::asin(a / 10.0)); show("acos", std::acos(a / 10.0)); show("atan", std::atan(a));
    show("sinh", std::sinh(a / 4.0)); show("cosh", std::cosh(a / 4.0)); show("tanh", std::tanh(a / 4.0));
    show("pow", std::pow(a, b)); show("pow v,d", std::pow(a, 0.5)); show("pow d,v", std::pow(2.0, a)); show("atan2", std::atan2(a, b));
    // slices: every second element; a 2x3 block by gslice; masks and index arrays
    std::valarray<double> s = a[std::slice(0, 3, 2)];
    show("slice", s);
    a[std::slice(1, 3, 2)] = 0.5;
    show("slice =", a);
    a[std::slice(0, 2, 3)] += std::valarray<double>(10.0, 2);
    show("slice +=", a);
    const int iv[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    std::valarray<int> m(iv, 12);
    const size_t sz[] = { 2, 3 }, st[] = { 6, 2 };
    std::gslice g(1, std::valarray<size_t>(sz, 2), std::valarray<size_t>(st, 2));
    std::valarray<int> blk = m[g];
    showInt("gslice", blk);
    m[g] = -1;
    showInt("gslice =", m);
    std::valarray<bool> mask = m > 5;
    std::valarray<int> big = m[mask];
    showInt("mask", big);
    m[mask] *= std::valarray<int>(2, big.size());
    showInt("mask *=", m);
    const size_t ix[] = { 11, 0, 5 };
    std::valarray<size_t> idx(ix, 3);
    std::valarray<int> picked = m[idx];
    showInt("indirect", picked);
    // the views passed straight to a const valarray&: each converted by valarray's constructor into a temporary (A6)
    showInt("indirect arg", m[idx]); showInt("gslice arg", m[g]); showInt("mask arg", m[mask]);
    show("slice arg", a[std::slice(0, 3, 2)]);
    m[idx] = std::valarray<int>(7, 3);
    showInt("indirect =", m);
    showInt("m % 4", m % 4); showInt("m & 6", m & 6); showInt("m | 1", m | 1); showInt("m ^ 3", m ^ 3); showInt("m << 1", m << 1); showInt("m >> 1", m >> 1); showInt("~m", ~m);
    std::valarray<int> r(m);
    r %= 5; r <<= 2; r |= 1; r &= 13; r ^= 4; r >>= 1;
    showInt("int compound", r);
    std::valarray<int> sized;
    sized.resize(4, 9);
    showInt("resize", sized);
    sized.resize(2);
    showInt("resize 0", sized);
    sized = m;
    showInt("assign", sized);
    sized = 3;
    showInt("assign v", sized);
    return 0;
}
