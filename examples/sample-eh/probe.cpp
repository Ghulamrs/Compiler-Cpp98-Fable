// probe.cpp - compiled by cppparse (sandbox\scripts\eh.ps1), and by cl for the reference output.
//
// Unwind tables: functions with every frame shape the code generator writes unwind info for, called from an ml64
// function that fills the callee-saved registers with patterns. At the bottom, the driver walks the stack back
// with RtlVirtualUnwind; the registers it restores in the ml64 frame must be the patterns again.
//   probeDeep: recursion with integers and doubles live across the call (pushed registers, xmm6.. saves)
//   probeBig:  a frame of more than a page (allocated after __chkstk)
//
// Exceptions (eh*): throw and catch by class, base class, int and double; destructors of the frames in between;
// nested regions, rethrow, a throw from a handler, a frame of more than a page, locals kept across a throw, a loop
// that throws on every other iteration. mixMiddle (probe-mix.cpp) may be compiled with the other model.
// Strict C++98.

extern "C" int probe_report(int depth);   // in the driver: walks the stack, returns 1000

double g_probeSink = 0.0;

int probeDeep(int n, double x)
{
    int a = n * 3, b = n + 7, c = n ^ 5, d = n * n, e = n - 2;
    double y = x * 1.5, z = x + 2.25, w = x * x;
    int r;
    if (n == 0) r = probe_report(0);
    else r = probeDeep(n - 1, x + 1.0);
    g_probeSink = g_probeSink + y + z + w;
    return r + a + b + c + d + e + static_cast<int>(y + z + w);
}

int probeBig(int n)
{
    char buf[6000];
    for (int i = 0; i < 6000; ++i) buf[i] = static_cast<char>(i & 63);
    int r = probeDeep(n, 0.5);
    return r + buf[n] + buf[5999];
}

// ---- exceptions ----

int g_destroyed = 0;   // sum of the ids of the Guards destroyed so far

struct Guard {
    int id;
    explicit Guard(int i) : id(i) {}
    ~Guard() { g_destroyed += id; }
};

struct Error {
    int code;
    explicit Error(int c) : code(c) {}
    virtual ~Error() {}
    virtual int kind() const { return 1; }
};

struct RangeError : public Error {
    explicit RangeError(int c) : Error(c) {}
    int kind() const { return 2; }
};

int thrower(int n)
{
    Guard g(n);
    if (n > 3) throw RangeError(n);
    if (n > 2) throw Error(n);
    if (n > 1) throw n;
    if (n < 0) throw 2.5;
    return n;
}

int ehBasic(int n)
{
    try {
        return thrower(n) * 10;
    } catch (const RangeError& e) {
        return 1000 + e.code * 10 + e.kind();
    } catch (const Error& e) {
        return 2000 + e.code * 10 + e.kind();
    } catch (int v) {
        return 3000 + v;
    } catch (double d) {
        return 4000 + static_cast<int>(d * 10);
    }
}

int deep(int n, int acc)
{
    Guard g(1);
    if (n == 0) throw Error(acc);
    return deep(n - 1, acc + n) + 1;
}

int ehDeep(int n)
{
    int before = g_destroyed;
    int r;
    try {
        r = deep(n, 0);
    } catch (const Error& e) {
        r = e.code;
    }
    return r * 1000 + (g_destroyed - before);
}

int ehLocals(int n)
{
    int a = n * 7, b = n + 11, c = n * n;
    double x = n * 0.5;
    int caught = 0;
    for (int i = 0; i < n; ++i) {
        try {
            if (i % 3 == 1) throw i;
            a += i;
        } catch (int v) {
            caught += v;
            b += 2;
        }
    }
    return a + b + c + caught + static_cast<int>(x * 10);
}

int ehNested(int n)
{
    int r = 0;
    try {
        try {
            Guard g(100);
            thrower(n);
            r = 1;
        } catch (const Error& e) {
            r = 10 + e.code;
            if (n > 3) throw;
        }
        r += 5;
    } catch (const RangeError& e) {
        r += 200 + e.code;
    } catch (...) {
        r += 900;
    }
    return r;
}

int ehFromHandler(int n)
{
    try {
        try {
            Guard g(20);
            throw n;
        } catch (int v) {
            throw Error(v + 1);
        }
    } catch (const Error& e) {
        return e.code;
    }
    return -1;
}

int bigCleanup(int n)
{
    char buf[5000];
    Guard g(7);
    for (int i = 0; i < 5000; ++i) buf[i] = static_cast<char>(i & 127);
    if (n) throw Error(buf[n]);
    return buf[10];
}

int ehBig(int n)
{
    int before = g_destroyed;
    try {
        return bigCleanup(n);
    } catch (const Error& e) {
        return e.code * 100 + (g_destroyed - before);
    }
}

int ehLoop(int n)
{
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        try {
            Guard g(1);
            if (i % 2) throw Error(i);
            sum += 1;
        } catch (const Error& e) {
            sum += e.code;
        }
    }
    return sum;
}

// across files: mixInner (here) throws through mixMiddle (probe-mix.cpp), whose handler does not match
int mixMiddle(int n);

int mixInner(int n)
{
    Guard g(3);
    if (n) throw Error(n * 2);
    return 0;
}

int ehMixed(int n)
{
    int before = g_destroyed;
    try {
        return mixMiddle(n);
    } catch (const Error& e) {
        return e.code * 100 + (g_destroyed - before);
    }
}
