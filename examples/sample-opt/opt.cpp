// opt.cpp - showcase for the version 3 optimiser.
// Each function exercises one transformation; the test script checks the
// optimisation log and the IR after -O1 / -O2 / -O3.

int square(int x)                          // inlined into usePoint at -O2
{
    return x * x;
}

int constants()                            // folds to "ret #14" at -O1
{
    int a = 3;
    int b = 4;
    return a * b + 2;
}

int deadcode(int x)                        // the unused product and the dead branch disappear at -O1
{
    int unused = x * 2;
    if (0) {
        x = 5;
    }
    return x;
}

int cse(int a, int b)                      // a * b is computed once at -O1
{
    int p = a * b;
    int q = a * b;
    return p + q;
}

int loopInvariant(int n, int k)            // k * 2 is hoisted out of the loop at -O2
{
    int s = 0;
    for (int i = 0; i < n; ++i) {
        int t = k * 2;
        s += t;
    }
    return s;
}

int unroll()                               // fully unrolled and folded to "ret #6" at -O3
{
    int s = 0;
    for (int i = 0; i < 4; ++i) {
        s += i;
    }
    return s;
}

int strength(int x)                        // x * 8 becomes x << 3, x / 1 and - 0 vanish
{
    return x * 8 + x / 1 - 0;
}

double mix(double d, int i)                // integer to floating conversion
{
    return d * i;
}

struct Point {
    int x, y;
    Point(int a, int b) : x(a), y(b) {}
    int sum() const { return x + y; }
};

int usePoint()                             // constructor call on a local, member call, inlined square
{
    Point p(1, 2);
    return p.sum() + square(3);
}

int branches(int v)                        // short-circuit logic and a conditional expression
{
    if (v > 0 && v < 10) return v;
    return v < 0 ? -v : 0;
}

unsigned pick(unsigned flags)              // switch dispatch
{
    switch (flags & 3u) {
    case 0: return 10u;
    case 1: return 20u;
    default: return 30u;
    }
}

// version 4: templates are instantiated and compiled per use
template <typename T>
T maxOf(const T& a, const T& b)
{
    return a < b ? b : a;
}

template <typename T, int N>
struct Buffer {
    T   items[N];
    int count;
    Buffer() : count(0) {}
    void add(const T& v) { items[count] = v; ++count; }
    T total() const
    {
        T s = 0;
        for (int i = 0; i < count; ++i) s += items[i];
        return s;
    }
};

int useTemplates()                         // Buffer<int, 4> and maxOf<int>: 3 + 4 + 7 = 14
{
    Buffer<int, 4> b;
    b.add(3);
    b.add(4);
    return b.total() + maxOf(7, 2);
}

// version 5: exceptions (setjmp/longjmp runtime in runtime/cppparse_rt.cpp)
struct Failure {
    int code;
    Failure(int c) : code(c) {}
};

struct BadIndex : public Failure {
    int index;
    BadIndex(int i) : Failure(100), index(i) {}
};

int throwCatchInt(int v)                   // v < 0 throws v, the handler returns -v
{
    try {
        if (v < 0) throw v;
        return v * 2;
    } catch (int e) {
        return -e;
    }
}

int catchAll(int v)                        // 1 when nothing is thrown, 2 after a double was thrown
{
    try {
        if (v) throw 2.5;
        return 1;
    } catch (...) {
        return 2;
    }
}

void mayFail(int v)                        // throws through a function without a handler
{
    if (v > 10) throw BadIndex(v);
    if (v < 0) throw Failure(v);
}

int throwThrough(int v)                    // class by reference, the base clause comes second
{
    int result = 0;
    try {
        mayFail(v);
        result = 1;
    } catch (const BadIndex& b) {
        result = 1000 + b.index;
    } catch (const Failure& f) {
        result = f.code;
    }
    return result;
}

int rethrown(int v)                        // the inner handler matches through the base and rethrows
{
    try {
        try {
            if (v) throw BadIndex(v);
            return 0;
        } catch (const Failure&) {
            throw;
        }
    } catch (const BadIndex& b) {
        return b.index * 2;
    }
}

int leaveTry(int n)                        // break leaves a try inside a loop: its frame must be popped
{
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        try {
            if (i == 3) break;
            sum += i;
        } catch (...) {
            sum = -1;
        }
    }
    try { throw sum; } catch (int s) { return s + 100; }
}
