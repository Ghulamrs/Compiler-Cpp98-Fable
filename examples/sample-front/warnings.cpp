// warnings.cpp - cl /W4 warns on every marked line and nowhere else; cppparse must do the same (the 2026-10-15
// review's warn.cpp, M12). Compiled by test 17 at -O0; the object is not run.
#include <cstdio>
#include <cstdlib>
int missingReturn(int x) { if (x > 0) return 1; }                  // C4715: S155
int unusedParam(int x, int y) { return x; }                         // C4100: S153 'y'
int signedUnsigned(int a, unsigned b) { return a < b; }             // C4018: S120
int narrowing() { double d = 2.7; int i = d; return i; }            // C4244: S160 'initializing' double to int
int assignInCondition(int a) { if (a = 3) return a; return 0; }     // C4706: S159
int uninit() { int u; return u; }                                   // C4700: S156
int unreachable() { return 1; std::printf("never"); }               // C4702: S157
int shadow(int a) { { int a = 2; return a; } }                      // C4457 + C4100: S158, S153
char truncated() { char c = 300; return c; }                        // C4309: S160 truncation of constant value
short narrowed(int v) { short s = v; return s; }                    // C4244: S160 int to short
int unusedLocal(int v) { int w = v; return 0; }                     // C4189: S154
int returned(long long v) { return v; }                             // C4244: S160 'return'
void argued(short s); void caller(int v) { argued(v); }             // C4244: S160 'argument'
int fine(int a) { for (;;) { if (a) return a; } }                   // no warning: the loop never ends
int fineSwitch(int a) { switch (a) { case 1: return 2; default: return 3; } }   // no warning
int fineAbort(int a) { if (a) return 1; std::abort(); }             // no warning: abort does not return
int fineParen(int a) { if ((a = 3)) return a; return 0; }           // no warning: the parentheses say it is meant
int fineRef(int a) { int r; int& ref = r; ref = a; return r; }      // no warning: written through the reference
int fineOut(int* p); int fineCall() { int v; fineOut(&v); return v; }       // no warning: the address is taken
int fineFill(char* p); int fineArray() { char buf[8]; fineFill(buf); return buf[0]; }   // no warning: arrays are not scalars
int main() { return 0; }
