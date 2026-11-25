// dbgtypes.cpp - reads the CodeView types and globals of a program linked with /DEBUG back through dbghelp (test 20).
// Built by cl; the program it inspects is built by cppparse --compile6 -g --obj (or, for the comparison, by cl /Z7).
//   dbgtypes <exe> [-types] [-local <function> <variable>]...
// prints, for every class, struct, union and enumeration of the PDB, the facts the program sample-dbg\types.cpp
// prints of its own types, in the same lines:
//     size <type> <bytes>
//     member <type> <member> <offset>            bits <type> <member> <first bit in the object> <width>
//     base <type> <base> <offset>                vbase <type> <base> <vbptr offset> <place in the vbtable>
//     enumerator <enumeration> <name> <value>    static <type> <member>      nested <type> <name>
// for every variable of the module "global <name> <bytes>", and for one of class type its members once more under
// the variable's name ("member <variable> <member> <offset>": the type is the one the variable's own record names,
// which tells two classes of one name apart); for each -local pair "local <function> <variable> <bytes>", the
// variable looked up in the function's scope. With -types every member is followed by a line with its type,
// "type <type> <member> <description>", the description spelled from what dbghelp answers (b<base type>.<bytes>,
// ptr<bytes>(...), ref(...), arr<count>(...), udt:<name>, enum:<name>, fn(...)->...), and a class with virtual
// functions by "vshape <type> <slots>".
// Exit code 1 when the module or a -local variable is not found.
#define _NO_CVCONST_H           // dbghelp.h then declares the symbol tags itself (they are the DIA SDK's cvconst.h's)
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>
#include <set>
#include <string>
#pragma comment(lib, "dbghelp.lib")

// cvconst.h's DataKind, as far as it is asked for here
enum { DataIsMember = 7, DataIsStaticMember = 8, DataIsConstant = 9 };

static HANDLE g_process;
static DWORD64 g_base;
static bool g_types;
static std::set<std::string> g_done;

static bool info(ULONG type, IMAGEHLP_SYMBOL_TYPE_INFO what, void* out) { return SymGetTypeInfo(g_process, g_base, type, what, out) != FALSE; }

static std::string nameOf(ULONG type)
{
    WCHAR* w = 0;
    if (!info(type, TI_GET_SYMNAME, &w) || !w) return std::string();
    char buffer[8192];
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buffer, sizeof buffer, NULL, NULL);
    LocalFree(w);
    return n > 0 ? std::string(buffer) : std::string();
}

static std::string describe(ULONG type, int depth)
{
    DWORD tag = 0;
    if (depth > 12 || !info(type, TI_GET_SYMTAG, &tag)) return "?";
    ULONG64 length = 0; info(type, TI_GET_LENGTH, &length);
    char text[64];
    DWORD inner = 0;
    switch (tag) {
    case SymTagBaseType: { DWORD bt = 0; info(type, TI_GET_BASETYPE, &bt); sprintf_s(text, "b%lu.%llu", bt, length); return text; }
    case SymTagPointerType: {
        BOOL reference = FALSE; info(type, TI_GET_IS_REFERENCE, &reference); info(type, TI_GET_TYPEID, &inner);
        sprintf_s(text, "ptr%llu(", length);
        return (reference ? std::string("ref(") : std::string(text)) + describe(inner, depth + 1) + ")";
    }
    case SymTagArrayType: { DWORD count = 0; info(type, TI_GET_COUNT, &count); info(type, TI_GET_TYPEID, &inner); sprintf_s(text, "arr%lu(", count); return text + describe(inner, depth + 1) + ")"; }
    case SymTagUDT: return "udt:" + nameOf(type);
    case SymTagEnum: return "enum:" + nameOf(type);
    case SymTagTypedef: return "typedef:" + nameOf(type);
    case SymTagFunctionType: {
        std::string s = "fn(";
        DWORD count = 0; info(type, TI_GET_CHILDRENCOUNT, &count);
        if (count > 0 && count < 64) {
            char block[sizeof(TI_FINDCHILDREN_PARAMS) + 64 * sizeof(ULONG)];
            TI_FINDCHILDREN_PARAMS* kids = (TI_FINDCHILDREN_PARAMS*)block;
            memset(block, 0, sizeof block); kids->Count = count;
            if (info(type, TI_FINDCHILDREN, kids))
                for (DWORD k = 0; k < count; ++k) { DWORD arg = 0; info(kids->ChildId[k], TI_GET_TYPEID, &arg); s += (k ? "," : "") + describe(arg, depth + 1); }
        }
        info(type, TI_GET_TYPEID, &inner);
        return s + ")->" + describe(inner, depth + 1);
    }
    default: sprintf_s(text, "tag%lu", tag); return text;
    }
}

