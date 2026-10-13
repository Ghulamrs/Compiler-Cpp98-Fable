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

// cppparse's side
int cpp_touchA();
int cpp_touchB();
int cpp_touchC();
const Counted* cpp_addrA();
const Counted* cpp_addrB();

#endif
