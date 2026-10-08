// iostream.cpp - cppparse's C++98 library: the standard stream objects. Compiled by cppparse
// (--compile6 -I ../include) and linked with every program that uses <iostream>.
#include <iostream>

namespace std {

__fileostream cout(1);
__fileostream cerr(2);
__fileostream clog(2);
__fileistream cin(0);

}
