// dbgvars.cpp - the driver of the value harness (row M10, docs/design-codeview.md section 6), built by cl, dbghelp.
//
//   dbgvars <program.exe> [-places <file>]
//
// Runs the program (sample-dbg\vars.cpp with varsck.cpp) under the Win32 debug API with a breakpoint on checkpoint().
// At each checkpoint it reads the caller's text (see vars.cpp), walks the stack (StackWalk64) and asks dbghelp for the
// variables in scope (SymSetContext, SymEnumSymbols) in the caller's frame and in every frame above it that announced
// a text (ckPush), reads each variable with a place from the frame's registers and memory and compares it with the
// text; then it lets the checkpoint return, stops at the first address of the caller's next line and compares the
// caller's variables there again. What is wrong:
//   - a declared parameter missing, or the parameters not in the text's order;
//   - a variable of the text that is not listed, that has no place where the text gives a value, that has a place
//     where the text says it has none, or whose value differs;
//   - a variable with a place that the text does not name (a reader would show a value nobody checked).
// A record without a place whose name the text does not give is a block's variable out of its range: noted, not wrong.
// -places writes, for each checkpoint's caller, the address (relative to the image) and every variable with its place,
// in the form diavars.exe prints, so that DIA's reading can be compared.
// Output: one line per checkpoint and per failure, then "dbgvars: <n> checkpoints, <v> values compared, <f> failures";
// exit code 0 only without failures.
#define _NO_CVCONST_H           // dbghelp.h then declares the symbol tags itself (they are the DIA SDK's cvconst.h's)
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#pragma comment(lib, "dbghelp.lib")

