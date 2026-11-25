// cross.cpp - side SIDE of the seven paths an exception can take between two sides (see ehx.h, main.cpp "cross"):
// a thrower, a frame with an object between, a function with handlers, a handler that rethrows.
#include "ehx.h"

void FN(crossThrow)(int kind)
{
    Guard g(1);
    if (kind == 0) throw 42;
    if (kind == 1) throw Derived(7);
    if (kind == 2) throw "text";
}

void FN(crossMiddle)(void (*f)(int), int kind)
{
    Guard g(10);
    f(kind);
}

int FN(crossCatch)(void (*f)(int), int kind)
{
    Guard g(100);
    try { f(kind); }
    catch (int v) { return v; }
    catch (const Err& e) { return e.code; }
    catch (...) { return -1; }
    return 0;
}

int FN(crossRethrow)(void (*f)(int), int kind)
{
    try { f(kind); }
    catch (...) { Guard g(1000); throw; }
    return 0;
}
