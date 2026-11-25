// intrinsic-refuse.cpp - what the intrinsics and the linker pragma refuse (row P1 stage 2, docs/design-selfhost.md 4.2,
// 4.5): by -D CASE=<n>. Case 1: the address of an intrinsic is the error S165. Case 2: a linker directive other than
// /alternatename is a warning and is dropped (the object is written without it). Case 3: a function of the same name
// with other parameters is not the intrinsic: an ordinary call
#include <intrin.h>

#if CASE == 1
long (*pick())(long volatile*) { return &_InterlockedIncrement; }
#elif CASE == 2
#pragma comment(linker, "/include:something")
#pragma comment(linker, "/alternatename:cppparse_probe_a=cppparse_probe_b")
#elif CASE == 3
extern "C" int _InterlockedOr(int volatile* p, int v);
int orIt(int volatile* p) { return _InterlockedOr(p, 1); }
#endif

long bump(long volatile* p) { return _InterlockedIncrement(p); }
