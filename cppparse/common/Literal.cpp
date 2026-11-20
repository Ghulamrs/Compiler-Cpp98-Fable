// Literal.cpp - escape sequences, string literal pieces and character constants decoded as cl does.
#include "Literal.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace cppparse {

namespace {

// a non-negative integer in 32-bit words, least significant first: exact comparisons of decimal literals
typedef std::vector<unsigned int> BigNum;

void bigMulAdd(BigNum& a, unsigned int m, unsigned int add)
{
    uint64 carry = add;
    for (size_t i = 0; i < a.size(); ++i) {
        const uint64 p = static_cast<uint64>(a[i]) * m + carry;
        a[i] = static_cast<unsigned int>(p & 0xFFFFFFFFu);
        carry = p >> 32;
    }
    if (carry) a.push_back(static_cast<unsigned int>(carry));
}

void bigPow(BigNum& a, unsigned int base, long n)
{
    for (; n > 0; --n) bigMulAdd(a, base, 0);
}

int bigCompare(BigNum a, BigNum b)
{
    while (!a.empty() && a.back() == 0) a.pop_back();
    while (!b.empty() && b.back() == 0) b.pop_back();
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (size_t i = a.size(); i-- > 0; ) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

// the decimal literal s (no sign, no suffix) compared exactly with the positive finite double m
int compareDecimal(const std::string& s, double m)
{
    BigNum left, right;
    long exp10 = 0;
    bool point = false;
    size_t i = 0;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (c >= '0' && c <= '9') { bigMulAdd(left, 10, static_cast<unsigned int>(c - '0')); if (point) --exp10; }
        else if (c == '.' && !point) point = true;
        else break;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        const bool negative = i < s.size() && s[i] == '-';
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
        long e = 0;
        for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i) if (e < 100000) e = e * 10 + (s[i] - '0');
        exp10 += negative ? -e : e;
    }
    int e2 = 0;
    const double mant = std::frexp(m, &e2);
    const uint64 whole = static_cast<uint64>(std::ldexp(mant, 53));
    right.push_back(static_cast<unsigned int>(whole & 0xFFFFFFFFu));
    right.push_back(static_cast<unsigned int>(whole >> 32));
    const long k = static_cast<long>(e2) - 53;
    if (exp10 > 0) bigPow(left, 10, exp10); else bigPow(right, 10, -exp10);
    if (k > 0) bigPow(right, 2, k); else bigPow(left, 2, -k);
    return bigCompare(left, right);
}

// a positive float as a double, its infinity as 2^128 (the float above the largest one when rounding)
double floatAsDouble(float v) { return v > 3.4028235e38f ? std::ldexp(1.0, 128) : static_cast<double>(v); }

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

// the byte of a code point in code page 1252, cl's execution character set for narrow literals on a Western
// Windows: Latin-1 but for the 27 characters the page holds at 0x80 to 0x9F; '?' for a character it lacks
unsigned long narrowOf(unsigned long v)
{
    static const unsigned short high[32] = {
        0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
        0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178
    };
    if (v < 0x80UL || (v >= 0xA0UL && v <= 0xFFUL)) return v;
    for (unsigned long k = 0; k < 32; ++k) if (high[k] == v) return 0x80UL + k;
    return '?';
}

// the code units of one character for a narrow or wide literal. A universal character name beyond 16 bits is a
// surrogate pair in a wide literal. In a narrow one it is its byte in the code page, '?' for a character the
// page lacks and two of them beyond 16 bits (cl converts the UTF-16 units one by one); a narrow piece joined to
// a wide literal (narrowPiece) is converted to the code page first and widened from there, so what the page
// lacks is '?' there too
void appendUnits(unsigned long v, bool universal, bool wide, bool narrowPiece, std::vector<unsigned long>& units)
{
    if (universal && v > 0xFFFFUL && v <= 0x10FFFFUL) {
        if (wide && !narrowPiece) {
            units.push_back(0xD800UL + ((v - 0x10000UL) >> 10));
            units.push_back(0xDC00UL + ((v - 0x10000UL) & 0x3FFUL));
        } else { units.push_back('?'); units.push_back('?'); }
        return;
    }
    if (universal && (!wide || narrowPiece)) { const unsigned long b = narrowOf(v); if (!wide || b == '?') v = b; }
    units.push_back(v & (wide ? 0xFFFFUL : 0xFFUL));
}

bool isUniversal(const std::string& text, size_t at) { return text[at] == '\\' && at + 1 < text.size() && (text[at + 1] == 'u' || text[at + 1] == 'U'); }

} // namespace

