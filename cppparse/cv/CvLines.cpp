// CvLines.cpp - see CvLines.hpp.
#include "CvLines.hpp"

namespace cppparse {

CvLines::CvLines(const std::string& fn, unsigned long fileEntry, unsigned long codeSize, const std::vector<std::pair<unsigned long, int> >& lines)
{
    std::vector<std::pair<unsigned long, int> > kept;
    for (size_t i = 0; i < lines.size(); ++i) if (lines[i].first < codeSize && lines[i].second > 0) kept.push_back(lines[i]);
    const unsigned long count = static_cast<unsigned long>(kept.size());
    m_table.secRel(fn);                              // offCon
    m_table.section(fn);                             // segCon
    m_table.put16(0);                                // flags (no columns)
    m_table.put32(codeSize);
    m_table.put32(fileEntry); m_table.put32(count); m_table.put32(12 + 8 * count);
    for (size_t i = 0; i < kept.size(); ++i) { m_table.put32(kept[i].first); m_table.put32((static_cast<unsigned long>(kept[i].second) & 0xFFFFFFUL) | 0x80000000UL); }
}

} // namespace cppparse
