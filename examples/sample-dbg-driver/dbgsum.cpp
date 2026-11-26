// dbgsum.cpp - reads the source files of a program linked with /DEBUG and their checksums back through dbghelp
// (test 22; docs\design-codeview.md, row M10 stage 5): every source file the PDB names for the program's module
// (SymEnumSourceFiles) with the checksum the PDB holds for it (SymGetSourceFileChecksum), which is the one the
// compiler wrote into the object's file table (DEBUG_S_FILECHKSMS) and the linker carried.
// Built by cl; the program it inspects is built by cppparse --compile6 -g --obj (or by cl /Z7) and linked with /DEBUG.
//   dbgsum <exe>
// prints one line per source file, in the PDB's order:
//   "<kind> <checksum as hexadecimal digits, lower case> <the file's path>"
// where <kind> is none, md5, sha1 or sha256 (CodeView's CHKSUM_TYPE 0 to 3; another prints as its number), and "-"
// stands for a checksum of no bytes. A file whose checksum dbghelp does not give is "unknown - <path> (<error>)".
// Exit code 1 when the PDB names no source file, 2 on a wrong call.
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>
#pragma comment(lib, "dbghelp.lib")

namespace {

HANDLE  g_process;
DWORD64 g_base;
int     g_files;

BOOL CALLBACK oneFile(PSOURCEFILE file, PVOID)
{
    ++g_files;
    DWORD kind = 0, bytes = 0;
    BYTE sum[64];
    memset(sum, 0, sizeof sum);
    if (!SymGetSourceFileChecksum(g_process, g_base, file->FileName, &kind, sum, sizeof sum, &bytes)) {
        printf("unknown - %s (%lu)\n", file->FileName, GetLastError());
        return TRUE;
    }
    static const char* const kinds[] = { "none", "md5", "sha1", "sha256" };
    if (kind < 4) printf("%s ", kinds[kind]); else printf("%lu ", kind);
    if (bytes == 0) printf("-");
    for (DWORD i = 0; i < bytes && i < sizeof sum; ++i) printf("%02x", sum[i]);
    printf(" %s\n", file->FileName);
    return TRUE;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { fprintf(stderr, "usage: dbgsum <exe>\n"); return 2; }
    g_process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    if (!SymInitialize(g_process, NULL, FALSE)) { fprintf(stderr, "SymInitialize failed: %lu\n", GetLastError()); return 1; }
    g_base = SymLoadModuleEx(g_process, NULL, argv[1], NULL, 0x10000000, 0, NULL, 0);
    if (!g_base) { fprintf(stderr, "SymLoadModuleEx(%s) failed: %lu\n", argv[1], GetLastError()); return 1; }
    if (!SymEnumSourceFiles(g_process, g_base, "*", oneFile, NULL)) { fprintf(stderr, "SymEnumSourceFiles failed: %lu\n", GetLastError()); return 1; }
    SymCleanup(g_process);
    if (g_files == 0) { fprintf(stderr, "the PDB of %s names no source file\n", argv[1]); return 1; }
    return 0;
}
