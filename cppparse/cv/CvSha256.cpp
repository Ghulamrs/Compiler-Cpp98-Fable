// CvSha256.cpp - see CvSha256.hpp.
#include "CvSha256.hpp"

#include <cstdio>
#include <vector>

namespace cppparse {

namespace {

// FIPS 180-4, 4.2.2: the first 32 bits of the fractional parts of the cube roots of the first 64 primes
const unsigned long kRound[64] = {
    0x428A2F98UL, 0x71374491UL, 0xB5C0FBCFUL, 0xE9B5DBA5UL, 0x3956C25BUL, 0x59F111F1UL, 0x923F82A4UL, 0xAB1C5ED5UL,
    0xD807AA98UL, 0x12835B01UL, 0x243185BEUL, 0x550C7DC3UL, 0x72BE5D74UL, 0x80DEB1FEUL, 0x9BDC06A7UL, 0xC19BF174UL,
    0xE49B69C1UL, 0xEFBE4786UL, 0x0FC19DC6UL, 0x240CA1CCUL, 0x2DE92C6FUL, 0x4A7484AAUL, 0x5CB0A9DCUL, 0x76F988DAUL,
    0x983E5152UL, 0xA831C66DUL, 0xB00327C8UL, 0xBF597FC7UL, 0xC6E00BF3UL, 0xD5A79147UL, 0x06CA6351UL, 0x14292967UL,
    0x27B70A85UL, 0x2E1B2138UL, 0x4D2C6DFCUL, 0x53380D13UL, 0x650A7354UL, 0x766A0ABBUL, 0x81C2C92EUL, 0x92722C85UL,
    0xA2BFE8A1UL, 0xA81A664BUL, 0xC24B8B70UL, 0xC76C51A3UL, 0xD192E819UL, 0xD6990624UL, 0xF40E3585UL, 0x106AA070UL,
    0x19A4C116UL, 0x1E376C08UL, 0x2748774CUL, 0x34B0BCB5UL, 0x391C0CB3UL, 0x4ED8AA4AUL, 0x5B9CCA4FUL, 0x682E6FF3UL,
    0x748F82EEUL, 0x78A5636FUL, 0x84C87814UL, 0x8CC70208UL, 0x90BEFFFAUL, 0xA4506CEBUL, 0xBEF9A3F7UL, 0xC67178F2UL
};

const unsigned long kWord = 0xFFFFFFFFUL;

// a 32-bit word turned right by n bits (0 < n < 32)
inline unsigned long rotr(unsigned long x, int n)
{
    return ((x >> n) | (x << (32 - n))) & kWord;
}

// the longest message that has a length in bits below 2^64
const uint64 kMaxBytes = uint64Max >> 3;

} // namespace

CvSha256::CvSha256() : m_fill(0), m_bytes(0), m_tooLong(false), m_done(false)
{
    // 5.3.3: the first 32 bits of the fractional parts of the square roots of the first eight primes
    m_state[0] = 0x6A09E667UL; m_state[1] = 0xBB67AE85UL; m_state[2] = 0x3C6EF372UL; m_state[3] = 0xA54FF53AUL;
    m_state[4] = 0x510E527FUL; m_state[5] = 0x9B05688CUL; m_state[6] = 0x1F83D9ABUL; m_state[7] = 0x5BE0CD19UL;
    for (size_t i = 0; i < BlockSize; ++i) m_buffer[i] = 0;
}

void CvSha256::block(const unsigned char* bytes)
{
    // 6.2.2: the message schedule, then 64 rounds over the eight working variables
    unsigned long w[64];
    for (int t = 0; t < 16; ++t)
        w[t] = (static_cast<unsigned long>(bytes[4 * t]) << 24) | (static_cast<unsigned long>(bytes[4 * t + 1]) << 16)
             | (static_cast<unsigned long>(bytes[4 * t + 2]) << 8) | static_cast<unsigned long>(bytes[4 * t + 3]);
    for (int t = 16; t < 64; ++t) {
        const unsigned long s0 = rotr(w[t - 15], 7) ^ rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
        const unsigned long s1 = rotr(w[t - 2], 17) ^ rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
        w[t] = (w[t - 16] + s0 + w[t - 7] + s1) & kWord;
    }
    unsigned long a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
    unsigned long e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];
    for (int t = 0; t < 64; ++t) {
        const unsigned long big1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const unsigned long ch = (e & f) ^ (~e & kWord & g);
        const unsigned long t1 = (h + big1 + ch + kRound[t] + w[t]) & kWord;
        const unsigned long big0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const unsigned long maj = (a & b) ^ (a & c) ^ (b & c);
        const unsigned long t2 = (big0 + maj) & kWord;
        h = g; g = f; f = e; e = (d + t1) & kWord;
        d = c; c = b; b = a; a = (t1 + t2) & kWord;
    }
    m_state[0] = (m_state[0] + a) & kWord; m_state[1] = (m_state[1] + b) & kWord;
    m_state[2] = (m_state[2] + c) & kWord; m_state[3] = (m_state[3] + d) & kWord;
    m_state[4] = (m_state[4] + e) & kWord; m_state[5] = (m_state[5] + f) & kWord;
    m_state[6] = (m_state[6] + g) & kWord; m_state[7] = (m_state[7] + h) & kWord;
}

