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

// cppparse's side
int cpp_touchA();
int cpp_touchB();
int cpp_touchC();
const Counted* cpp_addrA();
const Counted* cpp_addrB();
int cpp_flakyX();   // flakyX().which, -1 when it threw (caught in cppparse)
int cpp_flakyY();

#endif
