// CvSymbols.hpp - the symbol records of one DEBUG_S_SYMBOLS subsection.
//
//     S_OBJNAME (1101h)     {signature 0, the object's file name}                                  once per object
//     S_COMPILE3 (113Ch)    {flags: the language, C++ (1); machine: x64 (D0h); the front end's and the back end's
//                           version, four words each; the compiler's name}                         once per object
//     S_GPROC32 (1110h)     {parent 0, end 0, next 0 (the linker fills them in), the code's length, DbgStart,
//     S_LPROC32 (110Fh)     DbgEnd, type index, the function's address (offset, section), flags, the name it is
//                           shown by}; S_LPROC32 for a function that is not public
//     S_END (0006h)         closes the procedure
//     S_GDATA32 (110Dh)     {type index, the variable's address (offset, section), name}; S_LDATA32 for a variable
//     S_LDATA32 (110Ch)     of internal linkage and for a static local, which stands inside its procedure
//     S_CONSTANT (1107h)    {type index, value (a numeric leaf), name}: a constant that has no storage
//     S_UDT (1108h)         {type index, name}: a class, an enumeration, a typedef name
// With -g, inside a procedure (docs/design-codeview.md 5.3; CvScopes chooses the records):
//     S_FRAMEPROC (1012h)   {the frame's bytes below the saved registers, padding 0, its offset 0, the saved registers'
//                           bytes, the exception handler's address 0, flags: the base registers of locals and
//                           parameters (bits 14, 16), cl's flags}
//     S_REGREL32 (1111h)    {offset from the register, type index, register (334: RBP), name}: a variable in the frame
//     S_BLOCK32 (1103h)     {parent 0, end 0 (the linker), length, the block's address (offset, section), name ""} ...
//                           S_END: a block scope that is one piece of code
//     S_LOCAL (113Eh)       {type index, flags (1: a parameter, 100h: optimised out, no place follows), name}, behind
//                           it its places:
//     S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE (1144h) {offset from the frame's base}: wherever its scope is
//     S_DEFRANGE_REGISTER_REL (1145h) {register, flags 0, offset, range (offset, section, length), gaps (start from the
//                           range's offset, length)}: in the frame over a range of code, a block in pieces
// Without -g a procedure has DbgStart 0, DbgEnd the code's length and type 0 (T_NOTYPE); with it the LF_PROCEDURE or
// LF_MFUNCTION record of the function's type (CvTypes), DbgStart behind the entry sequence, DbgEnd at the epilogue.
//
// Strict C++98.
#ifndef CPPPARSE_CVSYMBOLS_HPP
#define CPPPARSE_CVSYMBOLS_HPP

#include "CvRecord.hpp"

#include <utility>
#include <vector>

namespace cppparse {

class CvSymbols {
public:
    // S_OBJNAME (the object's file name) and S_COMPILE3 (C++ for x64, "cppparse 6")
    void addModule(const std::string& objName);
    // S_GPROC32 / S_LPROC32 of the function with symbol 'fn', shown as 'display'; endProcedure writes its S_END,
    // what is added in between stands in the procedure
    void beginProcedure(const std::string& fn, const std::string& display, unsigned long codeSize, bool internal, unsigned long typeIndex,
                        unsigned long dbgStart = 0, unsigned long dbgEnd = ~0UL, int flags = 0);
    void endProcedure();
    // the records inside a procedure (-g)
    void addFrameProc(unsigned long frameBytes, unsigned long savedBytes, unsigned long flags);
    void addRegRel(long offset, unsigned long typeIndex, int reg, const std::string& name);
    void beginBlock(const std::string& fn, unsigned long start, unsigned long length);
    void endBlock();
    void addLocal(unsigned long typeIndex, int flags, const std::string& name);
    void addFramePointerFullScope(long offset);
    // gaps: {start relative to 'start', length}
    void addRegisterRel(int reg, long offset, const std::string& fn, unsigned long start, unsigned long length,
                        const std::vector<std::pair<unsigned long, unsigned long> >& gaps);
    // S_GDATA32 / S_LDATA32 of the variable with symbol 'symbol'
    void addData(const std::string& symbol, const std::string& display, unsigned long typeIndex, bool internal, bool thread = false);   // thread: S_GTHREAD32 / S_LTHREAD32 (1113h / 1112h)
    void addConstant(const std::string& display, unsigned long typeIndex, int64 value);
    void addUdt(const std::string& display, unsigned long typeIndex);

    // the records so far, their relocations at offsets within them
    const CvBytes& records() const { return m_records; }
    bool empty() const { return m_records.size() == 0; }

private:
    CvBytes m_records;
};

} // namespace cppparse

#endif
