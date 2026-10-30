// terminate.cpp - a whole program, compiled by cppparse and by cl (sandbox\scripts\eh.ps1, the cl reference with
// /Zc:implicitNoexcept-, C++98's destructors that may throw): an exception caught inside a destructor that the
// stack unwinding runs is fine; one that leaves such a destructor calls terminate (C++98 15.5.1), whose default
// handler aborts. The output before the abort and the exit code must equal cl's.
// Strict C++98.
#include <stdio.h>

struct Ok { ~Ok() { try { throw 1; } catch (int) {} } };
struct Bad { ~Bad() { throw 1; } };

int main()
{
    int r = 0;
    try { Ok o; throw 2; } catch (int x) { r = x; }
    printf("caught inside a destructor while unwinding: %d\n", r);
    fflush(stdout);
    try { Bad b; throw 2; } catch (...) { r = 9; }
    printf("not reached: %d\n", r);
    return 0;
}
