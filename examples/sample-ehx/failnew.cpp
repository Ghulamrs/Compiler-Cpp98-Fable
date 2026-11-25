// failnew.cpp - an operator new that fails on purpose (sandbox\scripts\failnew.ps1, row M7): linked into the
// self-built compiler, it replaces the library's and throws std::bad_alloc at the k-th allocation, k from the
// environment variable CPPPARSE_FAILNEW (0 or unset: never). The exception then unwinds through whatever frames of
// the compiler are active, up to the driver's handler: thousands of cleanups that no ordinary run executes.
// Strict C++98.
#include <new>
#include <stdlib.h>

static long g_count = 0;
static long g_failAt = -1;

void* operator new(size_t n) throw(std::bad_alloc)
{
    if (g_failAt < 0) { const char* k = getenv("CPPPARSE_FAILNEW"); g_failAt = k ? atol(k) : 0; }
    if (++g_count == g_failAt) throw std::bad_alloc();
    void* p = malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](size_t n) throw(std::bad_alloc) { return ::operator new(n); }
void operator delete(void* p) throw() { free(p); }
void operator delete[](void* p) throw() { free(p); }
