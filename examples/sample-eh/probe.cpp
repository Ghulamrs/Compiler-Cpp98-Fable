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

// catching a thrown pointer as a pointer to a public unambiguous base (adjusted unless null, also a base of a base
// and a base that is not first), with more cv-qualifiers on the pointee, as void*, by reference; a private or
// ambiguous base is not caught, as a pointer or as a class; a string literal is caught as char*
struct QA { int a; QA() : a(11) {} };
struct QB { int b; QB() : b(22) {} };
struct QC : QA, QB { int c; QC() : c(33) {} };
struct QD : QC { int d; QD() : d(44) {} };
struct QP : private QA, public QB { int p; };
struct QL : QB { int l; };
struct QM : QL, QB { int m; };   // two QB subobjects: ambiguous

static int pointerCase(int k, QD& d, QC& c)
{
    const QC* cc = &c;
    volatile QC* vc = &c;
    QC* nul = 0;
    QD* arr[1];
    arr[0] = &d;
    QP pv;
    QM am;
    try {
        switch (k) {
        case 0: throw &c;
        case 1: throw &d;
        case 2: throw nul;
        case 3: throw cc;
        case 4: throw vc;
        case 5: throw arr;
        case 6: throw &pv;
        case 7: throw &am;
        case 8: throw pv;
        case 9: throw am;
        case 10: throw "lit";
        case 11: throw &g_probeSink;
        default: throw &pointerCase;
        }
    }
    catch (QB* p) { return p ? 100 + p->b : 1; }
    catch (const QA* p) { return 200 + p->a; }
    catch (const volatile QB* p) { return 300 + static_cast<int>((const volatile char*)p - (const volatile char*)vc); }
    catch (QD** p) { return *p == &d ? 400 : 401; }
    catch (QA& a) { return 500 + a.a; }
    catch (QB& b) { return 600 + b.b; }
    catch (char* s) { return 700 + s[0]; }
    catch (const void* v) { return v == &g_probeSink ? 800 : 801; }
    catch (...) { return 900; }
}

QD g_qd;

int ehCatchPointer(int k)
{
    QD d;
    QC c;
    if (k < 13) return pointerCase(k, d, c);
    // the rethrown pointer is the thrown one, not the handler's. cl 19.52 (preview) crashes on this rethrow when the
    // thrown pointer is the address of a local (at /Od too), and here on `catch (QB*& p)` (cppparse and cl /Od take
    // that handler with the adjusted pointer)
    int r = 0;
    try {
        try { throw &g_qd; } catch (QB* p) { r = p->b; throw; }
    } catch (QA* q) { r = r * 1000 + (q == static_cast<QA*>(&g_qd) ? q->a : 99); }
    return r;
}

// replaced global allocation functions (they replace the library's in the whole program): a new-expression whose
// constructor throws frees through them; the library's sized forms, which cl calls there, call these
long g_gNew = 0;
int g_gDel = 0;
void* operator new(size_t n) { g_gNew += static_cast<long>(n); return malloc(n); }
void operator delete(void* p) { if (p) ++g_gDel; free(p); }
void* operator new[](size_t n) { g_gNew += static_cast<long>(n) * 1000; return malloc(n); }
void operator delete[](void* p) { if (p) g_gDel += 10; free(p); }

struct GT { int v; GT(int x) : v(x) { if (x == 9) throw 9; } };
struct GA { int v; GA(); ~GA() { ++g_destroyed; } };
GA::GA() : v(++g_ids) { if (v == 3) throw 3; }
struct GB { int v; GB() : v(++g_ids) { if (v == 2) throw 2; } };

int ehGlobalFree(int k)
{
    g_gNew = 0; g_gDel = 0; g_ids = 0;
    int r = 0;
    try {
        if (k == 0) new GT(9);
        else if (k == 1) new GA[4];
        else if (k == 2) new GB[5];
        else { int* p = new int(4); delete p; int* q = new int[3]; delete[] q; r = 1; }
    } catch (int e) { r = e; }
    return static_cast<int>(g_gNew) * 1000 + g_gDel * 10 + r;
}

// a thrown class with virtual bases: a handler for a virtual base gets that subobject (by value, by reference), a
// handler for a class with virtual bases copies it from the thrown object (an EE's ED part, with the EE's vbtable); a
// thrown pointer to such a class is caught as a pointer to a virtual base (and to a base of it, EW inside EV), found
// through the pointed-to object's vbtable, as the object may be of a derived class (an EE behind an ED*)
struct EU { int u; EU() : u(1) {} };
struct EW { int w; EW() : w(2) {} };
struct EV : EU, EW { int v; EV() : v(3) { ++g_destroyed; } EV(const EV& o) : v(o.v + 10) {} ~EV() { --g_destroyed; } };
struct EA : virtual EV { int a; EA() : a(5) {} };
struct EB : virtual EV { int b; EB() : b(6) {} };
struct ED : EA, EB { int d; ED() : d(7) {} };
struct EE : ED { int pad[4]; int e; EE() : e(8) { for (int i = 0; i < 4; ++i) pad[i] = 0; } };

static int vbaseByValue(int k, ED& d, EE& e)
{
    try { if (k == 0) throw d; else throw e; }
    catch (EV x) { return 100 + x.v; }
}

static int vbaseByRef(int k, ED& d, EE& e)
{
    try { if (k == 0) throw d; else throw e; }
    catch (EV& x) { return 150 + x.v; }
}

static int vbaseDerived(EE& e)
{
    try { throw e; }
    catch (ED x) { return 200 + x.v + x.d; }
}

