// Coff.hpp - version 5: writes an x86-64 COFF object file directly from the
// MASM text produced by X86.cpp, without ml64.
//
// The input is the subset of MASM that X86Module emits: EXTERN/PUBLIC,
// .data/.const/.code, PROC/ENDP, labels, DB/DD/DQ/REAL8/LABEL data, and the
// instruction forms listed in Coff.cpp (integer moves, ALU, shifts, setcc,
// movzx/movsx/movsxd, jumps, calls, push/leave/ret, and the SSE2 scalar
// double instructions). Every PROC is encoded on the worker pool; the
// sections, relocations (REL32 for code, ADDR64 for vtable entries) and the
// symbol table are then laid out in order, so the output is deterministic.
//
// Strict C++98.
#ifndef CPPPARSE_COFF_HPP
#define CPPPARSE_COFF_HPP

#include <string>
#include <vector>

namespace cppparse {

struct CoffResult {
    int functions, instructions, relocations, symbols;
    unsigned long textBytes, dataBytes, rdataBytes, fileBytes;
    std::vector<std::string> errors;     // lines that could not be encoded (the file is not written)
    CoffResult() : functions(0), instructions(0), relocations(0), symbols(0), textBytes(0), dataBytes(0), rdataBytes(0), fileBytes(0) {}
};

// assembles the MASM text and writes the object; false when a line could not be encoded or the file could not be written
bool writeCoffObject(const std::string& masmText, const std::string& objPath, int threads, CoffResult& result);

// assembles into memory only (used by the writer and by tests)
bool assembleCoff(const std::string& masmText, int threads, std::string& objectBytes, CoffResult& result);

} // namespace cppparse

#endif
