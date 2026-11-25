// diavars.cpp - the variables a program's PDB gives for code addresses, read through DIA (msdia140.dll, Visual Studio's
// reader), for the value harness of row M10 (docs/design-codeview.md section 6). Built by cl against the DIA SDK of
// Visual Studio; the DLL is loaded from the SDK without registration (NoRegCoCreate).
//
//   diavars <msdia140.dll> <program.exe> -fn <name>...        every variable record of the function, in order, with
//                                                             DIA's kind (P: parameter, L: local) and place
//   diavars <msdia140.dll> <program.exe> -rva <hex>...        the variables in scope at each address (relative to the
//                                                             image), innermost block last, with their places
// A place is "rbp-24", "rsp+8" (relative to a register), "reg:<n>" (in a register), "static" or "none" (listed, no
// place there). Output lines: "@<rva> <function>" then "  <P|L> <name> <place>".
#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "dia2.h"
#include "diacreate.h"

static std::string narrow(BSTR b)
{
    std::string s;
    if (!b) return s;
    for (const wchar_t* p = b; *p; ++p) s += static_cast<char>(*p < 128 ? *p : '?');
    return s;
}

static std::string regName(DWORD r)
{
    switch (r) {
    case 334: return "rbp";
    case 335: return "rsp";
    case 328: return "rax";
    case 329: return "rbx";
    case 330: return "rcx";
    case 331: return "rdx";
    case 332: return "rsi";
    case 333: return "rdi";
    }
    char b[16]; sprintf_s(b, "reg%lu", r); return b;
}

static std::string place(IDiaSymbol* s)
{
    DWORD loc = 0;
    if (s->get_locationType(&loc) != S_OK) return "none";
    char b[64];
    if (loc == LocIsRegRel) {
        DWORD reg = 0; LONG off = 0;
        s->get_registerId(&reg); s->get_offset(&off);
        sprintf_s(b, "%s%s%ld", regName(reg).c_str(), off < 0 ? "-" : "+", off < 0 ? -off : off);
        return b;
    }
    if (loc == LocIsEnregistered) { DWORD reg = 0; s->get_registerId(&reg); return "reg:" + regName(reg); }
    if (loc == LocIsStatic) return "static";
    if (loc == LocIsNull) return "none";
    sprintf_s(b, "loc%lu", loc);
    return b;
}

static void printData(IDiaSymbol* s)
{
    BSTR name = 0; s->get_name(&name);
    DWORD kind = 0; s->get_dataKind(&kind);
    const char* k = kind == DataIsParam || kind == DataIsObjectPtr ? "P" : kind == DataIsStaticLocal ? "S" : "L";
    printf("  %s %s %s\n", k, narrow(name).c_str(), place(s).c_str());
    SysFreeString(name);
}

// the data children of 'scope' valid at rva, then those of the block valid there, recursively
static void inScope(IDiaSymbol* scope, DWORD rva)
{
    IDiaEnumSymbols* e = 0;
    if (scope->findChildrenExByRVA(SymTagNull, NULL, nsNone, rva, &e) != S_OK || !e) return;
    std::vector<IDiaSymbol*> blocks;
    IDiaSymbol* c = 0; ULONG got = 0;
    while (e->Next(1, &c, &got) == S_OK && got == 1) {
        DWORD tag = 0; c->get_symTag(&tag);
        if (tag == SymTagData) { printData(c); c->Release(); }
        else if (tag == SymTagBlock) {
            // (DIA returns every block of the scope: the one that holds the address is the one in scope)
            DWORD start = 0; ULONGLONG length = 0;
            c->get_relativeVirtualAddress(&start); c->get_length(&length);
            if (rva >= start && rva - start < length) blocks.push_back(c); else c->Release();
        }
        else c->Release();
    }
    e->Release();
    for (size_t i = 0; i < blocks.size(); ++i) { inScope(blocks[i], rva); blocks[i]->Release(); }
}

static void allRecords(IDiaSymbol* scope, int depth)
{
    IDiaEnumSymbols* e = 0;
    if (scope->findChildren(SymTagNull, NULL, nsNone, &e) != S_OK || !e) return;
    IDiaSymbol* c = 0; ULONG got = 0;
    while (e->Next(1, &c, &got) == S_OK && got == 1) {
        DWORD tag = 0; c->get_symTag(&tag);
        if (tag == SymTagData) { printf("%*s", depth * 2, ""); printData(c); }
        else if (tag == SymTagBlock) { printf("%*s  {\n", depth * 2, ""); allRecords(c, depth + 1); printf("%*s  }\n", depth * 2, ""); }
        c->Release();
    }
    e->Release();
}

int main(int argc, char** argv)
{
    if (argc < 5) { fprintf(stderr, "usage: diavars <msdia140.dll> <program.exe> -fn <name>... | -rva <hex>...\n"); return 2; }
    CoInitialize(NULL);
    wchar_t dll[MAX_PATH], exe[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, dll, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, exe, MAX_PATH);
    IDiaDataSource* src = 0;
    HRESULT hr = NoRegCoCreate(dll, __uuidof(DiaSource), __uuidof(IDiaDataSource), (void**)&src);
    if (FAILED(hr)) { fprintf(stderr, "diavars: msdia140.dll not loaded (%08lx)\n", (unsigned long)hr); return 1; }
    hr = src->loadDataForExe(exe, NULL, NULL);
    if (FAILED(hr)) { fprintf(stderr, "diavars: no PDB for %s (%08lx)\n", argv[2], (unsigned long)hr); return 1; }
    IDiaSession* session = 0;
    if (FAILED(src->openSession(&session))) { fprintf(stderr, "diavars: no session\n"); return 1; }
    IDiaSymbol* global = 0;
    session->get_globalScope(&global);
    const bool byName = strcmp(argv[3], "-fn") == 0;
    for (int a = 4; a < argc; ++a) {
        if (byName) {
            wchar_t wn[512]; MultiByteToWideChar(CP_ACP, 0, argv[a], -1, wn, 512);
            IDiaEnumSymbols* e = 0;
            if (global->findChildren(SymTagFunction, wn, nsCaseSensitive, &e) != S_OK || !e) { printf("@%s not found\n", argv[a]); continue; }
            IDiaSymbol* fn = 0; ULONG got = 0;
            if (e->Next(1, &fn, &got) != S_OK || got != 1) { printf("@%s not found\n", argv[a]); e->Release(); continue; }
            DWORD frva = 0; ULONGLONG flen = 0; fn->get_relativeVirtualAddress(&frva); fn->get_length(&flen);
            printf("@%s rva %lx length %llx\n", argv[a], frva, flen);
            allRecords(fn, 0);
            fn->Release(); e->Release();
            continue;
        }
        const DWORD rva = strtoul(argv[a], 0, 16);
        IDiaSymbol* fn = 0;
        if (session->findSymbolByRVA(rva, SymTagFunction, &fn) != S_OK || !fn) { printf("@%lx no function\n", rva); continue; }
        BSTR name = 0; fn->get_name(&name);
        printf("@%lx %s\n", rva, narrow(name).c_str());
        SysFreeString(name);
        inScope(fn, rva);
        fn->Release();
    }
    global->Release(); session->Release(); src->Release();
    CoUninitialize();
    return 0;
}
