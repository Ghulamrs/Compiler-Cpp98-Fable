// sums.cpp - sample-dbg: a program of one unit and one header of its own (sums.h), with <vector> for headers of the
// library that have code. Test 22 builds it with -g, links it with /DEBUG and asks the PDB for each source file's
// checksum (sample-dbg-driver\dbgsum.cpp, SymGetSourceFileChecksum): it must be the SHA-256 of the file as it is on
// disk, and no longer after the file is edited.
#include "sums.h"

#include <cstdio>
#include <vector>

int main()
{
    std::vector<int> values;
    for (int i = 1; i <= 10; ++i) values.push_back(i * i);
    Pile<int> pile;
    pile.put(addUp(&values[0], static_cast<int>(values.size())));
    pile.put(15);
    std::printf("sum %d\n", pile.held());
    return pile.held() == 400 ? 0 : 1;
}