namespace {

// cvconst.h's BasicType, as far as it is asked for here
enum { btChar = 2, btWChar = 3, btInt = 6, btUInt = 7, btFloat = 8, btBool = 10, btLong = 13, btULong = 14 };

struct Expect { std::string name, member; bool unplaced; std::string value; };
struct Text { std::string fn; std::vector<std::string> params; std::vector<Expect> vars; bool ok; };

Text parseText(const std::string& s)
{
    Text t; t.ok = true;
    size_t at = 0;
    while (at <= s.size()) {
        size_t semi = s.find(';', at);
        if (semi == std::string::npos) semi = s.size();
        const std::string item = s.substr(at, semi - at);
        at = semi + 1;
        if (item.empty()) { if (semi == s.size()) break; continue; }
        const size_t eq = item.find('=');
        if (item.compare(0, 3, "fn=") == 0) t.fn = item.substr(3);
        else if (item.compare(0, 2, "P=") == 0) {
            const std::string list = item.substr(2);
            size_t p = 0;
            while (p < list.size()) { size_t c = list.find(',', p); if (c == std::string::npos) c = list.size(); if (c > p) t.params.push_back(list.substr(p, c - p)); p = c + 1; }
        }
        else if (item[0] == '-') { Expect e; e.name = item.substr(1); e.unplaced = true; t.vars.push_back(e); }
        else if (item[0] == '~') { Expect e; e.name = item.substr(1); e.unplaced = false; e.value = "~"; t.vars.push_back(e); }
        else if (eq != std::string::npos) {
            Expect e; e.unplaced = false; e.value = item.substr(eq + 1);
            std::string n = item.substr(0, eq);
            size_t dot = n.find("->"), len = 2;
            if (dot == std::string::npos) { dot = n.find('.'); len = 1; }
            if (dot != std::string::npos) { e.member = n.substr(dot + len); n = n.substr(0, dot); }
            e.name = n;
            t.vars.push_back(e);
        }
        else t.ok = false;
        if (semi == s.size()) break;
    }
    return t;
}

HANDLE g_proc = 0;
DWORD64 g_base = 0;
int g_failures = 0, g_compared = 0;
FILE* g_places = 0;

void fail(const std::string& where, const std::string& what)
{
    ++g_failures;
    printf("  FAIL %s: %s\n", where.c_str(), what.c_str());
}

bool readMem(DWORD64 addr, void* out, size_t n)
{
    SIZE_T got = 0;
    return ReadProcessMemory(g_proc, reinterpret_cast<LPCVOID>(addr), out, n, &got) && got == n;
}

std::string readString(DWORD64 addr)
{
    std::string s;
    char c = 0;
    for (int i = 0; i < 4096 && readMem(addr + static_cast<DWORD64>(i), &c, 1) && c; ++i) s += c;
    return s;
}

struct Var {
    std::string name;
    bool param, placed, isStatic;
    ULONG reg; LONG64 offset; DWORD64 address;   // a register and an offset, or a static's address
    ULONG type;
};

BOOL CALLBACK collect(PSYMBOL_INFO s, ULONG, PVOID user)
{
    std::vector<Var>& out = *static_cast<std::vector<Var>*>(user);
    Var v;
    v.name = std::string(s->Name, s->NameLen);
    v.param = (s->Flags & SYMFLAG_PARAMETER) != 0;
    v.reg = s->Register; v.offset = static_cast<LONG64>(s->Address); v.address = s->Address; v.type = s->TypeIndex;
    v.isStatic = false;
    v.placed = (s->Flags & (SYMFLAG_REGREL | SYMFLAG_FRAMEREL | SYMFLAG_REGISTER)) != 0;
    if (s->Flags & SYMFLAG_FRAMEREL) v.reg = 334;   // (our frames: S_FRAMEPROC names RBP)
    if (!v.placed && s->Address != 0 && !(s->Flags & SYMFLAG_NULL)) { v.placed = true; v.isStatic = true; }
    if (s->Tag == SymTagData) out.push_back(v);
    return TRUE;
}

DWORD64 regValue(const CONTEXT& c, ULONG reg)
{
    switch (reg) {
    case 334: return c.Rbp;
    case 335: return c.Rsp;
    case 328: return c.Rax; case 329: return c.Rbx; case 330: return c.Rcx; case 331: return c.Rdx;
    case 332: return c.Rsi; case 333: return c.Rdi;
    case 336: return c.R8; case 337: return c.R9; case 338: return c.R10; case 339: return c.R11;
    case 340: return c.R12; case 341: return c.R13; case 342: return c.R14; case 343: return c.R15;
    }
    return 0;
}

std::string placeText(const Var& v)
{
    if (!v.placed) return "none";
    if (v.isStatic) return "static";
    char b[64];
    const char* r = v.reg == 334 ? "rbp" : v.reg == 335 ? "rsp" : "reg";
    sprintf_s(b, "%s%s%lld", r, v.offset < 0 ? "-" : "+", v.offset < 0 ? -v.offset : v.offset);
    return b;
}

DWORD tagOf(ULONG type) { DWORD t = 0; SymGetTypeInfo(g_proc, g_base, type, TI_GET_SYMTAG, &t); return t; }
ULONG64 lengthOf(ULONG type) { ULONG64 n = 0; SymGetTypeInfo(g_proc, g_base, type, TI_GET_LENGTH, &n); return n; }
ULONG innerOf(ULONG type) { DWORD t = 0; SymGetTypeInfo(g_proc, g_base, type, TI_GET_TYPEID, &t); return t; }

// the address and type of member 'name' of the class 'type' at 'addr' (through its non-virtual bases); false: none
bool member(ULONG type, DWORD64 addr, const std::string& name, DWORD64& at, ULONG& mtype, int depth = 0)
{
    DWORD count = 0;
    if (depth > 8 || !SymGetTypeInfo(g_proc, g_base, type, TI_GET_CHILDRENCOUNT, &count) || count == 0) return false;
    std::vector<char> buf(sizeof(TI_FINDCHILDREN_PARAMS) + count * sizeof(ULONG));
    TI_FINDCHILDREN_PARAMS* kids = reinterpret_cast<TI_FINDCHILDREN_PARAMS*>(&buf[0]);
    kids->Count = count; kids->Start = 0;
    if (!SymGetTypeInfo(g_proc, g_base, type, TI_FINDCHILDREN, kids)) return false;
    for (DWORD i = 0; i < count; ++i) {
        const ULONG k = kids->ChildId[i];
        const DWORD tag = tagOf(k);
        DWORD off = 0; SymGetTypeInfo(g_proc, g_base, k, TI_GET_OFFSET, &off);
        if (tag == SymTagData) {
            WCHAR* wn = 0;
            if (!SymGetTypeInfo(g_proc, g_base, k, TI_GET_SYMNAME, &wn) || !wn) continue;
            std::string n; for (WCHAR* p = wn; *p; ++p) n += static_cast<char>(*p);
            LocalFree(wn);
            if (n == name) { at = addr + off; mtype = innerOf(k); return true; }
        }
        else if (tag == SymTagBaseClass) {
            BOOL isVirtual = FALSE; SymGetTypeInfo(g_proc, g_base, k, TI_GET_VIRTUALBASECLASS, &isVirtual);
            if (!isVirtual && member(innerOf(k), addr + off, name, at, mtype, depth + 1)) return true;
        }
    }
    return false;
}

// the value at addr of 'type' as text (a reference read through); false: not a value that can be compared
bool valueText(ULONG type, DWORD64 addr, std::string& out, int depth = 0)
{
    const DWORD tag = tagOf(type);
    if (tag == SymTagTypedef && depth < 4) return valueText(innerOf(type), addr, out, depth + 1);
    if (tag == SymTagPointerType) {
        DWORD64 p = 0;
        if (!readMem(addr, &p, 8)) return false;
        BOOL isRef = FALSE; SymGetTypeInfo(g_proc, g_base, type, TI_GET_IS_REFERENCE, &isRef);
        if (isRef && depth < 4) return valueText(innerOf(type), p, out, depth + 1);
        char b[32]; sprintf_s(b, "%llu", p); out = b; return true;
    }
    if (tag != SymTagBaseType && tag != SymTagEnum) return false;
    const ULONG64 n = lengthOf(type);
    unsigned char bytes[8] = { 0 };
    if (n == 0 || n > 8 || !readMem(addr, bytes, static_cast<size_t>(n))) return false;
    DWORD bt = 0; SymGetTypeInfo(g_proc, g_base, type, TI_GET_BASETYPE, &bt);
    char b[64];
    if (tag == SymTagBaseType && bt == btFloat) {
        double d = 0;
        if (n == 4) { float f; memcpy(&f, bytes, 4); d = f; } else memcpy(&d, bytes, 8);
        sprintf_s(b, "%.9g", d); out = b; return true;
    }
    const bool isSigned = tag == SymTagEnum || bt == btInt || bt == btLong || bt == btChar;
    unsigned long long u = 0;
    for (ULONG64 i = 0; i < n; ++i) u |= static_cast<unsigned long long>(bytes[i]) << (8 * i);
    if (isSigned && n < 8 && (u >> (8 * n - 1)) & 1) u |= ~0ULL << (8 * n);
    if (isSigned) sprintf_s(b, "%lld", static_cast<long long>(u)); else sprintf_s(b, "%llu", u);
    out = b;
    return true;
}

bool sameValue(const std::string& got, const std::string& want)
{
    if (got == want) return true;
    if (want.find('.') != std::string::npos || got.find('.') != std::string::npos || got.find('e') != std::string::npos) {
        const double a = atof(got.c_str()), b = atof(want.c_str());
        return fabs(a - b) <= 1e-6 * (fabs(b) > 1 ? fabs(b) : 1);
    }
    return false;
}

// the variables of one frame against its text
void compare(const Text& t, const CONTEXT& c, DWORD64 scopeAddr, const std::string& where, bool writePlaces)
{
    IMAGEHLP_STACK_FRAME f;
    memset(&f, 0, sizeof(f));
    f.InstructionOffset = scopeAddr;
    f.FrameOffset = c.Rbp;
    f.StackOffset = c.Rsp;
    SymSetContext(g_proc, &f, NULL);
    std::vector<Var> vars;
    SymEnumSymbols(g_proc, 0, NULL, collect, &vars);
    if (writePlaces && g_places) {
        fprintf(g_places, "@%llx %s\n", scopeAddr - g_base, t.fn.c_str());
        for (size_t i = 0; i < vars.size(); ++i) if (!vars[i].isStatic) fprintf(g_places, "  %s %s %s\n", vars[i].param ? "P" : "L", vars[i].name.c_str(), placeText(vars[i]).c_str());
    }
    // the parameters, in order
    std::vector<std::string> params;
    // (dbghelp lists "this" without SYMFLAG_PARAMETER, for cl's objects as well: it is the first parameter)
    for (size_t i = 0; i < vars.size(); ++i) if (vars[i].name == "this") params.push_back(vars[i].name);
    for (size_t i = 0; i < vars.size(); ++i) if (vars[i].param && vars[i].name != "this") params.push_back(vars[i].name);
    if (params != t.params) {
        std::string got, want;
        for (size_t i = 0; i < params.size(); ++i) got += (i ? "," : "") + params[i];
        for (size_t i = 0; i < t.params.size(); ++i) want += (i ? "," : "") + t.params[i];
        fail(where, "parameters [" + got + "], the source declares [" + want + "]");
    }
    std::map<std::string, bool> named;
    for (size_t e = 0; e < t.vars.size(); ++e) {
        const Expect& x = t.vars[e];
        named[x.name] = true;
        if (x.value == "~") continue;
        const Var* v = 0;
        bool listed = false;
        for (size_t i = 0; i < vars.size(); ++i) if (vars[i].name == x.name) { listed = true; if (vars[i].placed && !v) v = &vars[i]; }
        if (!listed) { fail(where, x.name + " is not listed"); continue; }
        if (x.unplaced) { if (v) fail(where, x.name + " has a place (" + placeText(*v) + "), the function never stores it"); continue; }
        if (!v) { fail(where, x.name + " is listed without a place"); continue; }
        if (x.value == "?") continue;
        DWORD64 addr = v->isStatic ? v->address : regValue(c, v->reg) + static_cast<DWORD64>(v->offset);
        ULONG type = v->type;
        if (!x.member.empty()) {
            // through a reference or a pointer to the class
            DWORD tag = tagOf(type);
            if (tag == SymTagPointerType) { DWORD64 p = 0; readMem(addr, &p, 8); addr = p; type = innerOf(type); }
            if (!member(type, addr, x.member, addr, type)) { fail(where, x.name + " has no member " + x.member); continue; }
        }
        std::string got;
        if (!valueText(type, addr, got)) { fail(where, x.name + (x.member.empty() ? "" : "." + x.member) + ": a value of a type the driver does not read"); continue; }
        ++g_compared;
        if (!sameValue(got, x.value)) fail(where, x.name + (x.member.empty() ? "" : "." + x.member) + " = " + got + ", the program says " + x.value);
    }
    for (size_t i = 0; i < vars.size(); ++i) {
        if (named.count(vars[i].name)) continue;
        if (vars[i].placed) fail(where, vars[i].name + " has a place (" + placeText(vars[i]) + ") and the program does not name it");
    }
}

std::string functionAt(DWORD64 addr)
{
    char buf[sizeof(SYMBOL_INFO) + 512];
    PSYMBOL_INFO s = reinterpret_cast<PSYMBOL_INFO>(buf);
    memset(buf, 0, sizeof(buf));
    s->SizeOfStruct = sizeof(SYMBOL_INFO); s->MaxNameLen = 500;
    DWORD64 disp = 0;
    if (!SymFromAddr(g_proc, addr, &disp, s)) return "?";
    return std::string(s->Name, s->NameLen);
}

DWORD64 symbolAddress(const char* name)
{
    char buf[sizeof(SYMBOL_INFO) + 512];
    PSYMBOL_INFO s = reinterpret_cast<PSYMBOL_INFO>(buf);
    memset(buf, 0, sizeof(buf));
    s->SizeOfStruct = sizeof(SYMBOL_INFO); s->MaxNameLen = 500;
    return SymFromName(g_proc, name, s) ? s->Address : 0;
}

struct Breakpoint { DWORD64 at; unsigned char saved; bool set; };

bool setBp(Breakpoint& b, DWORD64 at)
{
    b.at = at; b.set = false;
    if (!readMem(at, &b.saved, 1)) return false;
    const unsigned char cc = 0xCC;
    SIZE_T n = 0;
    if (!WriteProcessMemory(g_proc, reinterpret_cast<LPVOID>(at), &cc, 1, &n)) return false;
    FlushInstructionCache(g_proc, reinterpret_cast<LPCVOID>(at), 1);
    b.set = true;
    return true;
}

void clearBp(Breakpoint& b)
{
    if (!b.set) return;
    SIZE_T n = 0;
    WriteProcessMemory(g_proc, reinterpret_cast<LPVOID>(b.at), &b.saved, 1, &n);
    FlushInstructionCache(g_proc, reinterpret_cast<LPCVOID>(b.at), 1);
    b.set = false;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: dbgvars <program.exe> [-places <file>] | -scope <program.exe> <rva>...\n"); return 2; }
    if (strcmp(argv[1], "-scope") == 0 && argc >= 4) {
        // without running the program: what dbghelp lists at each address (relative to the image)
        g_proc = GetCurrentProcess();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
        SymInitialize(g_proc, NULL, FALSE);
        g_base = SymLoadModuleEx(g_proc, NULL, argv[2], NULL, 0x10000000, 0, NULL, 0);
        if (!g_base) { fprintf(stderr, "dbgvars: no symbols for %s\n", argv[2]); return 1; }
        for (int a = 3; a < argc; ++a) {
            const DWORD64 at = g_base + strtoull(argv[a], 0, 16);
            IMAGEHLP_STACK_FRAME f; memset(&f, 0, sizeof(f)); f.InstructionOffset = at;
            SymSetContext(g_proc, &f, NULL);
            std::vector<Var> vars;
            SymEnumSymbols(g_proc, 0, NULL, collect, &vars);
            printf("@%llx %s\n", at - g_base, functionAt(at).c_str());
            for (size_t i = 0; i < vars.size(); ++i) printf("  %s %s %s\n", vars[i].param ? "P" : "L", vars[i].name.c_str(), placeText(vars[i]).c_str());
        }
        return 0;
    }
    for (int a = 2; a + 1 < argc; ++a) if (strcmp(argv[a], "-places") == 0) fopen_s(&g_places, argv[a + 1], "w");
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    std::string cmd = std::string("\"") + argv[1] + "\"";
    std::vector<char> line(cmd.begin(), cmd.end()); line.push_back(0);
    if (!CreateProcessA(argv[1], &line[0], NULL, NULL, FALSE, DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi)) { fprintf(stderr, "dbgvars: cannot start %s\n", argv[1]); return 1; }
    g_proc = pi.hProcess;
    std::map<DWORD, HANDLE> threads;
    Breakpoint ck = { 0, 0, false }, ret = { 0, 0, false }, next = { 0, 0, false };
    DWORD64 ckStack = 0, ckDepth = 0;
    int checkpoints = 0, nextLines = 0;
    Text pending; std::string pendingWhere;
    bool stepping = false, done = false;
    DWORD exitCode = 0;
    while (!done) {
        DEBUG_EVENT ev;
        if (!WaitForDebugEvent(&ev, INFINITE)) break;
        DWORD status = DBG_CONTINUE;
        switch (ev.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT: {
            threads[ev.dwThreadId] = ev.u.CreateProcessInfo.hThread;
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
            SymInitialize(g_proc, NULL, FALSE);
            g_base = SymLoadModuleEx(g_proc, ev.u.CreateProcessInfo.hFile, argv[1], NULL, reinterpret_cast<DWORD64>(ev.u.CreateProcessInfo.lpBaseOfImage), 0, NULL, 0);
            const DWORD64 at = symbolAddress("checkpoint");
            ckStack = symbolAddress("g_ckStack"); ckDepth = symbolAddress("g_ckDepth");
            if (!g_base || !at || !ckStack || !ckDepth || !setBp(ck, at)) { fprintf(stderr, "dbgvars: no symbols for checkpoint in %s\n", argv[1]); TerminateProcess(g_proc, 1); }
            if (ev.u.CreateProcessInfo.hFile) CloseHandle(ev.u.CreateProcessInfo.hFile);
            break;
        }
        case CREATE_THREAD_DEBUG_EVENT: threads[ev.dwThreadId] = ev.u.CreateThread.hThread; break;
        case EXIT_THREAD_DEBUG_EVENT: threads.erase(ev.dwThreadId); break;
        case LOAD_DLL_DEBUG_EVENT:
            // the system's modules too: the stack is walked through them (an exception's dispatch) by their unwind tables
            SymLoadModuleEx(g_proc, ev.u.LoadDll.hFile, NULL, NULL, reinterpret_cast<DWORD64>(ev.u.LoadDll.lpBaseOfDll), 0, NULL, 0);
            if (ev.u.LoadDll.hFile) CloseHandle(ev.u.LoadDll.hFile);
            break;
        case EXIT_PROCESS_DEBUG_EVENT: exitCode = ev.u.ExitProcess.dwExitCode; done = true; break;
        case EXCEPTION_DEBUG_EVENT: {
            const EXCEPTION_RECORD& er = ev.u.Exception.ExceptionRecord;
            HANDLE th = threads[ev.dwThreadId];
            const DWORD64 at = reinterpret_cast<DWORD64>(er.ExceptionAddress);
            if (er.ExceptionCode == EXCEPTION_SINGLE_STEP && stepping) {
                stepping = false;
                setBp(ck, ck.at);
                break;
            }
            if (er.ExceptionCode != EXCEPTION_BREAKPOINT) { status = DBG_EXCEPTION_NOT_HANDLED; break; }
            CONTEXT c; memset(&c, 0, sizeof(c)); c.ContextFlags = CONTEXT_FULL;
            GetThreadContext(th, &c);
            if (ck.set && at == ck.at) {
                ++checkpoints;
                if (next.set) { fail(pendingWhere, "the next line's first address was not reached before the next checkpoint"); clearBp(next); }
                clearBp(ret);
                const Text t = parseText(readString(c.Rcx));
                char where[64]; sprintf_s(where, "checkpoint %d (%s)", checkpoints, t.fn.c_str());
                if (!t.ok) fail(where, "a text the driver cannot read");
                // the frames: 0 the checkpoint, 1 its caller, then the ones above
                std::vector<std::string> above;
                int depth = 0; readMem(ckDepth, &depth, 4);
                for (int k = depth - 1; k >= 0; --k) { DWORD64 p = 0; readMem(ckStack + 8 * static_cast<DWORD64>(k), &p, 8); above.push_back(readString(p)); }
                size_t nextAbove = 0;
                CONTEXT walk = c;
                STACKFRAME64 sf; memset(&sf, 0, sizeof(sf));
                sf.AddrPC.Offset = c.Rip; sf.AddrPC.Mode = AddrModeFlat;
                sf.AddrFrame.Offset = c.Rsp; sf.AddrFrame.Mode = AddrModeFlat;
                sf.AddrStack.Offset = c.Rsp; sf.AddrStack.Mode = AddrModeFlat;
                DWORD64 returnTo = 0;
                for (int frame = 0; frame < 64; ++frame) {
                    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, g_proc, th, &sf, &walk, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
                    if (sf.AddrPC.Offset == 0) break;
                    if (frame == 0) continue;
                    const DWORD64 scope = sf.AddrPC.Offset - 1;   // inside the call that left the frame
                    const std::string fn = functionAt(scope);
                    if (frame == 1) {
                        returnTo = sf.AddrPC.Offset;
                        if (fn != t.fn) fail(where, "the caller is " + fn);
                        compare(t, walk, scope, where, true);
                        continue;
                    }
                    if (nextAbove < above.size()) {
                        const Text up = parseText(above[nextAbove]);
                        if (up.fn == fn) { compare(up, walk, scope, std::string(where) + ", the frame of " + fn + " above it", false); ++nextAbove; }
                    }
                }
                for (size_t k = nextAbove; k < above.size(); ++k) fail(where, "no frame above it for the text " + above[k].substr(0, 40));
                printf("checkpoint %d %s: %d frame(s) above checked\n", checkpoints, t.fn.c_str(), static_cast<int>(nextAbove));
                pending = t; pendingWhere = std::string(where) + ", at the next line";
                if (returnTo) setBp(ret, returnTo);
                // continue: the checkpoint's first byte back, one step, the breakpoint again
                clearBp(ck); ck.set = false;
                c.Rip = ck.at; c.EFlags |= 0x100;
                SetThreadContext(th, &c);
                stepping = true;
                break;
            }
            if (ret.set && at == ret.at) {
                // back in the caller: the first address of the next line, in the same function
                clearBp(ret);
                c.Rip = at; SetThreadContext(th, &c);
                IMAGEHLP_LINE64 ln; memset(&ln, 0, sizeof(ln)); ln.SizeOfStruct = sizeof(ln);
                DWORD disp = 0;
                DWORD64 target = 0;
                // (the call is the last instruction of its line: the return address is often the next line's first)
                IMAGEHLP_LINE64 callLine; memset(&callLine, 0, sizeof(callLine)); callLine.SizeOfStruct = sizeof(callLine);
                DWORD callDisp = 0;
                const DWORD callNumber = SymGetLineFromAddr64(g_proc, at - 1, &callDisp, &callLine) ? callLine.LineNumber : 0;
                if (SymGetLineFromAddr64(g_proc, at, &disp, &ln)) {
                    if (ln.Address == at && ln.LineNumber != callNumber) target = at;
                    else while (SymGetLineNext64(g_proc, &ln)) if (ln.Address > at) { target = ln.Address; break; }
                }
                if (!target || functionAt(target) != functionAt(at)) fail(pendingWhere, "no next line in the function");
                else {
                    char rv[48]; sprintf_s(rv, " (line %lu, %llx)", ln.LineNumber, target - g_base); pendingWhere += rv;
                    if (target == at) { ++nextLines; compare(pending, c, at, pendingWhere, false); }
                    else setBp(next, target);
                }
                break;
            }
            if (next.set && at == next.at) {
                clearBp(next);
                c.Rip = at; SetThreadContext(th, &c);
                ++nextLines;
                compare(pending, c, at, pendingWhere, false);
                break;
            }
            break;   // the loader's breakpoint
        }
        default: break;
        }
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status);
    }
    if (g_places) fclose(g_places);
    printf("dbgvars: %d checkpoints, %d next lines, %d values compared, %d failures; the program exited with %lu\n", checkpoints, nextLines, g_compared, g_failures, exitCode);
    return g_failures == 0 && checkpoints > 0 && exitCode == 0 ? 0 : 1;
}