bool universalCharacterName(const std::string& text, size_t at, unsigned long& value, size_t& len)
{
    if (at + 1 >= text.size() || text[at] != '\\' || (text[at + 1] != 'u' && text[at + 1] != 'U')) return false;
    const size_t digits = text[at + 1] == 'u' ? 4 : 8;
    if (at + 2 + digits > text.size()) return false;
    unsigned long v = 0;
    for (size_t k = 0; k < digits; ++k) {
        const int h = hexDigit(text[at + 2 + k]);
        if (h < 0) return false;
        v = (v << 4) | static_cast<unsigned long>(h);
    }
    value = v;
    len = 2 + digits;
    return true;
}

const char* universalCharacterError(unsigned long v)
{
    if (v > 0x10FFFFUL || (v >= 0xD800UL && v <= 0xDFFFUL)) return "a universal-character-name specifies an invalid character (cl C3850)";
    if (v < 0x20UL || (v >= 0x7FUL && v <= 0x9FUL)) return "a universal-character-name specifies a control character (C++98 2.2/2; cl C3850)";
    // the basic source character set: every character below 0x7F but $ @ and `
    if (v < 0x7FUL && v != 0x24UL && v != 0x40UL && v != 0x60UL) return "a universal-character-name cannot designate a character in the basic character set (C++98 2.2/2; cl C3851)";
    return 0;
}