static long long valueOf(ULONG id)
{
    VARIANT v; memset(&v, 0, sizeof v);
    if (!info(id, TI_GET_VALUE, &v)) return 0;
    switch (v.vt) {
    case VT_I1: return v.cVal; case VT_UI1: return v.bVal; case VT_I2: return v.iVal; case VT_UI2: return v.uiVal;
    case VT_I4: case VT_INT: return v.lVal; case VT_UI4: case VT_UINT: return v.ulVal; case VT_I8: return v.llVal; case VT_UI8: return (long long)v.ullVal;
    default: return 0;
    }
}

// the members of class or enumeration 'type', printed under 'owner'; all: every kind of line, else the data members only
static void members(ULONG type, const std::string& owner, bool all)
{
    DWORD count = 0;
    if (!info(type, TI_GET_CHILDRENCOUNT, &count) || count == 0 || count > 100000) return;
    std::string block(sizeof(TI_FINDCHILDREN_PARAMS) + count * sizeof(ULONG), '\0');
    TI_FINDCHILDREN_PARAMS* kids = (TI_FINDCHILDREN_PARAMS*)&block[0];
    kids->Count = count;
    if (!info(type, TI_FINDCHILDREN, kids)) return;
    for (DWORD k = 0; k < count; ++k) {
        const ULONG id = kids->ChildId[k];
        DWORD tag = 0; if (!info(id, TI_GET_SYMTAG, &tag)) continue;
        DWORD inner = 0; info(id, TI_GET_TYPEID, &inner);
        DWORD offset = 0; info(id, TI_GET_OFFSET, &offset);
        if (tag == SymTagData) {
            DWORD kind = 0; info(id, TI_GET_DATAKIND, &kind);
            const std::string name = nameOf(id);
            if (kind == DataIsMember) {
                DWORD position = 0; ULONG64 bits = 0;
                if (info(id, TI_GET_BITPOSITION, &position) && info(id, TI_GET_LENGTH, &bits) && bits != 0) {
                    ULONG64 whole = 0; info(inner, TI_GET_LENGTH, &whole);
                    if (bits < whole * 8) { printf("bits %s %s %lu %llu\n", owner.c_str(), name.c_str(), offset * 8 + position, bits); if (g_types && all) printf("type %s %s %s\n", owner.c_str(), name.c_str(), describe(inner, 0).c_str()); continue; }
                }
                printf("member %s %s %lu\n", owner.c_str(), name.c_str(), offset);
                if (g_types && all) printf("type %s %s %s\n", owner.c_str(), name.c_str(), describe(inner, 0).c_str());
            }
            else if (!all) continue;
            else if (kind == DataIsStaticMember) { printf("static %s %s\n", owner.c_str(), name.c_str()); if (g_types) printf("type %s %s %s\n", owner.c_str(), name.c_str(), describe(inner, 0).c_str()); }
            else if (kind == DataIsConstant) printf("enumerator %s %s %lld\n", owner.c_str(), name.c_str(), valueOf(id));
        }
        else if (!all) continue;
        else if (tag == SymTagBaseClass) {
            BOOL isVirtual = FALSE; info(id, TI_GET_VIRTUALBASECLASS, &isVirtual);
            if (isVirtual) { DWORD vbptr = 0, place = 0; info(id, TI_GET_VIRTUALBASEPOINTEROFFSET, &vbptr); info(id, TI_GET_VIRTUALBASEDISPINDEX, &place); printf("vbase %s %s %lu %lu\n", owner.c_str(), nameOf(inner).c_str(), vbptr, place); }
            else printf("base %s %s %lu\n", owner.c_str(), nameOf(inner).c_str(), offset);
        }
        else if (tag == SymTagUDT || tag == SymTagEnum || tag == SymTagTypedef) printf("nested %s %s\n", owner.c_str(), nameOf(id).c_str());
    }
}

