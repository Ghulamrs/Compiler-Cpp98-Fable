// retaddr.cpp - _AddressOfReturnAddress (row P1 stage 2, docs/design-selfhost.md 4.2): in a function that would be
// frameless without it and in one with a frame, the returned address holds an address inside the calling function
// (RtlLookupFunctionEntry gives the caller's entry), and it plus 8 is the caller's stack pointer at the call, which the
// caller captured with RtlCaptureContext. What it prints must equal what cl's build prints (noinline: cl's intrinsic
// names the enclosing function's slot after inlining; cppparse never inlines a function that calls it).
#include <stdio.h>
#include <windows.h>
#include <intrin.h>

static int g_sink;

// nothing in memory and no call: frameless, were it not for the intrinsic
__declspec(noinline) void* leafSlot()
{
    return _AddressOfReturnAddress();
}

// a frame of its own: calls and locals in memory
__declspec(noinline) void* framedSlot(int n)
{
    volatile int buffer[16];
    for (int i = 0; i < 16; ++i) buffer[i] = n + i;
    char text[32];
    sprintf(text, "%d", buffer[n & 15]);
    g_sink += text[0];
    return _AddressOfReturnAddress();
}

static void report(const char* what, void* slot, DWORD64 callerRsp, DWORD64 caller)
{
    const DWORD64 ret = *(DWORD64*)slot;
    DWORD64 base = 0;
    PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(ret, &base, 0);
    const int inCaller = rf != 0 && base + rf->BeginAddress == caller && ret < base + rf->EndAddress;
    const int atCall = (DWORD64)slot + 8 == callerRsp;
    printf("%s: return address in the caller %s, slot + 8 is the caller's stack pointer %s\n", what, inCaller ? "yes" : "NO", atCall ? "yes" : "NO");
}

__declspec(noinline) static void callLeaf()
{
    CONTEXT here;
    RtlCaptureContext(&here);
    void* slot = leafSlot();
    report("frameless candidate", slot, here.Rsp, (DWORD64)&callLeaf);
}

__declspec(noinline) static void callFramed()
{
    CONTEXT here;
    RtlCaptureContext(&here);
    void* slot = framedSlot(3);
    report("framed", slot, here.Rsp, (DWORD64)&callFramed);
}

int main()
{
    callLeaf();
    callFramed();
    return 0;
}
