// EhUnwindInfo.hpp - the unwind tables of a function, as bytes for the object writer (Coff.cpp).
//
// EhUnwindInfo: the UNWIND_INFO of a PROC FRAME (Windows x64 exception handling): version 1, the flags, the prologue
// size, the unwind codes in reverse prologue order (each code slot: code offset, then operation | info << 4; larger
// values in the slots after it), padded to an even number of slots.
// EhRangeTable: what follows it in the table model (PROC FRAME:cppparse_eh_handler): the handler's address, then
// cppparse's range table, "CPPX", the entry count, the pad count, the entries, the landing pads. The runtime's
// walker reads it (runtime/cppparse_rt.cpp dispatch).
// Every address is image-relative (IMAGE_REL_AMD64_ADDR32NB): an EhReloc names the symbol, the field holds the addend.
//
// Strict C++98.
#ifndef CPPPARSE_EHUNWINDINFO_HPP
#define CPPPARSE_EHUNWINDINFO_HPP

#include <string>
#include <vector>

namespace cppparse {

// one prologue step of a PROC FRAME, from its unwind directive: where the instruction before it ends, and what it did
struct EhUnwindStep {
    enum Op { Push, Alloc, SaveXmm };
    Op            op;
    int           reg;        // Push: register number; SaveXmm: xmm number
    unsigned long value;      // Alloc: bytes; SaveXmm: offset from rsp after the prologue
    unsigned long offset;     // code offset just after the instruction
};

// an image-relative address at 'offset' of the bytes built: of 'symbol' plus the addend the field holds
struct EhReloc {
    unsigned long offset;
    std::string   symbol;
};

class EhUnwindInfo {
public:
    enum { FlagEHandler = 1, FlagUHandler = 2 };
    // appends the header and the codes of a prologue that begins at code offset 'start' (0: the function's own; a
    // funclet's begins where the funclet does) and ends at prologEnd; false with 'err' set
    static bool write(const std::vector<EhUnwindStep>& steps, long prologEnd, unsigned long start, int flags, std::vector<unsigned char>& out, std::string& err);
};

// a code range of a try region and its landing pad, as code offsets in the function
struct EhRange { unsigned long begin, end, pad; };

class EhRangeTable {
public:
    static const unsigned long kMagic;   // "CPPX": the first word of the table
    // appends the handler's address and the range table of function 'fn': the ranges, sorted by their begin, become
    // entries of one word each: the code offset in the function (20 bits) and the landing pad's number in the pad
    // table that follows (12 bits, 0: none); an entry holds up to the next one's offset, a pad of 0 marks code outside
    // every region (a gap between two ranges, the end of the last one). The entries are plain values; only the pad
    // table's addresses need relocations
    static bool write(const std::string& fn, const std::string& handler, const std::vector<EhRange>& ranges,
                      std::vector<unsigned char>& out, std::vector<EhReloc>& relocs, std::string& err);
};

} // namespace cppparse

#endif
