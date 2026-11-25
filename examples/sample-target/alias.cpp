// alias.cpp - the runtime's /alternatename directives (row P1 stage 2, docs/design-selfhost.md 3.6, probe n1 of 1.3):
// linked with cppparse's object of the runtime, the three handler names the runtime compares with are a library's
// where the C++ runtime underneath defines them (Microsoft's) and the runtime's own cppparse_no_handler where it does
// not (GRIDE's), under either linker
#include <stdio.h>

extern "C" {
    int cppparse_no_handler(void*, void*, void*, void*);
    int __CxxFrameHandler3(void*, void*, void*, void*);
    int __CxxFrameHandler4(void*, void*, void*, void*);
    int __GSHandlerCheck_EH(void*, void*, void*, void*);
    int __GSHandlerCheck_EH4(void*, void*, void*, void*);
    long cppparse_interlocked_increment(long volatile* value);   // something of the runtime, so that its object is linked
}

typedef int (*Handler)(void*, void*, void*, void*);

static const char* whose(Handler h)
{
    return (unsigned long long)h == (unsigned long long)&cppparse_no_handler ? "the runtime's cppparse_no_handler" : "a library's";
}

int main()
{
    long volatile n = 0;
    cppparse_interlocked_increment(&n);
    printf("__CxxFrameHandler3: %s\n", whose(&__CxxFrameHandler3));
    printf("__CxxFrameHandler4: %s\n", whose(&__CxxFrameHandler4));
    printf("__GSHandlerCheck_EH: %s\n", whose(&__GSHandlerCheck_EH));
    printf("__GSHandlerCheck_EH4: %s\n", whose(&__GSHandlerCheck_EH4));
    return n == 1 ? 0 : 1;
}
