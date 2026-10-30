// driver-life.cpp - links against the object cppparse generated for examples/sample-life/life.cpp
// with --msvc-mangling and checks the object-lifetime results. Built with MSVC:
//   cl /EHsc driver-life.cpp life.obj cppparse_rt.obj
#include <cstdio>

int liveObjects();
int liveAllocs();
int scopes();
int loops(int n);
int earlyReturn(int v);
int copies();
int byValue();
int temporaries();
int deepCopies();
int arrays();
int unwinding();
int ctorThrows();

extern "C" int cppparse_frame_depth();
extern "C" int cppparse_caught_depth();

static int g_failures = 0;

static void check(const char* what, long got, long expected)
{
    if (got == expected) std::printf("  ok   %-28s = %ld\n", what, got);
    else { std::printf("  FAIL %-28s = %ld, expected %ld\n", what, got, expected); ++g_failures; }
}

int main()
{
    std::setvbuf(stdout, 0, _IONBF, 0);   // every line reaches the console even if a check crashes
    check("scopes()", scopes(), 210);
    check("live after scopes", liveObjects(), 0);
    check("loops(5)", loops(5), 200);
    check("live after loops", liveObjects(), 0);
    check("earlyReturn(4)", earlyReturn(4), 8);
    check("live after earlyReturn", liveObjects(), 0);
    check("copies()", copies(), 217);
    check("live after copies", liveObjects(), 0);
    // LIFE_NRVO=1 (-O1 and up, as cl /O2): make() builds its local in the result object, one copy and one
    // destruction fewer than at -O0 (cl /Od)
#if LIFE_NRVO
    check("byValue()", byValue(), 2108);
#else
    check("byValue()", byValue(), 2208);
#endif
    check("live after byValue", liveObjects(), 0);
#if LIFE_NRVO
    check("temporaries()", temporaries(), 26);
#else
    check("temporaries()", temporaries(), 36);
#endif
    check("live after temporaries", liveObjects(), 0);
    check("deepCopies()", deepCopies(), 3946);
    check("buffers after deepCopies", liveAllocs(), 0);
    check("arrays()", arrays(), 563);
    check("live after arrays", liveObjects(), 0);
    check("unwinding()", unwinding(), 372);
    check("live after unwinding", liveObjects(), 0);
    check("buffers after unwinding", liveAllocs(), 0);
    check("ctorThrows()", ctorThrows(), 99);
    check("live after ctorThrows", liveObjects(), 0);
    check("buffers after ctorThrows", liveAllocs(), 0);
    check("frame depth", cppparse_frame_depth(), 0);
    check("caught depth", cppparse_caught_depth(), 0);
    std::printf(g_failures ? "%d FAILED\n" : "ALL OK\n", g_failures);
    return g_failures ? 1 : 0;
}