static BOOL CALLBACK onType(PSYMBOL_INFO sym, ULONG, PVOID)
{
    if (sym->Tag != SymTagUDT && sym->Tag != SymTagEnum) return TRUE;
    const std::string name = sym->Name;
    ULONG64 length = 0; info(sym->TypeIndex, TI_GET_LENGTH, &length);
    if (name.empty() || (sym->Tag == SymTagUDT && length == 0) || !g_done.insert(name).second) return TRUE;
    if (sym->Tag == SymTagUDT) printf("size %s %llu\n", name.c_str(), length);
    members(sym->TypeIndex, name, true);
    DWORD shape = 0, slots = 0;
    if (g_types && sym->Tag == SymTagUDT && info(sym->TypeIndex, TI_GET_VIRTUALTABLESHAPEID, &shape) && shape != 0 && info(shape, TI_GET_COUNT, &slots)) printf("vshape %s %lu\n", name.c_str(), slots);
    return TRUE;
}

static BOOL CALLBACK onSymbol(PSYMBOL_INFO sym, ULONG, PVOID)
{
    if (sym->Tag != SymTagData || sym->TypeIndex == 0) return TRUE;
    const std::string name = sym->Name;
    if (!g_done.insert("variable " + name).second) return TRUE;
    ULONG64 length = 0; info(sym->TypeIndex, TI_GET_LENGTH, &length);
    printf("global %s %llu\n", name.c_str(), length);
    DWORD tag = 0;
    if (info(sym->TypeIndex, TI_GET_SYMTAG, &tag) && tag == SymTagUDT) members(sym->TypeIndex, name, false);
    return TRUE;
}

struct LocalQuery { const char* variable; ULONG type; bool found; };
static BOOL CALLBACK onLocal(PSYMBOL_INFO sym, ULONG, PVOID context)
{
    LocalQuery* q = (LocalQuery*)context;
    if (strcmp(sym->Name, q->variable) == 0) { q->type = sym->TypeIndex; q->found = true; return FALSE; }
    return TRUE;
}

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: dbgtypes <exe> [-types] [-local <function> <variable>]...\n"); return 2; }
    g_process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME);
    if (!SymInitialize(g_process, NULL, FALSE)) { fprintf(stderr, "SymInitialize failed: %lu\n", GetLastError()); return 1; }
    g_base = SymLoadModuleEx(g_process, NULL, argv[1], NULL, 0x10000000, 0, NULL, 0);
    if (!g_base) { fprintf(stderr, "SymLoadModuleEx(%s) failed: %lu\n", argv[1], GetLastError()); return 1; }
    int failures = 0;
    for (int a = 2; a < argc; ++a) if (strcmp(argv[a], "-types") == 0) g_types = true;
    SymEnumTypes(g_process, g_base, onType, NULL);
    SymEnumSymbols(g_process, g_base, "*", onSymbol, NULL);
    for (int a = 2; a + 2 < argc; ++a) {
        if (strcmp(argv[a], "-local") != 0) continue;
        char buffer[sizeof(SYMBOL_INFO) + 512];
        SYMBOL_INFO* fn = (SYMBOL_INFO*)buffer;
        memset(buffer, 0, sizeof buffer); fn->SizeOfStruct = sizeof(SYMBOL_INFO); fn->MaxNameLen = 511;
        LocalQuery q = { argv[a + 2], 0, false };
        IMAGEHLP_STACK_FRAME frame; memset(&frame, 0, sizeof frame);
        if (SymFromName(g_process, argv[a + 1], fn)) {
            frame.InstructionOffset = fn->Address;
            SymSetContext(g_process, &frame, NULL);
            SymEnumSymbols(g_process, 0, "*", onLocal, &q);
        }
        ULONG64 length = 0;
        if (q.found && info(q.type, TI_GET_LENGTH, &length)) printf("local %s %s %llu\n", argv[a + 1], argv[a + 2], length);
        else { printf("local %s %s: not found\n", argv[a + 1], argv[a + 2]); ++failures; }
        a += 2;
    }
    return failures ? 1 : 0;
}
