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

// cppparse's side
void cpp_konstK(KonstSeen& s);
int cpp_touchA();
int cpp_touchB();
int cpp_touchC();
const Counted* cpp_addrA();
const Counted* cpp_addrB();
int cpp_flakyX();   // flakyX().which, -1 when it threw (caught in cppparse)
int cpp_flakyY();

#endif
