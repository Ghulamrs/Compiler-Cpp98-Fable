// diatypes.cpp - the member functions a program's PDB lists in its classes, read through DIA (msdia140.dll, Visual
// Studio's reader), for test 20 (row M10, docs/design-codeview.md stage 4). Built by cl against the DIA SDK of Visual
// Studio; the DLL is loaded from the SDK without registration (NoRegCoCreate). dbghelp, which dbgtypes.cpp reads
// through, does not tell a function's access, whether it is static, pure or introduces its vftable slot, nor what
// the compiler generated; DIA does.
//
//   diatypes <msdia140.dll> <program.exe> [-all] [-code]
// prints, for every class, struct and union of the PDB that has a size (each name once), one line per member function:
//     method <class> <name> <access> <kind> <type>
// access: public, protected, private. kind: plain, static, virtual (an override), intro@<n> (a virtual function
// that brings the vftable slot at offset n), pure, pure-intro@<n>. type: <result>(<parameters>) this=<type of this, or
// none> adjust=<this adjustment>, then " ctor" (a constructor), " ctor-vbases" (of a class with virtual bases), " udt" (a
// class result returned the C++ way); the types spelled as dbgtypes.cpp spells them, with "const " and "volatile "
// where DIA has them. What the compiler generated (cl: implicit constructors and operator=, __vecDelDtor, the
// thunk of a covariant override) is left out; -all keeps it, marked " generated". -code marks a function DIA has
// code for (" code").
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <set>
#include <string>
#include "dia2.h"
#include "diacreate.h"

static bool g_all, g_code;

static std::string narrow(BSTR b)
{
    std::string s;
    if (!b) return s;
    for (const wchar_t* p = b; *p; ++p) s += static_cast<char>(*p < 128 ? *p : '?');
    return s;
}

static std::string nameOf(IDiaSymbol* s)
{
    BSTR name = 0;
    if (s->get_name(&name) != S_OK) return std::string();
    const std::string out = narrow(name);
    SysFreeString(name);
    return out;
}

static std::string describe(IDiaSymbol* type, int depth);

// a function type: its result and parameters
static std::string signature(IDiaSymbol* fn, int depth)
{
    std::string s = "(";
    IDiaEnumSymbols* e = 0;
    if (fn->findChildren(SymTagFunctionArgType, NULL, nsNone, &e) == S_OK && e) {
        IDiaSymbol* a = 0; ULONG got = 0; bool first = true;
        while (e->Next(1, &a, &got) == S_OK && got == 1) {
            IDiaSymbol* at = 0;
            s += (first ? "" : ",") + (a->get_type(&at) == S_OK && at ? describe(at, depth + 1) : std::string("?"));
            if (at) at->Release();
            a->Release(); first = false;
        }
        e->Release();
    }
    IDiaSymbol* result = 0;
    const std::string r = fn->get_type(&result) == S_OK && result ? describe(result, depth + 1) : std::string("?");
    if (result) result->Release();
    return r + s + ")";
}

static std::string describe(IDiaSymbol* type, int depth)
{
    DWORD tag = 0;
    if (depth > 12 || type->get_symTag(&tag) != S_OK) return "?";
    BOOL isConst = FALSE, isVolatile = FALSE;
    type->get_constType(&isConst); type->get_volatileType(&isVolatile);
    const std::string cv = std::string(isConst ? "const " : "") + (isVolatile ? "volatile " : "");
    ULONGLONG length = 0; type->get_length(&length);
    char text[64];
    IDiaSymbol* inner = 0;
    std::string s;
    switch (tag) {
    case SymTagBaseType: { DWORD bt = 0; type->get_baseType(&bt); sprintf_s(text, "b%lu.%llu", bt, length); return cv + text; }
    case SymTagPointerType: {
        BOOL reference = FALSE; type->get_reference(&reference);
        s = type->get_type(&inner) == S_OK && inner ? describe(inner, depth + 1) : std::string("?");
        if (inner) inner->Release();
        return cv + (reference ? "ref(" : "ptr(") + s + ")";
    }
    case SymTagArrayType: {
        DWORD count = 0; type->get_count(&count);
        s = type->get_type(&inner) == S_OK && inner ? describe(inner, depth + 1) : std::string("?");
        if (inner) inner->Release();
        sprintf_s(text, "arr%lu(", count);
        return cv + text + s + ")";
    }
    case SymTagUDT: return cv + "udt:" + nameOf(type);
    case SymTagEnum: return cv + "enum:" + nameOf(type);
    case SymTagFunctionType: return "fn:" + signature(type, depth);
    default: sprintf_s(text, "tag%lu", tag); return text;
    }
}

