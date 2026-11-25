// TargetTls.hpp - thread-local storage of the Windows x64 target (docs/design-selfhost.md 4.1, row P1 stage 1).
//
// __declspec(thread) as cl implements it: a variable of namespace scope, static storage, no constructor or destructor,
// no initialiser or a constant one, alignment up to 16. Its initial value lies in the object's section .tls$, which
// the linker merges into the image's .tls; the loader copies that template into a block per thread and per module.
// An access takes the module's block from the thread's array at gs:[58h] by the index the C runtime's _tls_index
// holds, and adds the variable's offset in the section (a SECREL relocation):
//
//     mov  ecx, dword ptr [_tls_index]        ; REL32
//     mov  rd,  qword ptr gs:[58h]
//     mov  rd,  qword ptr [rd+rcx*8]
//     mov  edx, SECTIONREL <variable>         ; SECREL, 32 bits, zero-extended
//     add  rd,  rdx
//
// The sum is the variable's address in this thread; it reads no memory the program writes, so it may be reused within
// a function (a function runs in one thread), and it is never split. The object defines neither _tls_index nor
// _tls_used nor a TLS directory: they are the C runtime's (libcmt's tlssup.obj, grtwin's tls.obj).
//
// Here: what the front end refuses and how it says so, the listing's spelling of the section and of the sequence, and
// the section's characteristics. The front end, the IR generator and the back end call these; none keeps a table of
// its own.
//
// Strict C++98.
#ifndef CPPPARSE_TARGET_TLS_HPP
#define CPPPARSE_TARGET_TLS_HPP

#include <string>
#include <vector>

namespace cppparse {

class TargetTls {
public:
    // what a declaration with __declspec(thread) may not be; each is the error S164 with its own message
    enum Refusal {
        BlockScope,        // a variable of block scope, a static local among them
        Member,            // a data member of a class, static or not
        Function,          // a function
        Type,              // a class or enum declared with the attribute and no variable
        Typedef,           // a typedef
        Parameter,         // a function's parameter
        Added,             // a redeclaration adds the attribute to a variable declared without it
        Dropped,           // a redeclaration leaves out the attribute of a thread-local
        Dynamic,           // a constructor, a destructor or an initialiser that needs code
        Alignment,         // a type or __declspec(align) that asks for more than 16 bytes
        AddressConstant    // the address of a thread-local initialises a variable with static storage
    };
    static const char* code() { return "S164"; }
    // the message of a refusal; 'name' is the variable or the entity, 'detail' what the refusal names beside it (the
    // alignment, the thread-local whose address is taken)
    static std::string message(Refusal r, const std::string& name, const std::string& detail = std::string());

    static const int maxAlignment = 16;   // neither the loader's block nor the template's start (.tls, 16) promise more

    // the listing (MASM, as ml64 assembles it to cl's bytes): the section, the symbol the access reads, the sequence
    static const char* segmentBegin() { return "_TLS SEGMENT ALIGN(16) READ WRITE ALIAS(\".tls$\") 'DATA'"; }
    static const char* segmentEnd() { return "_TLS ENDS"; }
    static const char* indexSymbol() { return "_tls_index"; }
    // the five instructions that leave the address of the variable with symbol 'symbol' in the 64-bit register rd (not
    // rcx or rdx, which the sequence uses)
    static void addressSequence(const std::string& rd, const std::string& symbol, std::vector<std::string>& lines);

    // the object: the section's name and characteristics (initialised data, read, write, 16-byte alignment, as cl's)
    static const char* sectionName() { return ".tls$"; }
    static const unsigned long sectionFlags = 0xC0500040UL;
    static bool isSegmentName(const std::string& upperName) { return upperName == "_TLS"; }
};

} // namespace cppparse

#endif
