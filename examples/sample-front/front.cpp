// front.cpp - version 6 front-end regressions that compile (test 17): built by cppparse --compile6 and by cl,
// the two programs must print the same lines
#include <algorithm>
#include <cstdio>

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

// with a using-directive at file scope: std::count, std::min and std::max are visible unqualified
using namespace std;
int localHidesFileUsing(int n) { int count = 0, max = 2; while (count < n + max) ++count; return count; }

int main()
{
    sizes();
    hiding();
    injected();
    redeclared();
    std::printf("hiding after using namespace std: %d\n", localHidesFileUsing(5));
    // ::printf, found directly and through std's "using ::printf", is one function, not an ambiguity
    printf("unqualified printf after using namespace std: %d %s\n", 5, "ok");
    return 0;
}
