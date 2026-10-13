// terminate-catch.cpp - a whole program, compiled by cppparse and by cl (sandbox\scripts\eh.ps1): a copy constructor
// that throws while it initializes a by-value catch parameter calls terminate (C++98 15.5.1), even when an outer
// handler would take the new exception. The output before the abort and the exit code must equal cl's.
// Strict C++98.
#include <stdio.h>

struct E { int v; E(int x) : v(x) {} E(const E& o) : v(o.v) { if (v == 4) throw 6; } };

int main()
{
    int r = 0;
    try { throw E(3); } catch (E e) { r = e.v; }
    printf("copied: %d\n", r);
    fflush(stdout);
    try {
        try { throw E(4); } catch (E e) { r = e.v; }
    } catch (int x) { r = x; }
    printf("not reached: %d\n", r);
    return 0;
}
