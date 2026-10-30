// complex.cpp - <complex> (C3); built by cl with its own headers (the reference) and by cppparse --compile6 with
// include/, test 15 compares what the programs print at -O0 and -O2.
#include <complex>
#include <cstdio>
#include <iostream>
#include <sstream>

typedef std::complex<double> C;

static void show(const char* what, const C& c) { std::printf("%-10s (%.6f, %.6f)\n", what, c.real(), c.imag()); }

int main()
{
    C a(3, 4), b(1.5, -2), z;
    show("a", a); show("b", b); show("default", z);
    show("a+b", a + b); show("a-b", a - b); show("a*b", a * b); show("a/b", a / b);
    show("a+2", a + 2.0); show("2-a", 2.0 - a); show("a*2", a * 2.0); show("1/a", 1.0 / a);
    show("-a", -a); show("+b", +b);
    C c(a);
    c += b; c -= 0.5; c *= C(0, 1); c /= 2.0;
    show("compound", c);
    std::printf("== %d %d %d != %d\n", a == C(3, 4) ? 1 : 0, C(5, 0) == 5.0 ? 1 : 0, 3.0 == a ? 1 : 0, a != b ? 1 : 0);
    std::printf("abs %.6f arg %.6f norm %.6f real %.1f imag %.1f\n", std::abs(a), std::arg(a), std::norm(a), std::real(a), std::imag(a));
    show("conj", std::conj(a));
    show("polar", std::polar(2.0, 0.5));
    show("exp", std::exp(C(0.5, 1)));
    show("log", std::log(a));
    show("log10", std::log10(C(100, 0)));
    show("sqrt", std::sqrt(C(-4, 0)));
    show("sqrt2", std::sqrt(C(3, -4)));
    show("sin", std::sin(C(1, 1)));
    show("cos", std::cos(C(1, 1)));
    show("tan", std::tan(C(0.5, 0.25)));
    show("sinh", std::sinh(C(1, 0.5)));
    show("cosh", std::cosh(C(1, 0.5)));
    show("tanh", std::tanh(C(0.5, 0.5)));
    show("pow int", std::pow(a, 3));
    show("pow c", std::pow(a, C(0.5, 0.25)));
    show("pow d", std::pow(a, 2.5));
    show("pow d c", std::pow(2.0, C(1, 1)));
    std::complex<float> f(1.5f, 2.5f);
    C fromF(f);
    std::complex<float> back(a);
    show("from float", fromF);
    std::printf("to float (%.2f, %.2f)\n", back.real(), back.imag());
    std::ostringstream o;
    o << a << ' ' << b;
    o.precision(3);
    o << ' ' << C(1.0 / 3, 2.0 / 3);
    std::printf("<< [%s]\n", o.str().c_str());
    std::istringstream in("(1.5,-2.5) (7) 9 (bad");
    C r1, r2, r3, r4;
    in >> r1 >> r2 >> r3;
    show(">> pair", r1); show(">> (re)", r2); show(">> re", r3);
    in >> r4;
    std::printf(">> bad fails %d\n", in.fail() ? 1 : 0);
    std::cout << "cout " << C(0.25, -8) << std::endl;
    std::wostringstream w;
    w << std::complex<double>(1, 2);
    std::printf("wide %d chars\n", static_cast<int>(w.str().size()));
    return 0;
}
