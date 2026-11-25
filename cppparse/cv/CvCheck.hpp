// CvCheck.hpp - the invariants of well-formed CodeView records (docs/design-codeview.md 5.5/4), checked on the bytes
// the object writer is about to write, as EhClCheck checks cl's exception tables: a violation is an internal error
// of cppparse, found when the object is written and not by a debugger that shows nothing.
//     types (a)    every record is a whole number of four bytes, at most FF00h, of a leaf CvTypes writes; every type
//                  index a record holds is a fundamental type's or below the record's own; every class, struct,
//                  union and enumeration has a unique name
//     symbols      every type index a symbol record holds is a fundamental type's or one of the section's records
//     procedures (b)  a procedure has S_REGREL32 or S_LOCAL records, not both; its parameter records come first and are
//                  as many as its type says ("this" counted, "..." not); every S_BLOCK32 lies in its parent and apart
//                  from its siblings; a variable's frame ranges lie in the procedure, ascend and do not overlap, their
//                  gaps inside them; a frame variable is relative to RBP (or RSP)
//     frame ranges (c, the part of stage 2)  a frame variable's range given by code offsets holds no instruction of
//                  the prologue, of a funclet's entry or of an epilogue (from what the encoder knows of the code)
// (The register ranges of (c) come with stage 3.)
//
// Strict C++98.
#ifndef CPPPARSE_CVCHECK_HPP
#define CPPPARSE_CVCHECK_HPP

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
};

} // namespace cppparse

#endif
