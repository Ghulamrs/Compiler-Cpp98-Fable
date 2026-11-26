// CvFiles.cpp - see CvFiles.hpp.
#include "CvFiles.hpp"

namespace cppparse {

CvFiles::CvFiles(const CvModule* sources) : m_sources(sources)
{
    m_strings.put8(0);
    if (m_sources)
        for (size_t i = 0; i < m_sources->sources().size(); ++i) entry(m_sources->sources()[i].name);
}

unsigned long CvFiles::entry(const std::string& asNamed)
{
    // -g: a file the unit read has one entry, under the spelling it was first read by, whatever the case of the
    // letters a later inclusion names it with (cl's table has it once)
    const CvSource* const source = m_sources ? m_sources->source(asNamed) : 0;
    const std::string& name = source ? source->name : asNamed;
    std::map<std::string, unsigned long>::const_iterator it = m_offsets.find(name);
    if (it != m_offsets.end()) return it->second;
    const unsigned long off = m_checksums.size();
    m_checksums.put32(m_strings.size());
    m_strings.name(name);
    const std::string sum = source ? source->checksum : std::string();
    if (sum.size() == static_cast<size_t>(Sha256Bytes)) {
        m_checksums.put8(Sha256Bytes); m_checksums.put8(Sha256);
        m_checksums.raw(sum);
    } else {
        m_checksums.put8(0); m_checksums.put8(NoChecksum);
    }
    m_checksums.pad4();
    m_offsets[name] = off;
    return off;
}

} // namespace cppparse
