// driver-lib.cpp - links against the objects cppparse generated for examples/sample-lib/*.cpp with
// --msvc-mangling and checks the results of cppparse's own standard-library headers.
//   cl /EHsc driver-lib.cpp lib1.obj lib2.obj cppparse_rt.obj
#include <cstdio>

int stringBasics();
int stringOps();
int stringThrows();
int vectorBasics();
int vectorOfStrings();
int pairsAndSwap();
int stableSorting();
int mapBasics();
int mapOfStrings();
int mapValueType();
int setBasics();
int treeStress(int n);
int streamFormatting();
int streamParsing();
int fileRoundTrip();
int streamManipulators();
int streamFriends();
int linkageA();
int linkageB();
int explicitInstA();
int explicitInstB();
int limitsBasics();
int iteratorsAndFunctors();
int numericOps();
int containersAdaptors();
int bitsAndManipulators();
int exceptionsAndNew();

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
    check("stringThrows()", stringThrows(), 4843);
    check("vectorBasics()", vectorBasics(), 3752);
    check("vectorOfStrings()", vectorOfStrings(), 12);
    check("pairsAndSwap()", pairsAndSwap(), 305);
    check("stableSorting()", stableSorting(), 4);
    check("mapBasics()", mapBasics(), 2613);
    check("mapOfStrings()", mapOfStrings(), 731);
    check("mapValueType()", mapValueType(), 4321);
    check("setBasics()", setBasics(), 4253);
    check("treeStress(1000)", treeStress(1000), 1000);
    check("streamFormatting()", streamFormatting(), 1731);
    check("streamParsing()", streamParsing(), 1234);
    check("fileRoundTrip()", fileRoundTrip(), 77);
    check("streamManipulators()", streamManipulators(), 2551);
    check("streamFriends()", streamFriends(), 303);
    check("linkageA()", linkageA(), 1122);
    check("linkageB()", linkageB(), 2225);
    check("explicitInstA()", explicitInstA(), 3105);
    check("explicitInstB()", explicitInstB(), 4321);
    check("limitsBasics()", limitsBasics(), 1111);
    check("iteratorsAndFunctors()", iteratorsAndFunctors(), 3421);
    check("numericOps()", numericOps(), 2460);
    check("containersAdaptors()", containersAdaptors(), 1234);
    check("bitsAndManipulators()", bitsAndManipulators(), 5321);
    check("exceptionsAndNew()", exceptionsAndNew(), 2731);
    std::printf(g_failures ? "%d FAILED\n" : "ALL OK\n", g_failures);
    return g_failures ? 1 : 0;
}
