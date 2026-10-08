// driver-self.cpp - links against the object cppparse generated for examples/sample-self/self.cpp
// with --msvc-mangling and checks the results of the self-hosting constructs. Built with MSVC:
//   cl /EHsc driver-self.cpp self.obj cppparse_rt.obj legacy_stdio_definitions.lib
// The expected values are what the same source gives when cl compiles it against cppparse's headers.
#include <cstdio>

int aggregates();
int conditionalTemporaries();
int conditionalReferences();
int defaultArguments();
int pointerArithmetic();
int conditionalConversions();
int conditionalLvalues();
int addressOfElements();
int storeForwarding();
int postIncrement();
int castsAndStatics();
int narrowArithmetic();
int bigFrame();
int stringEscapes();

static int g_failures = 0;

static void check(const char* what, long got, long expected)
{
    if (got == expected) std::printf("  ok   %-28s = %ld\n", what, got);
    else { std::printf("  FAIL %-28s = %ld, expected %ld\n", what, got, expected); ++g_failures; }
}

int main()
{
    std::setvbuf(stdout, 0, _IONBF, 0);   // every line reaches the console even if a check crashes
    check("bigFrame()", bigFrame(), 1777);   // first, while little of the stack is committed
    check("aggregates()", aggregates(), 73466644);
    check("conditionalTemporaries()", conditionalTemporaries(), 44415);
    check("conditionalReferences()", conditionalReferences(), 11203207);
    check("defaultArguments()", defaultArguments(), 185286);
    check("pointerArithmetic()", pointerArithmetic(), 2868828);
    check("conditionalConversions()", conditionalConversions(), 11343);
    check("conditionalLvalues()", conditionalLvalues(), 51);
    check("addressOfElements()", addressOfElements(), 9291);
    check("storeForwarding()", storeForwarding(), 102);
    check("postIncrement()", postIncrement(), 5611);
    check("castsAndStatics()", castsAndStatics(), 70611111);
    check("narrowArithmetic()", narrowArithmetic(), 755793458);
    check("stringEscapes()", stringEscapes(), 16357);
    std::printf(g_failures ? "%d FAILED\n" : "ALL OK\n", g_failures);
    return g_failures ? 1 : 0;
}
