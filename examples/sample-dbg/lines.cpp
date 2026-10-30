// lines.cpp - CodeView line tables (test 18): built by cppparse --compile6 --obj, linked with /DEBUG, then
// sample-dbg-driver/dbgcheck.cpp reads the lines of these functions back through dbghelp. The line numbers of
// this file are part of the test: keep the functions where they are.
#include <stdio.h>

int twice(int x)
{
    int y = x * 2;
    y = y + 1;
    return y - 1;
}

struct Acc {
    int total;
    Acc() : total(0) {}
    void add(int v)
    {
        total += v;
        if (total > 100)
            total = 100;
    }
};

int main()
{
    Acc a;
    for (int i = 0; i < 5; ++i)
        a.add(twice(i));
    printf("total %d\n", a.total);
    return a.total == 20 ? 0 : 1;
}
