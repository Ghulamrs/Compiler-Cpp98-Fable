// CvNames.cpp - see CvNames.hpp.
#include "CvNames.hpp"

namespace cppparse {

namespace {

bool endsOpen(const std::string& s)
{
    return s.empty() || s[s.size() - 1] == ' ' || s[s.size() - 1] == '(';
}

std::string spaced(const std::string& s)
{
    return endsOpen(s) ? s : s + " ";
}

const char* pointerQualifiers(bool isConst, bool isVolatile)
{
    return isConst ? (isVolatile ? " const volatile" : " const") : (isVolatile ? " volatile" : "");
}

} // namespace

std::string CvNames::qualified(const std::string& type, bool isConst, bool isVolatile)
{
    if (!isConst && !isVolatile) return type;
    return type + pointerQualifiers(isConst, isVolatile) + " ";
}

CvTypeText CvNames::basic(const std::string& cxxName, bool isConst, bool isVolatile)
{
    CvTypeText t;
    t.left = qualified(cxxName == "long long" ? std::string("__int64") : cxxName == "unsigned long long" ? std::string("unsigned __int64") : cxxName, isConst, isVolatile);
    return t;
}

CvTypeText CvNames::named(const std::string& qualifiedName, bool isEnum, bool isConst, bool isVolatile)
{
    CvTypeText t;
    t.left = qualified((isEnum ? "enum " : "") + qualifiedName, isConst, isVolatile);
    return t;
}

CvTypeText CvNames::declared(const CvTypeText& t, const std::string& d)
{
    CvTypeText r;
    if (t.function) { r.left = t.left + "(__cdecl" + d; r.right = ")" + t.right; }
    else if (t.grouped) { r.left = t.left + "(" + d; r.right = ")" + t.right; }
    else { r.left = spaced(t.left) + d; r.right = t.right; }
    return r;
}

CvTypeText CvNames::pointer(const CvTypeText& to, bool isConst, bool isVolatile)
{
    CvTypeText r = declared(to, "*");
    r.left += pointerQualifiers(isConst, isVolatile);
    return r;
}

CvTypeText CvNames::reference(const CvTypeText& to)
{
    return declared(to, "&");
}

CvTypeText CvNames::memberPointer(const CvTypeText& to, const std::string& cls, bool isConst, bool isVolatile)
{
    // "int ns::In::*"; a function's convention stands before the class, "int (__cdecl ns::In::*)(int)"
    CvTypeText r = declared(to, (to.function ? " " : "") + cls + "::*");
    r.left += pointerQualifiers(isConst, isVolatile);
    return r;
}

CvTypeText CvNames::array(const CvTypeText& of, long count)
{
    CvTypeText r;
    r.left = spaced(of.left);
    r.right = "[" + integer(count) + "]" + of.right;
    r.grouped = true;
    return r;
}

CvTypeText CvNames::function(const CvTypeText& result, const std::vector<std::string>& params, bool variadic, bool constMethod)
{
    CvTypeText r;
    r.left = spaced(result.left);
    r.right = "(";
    for (size_t i = 0; i < params.size(); ++i) r.right += (i ? "," : "") + params[i];
    if (variadic) r.right += params.empty() ? "..." : ",...";
    else if (params.empty()) r.right += "void";
    r.right += ")";
    if (constMethod) r.right += "const ";
    r.right += result.right;
    r.grouped = true; r.function = true;
    return r;
}

std::string CvNames::text(const CvTypeText& t)
{
    return t.function ? t.left + "__cdecl" + t.right : t.left + t.right;
}

std::string CvNames::templateId(const std::string& name, const std::vector<std::string>& args)
{
    std::string s = name + "<";
    for (size_t i = 0; i < args.size(); ++i) s += (i ? "," : "") + args[i];
    if (s[s.size() - 1] == '>') s += ' ';
    return s + ">";
}

std::string CvNames::scoped(const std::string& scope, const std::string& name)
{
    return scope.empty() ? name : scope + "::" + name;
}

std::string CvNames::integer(int64 v)
{
    uint64 u = static_cast<uint64>(v);
    if (v < 0) u = static_cast<uint64>(0) - u;         // the magnitude, of the most negative value too
    std::string digits;
    do { digits.insert(digits.begin(), static_cast<char>('0' + static_cast<int>(u % 10))); u /= 10; } while (u != 0);
    return v < 0 ? "-" + digits : digits;
}

std::string CvNames::braced(const std::vector<std::string>& parts)
{
    std::string s = "{";
    for (size_t i = 0; i < parts.size(); ++i) s += (i ? "," : "") + parts[i];
    return s + "}";
}

std::string CvNames::vcallThunk(const std::string& cls, long slotOffset)
{
    return scoped(cls, "`vcall'{" + integer(slotOffset) + "}'");
}

std::string CvNames::localScope(const std::string& function, long block)
{
    return "`" + function + "'::`" + integer(block) + "'";
}

std::string CvNames::localScopeOfType(const std::string& function, long block)
{
    return function + "::__l" + integer(block);
}

std::string CvNames::unnamedType(const std::string& declarator)
{
    return declarator.empty() ? std::string("<unnamed-tag>") : "<unnamed-type-" + declarator + ">";
}

// RFC 1321. The message is a name, a few kilobytes at most
std::string CvNames::md5(const std::string& text)
{
    static const unsigned long kSine[64] = {
        0xd76aa478UL, 0xe8c7b756UL, 0x242070dbUL, 0xc1bdceeeUL, 0xf57c0fafUL, 0x4787c62aUL, 0xa8304613UL, 0xfd469501UL,
        0x698098d8UL, 0x8b44f7afUL, 0xffff5bb1UL, 0x895cd7beUL, 0x6b901122UL, 0xfd987193UL, 0xa679438eUL, 0x49b40821UL,
        0xf61e2562UL, 0xc040b340UL, 0x265e5a51UL, 0xe9b6c7aaUL, 0xd62f105dUL, 0x02441453UL, 0xd8a1e681UL, 0xe7d3fbc8UL,
        0x21e1cde6UL, 0xc33707d6UL, 0xf4d50d87UL, 0x455a14edUL, 0xa9e3e905UL, 0xfcefa3f8UL, 0x676f02d9UL, 0x8d2a4c8aUL,
        0xfffa3942UL, 0x8771f681UL, 0x6d9d6122UL, 0xfde5380cUL, 0xa4beea44UL, 0x4bdecfa9UL, 0xf6bb4b60UL, 0xbebfbc70UL,
        0x289b7ec6UL, 0xeaa127faUL, 0xd4ef3085UL, 0x04881d05UL, 0xd9d4d039UL, 0xe6db99e5UL, 0x1fa27cf8UL, 0xc4ac5665UL,
        0xf4292244UL, 0x432aff97UL, 0xab9423a7UL, 0xfc93a039UL, 0x655b59c3UL, 0x8f0ccc92UL, 0xffeff47dUL, 0x85845dd1UL,
        0x6fa87e4fUL, 0xfe2ce6e0UL, 0xa3014314UL, 0x4e0811a1UL, 0xf7537e82UL, 0xbd3af235UL, 0x2ad7d2bbUL, 0xeb86d391UL };
    static const int kShift[64] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                                    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
    const unsigned long mask = 0xFFFFFFFFUL;   // (unsigned long may have more than 32 bits)
    std::string m = text;
    m += static_cast<char>(0x80);
    while (m.size() % 64 != 56) m += '\0';
    const uint64 bits = static_cast<uint64>(text.size()) * 8;
    for (int i = 0; i < 8; ++i) m += static_cast<char>((bits >> (8 * i)) & 0xFF);
    unsigned long h[4] = { 0x67452301UL, 0xefcdab89UL, 0x98badcfeUL, 0x10325476UL };
    for (size_t block = 0; block < m.size(); block += 64) {
        unsigned long w[16];
        for (int i = 0; i < 16; ++i) {
            w[i] = 0;
            for (int k = 0; k < 4; ++k) w[i] |= static_cast<unsigned long>(static_cast<unsigned char>(m[block + static_cast<size_t>(4 * i + k)])) << (8 * k);
        }
        unsigned long a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 64; ++i) {
            unsigned long f; int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | (~d & mask)); g = (7 * i) % 16; }
            const unsigned long sum = (a + (f & mask) + kSine[i] + w[g]) & mask;
            a = d; d = c; c = b;
            b = (b + (((sum << kShift[i]) | (sum >> (32 - kShift[i]))) & mask)) & mask;
        }
        h[0] = (h[0] + a) & mask; h[1] = (h[1] + b) & mask; h[2] = (h[2] + c) & mask; h[3] = (h[3] + d) & mask;
    }
    std::string hex;
    for (int i = 0; i < 4; ++i)
        for (int k = 0; k < 4; ++k) { const unsigned long byte = (h[i] >> (8 * k)) & 0xFF; hex += "0123456789abcdef"[byte >> 4]; hex += "0123456789abcdef"[byte & 15]; }
    return hex;
}

std::string CvNames::cutName(const std::string& name)
{
    if (name.size() <= 4096) return name;
    return name.substr(0, 4064) + md5(name);
}

std::string CvNames::cutUniqueName(const std::string& uniqueName)
{
    if (uniqueName.size() <= 4096) return uniqueName;
    return ".??@" + md5(uniqueName.compare(0, 1, ".") == 0 ? uniqueName.substr(1) : uniqueName) + "@";
}

std::string CvNames::fromIrName(const std::string& irName)
{
    size_t p = irName.find('(');
    if (p != std::string::npos && p >= 8 && irName.compare(p - 8, 8, "operator") == 0 && p + 1 < irName.size() && irName[p + 1] == ')') p = irName.find('(', p + 2);
    return p == std::string::npos ? irName : irName.substr(0, p);
}

} // namespace cppparse
