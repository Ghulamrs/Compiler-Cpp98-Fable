// driver-self.cpp - links against the object cppparse generated for examples/sample-self/self.cpp
// with --msvc-mangling and checks the results of the self-hosting constructs. Built with MSVC:
//   cl /EHsc driver-self.cpp self.obj cppparse_rt.obj legacy_stdio_definitions.lib
// The expected values are what the same source gives when cl compiles it against cppparse's headers.
#include <cstdio>
#include <cstddef>
#ifdef _WIN32
#include <windows.h>
#endif

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
int carriedCounter(const int* left, int nl, const int* right, int nr, int* out);
int sharedStatic();
int nestedStatics();
int wideConstants();
double stackDoubles(double a, double b, double c, double d, double e, double f, double g);
float stackFloats(float a, float b, float c, float d, float e, float f, float g);
int exitOrder();

static int g_failures = 0;

static int g_sinkCalls = 0;
void selfSink(std::size_t n) { if (n < 100) ++g_sinkCalls; }

static long carried()
{
    int left[5] = { 1, 4, 6, 20, 30 }, right[4] = { 2, 3, 7, 10 }, out[9];
    long n = carriedCounter(left, 5, right, 4, out);
    long s = 0;
    for (int i = 0; i < 9; ++i) s = s * 3 + out[i];
    return g_sinkCalls * 10000000L + n * 100000L + s;
}

static int g_shared[4];

#ifdef _WIN32
static DWORD WINAPI callShared(LPVOID slot)
{
    g_shared[reinterpret_cast<std::size_t>(slot)] = sharedStatic();
    return 0;
}
#endif

static long staticFromThreads()            // the threads race into the initializer of one static local
{
#ifdef _WIN32
    HANDLE h[4];
    for (std::size_t i = 0; i < 4; ++i) h[i] = CreateThread(0, 0, callShared, reinterpret_cast<LPVOID>(i), 0, 0);
    WaitForMultipleObjects(4, h, TRUE, INFINITE);
    for (int i = 0; i < 4; ++i) CloseHandle(h[i]);
#else
    for (int i = 0; i < 4; ++i) g_shared[i] = sharedStatic();   // no threads on this host: the value is still checked
#endif
    for (int i = 0; i < 4; ++i) if (g_shared[i] != g_shared[0]) return -g_shared[i];
    return g_shared[0];
}
static long floatsOnStack()               // the double arguments leave bits in the upper halves of the stack slots
{
    double d = stackDoubles(1e300, -1e300, 3.25e-300, 7.5, 1.0 / 3.0, -2.0 / 3.0, 1.0 / 7.0);
    float f = stackFloats(0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f);
    return static_cast<long>(f * 1000) + (d > 7.3 && d < 7.31 ? 1 : 0);
}

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
    check("carriedCounter()", carried(), 20914949);
    check("sharedStatic() in 4 threads", staticFromThreads(), 421);
    check("nestedStatics()", nestedStatics(), 1005);
    check("wideConstants()", wideConstants(), 1415111);
    check("stackFloats()", floatsOnStack(), 70001);
    check("exitOrder() before exit", exitOrder(), 99);   // nothing destroyed yet; the order is printed after main
    std::printf(g_failures ? "%d FAILED\n" : "ALL OK\n", g_failures);
    return g_failures ? 1 : 0;
}
