// types2.cpp - sample-dbg: the second unit of the types program (test 20).
// It has, under the names types.cpp uses, a class of another layout in its own unnamed namespace, and it defines
// the classes of types.h a second time: a debugger must show each unit's variables by that unit's types. In the
// mixed build this unit is compiled by cl /Z7, so that one program has the shared classes from both compilers.
#include "types.h"

namespace { struct Node { double weight; int a; char b; short c; }; }   // types.cpp's Node is { int a; char b; }

Node g_node2;
shared::Pair g_pair2 = { 3, 4.0 };
shared::Arr<double, 3> g_arr2;
shared::Counter g_counter2;
shared::Mode g_mode2 = shared::Auto;

static int localSizes()
{
    // a class local to a function, of a name another function's has too
    struct Local { int a; int b; };
    static Local s_local;
    s_local.a = 1; s_local.b = 2;
    return static_cast<int>(sizeof(Local)) + s_local.a;
}

static int localSizesOther()
{
    struct Local { double d; char c; };
    static Local s_local;
    s_local.d = 1.0; s_local.c = 'c';
    return static_cast<int>(sizeof(Local)) + s_local.c;
}

void reportSecondUnit()
{
    GLOBAL(g_node2); GLOBAL(g_pair2); GLOBAL(g_arr2); GLOBAL(g_counter2); GLOBAL(g_mode2);
    factMember("g_node2", "weight", (const char*)&g_node2.weight - (const char*)&g_node2);
    factMember("g_node2", "a", (const char*)&g_node2.a - (const char*)&g_node2);
    factMember("g_node2", "b", (const char*)&g_node2.b - (const char*)&g_node2);
    factMember("g_node2", "c", (const char*)&g_node2.c - (const char*)&g_node2);
    factMember("g_pair2", "first", (const char*)&g_pair2.first - (const char*)&g_pair2);
    factMember("g_pair2", "second", (const char*)&g_pair2.second - (const char*)&g_pair2);
    factMember("g_counter2", "m_n", (const char*)&g_counter2.m_n - (const char*)&g_counter2);
    factMember("g_arr2", "count", (const char*)&g_arr2.count - (const char*)&g_arr2);
    factLocal("localSizes", "s_local", localSizes() - 1);
    factLocal("localSizesOther", "s_local", localSizesOther() - 'c');
    g_counter2.next();
}