void CvSha256::add(const char* bytes, size_t count)
{
    if (m_done || count == 0) return;
    if (m_tooLong || static_cast<uint64>(count) > kMaxBytes - m_bytes) { m_tooLong = true; return; }
    m_bytes += static_cast<uint64>(count);
    const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes);
    if (m_fill != 0) {
        // fill the block that is begun
        while (count != 0 && m_fill < BlockSize) { m_buffer[m_fill++] = *p++; --count; }
        if (m_fill < BlockSize) return;
        block(m_buffer);
        m_fill = 0;
    }
    for (; count >= BlockSize; p += BlockSize, count -= BlockSize) block(p);
    for (; count != 0; --count) m_buffer[m_fill++] = *p++;
}

std::string CvSha256::digest()
{
    if (m_tooLong) return std::string();
    if (!m_done) {
        // 5.1.1: a 1 bit, zeros up to byte 56 of a block (of the next one when this one is too full), the length in bits
        const uint64 bits = m_bytes << 3;
        m_buffer[m_fill++] = 0x80;
        if (m_fill > 56) {
            while (m_fill < BlockSize) m_buffer[m_fill++] = 0;
            block(m_buffer);
            m_fill = 0;
        }
        while (m_fill < 56) m_buffer[m_fill++] = 0;
        for (int i = 0; i < 8; ++i) m_buffer[56 + i] = static_cast<unsigned char>((bits >> (8 * (7 - i))) & 0xFF);
        block(m_buffer);
        m_fill = 0;
        m_done = true;
    }
    std::string out;
    for (int i = 0; i < 8; ++i)
        for (int k = 3; k >= 0; --k) out += static_cast<char>(static_cast<unsigned char>((m_state[i] >> (8 * k)) & 0xFF));
    return out;
}

std::string CvSha256::of(const std::string& bytes)
{
    CvSha256 sum;
    sum.add(bytes);
    return sum.digest();
}

bool CvSha256::ofFile(const std::string& path, std::string& digest)
{
    digest.clear();
    // C's own file, whose error indicator tells a failed read from the file's end (a stream gives both as a short
    // count); MSVC marks fopen as deprecated for its fopen_s, which is not C++98
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    std::FILE* const in = std::fopen(path.c_str(), "rb");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    if (!in) return false;
    CvSha256 sum;
    // pieces of a length that is no multiple of a block, so that a file of any size takes add() through a block that
    // is begun as well as through whole ones
    std::vector<char> piece(65521);
    bool failed = false;
    for (;;) {
        const size_t got = std::fread(&piece[0], 1, piece.size(), in);
        if (got != 0) sum.add(&piece[0], got);
        if (got < piece.size()) { failed = std::ferror(in) != 0; break; }
    }
    std::fclose(in);
    if (failed) return false;
    digest = sum.digest();
    return !digest.empty();
}

std::string CvSha256::hex(const std::string& bytes)
{
    static const char kDigits[] = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < bytes.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(bytes[i]);
        out += kDigits[b >> 4];
        out += kDigits[b & 15];
    }
    return out;
}

} // namespace cppparse
