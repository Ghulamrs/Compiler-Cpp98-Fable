// CvFiles.hpp - the module's file table: the source files its line tables name and, with -g, every file of the unit.
//
//     DEBUG_S_FILECHKSMS (F4h)    per file {the offset of its name in the string table (4 bytes), the checksum's
//                                 length (1), its kind (1), the checksum, zeros to four bytes}
//     DEBUG_S_STRINGTABLE (F3h)   "" and then the file names, each ended by a 0
// A line table names its file by the offset of the file's entry in the checksum table.
//
// Without -g the table has the files that line tables name, in the order they are first named, and an entry has no
// checksum: length 0, kind 0 (none), 8 bytes. With -g the table is cl 19.52's: first every file the unit read (the
// module's description has them, CvModule's sources, in the order of reading: the unit, then each header where it
// is first included), whether a line table names it or not, and an entry carries the SHA-256 the description holds
// for the file (of the bytes the preprocessor read, hashed by the driver with CvSha256): length 32, kind 3, 40 bytes
// with the two zeros behind the checksum. A debugger holds it against the file it is about to show. A name the unit
// read no file by, which only a #line directive gives, has an entry without a checksum under -g too, as it has in
// cl's objects.
//
// Strict C++98.
#ifndef CPPPARSE_CVFILES_HPP
#define CPPPARSE_CVFILES_HPP

#include "CvModule.hpp"
#include "CvRecord.hpp"

#include <map>

namespace cppparse {

class CvFiles {
public:
    // CodeView's kinds of checksum (CHKSUM_TYPE_NONE, CHKSUM_TYPE_SHA_256; 1 and 2, MD5 and SHA-1, are never written)
    enum { NoChecksum = 0, Sha256 = 3, Sha256Bytes = 32 };

    // sources: with -g, the description whose sources are the first entries and give each its checksum; 0: the
    // table has what entry() is asked for, and no entry has a checksum
    explicit CvFiles(const CvModule* sources = 0);
    // the offset of the file's entry in the checksum table; a name not seen before gets an entry (with sources: a
    // name of one of them in another case of its letters is that source's entry)
    unsigned long entry(const std::string& name);

    const CvBytes& checksums() const { return m_checksums; }
    const CvBytes& strings() const { return m_strings; }

private:
    const CvModule*                      m_sources;
    CvBytes                              m_strings, m_checksums;
    std::map<std::string, unsigned long> m_offsets;
};

} // namespace cppparse

#endif
