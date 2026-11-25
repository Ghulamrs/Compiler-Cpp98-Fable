// replacearray.cpp - a program that replaces the array forms only, operator new[] and operator delete[] (18.4.1.2):
// a new-expression for an array and the library's nothrow array forms call them, one for a single object calls the
// library's operator new, and so does the program's operator new[] itself. Test 15 compares the output with cl's
// build with its own library, at -O0 and -O2 (row G17).
#include <new>
#include <iostream>
#include <cstddef>

static bool   g_on = false;
static int    g_news = 0, g_deletes = 0;
static size_t g_bytes = 0;

void* operator new[](size_t n) throw(std::bad_alloc)
{
    if (g_on) { ++g_news; g_bytes += n; }
    return ::operator new(n);          // the library's
}

void operator delete[](void* p) throw()
{
    if (g_on && p) ++g_deletes;
    ::operator delete(p);
}

int main()
{
    std::cout << "replacearray" << std::endl;
    g_on = true;
    int*    a = new int(1);                        // not an array: the library's operator new
    int*    b = new int[6];
    double* c = new (std::nothrow) double[4];
    delete a;
    delete[] b;
    delete[] c;
    g_on = false;
    std::cout << g_news << " new[], " << g_deletes << " delete[], " << g_bytes << " bytes" << std::endl;
    return 0;
}
