// CvRecord.cpp - see CvRecord.hpp.
#include "CvRecord.hpp"

namespace cppparse {

void CvBytes::put16(unsigned long v)
{
    put8(static_cast<int>(v)); put8(static_cast<int>(v >> 8));
}

void CvBytes::put32(unsigned long v)
{
    for (int i = 0; i < 4; ++i) put8(static_cast<int>(v >> (8 * i)));
}

void CvBytes::name(const std::string& text)
{
    m_bytes += text; m_bytes += '\0';
}

void CvBytes::pad4()
{
    while (m_bytes.size() % 4) put8(0);
}

void CvBytes::relocation(const std::string& symbol, CvReloc::Kind kind)
{
    CvReloc r; r.offset = size(); r.symbol = symbol; r.kind = kind;
    m_relocs.push_back(r);
}

void CvBytes::secRel(const std::string& symbol, unsigned long addend)
{
    relocation(symbol, CvReloc::SecRel);
    put32(addend);
}

void CvBytes::section(const std::string& symbol)
{
    relocation(symbol, CvReloc::Section);
    put16(0);
}

void CvBytes::append(const CvBytes& other)
{
    const unsigned long base = size();
    for (size_t r = 0; r < other.m_relocs.size(); ++r) { CvReloc rl = other.m_relocs[r]; rl.offset += base; m_relocs.push_back(rl); }
    m_bytes += other.m_bytes;
}

void CvBytes::symbolRecord(unsigned long kind, const CvBytes& body)
{
    const unsigned long padded = (body.size() + 3) / 4 * 4;
    put16(padded + 2); put16(kind);
    append(body);
    for (unsigned long k = body.size(); k < padded; ++k) put8(0);
}

void CvBytes::numeric(int64 v, bool isUnsigned)
{
    const uint64 u = static_cast<uint64>(v);
    const bool negative = !isUnsigned && v < 0;
    if (!negative && u < 0x8000) { put16(static_cast<unsigned long>(u)); return; }
    int bytes;
    if (negative) {
        if (v >= -128) { put16(0x8000); bytes = 1; }
        else if (v >= -32768) { put16(0x8001); bytes = 2; }
        else if (v >= -static_cast<int64>(0x7FFFFFFF) - 1) { put16(0x8003); bytes = 4; }
        else { put16(0x8009); bytes = 8; }
    }
    else if (u <= 0xFFFF) { put16(0x8002); bytes = 2; }
    else if (u <= 0x7FFFFFFF) { put16(0x8003); bytes = 4; }
    else if (u <= 0xFFFFFFFFu) { put16(0x8004); bytes = 4; }
    else if (u <= static_cast<uint64>(int64Max)) { put16(0x8009); bytes = 8; }
    else { put16(0x800A); bytes = 8; }
    for (int i = 0; i < bytes; ++i) put8(static_cast<int>((u >> (8 * i)) & 0xFF));
}

void CvBytes::padLeaf()
{
    while (m_bytes.size() % 4) put8(0xF0 + static_cast<int>(4 - m_bytes.size() % 4));
}

void CvBytes::typeRecord(unsigned long leaf, const CvBytes& body)
{
    // with its length field the record is a multiple of four bytes
    const unsigned long padded = (4 + body.size() + 3) / 4 * 4;
    put16(padded - 2); put16(leaf);
    append(body);
    for (unsigned long k = 4 + body.size(); k < padded; ++k) put8(0xF0 + static_cast<int>(padded - k));
}

} // namespace cppparse
