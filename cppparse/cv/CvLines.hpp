// CvLines.hpp - a function's line table, the data of a DEBUG_S_LINES (F2h) subsection.
//
//     {the function's address (offset, section), flags 0 (no columns), the code's length}
//     one block: {the file's entry in the checksum table (CvFiles), the number of lines, the block's length,
//                 per line {code offset, the source line | 80000000h (a statement)}}
// The lines are the listing's ";@line N" markers as the encoder recorded them: (code offset, source line) pairs in
// offset order, one line per offset. A pair behind the code or without a line is left out.
//
// Strict C++98.
#ifndef CPPPARSE_CVLINES_HPP
#define CPPPARSE_CVLINES_HPP

#include "CvRecord.hpp"

#include <utility>

namespace cppparse {

class CvLines {
public:
    // the table of the function with symbol 'fn', 'codeSize' bytes of code, its source the file of 'fileEntry'
    CvLines(const std::string& fn, unsigned long fileEntry, unsigned long codeSize, const std::vector<std::pair<unsigned long, int> >& lines);

    const CvBytes& table() const { return m_table; }

private:
    CvBytes m_table;
};

} // namespace cppparse

#endif
