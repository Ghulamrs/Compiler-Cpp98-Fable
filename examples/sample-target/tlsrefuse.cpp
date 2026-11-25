// tlsrefuse.cpp - what __declspec(thread) may not be (row P1 stage 1, docs/design-selfhost.md 4.1): each case, chosen
// by -D CASE=<n>, is the error S164 and no object; target.ps1 names the words each message must hold
struct Ctor { Ctor(); int v; };
struct Dtor { ~Dtor(); int v; };
struct __declspec(align(32)) Wide { int v; };
int plain;
int other();

#if CASE == 1      // a static local
int f1() { static __declspec(thread) int n; return ++n; }
#elif CASE == 2    // an automatic local
int f2() { __declspec(thread) int n = 0; return n; }
#elif CASE == 3    // a static data member
struct S3 { static __declspec(thread) int n; };
#elif CASE == 4    // a function
__declspec(thread) int f4();
#elif CASE == 5    // a type and no variable
__declspec(thread) struct T5 { int v; };
#elif CASE == 6    // a parameter
int f6(__declspec(thread) int p) { return p; }
#elif CASE == 7    // a redeclaration that adds the attribute
extern int r7;
__declspec(thread) int r7;
#elif CASE == 8    // a redeclaration that drops it
extern __declspec(thread) int r8;
int r8 = 1;
#elif CASE == 9    // a constructor to run
__declspec(thread) Ctor c9;
#elif CASE == 10   // a destructor to run
__declspec(thread) Dtor d10;
#elif CASE == 11   // an initialiser that needs code
__declspec(thread) int i11 = other();
#elif CASE == 12   // more than 16 bytes of alignment
__declspec(thread) Wide w12;
#elif CASE == 13   // a thread-local's address initialising a static
__declspec(thread) int t13;
int* p13 = &t13;
#elif CASE == 14   // ... and another thread-local
__declspec(thread) int t14;
__declspec(thread) int* p14 = &t14;
#elif CASE == 15   // a typedef
typedef __declspec(thread) int T15;
#elif CASE == 16   // a member of a class
struct S16 { __declspec(thread) int n; };
#endif

int keep() { return plain; }
