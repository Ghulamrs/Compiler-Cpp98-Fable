// state.cpp - side SIDE and the state both runtimes must share: std::uncaught_exception while the other side's
// exception unwinds this side's frame, the terminate handler, and the standard exception classes thrown across (each
// side with the library its compiler has; EHX_ONE_LIBRARY: both with cppparse's, then also caught by value). See
// ehx.h, main.cpp "state" and "terminate".
#include "ehx.h"
#include <exception>
#include <stdexcept>
#include <string.h>

void note(int v);                // main.cpp: appends a digit to g_log

namespace {
struct Asks { ~Asks() { note(std::uncaught_exception() ? 1 : 2); } };
void copyText(char* to, int size, const char* from)
{
    int n = 0;
    while (from[n] && n + 1 < size) { to[n] = from[n]; ++n; }
    to[n] = 0;
}
}

// a destructor of this frame asks whether an exception is in flight
int FN(stateUncaught)(void (*f)(int))
{
    Asks a;
    f(0);
    return 0;
}

void FN(stateSetTerminate)(void (*handler)()) { std::set_terminate(handler); }

void FN(stateThrowStd)(int k)
{
    if (k == 0) throw std::runtime_error("runtime message");
    if (k == 1) throw std::out_of_range("range message");
    if (k == 2) throw std::logic_error("logic message");
}

// by reference: the handler reads the message through the object's own virtual function
int FN(stateCatchStd)(void (*f)(int), int k, char* text, int size)
{
    text[0] = 0;
    try { f(k); }
    catch (const std::out_of_range& e) { copyText(text, size, e.what()); return 1; }
    catch (const std::logic_error& e) { copyText(text, size, e.what()); return 2; }
    catch (const std::exception& e) { copyText(text, size, e.what()); return 3; }
    return 0;
}

#ifdef EHX_ONE_LIBRARY
// by value: the thrower's copy constructor builds the handler's variable, so both sides need one layout of the class
int FN(stateCatchStdValue)(void (*f)(int), int k, char* text, int size)
{
    text[0] = 0;
    try { f(k); }
    catch (std::logic_error e) { copyText(text, size, e.what()); return 2; }
    catch (std::runtime_error e) { copyText(text, size, e.what()); return 3; }
    return 0;
}
#endif
