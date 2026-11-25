// new-refused.cpp - must be rejected (S107): new-expressions that name no operator new. A class that declares an
// operator new or new[] hides the global ones, the reserved placement forms included (3.4.2, 12.5; cl C2660), so a
// class with placement forms alone has none for a plain new (row G13; front2.cpp newhide:: has what stays valid); and
// no global operator new[] takes an Arena (cl C2665). The allocations refused here before are generated as cl's since
// rows 21 and B6 (front2.cpp newarr::): multidimensional arrays, placement new of an array with other arguments
#include <new>
struct Arena { int used; };
struct C { int id; static void* operator new(size_t n); static void* operator new[](size_t n); };
char buf[64];
Arena arena;
C* one() { return new (buf) C; }
C* many() { return new (buf) C[2]; }
int* ints() { return new (arena) int[4]; }
struct P { int id; static void* operator new(size_t n, Arena& a); static void* operator new[](size_t n, Arena& a); };
P* plain() { return new P; }                        // line 15: P's operators hide the global ones, none takes the size alone
P* plainMany() { return new P[2]; }
int main() { return 0; }
