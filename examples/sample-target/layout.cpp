// layout.cpp - the unwind structures' layout (row P1 stage 2, docs/design-selfhost.md 4.3): compiled by cl against
// the SDK's <windows.h> and by cppparse against platform\windows.h, it must print the same lines
#include <stddef.h>
#include <stdio.h>
#include <windows.h>

#define SHOW(text, value) printf("%-36s %u\n", text, (unsigned)(value))

int main()
{
    SHOW("sizeof(M128A)", sizeof(M128A));
    SHOW("sizeof(CONTEXT)", sizeof(CONTEXT));
    SHOW("CONTEXT.ContextFlags", offsetof(CONTEXT, ContextFlags));
    SHOW("CONTEXT.Rbx", offsetof(CONTEXT, Rbx));
    SHOW("CONTEXT.Rsp", offsetof(CONTEXT, Rsp));
    SHOW("CONTEXT.Rbp", offsetof(CONTEXT, Rbp));
    SHOW("CONTEXT.Rsi", offsetof(CONTEXT, Rsi));
    SHOW("CONTEXT.Rdi", offsetof(CONTEXT, Rdi));
    SHOW("CONTEXT.R12", offsetof(CONTEXT, R12));
    SHOW("CONTEXT.R13", offsetof(CONTEXT, R13));
    SHOW("CONTEXT.R14", offsetof(CONTEXT, R14));
    SHOW("CONTEXT.R15", offsetof(CONTEXT, R15));
    SHOW("CONTEXT.Rip", offsetof(CONTEXT, Rip));
    SHOW("CONTEXT.Xmm6", offsetof(CONTEXT, Xmm6));
    SHOW("CONTEXT.Xmm15", offsetof(CONTEXT, Xmm15));
    SHOW("CONTEXT.VectorRegister", offsetof(CONTEXT, VectorRegister));
    SHOW("CONTEXT.LastExceptionFromRip", offsetof(CONTEXT, LastExceptionFromRip));
    SHOW("sizeof(EXCEPTION_RECORD)", sizeof(EXCEPTION_RECORD));
    SHOW("EXCEPTION_RECORD.ExceptionAddress", offsetof(EXCEPTION_RECORD, ExceptionAddress));
    SHOW("EXCEPTION_RECORD.ExceptionInformation", offsetof(EXCEPTION_RECORD, ExceptionInformation));
    SHOW("sizeof(RUNTIME_FUNCTION)", sizeof(RUNTIME_FUNCTION));
    SHOW("sizeof(DISPATCHER_CONTEXT)", sizeof(DISPATCHER_CONTEXT));
    SHOW("DISPATCHER_CONTEXT.FunctionEntry", offsetof(DISPATCHER_CONTEXT, FunctionEntry));
    SHOW("DISPATCHER_CONTEXT.HandlerData", offsetof(DISPATCHER_CONTEXT, HandlerData));
    SHOW("DISPATCHER_CONTEXT.ScopeIndex", offsetof(DISPATCHER_CONTEXT, ScopeIndex));
    SHOW("UNW_FLAG_EHANDLER|UNW_FLAG_UHANDLER", UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER);
    return 0;
}
