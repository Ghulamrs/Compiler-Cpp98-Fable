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

#include <stdlib.h>

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

// a catch inside a loop (table model): the loop keeps loop-invariant values in callee-saved registers (the
// hoisted addresses of the string literals, as printf's formats would be) while calls in the try throw. A landing
// pad resumes the frame with its registers as they were at the call that threw, so no code that may throw to it
// may reuse those registers, the inlined throws laid out after the loop included
int g_ehBad = -1;
extern "C" void eh_note(const char* s, int v);   // in the driver (cl): mixes s and v into a checksum
void throwIf(int v) { if (v == g_ehBad) throw v; }
void ehCatchLoop(int n)
{
    for (int k = 1; k < n; ++k) {
        g_ehBad = k % 4;
        try { eh_note("a%d ", 1); throwIf(1); eh_note("b%d ", 2); throwIf(2); eh_note("c%d ", 3); throwIf(3); }
        catch (int e) { eh_note("caught %d\n", e); }
    }
    eh_note("end\n", n);
}

// the same in nested loops, with objects to destroy, and throws from three calls down; each level keeps its own
// values in callee-saved registers across its calls (the unwinder gives every frame's registers back)
int ehLevel3(int a, int b)
{
    int s = a, t = b, u = a ^ b;
    for (int i = 0; i < 3; ++i) { s += t * i; t = t * 3 + u; u += s; throwIf(a + i); eh_note("l3", u); }
    return (s + t + u) & 0xffff;
}
int ehLevel2(int a, int b)
{
    Guard g(1);
    int x = a * 7, y = b * 5, z = a + b;
    for (int i = 0; i < 2; ++i) { x += ehLevel3(a + i, b); y ^= x; z += y; }
    return (x + y + z) & 0xffff;
}
int ehLevel1(int n, int b)
{
    int acc = 0;
    for (int i = 0; i < n; ++i) { Guard g(1); acc += ehLevel2(i, b); }
    return acc;
}
void ehNestedLoops(int n)
{
    for (int k = 0; k < n; ++k) {
        for (int j = 0; j < 3; ++j) {
            g_ehBad = (k * 3 + j) % 5;
            try { Guard g(1); eh_note("t%d ", k * 10 + j); eh_note("r%d ", ehLevel1(k, j)); }
            catch (int e) { eh_note("c%d ", e); }
        }
        try { eh_note("a%d ", k); throwIf(k); eh_note("b%d ", k); throwIf(k + 1); } catch (int e) { eh_note("caught %d\n", e); }
    }
    eh_note("end\n", n);
}

// ---- destructors that throw (C++98 allows it; the cl reference is built with /Zc:implicitNoexcept-) ----
// The members and bases of a destructor that throws are still destroyed, the exception reaches the handler; a delete
// whose destructor throws does not free (as cl); delete[] and arrays at scope exit destroy the remaining elements;
// a new-expression whose constructor throws frees through the matching operator delete

long g_dlog = 0;   // the destructors run, one digit each, newest last
int g_frees = 0, g_arrFrees = 0, g_ids = 0;

struct DA { virtual ~DA(); static void* operator new(size_t n) { return malloc(n); } static void operator delete(void* p) { ++g_frees; free(p); } };
DA::~DA() { g_dlog = g_dlog * 10 + 1; }
struct DM { ~DM() { g_dlog = g_dlog * 10 + 2; } };
struct DB : DA { DM m; };
struct DD : DB { ~DD() { g_dlog = g_dlog * 10 + 3; throw 7; } };
struct DP { DM m; ~DP() { g_dlog = g_dlog * 10 + 4; throw 8; } };
struct DT { DM m; DP p; };   // implicit destructor: p's throws, m is still destroyed

int ehDtorThrow(int k)
{
    g_dlog = 0; g_frees = 0;
    int r = 0;
    try {
        if (k == 0) delete (DB*)new DD;
        else if (k == 1) { DD d; }
        else if (k == 2) { DP p; }
        else if (k == 3) delete new DP;
        else { DT t; }
    } catch (int x) { r = x; }
    return static_cast<int>(g_dlog) * 1000 + r * 10 + g_frees;
}

struct AV { int i; AV(); virtual ~AV() { g_dlog = g_dlog * 10 + i; if (i == 3) throw 1; }
    static void* operator new[](size_t s) { return malloc(s); } static void operator delete[](void* p) { ++g_arrFrees; free(p); } };