bool identifierCharacter(unsigned long v)
{
    // Annex E of ISO/IEC 14882:1998, in its order: Latin, Greek, Cyrillic, Armenian, Hebrew, Arabic, Devanagari,
    // Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, Malayalam, Thai, Lao, Georgian, Hiragana,
    // Katakana, Bopomofo, Hangul, CJK unified ideographs
    static const unsigned short ranges[][2] = {
        {0x00C0, 0x00D6}, {0x00D8, 0x00F6}, {0x00F8, 0x01F5}, {0x01FA, 0x0217}, {0x0250, 0x02A8}, {0x1E00, 0x1E9A}, {0x1EA0, 0x1EF9},
        {0x0384, 0x0384}, {0x0388, 0x038A}, {0x038C, 0x038C}, {0x038E, 0x03A1}, {0x03A3, 0x03CE}, {0x03D0, 0x03D6}, {0x03DA, 0x03DA},
        {0x03DC, 0x03DC}, {0x03DE, 0x03DE}, {0x03E0, 0x03E0}, {0x03E2, 0x03F3}, {0x1F00, 0x1F15}, {0x1F18, 0x1F1D}, {0x1F20, 0x1F45},
        {0x1F48, 0x1F4D}, {0x1F50, 0x1F57}, {0x1F59, 0x1F59}, {0x1F5B, 0x1F5B}, {0x1F5D, 0x1F5D}, {0x1F5F, 0x1F7D}, {0x1F80, 0x1FB4},
        {0x1FB6, 0x1FBC}, {0x1FC2, 0x1FC4}, {0x1FC6, 0x1FCC}, {0x1FD0, 0x1FD3}, {0x1FD6, 0x1FDB}, {0x1FE0, 0x1FEC}, {0x1FF2, 0x1FF4},
        {0x1FF6, 0x1FFC},
        {0x0401, 0x040D}, {0x040F, 0x044F}, {0x0451, 0x045C}, {0x045E, 0x0481}, {0x0490, 0x04C4}, {0x04C7, 0x04C8}, {0x04CB, 0x04CC},
        {0x04D0, 0x04EB}, {0x04EE, 0x04F5}, {0x04F8, 0x04F9},
        {0x0531, 0x0556}, {0x0561, 0x0587},
        {0x05D0, 0x05EA}, {0x05F0, 0x05F4},
        {0x0621, 0x063A}, {0x0640, 0x0652}, {0x0670, 0x06B7}, {0x06BA, 0x06BE}, {0x06C0, 0x06CE}, {0x06E5, 0x06E7},
        {0x0905, 0x0939}, {0x0958, 0x0962},
        {0x0985, 0x098C}, {0x098F, 0x0990}, {0x0993, 0x09A8}, {0x09AA, 0x09B0}, {0x09B2, 0x09B2}, {0x09B6, 0x09B9}, {0x09DC, 0x09DD},
        {0x09DF, 0x09E1}, {0x09F0, 0x09F1},
        {0x0A05, 0x0A0A}, {0x0A0F, 0x0A10}, {0x0A13, 0x0A28}, {0x0A2A, 0x0A30}, {0x0A32, 0x0A33}, {0x0A35, 0x0A36}, {0x0A38, 0x0A39},
        {0x0A59, 0x0A5C}, {0x0A5E, 0x0A5E},
        {0x0A85, 0x0A8B}, {0x0A8D, 0x0A8D}, {0x0A8F, 0x0A91}, {0x0A93, 0x0AA8}, {0x0AAA, 0x0AB0}, {0x0AB2, 0x0AB3}, {0x0AB5, 0x0AB9},
        {0x0AE0, 0x0AE0},
        {0x0B05, 0x0B0C}, {0x0B0F, 0x0B10}, {0x0B13, 0x0B28}, {0x0B2A, 0x0B30}, {0x0B32, 0x0B33}, {0x0B36, 0x0B39}, {0x0B5C, 0x0B5D},
        {0x0B5F, 0x0B61},
        {0x0B85, 0x0B8A}, {0x0B8E, 0x0B90}, {0x0B92, 0x0B95}, {0x0B99, 0x0B9A}, {0x0B9C, 0x0B9C}, {0x0B9E, 0x0B9F}, {0x0BA3, 0x0BA4},
        {0x0BA8, 0x0BAA}, {0x0BAE, 0x0BB5}, {0x0BB7, 0x0BB9},
        {0x0C05, 0x0C0C}, {0x0C0E, 0x0C10}, {0x0C12, 0x0C28}, {0x0C2A, 0x0C33}, {0x0C35, 0x0C39}, {0x0C60, 0x0C61},
        {0x0C85, 0x0C8C}, {0x0C8E, 0x0C90}, {0x0C92, 0x0CA8}, {0x0CAA, 0x0CB3}, {0x0CB5, 0x0CB9}, {0x0CE0, 0x0CE1},
        {0x0D05, 0x0D0C}, {0x0D0E, 0x0D10}, {0x0D12, 0x0D28}, {0x0D2A, 0x0D39}, {0x0D60, 0x0D61},
        {0x0E01, 0x0E30}, {0x0E32, 0x0E33}, {0x0E40, 0x0E46}, {0x0E4F, 0x0E5B},
        {0x0E81, 0x0E82}, {0x0E84, 0x0E84}, {0x0E87, 0x0E88}, {0x0E8A, 0x0E8A}, {0x0E8D, 0x0E8D}, {0x0E94, 0x0E97}, {0x0E99, 0x0E9F},
        {0x0EA1, 0x0EA3}, {0x0EA5, 0x0EA5}, {0x0EA7, 0x0EA7}, {0x0EAA, 0x0EAB}, {0x0EAD, 0x0EB0}, {0x0EB2, 0x0EB3}, {0x0EBD, 0x0EBD},
        {0x0EC0, 0x0EC4}, {0x0EC6, 0x0EC6},
        {0x10A0, 0x10C5}, {0x10D0, 0x10F6},
        {0x3041, 0x3094}, {0x309B, 0x309E},
        {0x30A1, 0x30FE},
        {0x3105, 0x312C},
        {0x1100, 0x1159}, {0x1161, 0x11A2}, {0x11A8, 0x11F9},
        {0xF900, 0xFA2D}, {0xFB1F, 0xFB36}, {0xFB38, 0xFB3C}, {0xFB3E, 0xFB3E}, {0xFB40, 0xFB41}, {0xFB42, 0xFB44}, {0xFB46, 0xFBB1},
        {0xFBD3, 0xFD3F}, {0xFD50, 0xFD8F}, {0xFD92, 0xFDC7}, {0xFDF0, 0xFDFB}, {0xFE70, 0xFE72}, {0xFE74, 0xFE74}, {0xFE76, 0xFEFC},
        {0xFF21, 0xFF3A}, {0xFF41, 0xFF5A}, {0xFF66, 0xFFBE}, {0xFFC2, 0xFFC7}, {0xFFCA, 0xFFCF}, {0xFFD2, 0xFFD7}, {0xFFDA, 0xFFDC},
        {0x4E00, 0x9FA5}
    };
    if (v > 0xFFFFUL) return false;
    for (size_t k = 0; k < sizeof ranges / sizeof ranges[0]; ++k) if (v >= ranges[k][0] && v <= ranges[k][1]) return true;
    return false;
}

