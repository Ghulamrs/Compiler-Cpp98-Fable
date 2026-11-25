// tls5.cpp - five accesses to thread-locals, compiled by cppparse and by cl (row P1 stage 1, docs/design-selfhost.md
// 4.1): the listing assembled by ml64 must give --obj's code bytes and relocations, and .tls$, its symbols and its
// relocations must be cl's
int helper(int x) { return x + 1; }

__declspec(thread) int g_count = 5;
static __declspec(thread) long s_hidden;
struct Pair { int a; int b; };
__declspec(thread) Pair g_pair = { 1, 2 };
__declspec(thread) int (*g_fn)(int) = helper;    // an address constant: an ADDR64 relocation in .tls$
static __declspec(thread) double s_scale = 1.5;
extern __declspec(thread) int g_other;           // defined in another object

int get() { return g_count; }
void bump() { ++g_count; s_hidden += 2; s_scale *= 2.0; }
int* pairB() { return &g_pair.b; }
int callFn(int x) { return g_fn(x) + (int)s_hidden + (int)s_scale; }
int readOther() { return g_other; }
