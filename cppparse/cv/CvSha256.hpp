// CvSha256.hpp - SHA-256 (FIPS 180-4, the Secure Hash Standard): the checksum of a source file in the file table of a
// -g object (CvFiles; docs/design-codeview.md 5.1), which a debugger holds against the file it is about to show.
//
// The message is given in pieces (add) and ends with digest(): the standard's padding (a 1 bit, zeros up to 56 bytes
// of a 64-byte block, the message's length in bits as 8 bytes, the highest first) and the eight state words, the
// highest byte of each first. The length is counted in bytes in 64 bits, so a message may have up to 2^61 - 1 bytes
// (the standard's limit of 2^64 - 1 bits); a longer one has no digest. A word of the algorithm is an unsigned long
// held to 32 bits, so that the result does not depend on how wide a long is.
//
// Strict C++98.
#ifndef CPPPARSE_CVSHA256_HPP
#define CPPPARSE_CVSHA256_HPP

#include "Int64.hpp"

#include <cstddef>
#include <string>

namespace cppparse {

class CvSha256 {
public:
    enum { Size = 32, BlockSize = 64 };   // the bytes of a digest, of a block of the message

    CvSha256();
    // the next bytes of the message
    void add(const char* bytes, size_t count);
    void add(const std::string& bytes) { add(bytes.data(), bytes.size()); }
    // the digest of the message so far, Size bytes; empty for a message beyond the standard's limit. It ends the
    // message: what is added afterwards is not counted
    std::string digest();

    // the digest of a message that is at hand whole
    static std::string of(const std::string& bytes);
    // the digest of a file's bytes as they are on disk; false, and 'digest' empty, when the file cannot be opened or
    // a read fails before its end
    static bool ofFile(const std::string& path, std::string& digest);
    // bytes as hexadecimal digits, two a byte, in lower case
    static std::string hex(const std::string& bytes);

private:
    // one block of 64 bytes into the state
    void block(const unsigned char* bytes);

    unsigned long m_state[8];
    unsigned char m_buffer[BlockSize];   // the bytes behind the last whole block
    size_t        m_fill;                // how many of them
    uint64        m_bytes;               // the message's length so far
    bool          m_tooLong, m_done;
};

} // namespace cppparse

#endif
