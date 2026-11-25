// CvSection.cpp - see CvSection.hpp.
#include "CvSection.hpp"

namespace cppparse {

void CvSection::subsection(CvBytes& out, Kind kind, const CvBytes& data)
{
    out.put32(static_cast<unsigned long>(kind)); out.put32(data.size());
    out.append(data);
    out.pad4();
}

void CvSection::add(const CvLines& lines)
{
    subsection(m_lines, Lines, lines.table());
}

void CvSection::write(CvBytes& out, const CvFiles* files) const
{
    out.put32(4);                                    // CV_SIGNATURE_C13
    subsection(out, Symbols, m_symbols.records());
    out.append(m_lines);
    if (files) {
        subsection(out, Checksums, files->checksums());
        subsection(out, Strings, files->strings());
    }
}

} // namespace cppparse
