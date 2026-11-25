// CvFiles.cpp - see CvFiles.hpp.
#include "CvFiles.hpp"

namespace cppparse {

CvFiles::CvFiles()
{
    m_strings.put8(0);
}

unsigned long CvFiles::entry(const std::string& name)
{
    std::map<std::string, unsigned long>::const_iterator it = m_offsets.find(name);
    if (it != m_offsets.end()) return it->second;
    const unsigned long off = m_checksums.size();
    m_checksums.put32(m_strings.size());
    m_strings.name(name);
    m_checksums.put8(0); m_checksums.put8(0);        // cbChecksum 0, type 0: no checksum
    m_checksums.pad4();
    m_offsets[name] = off;
    return off;
}

} // namespace cppparse