static void methods(IDiaSymbol* cls, const std::string& owner)
{
    IDiaEnumSymbols* e = 0;
    if (cls->findChildren(SymTagFunction, NULL, nsNone, &e) != S_OK || !e) return;
    IDiaSymbol* f = 0; ULONG got = 0;
    while (e->Next(1, &f, &got) == S_OK && got == 1) {
        BOOL generated = FALSE; f->get_compilerGenerated(&generated);
        IDiaSymbol* type = 0;
        if ((generated && !g_all) || f->get_type(&type) != S_OK || !type) { f->Release(); continue; }
        DWORD access = 0; f->get_access(&access);
        BOOL isVirtual = FALSE, pure = FALSE, intro = FALSE;
        f->get_virtual(&isVirtual); f->get_pure(&pure); f->get_intro(&intro);
        IDiaSymbol* self = 0;
        const bool hasThis = type->get_objectPointerType(&self) == S_OK && self;
        char kind[48];
        DWORD slot = 0;
        // (a static member function is the one without a this: DIA's own flag for it is not set from the member list)
        if (isVirtual && intro) { f->get_virtualBaseOffset(&slot); sprintf_s(kind, "%s@%lu", pure ? "pure-intro" : "intro", slot); }
        else if (isVirtual) strcpy_s(kind, pure ? "pure" : "virtual");
        else strcpy_s(kind, hasThis ? "plain" : "static");
        std::string t = signature(type, 0);
        t += " this=" + (hasThis ? describe(self, 1) : std::string("none"));
        if (self) self->Release();
        LONG adjust = 0; type->get_thisAdjust(&adjust);
        char number[32]; sprintf_s(number, " adjust=%ld", adjust);
        t += number;
        BOOL ctor = FALSE, ctorVbases = FALSE, udt = FALSE;
        type->get_constructor(&ctor); type->get_isConstructorVirtualBase(&ctorVbases); type->get_isCxxReturnUdt(&udt);
        if (ctor) t += ctorVbases ? " ctor-vbases" : " ctor";
        if (udt) t += " udt";
        if (generated) t += " generated";
        if (g_code) { DWORD rva = 0; if (f->get_relativeVirtualAddress(&rva) == S_OK && rva != 0) t += " code"; }
        // (a function DIA has found the code of carries that procedure's name, the class before it)
        std::string name = nameOf(f);
        if (name.size() > owner.size() + 2 && name.compare(0, owner.size(), owner) == 0 && name.compare(owner.size(), 2, "::") == 0) name.erase(0, owner.size() + 2);
        printf("method %s %s %s %s %s\n", owner.c_str(), name.c_str(), access == CV_private ? "private" : access == CV_protected ? "protected" : "public", kind, t.c_str());
        type->Release(); f->Release();
    }
    e->Release();
}

int main(int argc, char** argv)
{
    if (argc < 3) { fprintf(stderr, "usage: diatypes <msdia140.dll> <program.exe> [-all] [-code]\n"); return 2; }
    for (int a = 3; a < argc; ++a) { if (strcmp(argv[a], "-all") == 0) g_all = true; else if (strcmp(argv[a], "-code") == 0) g_code = true; }
    CoInitialize(NULL);
    wchar_t dll[MAX_PATH], exe[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, dll, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, exe, MAX_PATH);
    IDiaDataSource* src = 0;
    HRESULT hr = NoRegCoCreate(dll, __uuidof(DiaSource), __uuidof(IDiaDataSource), (void**)&src);
    if (FAILED(hr)) { fprintf(stderr, "diatypes: msdia140.dll not loaded (%08lx)\n", (unsigned long)hr); return 1; }
    hr = src->loadDataForExe(exe, NULL, NULL);
    if (FAILED(hr)) { fprintf(stderr, "diatypes: no PDB for %s (%08lx)\n", argv[2], (unsigned long)hr); return 1; }
    IDiaSession* session = 0;
    if (FAILED(src->openSession(&session))) { fprintf(stderr, "diatypes: no session\n"); return 1; }
    IDiaSymbol* global = 0;
    session->get_globalScope(&global);
    std::set<std::string> done;
    IDiaEnumSymbols* e = 0;
    if (global->findChildren(SymTagUDT, NULL, nsNone, &e) == S_OK && e) {
        IDiaSymbol* cls = 0; ULONG got = 0;
        while (e->Next(1, &cls, &got) == S_OK && got == 1) {
            ULONGLONG length = 0; cls->get_length(&length);
            const std::string name = nameOf(cls);
            if (length != 0 && !name.empty() && done.insert(name).second) methods(cls, name);
            cls->Release();
        }
        e->Release();
    }
    global->Release(); session->Release(); src->Release();
    CoUninitialize();
    return 0;
}
