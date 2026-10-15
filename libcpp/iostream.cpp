// iostream.cpp - cppparse's C++98 library: the standard stream objects. Compiled by cppparse
// (--compile6 -I ../include) and linked with every program that uses <iostream>.
#include <iostream>

namespace std {

__fileostream cout(1);
__fileostream cerr(2);
__fileostream clog(2);
__fileistream cin(0);

// cin and cerr are tied to cout (27.3.1): cout is flushed before they read or write
struct __tie_streams { __tie_streams() { cin.tie(&cout); cerr.tie(&cout); } };
static __tie_streams __tie_streams_now;

}
