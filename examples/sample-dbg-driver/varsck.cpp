// varsck.cpp - the value harness's checkpoint (row M10, sample-dbg\vars.cpp): built by cl, linked into the program.
// checkpoint() does nothing a program would notice; the driver (dbgvars.cpp) stops at it. ckPush and ckPop keep the
// texts of the frames above a checkpoint in g_ckStack, where the driver reads them.
extern "C" {

volatile int g_ckCount = 0;
const char* g_ckStack[64];
int g_ckDepth = 0;

__declspec(noinline) void checkpoint(const char* text)
{
    if (text) ++g_ckCount;
}

void ckPush(const char* text)
{
    if (g_ckDepth < 64) g_ckStack[g_ckDepth] = text;
    ++g_ckDepth;
}

void ckPop()
{
    if (g_ckDepth > 0) --g_ckDepth;
}

}
