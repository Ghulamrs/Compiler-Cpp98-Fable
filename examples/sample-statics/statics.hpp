// statics.hpp - inline functions with dynamically initialised static locals, shared by statics.cpp (compiled
// by cppparse) and driver-statics.cpp (compiled by cl /O2, which inlines them). The object and its guard are
// COMDATs both compilers emit under cl's names ("?$TSS0@..." with the CRT's _Init_thread_header/footer), so
// each object is constructed once whichever side reaches it first.
#ifndef SAMPLE_STATICS_HPP
#define SAMPLE_STATICS_HPP

extern int g_ctorCalls;
struct Counted { int id; Counted(); };

inline Counted& sharedA() { static Counted a; return a; }
inline Counted& sharedB() { static Counted b; return b; }
inline int& sharedC() { static int c = sharedA().id * 10 + 1; static Counted d; return c; }

// constructors that throw on their first attempt (per which) and succeed on the second: the guard goes back
// to "not built" (_Init_thread_abort), so the next call constructs the object; later calls do not. An
// exception does not cross between the two compilers' runtimes, so each first attempt throws and is caught
// on one side: FlakyP's constructor is cppparse's and first runs there, FlakyQ's is cl's
extern int g_flakyAttempts[2];
extern int g_flakyBuilt[2];
struct FlakyP { int which; FlakyP(int w); };
struct FlakyQ { int which; FlakyQ(int w); };
inline FlakyP& flakyX() { static FlakyP x(0); return x; }   // throws first from cppparse
inline FlakyQ& flakyY() { static FlakyQ y(1); return y; }   // throws first from cl

// statics cl initialises as constants: static data (the folded value, in the function's COMDATs) and no guard,
// so the counted object after them gets $TSS0 and the second one $TSS1 on both sides. cl /O2 inlines konstK
// into the driver (STATICS_INLINE is __forceinline there) and reads u, d, f, g, z, c, b as data from whichever
// unit's COMDAT the linker keeps; each call returns the values and then moves them on, from either side
#ifndef STATICS_INLINE
#define STATICS_INLINE inline
#endif
struct KonstSeen { int u; double d; float f; float g; double z; char c; bool b; int t; int t2; };
STATICS_INLINE void konstK(KonstSeen& s)
{
    static int u = int();
    static double d = 1.0/4;
    static float f = 1.5f*2;
    static Counted t;
    static float g = (float)(1.0/3) + 1;
    static double z = double();
    static char c = 'a' + 1;
    static bool b = 0.5;
    static Counted t2;
    s.u = u; s.d = d; s.f = f; s.g = g; s.z = z; s.c = c; s.b = b; s.t = t.id; s.t2 = t2.id;
    ++u; d += 1; f += 1; g += 1; z -= 1; ++c; b = !b;
}

// statics in nested blocks, named after cl's block numbers (the body 2; each compound statement, if, branch,
// loop and switch takes the next one in source order), so cl's inlined copy and cppparse's reach the same
// objects and guards
struct NestedSeen { const Counted* p; const Counted* s; const Counted* x; const Counted* y; int w; };
STATICS_INLINE void nestedN(int k, NestedSeen& n)
{
    n.p = 0; n.s = 0; n.x = 0; n.y = 0; n.w = 0;
    if (k > 0) { static Counted p; n.p = &p; } else { static Counted q; n.p = &q; }
    for (int i = 0; i < 1; ++i) { static Counted s; n.s = &s; { static int w = 7; n.w = w++; } }
    while (k == 5) { static Counted never; n.x = &never; break; }
    switch (k) { case 1: { static Counted x; n.x = &x; } break; default: break; }
    { { static Counted y; n.y = &y; } }
}

// constants folded with the usual arithmetic conversions (float op int is float, rounded after each float
// operation, overflow to infinity), conversions to unsigned, sizeof and long long: data with cl's bytes, so
// the counted object after them is $TSS0 on both sides. Also as cl: a float literal rounded once from its
// decimal (fh, halfway as a double), out-of-range casts of floating constants modulo 2^N with the untruncated
// value kept for a floating type (dv 300.5, ni 7FFFFFFFh, nl, dc 3e9) and a char array from concatenated pieces
struct KonstFold { float f3; double d17; double d16; float f6; unsigned u; float fm; double ds; double dl; float fh; double dv; int ni; long long nl; double dc; char cs[4]; int t; };
STATICS_INLINE void konstF(KonstFold& s)
{
    static float f3 = 16777217 + 1.0f;
    static double d17 = 0.1f * 3;
    static double d16 = (float)(1.0 / 3) + 1;
    static float f6 = 1.0e38f * 10 / 10;
    static unsigned u = 3000000000.0;
    static float fm = 3.4028235e38f;
    static double ds = sizeof(int);
    static double dl = 5LL;
    static float fh = 1.00000005960464477550f;
    static double dv = (unsigned char)300.0 + 0.5;
    static int ni = (int)-2147483649.0;
    static long long nl = (long long)-9.3e18;
    static double dc = (int)3e9;
    static char cs[] = "\x41" "b" "\0" "c";
    static Counted t;
    s.f3 = f3; s.d17 = d17; s.d16 = d16; s.f6 = f6; s.u = u; s.fm = fm; s.ds = ds; s.dl = dl; s.t = t.id;
    s.fh = fh; s.dv = dv; s.ni = ni; s.nl = nl; s.dc = dc;
    for (int i = 0; i < 4; ++i) s.cs[i] = cs[i];
}

// a function-try-block: its try block is block 3, as cl numbers it
STATICS_INLINE const Counted* tryN(int a)
try { if (a) { static Counted s1; return &s1; } static Counted s3; return &s3; }
catch (int) { return 0; }

// cppparse's side
void cpp_konstK(KonstSeen& s);
void cpp_konstF(KonstFold& s);
const Counted* cpp_tryN(int a);
void cpp_nestedN(int k, NestedSeen& n);
int cpp_touchA();
int cpp_touchB();
int cpp_touchC();
const Counted* cpp_addrA();
const Counted* cpp_addrB();
int cpp_flakyX();   // flakyX().which, -1 when it threw (caught in cppparse)
int cpp_flakyY();

#endif
