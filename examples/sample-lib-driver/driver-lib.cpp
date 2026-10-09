// driver-lib.cpp - links against the objects cppparse generated for examples/sample-lib/*.cpp with
// --msvc-mangling and checks the results of cppparse's own standard-library headers.
//   cl /EHsc driver-lib.cpp lib1.obj lib2.obj cppparse_rt.obj
#include <cstdio>

int stringBasics();
int stringOps();
int vectorBasics();
int vectorOfStrings();
int pairsAndSwap();
int stableSorting();
int mapBasics();
int mapOfStrings();
int setBasics();
int treeStress(int n);
int streamFormatting();
int streamParsing();
int fileRoundTrip();

static int g_failures = 0;

static void check(const char* what, long got, long expected)
{
    if (got == expected) std::printf("  ok   %-28s = %ld\n", what, got);
    else { std::printf("  FAIL %-28s = %ld, expected %ld\n", what, got, expected); ++g_failures; }
}

int main()
{
    std::setvbuf(stdout, 0, _IONBF, 0);
    check("stringBasics()", stringBasics(), 1164);
    check("stringOps()", stringOps(), 42);
    check("vectorBasics()", vectorBasics(), 3752);
    check("vectorOfStrings()", vectorOfStrings(), 12);
    check("pairsAndSwap()", pairsAndSwap(), 305);
    check("stableSorting()", stableSorting(), 4);
    check("mapBasics()", mapBasics(), 2613);
    check("mapOfStrings()", mapOfStrings(), 731);
    check("setBasics()", setBasics(), 4253);
    check("treeStress(1000)", treeStress(1000), 1000);
    check("streamFormatting()", streamFormatting(), 1731);
    check("streamParsing()", streamParsing(), 1234);
    check("fileRoundTrip()", fileRoundTrip(), 77);
    std::printf(g_failures ? "%d FAILED\n" : "ALL OK\n", g_failures);
    return g_failures ? 1 : 0;
}