static int vbasePointer(int k, ED* p)
{
    try {
        switch (k) {
        case 0: throw p;
        case 1: throw static_cast<EA*>(p);
        case 2: throw static_cast<ED*>(0);
        default: throw static_cast<const ED*>(p);   // not caught as EV*: as const EW*, EW inside the virtual base
        }
    }
    catch (EV* v) { return v ? 300 + v->v : 301; }
    catch (const EW* w) { return 600 + w->w; }
    catch (...) { return 900; }
}

static int vbasePointer2(int k, ED* p)
{
    try {
        switch (k) {
        case 0: throw p;                        // EB at 16 in ED, a non-virtual base
        case 1: throw static_cast<EA*>(p);      // no EB: EW through EA's vbtable
        default: throw static_cast<EE*>(p);
        }
    }
    catch (EB* b) { return 400 + b->b; }
    catch (EW* w) { return 500 + w->w; }
    catch (...) { return 900; }
}

int ehCatchVbase(int k)
{
    ED d;
    EE e;
    g_destroyed = 2;
    int r;
    if (k < 2) r = vbaseByValue(k, d, e);
    else if (k < 4) r = vbaseByRef(k - 2, d, e);
    else if (k == 4) r = vbaseDerived(e);
    else if (k < 9) r = vbasePointer(k - 5, &d);
    else if (k < 13) r = vbasePointer(k - 9, &e);
    else if (k < 16) r = vbasePointer2(k - 13, &e);
    else r = vbasePointer2(0, &d);
    return r * 10 + g_destroyed;
}

// ---- function-try-blocks (C++98 15.3/10-16) ----
// a constructor's handler runs after the bases and members built so far are destroyed and rethrows when it
// reaches its end (the object is never constructed); a destructor's likewise (its handler may return); an ordinary
// function's handler returns normally. The handlers take references: cl copies a by-value handler parameter
// before it unwinds (MSVC's two-phase handling), cppparse after, a documented difference.
static char g_ftLog[128];
static int g_ftLen = 0;
static void ftNote(char c) { if (g_ftLen < 127) { g_ftLog[g_ftLen++] = c; g_ftLog[g_ftLen] = 0; } }
struct FtM { int id; explicit FtM(int i) : id(i) { ftNote(static_cast<char>('a' + i)); } ~FtM() { ftNote(static_cast<char>('A' + id)); } };
struct FtB { FtB() { ftNote('b'); } ~FtB() { ftNote('B'); } };
struct FtE { int c; explicit FtE(int x) : c(x) {} };
static int ftThrowing(int a) { if (a == 1) throw FtE(11); return 2; }
struct FtC : FtB {
    FtM m1, m2; int v;
    FtC(int a) try : FtB(), m1(1), m2(ftThrowing(a)), v(a) { ftNote('y'); if (a == 2) throw FtE(22); }
    catch (const FtE& e) { ftNote('h'); ftNote(static_cast<char>('0' + e.c / 11)); }
    ~FtC() try { ftNote('d'); if (v == 3 || v == 5) throw 9; } catch (int e) { ftNote('H'); ftNote(static_cast<char>('0' + e)); if (v == 5) return; }
};
static int ftPlain(int a) try { if (a) throw a * 2; return 1; } catch (int e) { ftNote('p'); return e; }
const char* ehFunctionTry(int k)
{
    g_ftLen = 0; g_ftLog[0] = 0;
    try {
        if (k == 4) { ftNote(static_cast<char>('0' + ftPlain(0))); ftNote(static_cast<char>('0' + ftPlain(3))); return g_ftLog; }
        FtC f(k);
        ftNote('c');
    }
    catch (const FtE& e) { ftNote('m'); ftNote(static_cast<char>('0' + e.c / 11)); }
    catch (int e) { ftNote('n'); ftNote(static_cast<char>('0' + e)); }
    ftNote('.');
    return g_ftLog;
}

// ---- RTTI (C++98 5.2.7, 5.2.8): a failed dynamic_cast to a reference throws std::bad_cast, typeid of a null
// dereference std::bad_typeid, both thrown by <typeinfo>'s helpers through cppparse's runtime ----
#include <typeinfo>
struct RtB { virtual ~RtB() {} int b; };
struct RtC { virtual ~RtC() {} int c; };
struct RtD : RtB, RtC { int d; };
int ehRtti(int k)
{
    RtD d; RtB lone; RtB* pb = k == 0 ? static_cast<RtB*>(&d) : &lone;
    try {
        if (k == 2) { RtB* n = 0; return typeid(*n) == typeid(RtD) ? 1 : 2; }
        RtD& r = dynamic_cast<RtD&>(*pb);
        return (&r == &d ? 10 : 11) + (dynamic_cast<RtC*>(pb) == static_cast<RtC*>(&d) ? 100 : 0);
    }
    catch (std::bad_cast& e) { return 20 + (e.what()[0] == 'B' ? 1 : 0) + (dynamic_cast<RtD*>(pb) == 0 ? 100 : 0); }
    catch (std::bad_typeid& e) { return 30 + (e.what()[0] == 'A' ? 1 : 0); }
}

// ---- nested live handlers: a handler that calls deeper while its own exception is still caught; the runtime's
// stack of caught exceptions grows as needed (it held 32 and dropped the 33rd) ----
int ehDeepHandlers(int n)
{
    int r = 0;
    try { throw n; }
    catch (int e) { r = e; if (n < 40) r += ehDeepHandlers(n + 1); }
    return r;
}
