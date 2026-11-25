// replacepart.cpp - a program that replaces only one allocation function, operator new (18.4.1.1/2 lets it replace
// each of the eight by itself): the library keeps the other seven. Its operator new[] and its nothrow forms call the
// program's operator new (18.4.1.2, 18.4.1.1/7), and its deallocation functions free what that one got from malloc.
// Test 15 compares the output with cl's build with its own library, at -O0 and -O2 (row G17).
#include <new>
#include <iostream>
#include <cstdlib>
#include <cstddef>

static bool   g_on = false;
static int    g_news = 0;
static size_t g_bytes = 0;

void* operator new(size_t n) throw(std::bad_alloc)
{
    if (g_on) { ++g_news; g_bytes += n; }
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}

struct Pair {
    int a, b;
    Pair() : a(1), b(2) {}
    ~Pair() { a = 0; }
};

int main()
{
    std::cout << "replacepart" << std::endl;
    g_on = true;
    int*  a = new int(1);
    int*  b = new int[6];
    Pair* c = new Pair[2];
    int*  d = new (std::nothrow) int(2);
    char* e = new (std::nothrow) char[50];
    void* f = ::operator new[](8);
    g_on = false;
    std::cout << g_news << " new, " << g_bytes << " bytes, " << c[1].b << std::endl;
    // the library's deallocation functions
    delete a;
    delete[] b;
    delete[] c;
    delete d;
    delete[] e;
    ::operator delete[](f);
    std::cout << "freed" << std::endl;
    return 0;
}
