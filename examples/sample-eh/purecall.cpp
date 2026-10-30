// purecall.cpp - a whole program, compiled by cppparse and by cl (sandbox\scripts\eh.ps1): a pure virtual function
// called while the object is still under construction, so the vfptr points at the abstract class's table. That slot
// holds vcruntime's _purecall (R6025 "pure virtual function call", abort), as cl's tables do, instead of 0 (a jump
// to address 0: segfault). The output before the abort and the exit code must equal cl's.
// The call goes through a volatile function pointer: a plain f() in the constructor is devirtualised by cl (the
// dynamic type is known there), which then needs a body for S::f (LNK2019).
// Strict C++98.
#include <stdio.h>

struct S { S(); virtual void f() = 0; virtual int g() const = 0; };
void callF(S* s) { s->f(); }
void (*volatile g_call)(S*) = callF;
S::S() { printf("before the call\n"); fflush(stdout); g_call(this); printf("not reached\n"); }
struct D : S { void f() {} int g() const { return 1; } };

int main()
{
    D d;
    printf("not reached: %d\n", d.g());
    return 0;
}
