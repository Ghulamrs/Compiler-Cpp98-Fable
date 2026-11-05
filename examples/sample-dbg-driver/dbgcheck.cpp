// dbgcheck.cpp - reads the CodeView line tables of a program linked with /DEBUG back through dbghelp (test 18):
// for every function named on the command line, the source line at the function's start address
// (SymGetLineFromAddr64) and the lines that follow inside it (SymGetLineNext64).
// Built by cl; the program it inspects is built by cppparse --compile6 --obj and linked with /DEBUG.
//   dbgcheck <exe> <function>...
// prints one line per function: "<function>: <file name> <line at the start> <following lines>...", and exits 1
// when a function has no symbol or no line.
//   dbgcheck -offsets <exe> <function>...
// prints "<function>: size <bytes> <offset>:<line>..." instead, every line record of the function with its code offset
// from the function's start (the offsets must be instruction starts: a jump shortened by the COFF writer moves the
// code after it)
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>
#pragma comment(lib, "dbghelp.lib")

int main(int argc, char** argv)
{
    const bool offsets = argc > 1 && strcmp(argv[1], "-offsets") == 0;
    const int first = offsets ? 2 : 1;
    if (argc < first + 2) { fprintf(stderr, "usage: dbgcheck [-offsets] <exe> <function>...\n"); return 2; }
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    if (!SymInitialize(process, NULL, FALSE)) { fprintf(stderr, "SymInitialize failed: %lu\n", GetLastError()); return 1; }
    DWORD64 base = SymLoadModuleEx(process, NULL, argv[first], NULL, 0x10000000, 0, NULL, 0);
    if (!base) { fprintf(stderr, "SymLoadModuleEx(%s) failed: %lu\n", argv[first], GetLastError()); return 1; }
    int failures = 0;
    for (int a = first + 1; a < argc; ++a) {
        char buffer[sizeof(SYMBOL_INFO) + 512];
        SYMBOL_INFO* sym = (SYMBOL_INFO*)buffer;
        memset(buffer, 0, sizeof buffer);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        if (!SymFromName(process, argv[a], sym)) { printf("%s: no symbol (%lu)\n", argv[a], GetLastError()); ++failures; continue; }
        IMAGEHLP_LINE64 line;
        memset(&line, 0, sizeof line);
        line.SizeOfStruct = sizeof line;
        DWORD displacement = 0;
        if (!SymGetLineFromAddr64(process, sym->Address, &displacement, &line)) { printf("%s: no line at its start (%lu)\n", argv[a], GetLastError()); ++failures; continue; }
        const char* file = strrchr(line.FileName, '\\');
        file = file ? file + 1 : line.FileName;
        if (offsets) printf("%s: size %lu %lu:%lu", argv[a], sym->Size, (unsigned long)(line.Address - sym->Address), line.LineNumber);
        else {
            printf("%s: %s %lu", argv[a], file, line.LineNumber);
            if (displacement) printf("(+%lu)", displacement);
        }
        const DWORD64 end = sym->Address + sym->Size;
        for (int n = 0; n < 500; ++n) {
            if (!SymGetLineNext64(process, &line)) break;
            if (line.Address >= end || line.Address < sym->Address) break;
            if (offsets) printf(" %lu:%lu", (unsigned long)(line.Address - sym->Address), line.LineNumber);
            else printf(" %lu", line.LineNumber);
        }
        printf("\n");
    }
    SymCleanup(process);
    return failures ? 1 : 0;
}
