// iostream.cpp - cppparse's C++98 library: the standard stream objects and the global allocation functions.
// Compiled by cppparse (--compile6 -I ../include) and linked with every program that uses <iostream>.
#include <iostream>
#include <new>
#include <cstdlib>

// the global allocation functions (18.4.1): a new-expression calls them by their MSVC names (??2@YAPEAX_K@Z and
// so on), so a program that links this unit gets these instead of the C runtime's, whose failure is reported
// through MSVC's exception handling, which no cppparse frame can catch; these throw std::bad_alloc through
// cppparse's runtime. A program may still define its own (18.4.1.1/2) in a unit of its own; there is no new_handler.
void* operator new(size_t n)
{
    void* p = malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](size_t n)
{
    void* p = malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) { free(p); }
void operator delete[](void* p) { free(p); }

namespace std {

__fileostream cout(1);
__fileostream cerr(2);
__fileostream clog(2);
__fileistream cin(0);

// cin and cerr are tied to cout (27.3.1): cout is flushed before they read or write
struct __tie_streams { __tie_streams() { cin.tie(&cout); cerr.tie(&cout); } };
static __tie_streams __tie_streams_now;

}
