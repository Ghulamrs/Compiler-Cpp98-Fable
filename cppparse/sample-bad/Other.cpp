// Other.cpp - second translation unit with one-definition-rule violations.
#include "Widget.h"

int g_dup = 2;                       // E160 also defined in Widget.cpp
int twice() { return 2; }            // E163 also defined in Widget.cpp

struct Dup { int b; };               // E120 also defined in Widget.h

enum Color { Red, Green };
enum Shade { Red, Dark };            // E162 duplicate enumerator Red

void Base::run() { }                 // E131 duplicate definition (also in Widget.cpp)

int useAll()
{
    Color c = Green;
    Shade s = Dark;
    return (int)c + (int)s + twice() + g_dup;
}
