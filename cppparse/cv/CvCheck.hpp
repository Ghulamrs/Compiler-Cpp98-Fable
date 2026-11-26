// CvCheck.hpp - the invariants of well-formed CodeView records (docs/design-codeview.md 5.5/4), checked on the bytes
// the object writer is about to write, as EhClCheck checks cl's exception tables: a violation is an internal error
// of cppparse, found when the object is written and not by a debugger that shows nothing.
//     types (a)    every record is a whole number of four bytes, at most FF00h, of a leaf CvTypes writes; every type
//                  index a record holds is a fundamental type's or below the record's own; every class, struct,
//                  union and enumeration has a unique name; a member function a class lists has an access, a kind
//                  CvTypes writes and an LF_MFUNCTION as its type (without a type of this exactly when it is static),
//                  its vftable slot is a multiple of eight bytes, and an overloaded name's LF_METHODLIST holds as
//                  many functions as the name says, two at least
//     symbols      every type index a symbol record holds is a fundamental type's or one of the section's records
//     procedures (b)  a procedure has S_REGREL32 or S_LOCAL records, not both; its parameter records come first and are
//                  as many as its type says ("this" counted, "..." not); every S_BLOCK32 lies in its parent and apart
//                  from its siblings; a variable's ranges lie in the procedure, ascend and do not overlap, their
//                  gaps inside them; a frame variable is relative to RBP (or RSP); a register is one of CvRegisters'
//                  table (never ah, ch, dh, bh, never a part of rsp or rbp) and holds a value of its variable's type:
//                  the size, and a floating-point value in an xmm register only
//     places (c)   from what the encoder knows of the code: a range of a frame slot given by code offsets, and every
//                  register range, holds no instruction of the prologue, of a funclet's entry or of an epilogue; inside
//                  a register range no instruction writes that register (a call writes every volatile one, so a range
//                  in a volatile register holds no call; an instruction the encoder's table of written registers
//                  does not know writes them all)
//     files        the file table (CvFiles): every entry is a whole number of four bytes, names a string of the string
//                  table by its start, and has no checksum (length 0, kind 0) or a SHA-256 (length 32, kind 3), zeros
//                  behind it; the unit's sources, the first entries, each have the SHA-256; every file a line table
//                  names is the start of an entry
//
// Strict C++98.
#ifndef CPPPARSE_CVCHECK_HPP
#define CPPPARSE_CVCHECK_HPP

#include "CvRegisters.hpp"

#include <string>
#include <utility>
#include <vector>

namespace cppparse {

class CvCheck {
public:
    // the records of a .debug$T section (its bytes, the signature included): what is wrong is appended to
    // 'problems'; returns the number of records
    static unsigned long types(const std::string& section, std::vector<std::string>& problems);
    // the records of a DEBUG_S_SYMBOLS subsection against the number of type records the object has
    static void symbols(const std::string& records, unsigned long typeCount, std::vector<std::string>& problems);
    // (b) on the records of a DEBUG_S_SYMBOLS subsection, with the object's .debug$T (its bytes)
    static void procedures(const std::string& records, const std::string& typesSection, std::vector<std::string>& problems);
    // (c) the ranges (start, end) of a function's frame variables against its prologue and epilogue code
    static void frameRanges(const std::string& fn, const std::vector<std::pair<unsigned long, unsigned long> >& ranges,
                            const std::vector<std::pair<unsigned long, unsigned long> >& windows, std::vector<std::string>& problems);
    // (c) the ranges of a function's variables in registers against its instructions: 'writes' holds, in code order,
    // the offset of every instruction and the registers it writes (a mask of CvRegisters' families), 'windows' the
    // prologue and epilogue code
    static void registerRanges(const std::string& fn, const std::vector<CvRegisterRange>& ranges,
                               const std::vector<std::pair<unsigned long, unsigned long> >& writes,
                               const std::vector<std::pair<unsigned long, unsigned long> >& windows, std::vector<std::string>& problems);
    // the file a line table names (the bytes of a DEBUG_S_LINES subsection's data, CvLines): the offset of an entry of
    // the file table; FFFFFFFFh for bytes that are no line table
    static unsigned long fileOf(const std::string& lineTable);
    // the file table: the entries (the data of DEBUG_S_FILECHKSMS), the string table's bytes, the entries the
    // line tables name by their offsets (fileOf), and how many entries, the first ones, are the unit's sources: each
    // of those has its SHA-256
    static void files(const std::string& entries, const std::string& strings, const std::vector<unsigned long>& named, unsigned long sources, std::vector<std::string>& problems);
};

} // namespace cppparse

#endif
