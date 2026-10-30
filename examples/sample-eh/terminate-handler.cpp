// terminate-handler.cpp - a whole program, compiled by cppparse and by cl (sandbox\scripts\eh.ps1): an exception that
// leaves a handler destroys the handler's by-value parameter first; when that destructor throws, the program
// terminates before the handled exception is released (cl prints "h ~E1" and aborts).
// Strict C++98.
#include <stdio.h>

struct E { int c; E() : c(0) {} E(const E& o) : c(o.c + 1) {} ~E() { printf("~E%d ", c); if (c == 1) throw 4; } };

int main()
{
    setvbuf(stdout, 0, _IONBF, 0);
    printf("A ");
    try { try { throw E(); } catch (E e) { printf("h "); throw 3; } } catch (...) { printf("no "); }
    printf("end\n");
    return 0;
}