std::string utf8Of(unsigned long v)
{
    std::string s;
    if (v < 0x80UL) s += static_cast<char>(v);
    else if (v < 0x800UL) { s += static_cast<char>(0xC0UL | (v >> 6)); s += static_cast<char>(0x80UL | (v & 0x3FUL)); }
    else if (v < 0x10000UL) { s += static_cast<char>(0xE0UL | (v >> 12)); s += static_cast<char>(0x80UL | ((v >> 6) & 0x3FUL)); s += static_cast<char>(0x80UL | (v & 0x3FUL)); }
    else if (v <= 0x10FFFFUL) { s += static_cast<char>(0xF0UL | (v >> 18)); s += static_cast<char>(0x80UL | ((v >> 12) & 0x3FUL)); s += static_cast<char>(0x80UL | ((v >> 6) & 0x3FUL)); s += static_cast<char>(0x80UL | (v & 0x3FUL)); }
    else s += '?';
    return s;
}

bool stringLiteralUnits(const std::string& text, std::vector<unsigned long>& units)
{
    units.clear();
    // the bodies of the pieces, between their quotes; a narrow piece next to a wide one is widened
    std::vector<size_t> begins, ends;
    std::vector<bool> widePiece;
    bool wide = false;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i] != '"') ++i;   // the prefix and the space between pieces
        if (i >= text.size()) break;
        widePiece.push_back(i > 0 && text[i - 1] == 'L');
        if (widePiece.back()) wide = true;
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
            appendUnits(v, universal, wide, !widePiece[p], units);
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
long charLiteralValue(const std::string& text, int& count, bool preprocessor)
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
        if (universal) {
            // a universal character name is no escape sequence of the rule above, and its code units are always
            // shifted in: in a wide constant the character or the first unit of its surrogate pair (cl C4066
            // drops the second), in a narrow one its byte in the code page ('?' twice beyond 16 bits), and in
            // an #if its UTF-8 bytes ('é' is C3A9h there, as cl's preprocessor reads it)
            std::vector<unsigned long> units;
            if (wide) units.push_back(c > 0xFFFFUL && c <= 0x10FFFFUL ? 0xD800UL + ((c - 0x10000UL) >> 10) : c & 0xFFFFUL);
            else if (preprocessor) { const std::string u = utf8Of(c); for (size_t k = 0; k < u.size(); ++k) units.push_back(static_cast<unsigned char>(u[k])); }
            else appendUnits(c, true, false, false, units);
            for (size_t k = 0; k < units.size(); ++k) {
                if (count == 0) first = units[k];
                v = ((v << 8) | units[k]) & 0xFFFFFFFFUL;
                ++count;
            }
            continue;
        }
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

double floatingLiteralValue(const std::string& text, bool asFloat)
{
    std::string s = text;
    while (!s.empty() && std::strchr("fFlL", s[s.size() - 1])) s.erase(s.size() - 1);
    const double x = std::strtod(s.c_str(), 0);
    if (!asFloat) return x;
    const float f = static_cast<float>(x);
    if (static_cast<double>(f) == x || x != x) return f;
    // the double rounding goes wrong only when x lies exactly halfway between f and the float on its other side;
    // then the decimal itself, compared exactly with that midpoint, decides (a tie stays f, the even one)
    unsigned int bits = 0;
    std::memcpy(&bits, &f, 4);
    const double fd = floatAsDouble(f);
    const unsigned int nb = fd < x ? bits + 1 : bits - 1;
    float g = 0;
    std::memcpy(&g, &nb, 4);
    const double gd = floatAsDouble(g);
    if ((fd + gd) / 2 != x) return f;
    const int c = compareDecimal(s, x);
    if (c == 0) return f;
    return (c > 0) == (fd < gd) ? g : f;
}

bool foldFloatToInteger(double d, int bits, bool isUnsigned, uint64& out, double& raw)
{
    if (d != d || d > 1.7976931348623157e308 || d < -1.7976931348623157e308) return false;
    const double n = std::fmod(d < 0 ? std::ceil(d) : std::floor(d), 18446744073709551616.0);   // exact
    uint64 u = n < 0 ? static_cast<uint64>(0) - static_cast<uint64>(-n) : static_cast<uint64>(n);
    raw = isUnsigned ? static_cast<double>(u) : static_cast<double>(static_cast<int64>(u));
    if (bits < 64) {
        const uint64 mask = (static_cast<uint64>(1) << bits) - 1;
        u &= mask;
        if (!isUnsigned && (u >> (bits - 1)) != 0) u |= ~mask;   // sign-extended
    }
    out = u;
    return true;
}

} // namespace cppparse
