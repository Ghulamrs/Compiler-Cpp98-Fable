// catch.cpp - side SIDE's handlers: what the other side (or main.cpp) throws is caught here, by value, by reference
// and by pointer, in nested tries, in loops, with objects alive around (see ehx.h, main.cpp "catch").
#include "ehx.h"

void note(int v);                // main.cpp: appends a digit to g_log

// ---- which handler takes a thrown type, and what it sees (the same handlers as main.cpp's, on this side) ----
int FN(catchExact)(void (*f)(int), int k)
{
    try { f(k); }
    catch (int v) { g_value = v; return 1; }
    catch (double v) { g_value = static_cast<int>(v * 10); return 2; }
    catch (Colour v) { g_value = v; return 3; }
    catch (unsigned long v) { g_value = static_cast<int>(v); return 4; }
    catch (char* p) { g_value = p[0]; return 5; }
    catch (const char* p) { g_value = p[1]; return 6; }
    catch (int* p) { g_value = *p; return 7; }
    catch (const int* p) { g_value = *p + 1; return 8; }
    catch (Plain& p) { g_value = p.a + p.b; return 9; }
    catch (DD& d) { g_value = d.code * 100 + d.d; return 10; }
    catch (VD& v) { g_value = v.code * 100 + v.v; return 11; }
    catch (Derived& d) { g_value = d.code * 100 + d.o; return 12; }
    catch (Single& x) { g_value = x.code * 100 + x.s; return 22; }
    catch (Single* p) { g_value = p->code + p->s; return 23; }
    catch (Err& e) { g_value = e.code * 10 + e.kind(); return 13; }
    catch (DD* p) { g_value = p->code + p->d; return 14; }
    catch (VD* p) { g_value = p->code + p->v + p->kind(); return 15; }
    catch (Derived* p) { g_value = p->code + p->other(); return 16; }
    catch (const Derived* p) { g_value = p->code + 1; return 17; }
    catch (Hidden&) { return 18; }
    catch (Twice& t) { g_value = t.t; return 19; }
    catch (int (*fp)(int)) { g_value = fp(21); return 20; }
    catch (...) { return 99; }
    return 0;
}

int FN(catchBase)(void (*f)(int), int k)
{
    try { f(k); }
    catch (const Err& e) { g_value = e.code * 10 + e.kind(); return 1; }
    catch (const Other& o) { g_value = o.other(); return 2; }
    catch (const Err* p) { g_value = p->code * 10 + p->kind(); return 3; }
    catch (const Other* p) { g_value = p->other(); return 4; }
    catch (const void* p) { g_value = p != 0; return 5; }
    catch (const volatile int& v) { g_value = v; return 6; }
    catch (...) { return 99; }
    return 0;
}

// by value: the handler's variable is a copy (a class with virtual bases too); a handler without a name copies nothing
int FN(catchValue)(void (*f)(int), int k)
{
    try { f(k); }
    catch (Other o) { g_value = o.o; return 1; }
    catch (DD d) { g_value = d.code * 100 + d.d + d.kind(); return 7; }
    catch (VD v) { g_value = v.code * 100 + v.v + v.kind(); return 8; }
    catch (Err e) { g_value = e.code * 10 + e.kind(); return 2; }
    catch (Plain p) { g_value = p.a; return 3; }
    catch (void* p) { g_value = p != 0; return 4; }
    catch (const volatile char* p) { g_value = p[2]; return 5; }
    catch (const volatile void* p) { g_value = p != 0; return 6; }
    catch (...) { return 99; }
    return 0;
}
int FN(catchUnnamed)(void (*f)(int), int k)
{
    try { f(k); }
    catch (Err) { return 1; }
    catch (int) { return 2; }
    catch (...) { return 99; }
    return 0;
}

// a handler rethrows through a function of the other side ("throw;" there); the outer handler gets the same object
int FN(catchRethrown)(void (*f)(int), void (*again)(), int k)
{
    const Err* first = 0;
    try {
        try { f(k); }
        catch (Err& e) { first = &e; again(); }
        catch (int) { again(); }
    }
    catch (Err& e) { g_value = e.code; return &e == first ? 1 : 2; }
    catch (int v) { g_value = v; return 3; }
    catch (...) { return 99; }
    return 0;
}

// ---- shapes of handlers ----
// an exception that leaves a handler is caught by the try around it in the same function; the objects of the
// handler and of the inner try are destroyed on the way
int FN(catchLeaving)(void (*f)(int))
{
    Guard a(1);
    try {
        Guard b(2);
        try { Guard c(3); f(0); }
        catch (int v) { Guard d(4); f(v + 10); note(9); }
        note(8);
    }
    catch (int v) { Guard e(5); return v; }
    return -1;
}

// a try inside a handler, whose handler takes what the first handler's code throws
int FN(catchInHandler)(void (*f)(int))
{
    int r = 0;
    try { f(0); }
    catch (int v) {
        Guard a(1);
        try { Guard b(2); f(v + 1); }
        catch (int w) { r = v * 10 + w; }
        r += 100;
    }
    return r;
}

// two exceptions alive at once in one frame: the first handler's reference must still see the first object (each throw
// has an exception object of its own). cl 19.52 /EHsc at /O1 and up miscompiles this function (it reads through a null
// pointer; /EHa and /Od run it): its /EHsc copy only states the result, the /EHa build (EHX_EHA) runs the code
int FN(catchTwoLive)(int)
{
#if defined(_MSC_VER) && !defined(EHX_EHA)
    note(2);
    return 1;
#else
    try { throw 1; }
    catch (int& first) {
        try { throw 2; } catch (int) { note(2); }
        return first;
    }
    return 0;
#endif
}

// handlers in a loop: break, continue and fall-through out of a handler, a counter the loop keeps across the try
int FN(catchLoop)(void (*f)(int), int n)
{
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        Guard g(1);
        try {
            if (i % 3 != 0) f(i);
            sum += 1;
        }
        catch (int v) {
            sum += 10 * v;
            if (v == 4) continue;
            if (v == 7) break;
        }
        sum += 100;
    }
    return sum;
}

// return and goto out of a handler, with objects of the function, of the try and of the handler
int FN(catchReturn)(void (*f)(int), int k)
{
    Guard a(1);
    try { Guard b(2); f(k); }
    catch (int v) {
        Guard c(3);
        if (k == 0) return v;
        if (k == 1) goto out;
        note(7);
    }
    note(8);
out:
    return 50;
}

// a class result built in a handler
Plain FN(catchResult)(void (*f)(int))
{
    Plain p;
    p.a = 1; p.b = 2;
    try { f(0); }
    catch (int v) { p.a = v + 5; p.b = v + 7; return p; }
    return p;
}

// a constructor's function-try-block: its handler runs after the members built so far are destroyed, and the
// exception goes on when the handler ends
namespace {
struct Member { int id; explicit Member(int i) : id(i) {} ~Member() { note(id); } };
struct Built {
    Member m;
    Member n;
    explicit Built(void (*f)(int)) try : m(4), n(5) { f(0); } catch (int) { note(9); }
};
// an object whose copy constructor throws: "throw b" never throws b
struct Bomb {
    Bomb() {}
    Bomb(const Bomb&) { throw 7; }
};
}
int FN(catchFunctionTry)(void (*f)(int))
{
    try { Built b(f); return b.m.id; }
    catch (int v) { return 20 + v; }
    return 0;
}
void FN(throwBomb)(int)
{
    Bomb b;
    throw b;
}
