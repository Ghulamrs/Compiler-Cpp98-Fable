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

// with a using-directive at file scope: std::count, std::min and std::max are visible unqualified
using namespace std;
int localHidesFileUsing(int n) { int count = 0, max = 2; while (count < n + max) ++count; return count; }

int main()
{
    sizes();
    hiding();
    injected();
    redeclared();
    memberTemplates();
    std::printf("hiding after using namespace std: %d\n", localHidesFileUsing(5));
    // ::printf, found directly and through std's "using ::printf", is one function, not an ambiguity
    printf("unqualified printf after using namespace std: %d %s\n", 5, "ok");
    // library results are typed by the headers: a double from strtod, a size_t from string::size
    std::string s("abc"); double d = std::strtod("2.5", 0); std::size_t n = s.size(); long long q = n * 3;
    printf("library results: %f %d %lld\n", d * 2, (int)n, q);
    return 0;
}
