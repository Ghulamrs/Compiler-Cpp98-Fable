// Literal.cpp - escape sequences, string literal pieces and character constants decoded as cl does.
#include "Literal.hpp"

namespace cppparse {

namespace {

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// one source character or escape sequence of a literal's body at text[i] (before end); i moves past it. A
// universal character name gives its code point
unsigned long literalChar(const std::string& text, size_t& i, size_t end, bool& escape)
{
    const unsigned char c = static_cast<unsigned char>(text[i++]);
    escape = c == '\\' && i < end;
    if (!escape) return c;
    const char x = text[i++];
    switch (x) {
    case 'n': return 10;
    case 't': return 9;
    case 'v': return 11;
    case 'b': return 8;
    case 'r': return 13;
    case 'f': return 12;
    case 'a': return 7;
    case 'x': case 'u': case 'U': {
        // \x takes every hex digit that follows; \u four, \U eight
        const size_t most = x == 'x' ? end : x == 'u' ? 4 : 8;
        unsigned long v = 0;
        for (size_t k = 0; k < most && i < end && hexDigit(text[i]) >= 0; ++k, ++i) v = ((v << 4) | static_cast<unsigned long>(hexDigit(text[i]))) & 0xFFFFFFFFUL;
        return v;
    }
    default:
        if (x >= '0' && x <= '7') {
            unsigned long v = static_cast<unsigned long>(x - '0');
            for (int k = 1; k < 3 && i < end && text[i] >= '0' && text[i] <= '7'; ++k, ++i) v = v * 8 + static_cast<unsigned long>(text[i] - '0');
            return v;
        }
        return static_cast<unsigned char>(x);   // \\ \' \" \? and unknown escapes (cl: warning C4129) are the character
    }
}

// the code units of one character for a narrow or wide literal: a universal character name beyond 16 bits is a
// surrogate pair; beyond 8 bits in a narrow literal it is '?', as cl gives a character the code page lacks
void appendUnits(unsigned long v, bool universal, bool wide, std::vector<unsigned long>& units)
{
    if (wide && universal && v > 0xFFFFUL && v <= 0x10FFFFUL) {
        units.push_back(0xD800UL + ((v - 0x10000UL) >> 10));
        units.push_back(0xDC00UL + ((v - 0x10000UL) & 0x3FFUL));
        return;
    }
    if (!wide && universal && v > 0xFFUL) v = '?';
    units.push_back(v & (wide ? 0xFFFFUL : 0xFFUL));
}

bool isUniversal(const std::string& text, size_t at) { return text[at] == '\\' && at + 1 < text.size() && (text[at + 1] == 'u' || text[at + 1] == 'U'); }

} // namespace

bool stringLiteralUnits(const std::string& text, std::vector<unsigned long>& units)
{
    units.clear();
    // the bodies of the pieces, between their quotes; a narrow piece next to a wide one is widened
    std::vector<size_t> begins, ends;
    bool wide = false;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i] != '"') ++i;   // the prefix and the space between pieces
        if (i >= text.size()) break;
        if (i > 0 && text[i - 1] == 'L') wide = true;
        size_t end = ++i;
        while (end < text.size() && text[end] != '"') end += text[end] == '\\' ? 2 : 1;
        if (end > text.size()) end = text.size();
        begins.push_back(i);
        ends.push_back(end);
        i = end + 1;
    }
    for (size_t p = 0; p < begins.size(); ++p) {
        for (size_t k = begins[p]; k < ends[p]; ) {
            const bool universal = isUniversal(text, k);
            bool escape = false;
            const unsigned long v = literalChar(text, k, ends[p], escape);
            appendUnits(v, universal, wide, units);
        }
    }
    return wide;
}

long stringLiteralLength(const std::string& text)
{
    std::vector<unsigned long> units;
    stringLiteralUnits(text, units);
    return static_cast<long>(units.size());
}

// cl shifts each character of a multicharacter constant in from the right, until the second escape sequence:
// from there on each character goes above the ones before ('ab' 6162h, 'a\n\tb' 6209610Ah, '\x12\x34' 3412h)
long charLiteralValue(const std::string& text, int& count)
{
    count = 0;
    const bool wide = !text.empty() && text[0] == 'L';
    size_t i = wide ? 2 : 1;
    size_t end = text.size();
    if (end > i && text[end - 1] == '\'') --end;
    unsigned long v = 0, first = 0;
    int escapes = 0;
    bool above = false;
    while (i < end) {
        const bool universal = isUniversal(text, i);
        bool escape = false;
        unsigned long c = literalChar(text, i, end, escape);
        if (!wide && universal && c > 0xFFUL) c = '?';
        c &= wide ? 0xFFFFUL : 0xFFUL;
        if (count == 0) first = c;
        if (escape && ++escapes >= 2) above = true;
        if (above) { if (count < 4) v |= c << (8 * count); }
        else v = ((v << 8) | c) & 0xFFFFFFFFUL;
        ++count;
    }
    if (wide) return static_cast<long>(first);
    if (count <= 1) return static_cast<signed char>(static_cast<unsigned char>(first));
    return static_cast<long>(static_cast<int>(static_cast<unsigned int>(v & 0xFFFFFFFFUL)));
}

} // namespace cppparse
