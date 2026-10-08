// Widget.cpp - deliberately broken implementation used to test the health checker.
#include "Widget.h"
#include <cstdio>
#include <stdexcept>

int g_dup = 1;                       // E160 also defined in Other.cpp
int twice() { return 1; }            // E163 also defined in Other.cpp

Base::Base() {}
Base::~Base() { throw 1; }           // W147 throw in destructor
void Base::run() {}

Widget::Widget(int id, const char* label)
    : m_label(label), m_id(id), gadgetX(0)   // W137 order differs, E136 gadgetX is not a member, W138 count never initialised
{
}

void Widget::run()
{
    int unused = 0;                  // W151 unused local
    int m_id = 3;                    // W152 shadows the member
    if (count = 5) {                 // W153 assignment in condition
        return;
    }
    for (int i = 0; i < 3; ++i);     // W154 empty statement
    helper(1, 2);                    // E142 wrong argument count
    missingFunction(3);              // I146 not declared anywhere
    undeclaredMember();              // E143 not a member of Widget or Base
    Base* b = this;
    b.run();                         // E144 '.' on a pointer
    b->nothing();                    // E143 Base has no member nothing
    try { run(); }
    catch (...) { }
    catch (std::exception e) { }     // E156 handler after catch(...), W155 catch by value
    return;
    printf("unreachable\n");         // W157 unreachable
}

int Widget::value()                  // E130 declared const, defined without const
{
    return m_id;
}

float Widget::scale(double factor = 1.0)   // E134 return type differs, E135 default argument repeated
{
    return (float)factor;
}

int Widget::noReturn(int x)          // E130 not declared; W140 missing return
{
    x = x + 1;
}

void Widget::badVoid()               // E130 not declared; W141 return value in void
{
    return 1;
}

Tool::Tool() {}

void Tool::run(int level)            // W150 unused parameter
{
    Widget w(1);                     // E145 no constructor takes 1 argument
    Widget w2;                       // E145 no default constructor
    Widget w3(1, "a", 3);            // E145 no constructor takes 3 arguments
    Tool t;                          // E145 abstract class
    w.helper();
    w.value(1);                      // E142 value() takes no arguments
}

#ifdef TRAILING                      // E101 never closed
int trailing = 1;
