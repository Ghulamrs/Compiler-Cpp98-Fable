// CvFiles.hpp - the module's file table: the source files its line tables name.
//
//     DEBUG_S_FILECHKSMS (F4h)    per file {the offset of its name in the string table, the checksum's length 0,
//                                 its type 0 (none), padding to four bytes}
//     DEBUG_S_STRINGTABLE (F3h)   "" and then the file names, each ended by a 0
// A line table names its file by the offset of the file's entry in the checksum table.
//
// Strict C++98.
#ifndef CPPPARSE_CVFILES_HPP
#define CPPPARSE_CVFILES_HPP

#include "CvRecord.hpp"

#include <map>

namespace cppparse {

class CvFiles {
public:
    CvFiles();
    // the offset of the file's entry in the checksum table; a name not seen before gets an entry
    unsigned long entry(const std::string& name);

    const CvBytes& checksums() const { return m_checksums; }
    const CvBytes& strings() const { return m_strings; }

private:
    CvBytes                              m_strings, m_checksums;
    std::map<std::string, unsigned long> m_offsets;
};

} // namespace cppparse

#endif
