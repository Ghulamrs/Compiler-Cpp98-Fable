// TargetIntrinsics.hpp - the compiler intrinsics of the Windows x64 target (docs/design-selfhost.md 4.2, row P1
// stage 2).
//
// A function declared with C linkage, not defined, with one of these names and exactly these parameters is an
// intrinsic when it is called by name; taking its address is the error S165. cl's <intrin.h> declares the same
// functions with the same meaning, cppparse's is platform/intrin.h:
//
//     long  _InterlockedIncrement(long volatile* p)                    mov eax, 1  / lock xadd [p], eax / inc eax
//     long  _InterlockedDecrement(long volatile* p)                    mov eax, -1 / lock xadd [p], eax / dec eax
//     long  _InterlockedExchange(long volatile* p, long v)             mov eax, v  / xchg [p], eax       (the old value)
//     long  _InterlockedCompareExchange(long volatile* p, long x, long c)   mov eax, c / lock cmpxchg [p], x (the old value)
//     void* _AddressOfReturnAddress(void)                               lea rax, [rbp+8]
//
// To the optimiser the first four read and write *p and are barriers as a call is; the fifth is a value of the
// frame. A function that contains _AddressOfReturnAddress keeps its frame (rbp is its own, [rbp+8] its return
// address, at every level) and is never inlined, so the value is about the function the source wrote it in.
//
// Strict C++98.
#ifndef CPPPARSE_TARGET_INTRINSICS_HPP
#define CPPPARSE_TARGET_INTRINSICS_HPP

#include <string>
#include <vector>

namespace cppparse {

namespace sema { struct Symbol; }

class TargetIntrinsics {
public:
    enum Kind { None, Increment, Decrement, Exchange, CompareExchange, AddressOfReturnAddress };
    // the intrinsic a function symbol is (C linkage, not defined, the name, exactly the parameters), None otherwise
    static Kind of(const sema::Symbol* fn);
    static Kind byName(const std::string& name);   // by the IR operation's sub-operation
    static const char* name(Kind k);
    static bool interlocked(Kind k) { return k == Increment || k == Decrement || k == Exchange || k == CompareExchange; }
    static const char* code() { return "S165"; }
    static std::string addressMessage(const std::string& name);
    // the code: 'p' the register holding the address, 'v' the second operand's 32-bit register or immediate, 'c'
    // the third's; the result is in eax (rax for _AddressOfReturnAddress), which the sequence uses
    static void sequence(Kind k, const std::string& p, const std::string& v, const std::string& c, std::vector<std::string>& lines);
};

} // namespace cppparse

#endif
