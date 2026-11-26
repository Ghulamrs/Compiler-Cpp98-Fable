// CvScopes.hpp - a procedure's variables and block scopes as CodeView symbols (docs/design-codeview.md 5.3, 5.4).
//
// The listing's markers of one procedure (-g), with the code offsets of the position markers that the object writer's
// encoder recorded:
//     ;@proc T<n> <frame> <saved> <flags>        its type and S_FRAMEPROC's numbers
//     ;@block <id> <parent>                       a block scope that declares something
//     ;@local <block> T<n> [rbp-N] <p|l>[r] <name>   a variable in the frame; ;@gone ... without a place
//     ;@static <block> T<n> <symbol> <name>       a static local
//     ;@prologend, ;@epilog, ;@scope <id>         DbgStart; an epilogue; the code from here on belongs to block <id>
//                                                 (-1: the entry sequence, an epilogue, a funclet's entry)
//     ;@var <n> <block> T<n> <p|l>[r] <name>      a variable whose place is given over ranges of the code (-O1 and up)
//     ;@loc <n> <register> <bytes> | [rbp-N] | -  from here on variable <n> is in that register (the part of it that
//                                                 holds a value of so many bytes), in that frame slot, or nowhere
// A block's code is the code of its scope and of its descendants'. A procedure is described in one of two forms, never
// a mixture (probe r: dbghelp drops an S_REGREL32 that follows an S_LOCAL):
//     plain    every variable has its frame slot and every block is one piece: S_REGREL32 off RBP, S_BLOCK32 per block
//     ranged   any other: S_LOCAL per variable, all of them in the procedure (no S_BLOCK32: dbghelp does not look into
//              the blocks of a procedure that has an S_LOCAL, measured in stage 2), the parameters first; a variable of
//              the procedure's own block in the frame by S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE, one of an inner block
//              by S_DEFRANGE_REGISTER_REL over the block's pieces of code; a ;@var variable by S_DEFRANGE_REGISTER or
//              S_DEFRANGE_REGISTER_REL over the code between its ;@loc markers, cut to its block's pieces; nothing and
//              flag 100h where it has no place;
//              static locals in the procedure; S_GPROC32's flag 80h, as cl's /O2 procedures (which have no blocks either)
// A block without code (its statements were never generated) is not described, nor are its variables; its static
// locals stand in the enclosing block.
//
// Strict C++98.
#ifndef CPPPARSE_CVSCOPES_HPP
#define CPPPARSE_CVSCOPES_HPP

#include "CvRegisters.hpp"
#include "CvSymbols.hpp"
#include "CvTypes.hpp"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cppparse {

struct CvFrameVariable {
    int         block;              // 0: the procedure's own
    int         type;               // a number in the module's CvModule
    bool        param, placed, byAddress;
    long        offset;             // placed: from rbp
    int         ranged;             // its number in ;@var: its places are the ;@loc markers of that number; -1: not one
    std::string name;
    CvFrameVariable() : block(0), type(0), param(false), placed(false), byAddress(false), offset(0), ranged(-1) {}
};

// a range of code a variable is in one place over: a register (CodeView's number) or, reg 0, the frame at rbp + offset
struct CvPlaceRange {
    unsigned long start, end;
    int           reg;
    long          offset;
    CvPlaceRange() : start(0), end(0), reg(0), offset(0) {}
};

struct CvStaticLocal {
    int         block;
    int         type;
    std::string symbol, name;
    CvStaticLocal() : block(0), type(0) {}
};

// what the markers say about one procedure
struct CvProcedureMarks {
    int           type;             // ;@proc's T<n> (0: no -g)
    unsigned long frameBytes, savedBytes, frameFlags;
    std::vector<int>             blockParents;   // of block k at [k - 1]
    std::vector<CvFrameVariable> variables;      // in marker order: the parameters first
    std::vector<CvStaticLocal>   statics;
    std::vector<std::pair<unsigned long, std::string> > positions;   // code offset, ";@prologend" / ";@epilog" / ";@scope N" / ";@loc N <place>"
    CvProcedureMarks() : type(0), frameBytes(0), savedBytes(0), frameFlags(0) {}
};

class CvScopes {
public:
    // hasFrame: the procedure sets up rbp (PROC FRAME, and the markers say a frame size or saved registers)
    CvScopes(const CvProcedureMarks& marks, unsigned long codeSize, bool hasFrame);

    unsigned long dbgStart() const { return m_dbgStart; }
    unsigned long dbgEnd() const   { return m_dbgEnd; }
    bool ranged() const            { return m_ranged; }
    int  procedureFlags() const    { return m_ranged ? 0x80 : 0; }

    // the records between the procedure's S_GPROC32 and its S_END. A static local whose symbol is not in 'defined' has
    // no record (a relocation to it would add an undefined external)
    void write(CvSymbols& out, CvTypes& types, const std::string& fn, const std::set<std::string>& defined) const;

    // the code ranges (start, end) given to frame variables by offsets (S_DEFRANGE_REGISTER_REL), for CvCheck
    const std::vector<std::pair<unsigned long, unsigned long> >& offsetRanges() const { return m_offsetRanges; }
    // the ranges of the variables in registers (S_DEFRANGE_REGISTER), for CvCheck (c)
    const std::vector<CvRegisterRange>& registerRanges() const { return m_registerRanges; }
    // the code of scope -1 after DbgStart: the epilogues and the funclets' entries
    const std::vector<std::pair<unsigned long, unsigned long> >& outside() const { return m_outside; }
    // counts for the reports (docs/design-codeview.md section 6): blocks in pieces, variables without a place
    int blocksInPieces() const { return m_inPieces; }

private:
    typedef std::vector<std::pair<unsigned long, unsigned long> > Pieces;
    int  recordOf(int block) const;                 // the nearest block, itself or an ancestor, that has a record
    void placeRanges();                             // m_ranges from the ;@loc markers, each cut to its variable's block
    void writeScope(CvSymbols& out, CvTypes& types, const std::string& fn, const std::set<std::string>& defined, int scope) const;

    const CvProcedureMarks& m_marks;
    unsigned long           m_codeSize, m_dbgStart, m_dbgEnd;
    bool                    m_hasFrame, m_ranged;
    std::vector<Pieces>     m_pieces;               // of block k at [k] (0: the procedure, one piece)
    std::vector<int>        m_parent;               // [k]: block k's parent
    int                     m_inPieces;
    mutable std::vector<std::pair<unsigned long, unsigned long> > m_offsetRanges;
    std::vector<std::vector<CvPlaceRange> > m_ranges;   // of the variable at the same index of the markers' list (a ;@var one)
    std::vector<CvRegisterRange> m_registerRanges;
    std::vector<std::pair<unsigned long, unsigned long> > m_outside;
};

} // namespace cppparse

#endif