AV::AV() : i(++g_ids) {}
struct AN { int i; AN(); ~AN() { g_dlog = g_dlog * 10 + i; if (i == 3) throw 2; }
    static void* operator new[](size_t s) { return malloc(s); } static void operator delete[](void* p) { ++g_arrFrees; free(p); } };
AN::AN() : i(++g_ids) {}
struct AC { int i; AC() : i(++g_ids) { if (i == 4) throw 3; } ~AC() { g_dlog = g_dlog * 10 + i; }
    static void* operator new[](size_t s) { return malloc(s); } static void operator delete[](void* p) { ++g_arrFrees; free(p); } };
struct NT { NT() { throw 4; } static void* operator new(size_t s) { return malloc(s); } static void operator delete(void* p) { ++g_frees; free(p); } };
struct NM { AC a[3]; NM() { throw 6; } };   // a member array: built, then destroyed when the body throws

int ehArrays(int k)
{
    g_dlog = 0; g_ids = 0; g_arrFrees = 0; g_frees = 0;
    int r = 0;
    try {
        if (k == 0) { AV* p = new AV[5]; delete[] p; }
        else if (k == 1) { AN* q = new AN[5]; delete[] q; }
        else if (k == 2) { AN a[5]; }
        else if (k == 3) new AC[6];
        else if (k == 4) new NT;
        else { NM m; }
    } catch (int x) { r = x; }
    return static_cast<int>(g_dlog) * 1000 + r * 100 + g_arrFrees * 10 + g_frees;
}

// catch by value: the handler's object is copy-constructed from the thrown one and destroyed when the handler ends
int g_copies = 0, g_live = 0;
struct CV { int v; CV(int x) : v(x) { ++g_live; } CV(const CV& o) : v(o.v + 100) { ++g_copies; ++g_live; } ~CV() { --g_live; } };

int catchValue(int k)
{
    try {
        throw CV(k);
    } catch (CV x) {
        x.v += 1;
        if (k == 2) return x.v * 100 + g_copies * 10 + g_live;
        if (k == 3) throw 5;
        return x.v;
    }
}

int ehCatchValue(int k)
{
    g_copies = 0;
    int r;
    try { r = catchValue(k); } catch (int x) { r = x; }
    return r * 1000 + g_copies * 10 + g_live;
}

// a class without destructor (no cookie) and a sized operator delete[]: new P[3] whose second constructor throws
// frees through it with count * size
int g_pn = 0, g_psize = 0;
struct PS { int id; PS() : id(g_pn++) { if (id == 1) throw 9; }
    static void* operator new[](size_t s) { return malloc(s); } static void operator delete[](void* p, size_t s) { g_psize = static_cast<int>(s); free(p); } };

int ehNoCookie()
{
    g_pn = 0; g_psize = 0;
    int r = 0;
    try { PS* p = new PS[3]; delete[] p; } catch (int x) { r = x; }
    return r * 100 + g_psize;
}

// leaving a by-value handler with an exception: its parameter is destroyed before the exception it handles
struct HB { int v; HB(int x) : v(x) {} HB(const HB& o) : v(o.v + 10) {} ~HB() { g_dlog = g_dlog * 100 + v; } };

int ehHandlerOrder()
{
    g_dlog = 0;
    int r = 0;
    try {
        try { throw HB(9); } catch (HB b) {
            try { throw HB(4); } catch (HB c) { r = c.v; }
            throw 5;
        }
    } catch (int e) { r = r * 10 + e; }
    return static_cast<int>(g_dlog % 1000000) * 1000 + r;
}

// catching a base that is not the first: the handler gets that base subobject, by reference and by value
struct XP { int pad; XP() : pad(5) {} };
struct MB : XP, HB { MB() : HB(7) {} };

int ehCatchBase(int byValue)
{
    g_dlog = 0;
    int r = 0;
    if (byValue) { try { throw MB(); } catch (HB b) { r = b.v; } }
    else { try { throw MB(); } catch (HB& b) { r = b.v; } }
    return r * 1000 + static_cast<int>(g_dlog % 1000);
}

// temporaries of a full-expression whose destructor throws: the others are still destroyed, the throwing one is
// not destroyed again by the unwind handler (the flag is cleared before the destructor runs)
int g_tbad = -1;
struct TT { int v; TT(int x) : v(x) {} ~TT() { g_dlog = g_dlog * 10 + v; if (v == g_tbad) throw v; } int get() const { return v; } };

int ehTempDtor(int k)
{
    g_dlog = 0; g_tbad = k;
    int r = 0;
    try { r = TT(1).get() + TT(2).get(); } catch (int e) { r = 100 + e; }
    return static_cast<int>(g_dlog) * 1000 + r;
}
