// CvRecord.hpp - the bytes of CodeView debug information under construction (CvBytes) and their relocations (CvReloc).
//
// The folder cppparse/cv holds what is specific to CodeView, the debug information of cl's objects that link /DEBUG
// reads into the PDB, as classes the object writer (Coff.cpp) calls; they know nothing of the object writer's or
// the code generator's own types (docs/design-codeview.md, section 9):
//     CvBytes    bytes with their relocations: little-endian fields, names, numeric leaves, padding, a symbol
//                record, a type record
//     CvSymbols  the symbol records of a DEBUG_S_SYMBOLS subsection: the module's, a procedure's, a variable's
//     CvLines    a function's DEBUG_S_LINES table
//     CvFiles    the module's file table: the checksum entries and the string table
//     CvSection  one .debug$S: the signature and its subsections
//     CvNames    the names the records show, in cl's spelling
//     CvModule   with -g: what the module's types and variables are made from (CvType, CvClass, CvVariable)
//     CvTypes    with -g: the type records of .debug$T
//     CvCheck    the invariants of the records, checked on the bytes about to be written
// Every field is little-endian. An address is two fields, the offset in the section (4 bytes) and the section's
// number (2 bytes), which the linker fills in: a CvReloc of each kind names the symbol, the fields hold 0.
//
// Strict C++98.
#ifndef CPPPARSE_CVRECORD_HPP
#define CPPPARSE_CVRECORD_HPP

#include "Int64.hpp"

#include <string>
#include <vector>

namespace cppparse {

// the field at 'offset' of the bytes built gets the address of 'symbol': its offset in its section (SecRel, the
// object writer's IMAGE_REL_AMD64_SECREL) or its section's number (Section, IMAGE_REL_AMD64_SECTION)
struct CvReloc {
    enum Kind { SecRel, Section };
    unsigned long offset;
    std::string   symbol;
    Kind          kind;
};

class CvBytes {
public:
    void put8(int v) { m_bytes += static_cast<char>(v & 0xFF); }
    void put16(unsigned long v);
    void put32(unsigned long v);
    // the characters and the 0 that ends them
    void name(const std::string& text);
    // zeros up to a multiple of four bytes
    void pad4();
    // the two fields of an address, each 0 with its relocation against 'symbol' (the offset field holds 'addend': an
    // address within the symbol's code, such as a block's start in its function)
    void secRel(const std::string& symbol, unsigned long addend = 0);    // 4 bytes
    void section(const std::string& symbol);   // 2 bytes
    // the bytes of 'other', its relocations moved to where they are here
    void append(const CvBytes& other);
    // a symbol record: the length of what follows (2 bytes), the kind (2), the body padded with zeros to four bytes
    void symbolRecord(unsigned long kind, const CvBytes& body);
    // bytes as they are (no relocations)
    void raw(const std::string& bytes) { m_bytes += bytes; }
    // a numeric leaf, the form of a size, an offset or a value in a type record and in S_CONSTANT: a value from 0 to
    // 7FFFh as its two bytes, any other as the leaf of the smallest form that holds it and the value: LF_CHAR (8000h,
    // 1 byte), LF_SHORT (8001h), LF_USHORT (8002h), LF_LONG (8003h), LF_ULONG (8004h), LF_QUADWORD (8009h),
    // LF_UQUADWORD (800Ah). isUnsigned: v holds an unsigned 64-bit value
    void numeric(int64 v, bool isUnsigned);
    // LF_PAD bytes (F3h, F2h, F1h: the number of bytes to the boundary) up to a multiple of four bytes: the padding
    // of a type record and of a member of a field list
    void padLeaf();
    // a type record: the length of what follows (2 bytes), the leaf (2), the body, LF_PAD bytes to four bytes
    void typeRecord(unsigned long leaf, const CvBytes& body);

    unsigned long size() const { return static_cast<unsigned long>(m_bytes.size()); }
    const std::string& bytes() const { return m_bytes; }
    const std::vector<CvReloc>& relocs() const { return m_relocs; }

private:
    void relocation(const std::string& symbol, CvReloc::Kind kind);

    std::string          m_bytes;
    std::vector<CvReloc> m_relocs;
};

} // namespace cppparse

#endif
