// CvRegisters.hpp - CodeView's numbers of the x64 registers (cvconst.h's CV_AMD64_*), for a variable that lives in a
// register (S_DEFRANGE_REGISTER, docs/design-codeview.md 5.3).
//
// A record names the part of the register that holds the value, by the value's size and kind, as cl does: al, ax, eax
// or rax for 1, 2, 4 or 8 bytes of an integer or a pointer; XMMn_0 (the low 32 bits) for a float, XMMnL (the low 64)
// for a double. The numbers are a table per register: they do not follow the encoding order (rax 328, rbx 329, rcx
// 330, rdx 331; sil and dil 324, 325; r8b to r15b 344 to 351), so nothing here is computed from a register's number
// in an instruction. ah, ch, dh and bh (5 to 8) are never written.
//
// A register is also a number of its own here, its "family": 0 to 15 the integer registers as instructions encode
// them (rax 0, rcx 1, rdx 2, rbx 3, rsp 4, rbp 5, rsi 6, rdi 7, r8 to r15), 16 to 31 xmm0 to xmm15. The object
// writer's encoder says in these which registers an instruction writes (CvCheck (c)). Volatile are the registers a
// call does not keep: rax, rcx, rdx, r8 to r11, xmm0 to xmm5.
//
// Strict C++98.
#ifndef CPPPARSE_CVREGISTERS_HPP
#define CPPPARSE_CVREGISTERS_HPP

#include <string>

namespace cppparse {

// a range of a function's code (offsets from its start) over which a variable is in the register 'reg' (its
// CodeView number): what CvScopes writes as S_DEFRANGE_REGISTER and CvCheck (c) holds against the code
struct CvRegisterRange {
    unsigned long start, end;
    int           reg;
};

class CvRegisters {
public:
    enum { Rbp = 334, Rsp = 335, Families = 32 };

    // the family of a register by any of its names ("rbx", "ebx", "bx", "bl", "r12d", "xmm6"); -1: not a register
    static int family(const std::string& name);
    // CodeView's number of the part of 'family' that holds a value of 'size' bytes (an integer register: 1, 2, 4, 8;
    // an xmm register: 4, a float, or 8, a double); 0: there is none
    static int number(int family, int size);
    // what a number names: its family (-1: not a number of the table, 5 to 8 among them), the bytes of the value it
    // holds, and whether that is a floating-point value
    static int  familyOf(int number);
    static int  sizeOf(int number);
    static bool isFloat(int number) { return familyOf(number) >= 16; }
    static bool isVolatile(int family);
    // a set of families as a mask (bit f: family f), for the registers an instruction writes
    static unsigned long bit(int family) { return family >= 0 && family < Families ? 1UL << family : 0UL; }
    static unsigned long volatileMask();
    static unsigned long allMask() { return 0xFFFFFFFFUL; }
    // "rbx", "xmm6": for messages
    static std::string name(int family);
};

} // namespace cppparse

#endif
