// CvSection.hpp - one .debug$S section of an object.
//
// CV_SIGNATURE_C13 (4), then subsections of {kind (4 bytes), length (4), data, padding to four bytes}:
//     DEBUG_S_SYMBOLS (F1h)       the symbol records (CvSymbols)
//     DEBUG_S_LINES (F2h)         one per function: its line table (CvLines)
//     DEBUG_S_FILECHKSMS (F4h)    the module's file table (CvFiles), in the module's section only
//     DEBUG_S_STRINGTABLE (F3h)
// As cl lays it out: the functions of .text share one section, which also holds the module's symbols and its file
// table; a COMDAT function gets an associative section of its own (its symbols and lines) that refers to that table.
//
// Strict C++98.
#ifndef CPPPARSE_CVSECTION_HPP
#define CPPPARSE_CVSECTION_HPP

#include "CvFiles.hpp"
#include "CvLines.hpp"
#include "CvSymbols.hpp"

namespace cppparse {

class CvSection {
public:
    enum Kind { Symbols = 0xF1, Lines = 0xF2, Strings = 0xF3, Checksums = 0xF4 };

    // the symbols of its DEBUG_S_SYMBOLS subsection, the first one
    CvSymbols& symbols() { return m_symbols; }
    // a function's line table: the DEBUG_S_LINES subsections follow the symbols in the order they are added
    void add(const CvLines& lines);

    // appends the section to 'out': the signature, the symbols, the line tables and, given 'files', the file table
    void write(CvBytes& out, const CvFiles* files) const;

private:
    // a subsection appended to 'out'; the relocations of its data are moved to where the data is
    static void subsection(CvBytes& out, Kind kind, const CvBytes& data);

    CvSymbols m_symbols;
    CvBytes   m_lines;     // the DEBUG_S_LINES subsections added so far
};

} // namespace cppparse

#endif
