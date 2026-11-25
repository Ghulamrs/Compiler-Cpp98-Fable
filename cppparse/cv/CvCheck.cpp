// CvCheck.cpp - see CvCheck.hpp.
#include "CvCheck.hpp"

#include <sstream>

namespace cppparse {

namespace {

const unsigned long kFirstIndex = 0x1000;

class Reader {
public:
    Reader(const std::string& bytes, size_t at, size_t end) : m_b(bytes), m_at(at), m_end(end), m_ok(true) {}
    unsigned long u8()  { return take(1); }
    unsigned long u16() { return take(2); }
    unsigned long u32() { return take(4); }
    // a numeric leaf: a value below 8000h, or the leaf of a longer value and its bytes
    void numeric()
    {
        const unsigned long v = u16();
        if (v < 0x8000) return;
        static const unsigned long sized[][2] = { {0x8000, 1}, {0x8001, 2}, {0x8002, 2}, {0x8003, 4}, {0x8004, 4}, {0x8009, 8}, {0x800A, 8} };
        for (size_t i = 0; i < sizeof(sized) / sizeof(sized[0]); ++i)
            if (sized[i][0] == v) { skip(sized[i][1]); return; }
        m_ok = false;
    }
    // a name: its length, without the 0 that ends it
    size_t name()
    {
        size_t n = 0;
        while (m_at < m_end && m_b[m_at] != '\0') { ++m_at; ++n; }
        if (m_at >= m_end) m_ok = false; else ++m_at;
        return n;
    }
    void skip(size_t n) { if (m_at + n > m_end) { m_ok = false; m_at = m_end; } else m_at += n; }
    // LF_PAD bytes up to a multiple of four from 'base'
    void pad(size_t base) { while (m_at < m_end && (m_at - base) % 4 != 0 && static_cast<unsigned char>(m_b[m_at]) >= 0xF0) ++m_at; }
    size_t at() const { return m_at; }
    bool atEnd() const { return m_at >= m_end; }
    bool ok() const { return m_ok; }
private:
    unsigned long take(int n)
    {
        if (m_at + static_cast<size_t>(n) > m_end) { m_ok = false; m_at = m_end; return 0; }
        unsigned long v = 0;
        for (int i = 0; i < n; ++i) v |= static_cast<unsigned long>(static_cast<unsigned char>(m_b[m_at + static_cast<size_t>(i)])) << (8 * i);
        m_at += static_cast<size_t>(n);
        return v;
    }
    const std::string& m_b;
    size_t m_at, m_end;
    bool   m_ok;
};

std::string hexText(unsigned long v)
{
    std::ostringstream s;
    s << std::hex << std::uppercase << v << "h";
    return s.str();
}

// one record's checks: the indices it holds against its own
class TypeRecord {
public:
    TypeRecord(unsigned long self, std::vector<std::string>& problems) : m_self(self), m_problems(problems) {}
    void index(unsigned long idx, const char* what)
    {
        if (idx < kFirstIndex || idx < m_self) return;
        problem(std::string(what) + " " + hexText(idx) + " is not below the record");
    }
    void problem(const std::string& text) { m_problems.push_back("type " + hexText(m_self) + ": " + text); }
private:
    unsigned long             m_self;
    std::vector<std::string>& m_problems;
};

void classNames(Reader& r, unsigned long properties, TypeRecord& rec)
{
    const size_t name = r.name();
    const size_t unique = (properties & 0x200) ? r.name() : 0;
    if (name == 0) rec.problem("a class or enumeration without a name");
    if (unique == 0) rec.problem("a class or enumeration without a unique name");
}

void fieldList(Reader& r, size_t base, TypeRecord& rec)
{
    while (r.ok() && !r.atEnd()) {
        const unsigned long leaf = r.u16();
        switch (leaf) {
        case 0x1400: r.u16(); rec.index(r.u32(), "a base class"); r.numeric(); break;
        case 0x1401: case 0x1402: r.u16(); rec.index(r.u32(), "a virtual base"); rec.index(r.u32(), "a vbptr's type"); r.numeric(); r.numeric(); break;
        case 0x1404: r.u16(); rec.index(r.u32(), "a continued field list"); break;
        case 0x1409: r.u16(); rec.index(r.u32(), "a vfptr's type"); break;
        case 0x1502: r.u16(); r.numeric(); r.name(); break;
        case 0x150D: r.u16(); rec.index(r.u32(), "a member's type"); r.numeric(); r.name(); break;
        case 0x150E: r.u16(); rec.index(r.u32(), "a static member's type"); r.name(); break;
        case 0x1510: r.u16(); rec.index(r.u32(), "a nested type"); r.name(); break;
        default: rec.problem("field list member of unknown leaf " + hexText(leaf)); return;
        }
        r.pad(base);
    }
}

} // namespace

unsigned long CvCheck::types(const std::string& section, std::vector<std::string>& problems)
{
    unsigned long count = 0;
    if (section.size() < 4 || section[0] != 4 || section[1] != 0 || section[2] != 0 || section[3] != 0) { problems.push_back(".debug$T: no CV_SIGNATURE_C13"); return 0; }
    size_t at = 4;
    while (at < section.size()) {
        const unsigned long self = kFirstIndex + count;
        TypeRecord rec(self, problems);
        Reader head(section, at, section.size());
        const unsigned long length = head.u16(), leaf = head.u16();
        if (!head.ok() || length < 2 || at + 2 + length > section.size()) { rec.problem("a record that passes the end of the section"); break; }
        if ((length + 2) % 4 != 0) rec.problem("a record that is not a multiple of four bytes");
        if (length > 0xFF00) rec.problem("a record of more than FF00h bytes");
        const size_t end = at + 2 + length;
        Reader r(section, at + 4, end);
        switch (leaf) {
        case 0x000A: r.u16(); break;
        case 0x1001: rec.index(r.u32(), "the modified type"); r.u16(); break;
        case 0x1002: {
            rec.index(r.u32(), "the pointee");
            const unsigned long attributes = r.u32(), mode = (attributes >> 5) & 7;
            if (mode == 2 || mode == 3) { rec.index(r.u32(), "the class of a pointer to member"); r.u16(); }
            break;
        }
        case 0x1008: rec.index(r.u32(), "the result"); r.u8(); r.u8(); r.u16(); rec.index(r.u32(), "the argument list"); break;
        case 0x1009: rec.index(r.u32(), "the result"); rec.index(r.u32(), "the class"); rec.index(r.u32(), "the type of this"); r.u8(); r.u8(); r.u16(); rec.index(r.u32(), "the argument list"); r.u32(); break;
        case 0x1201: { const unsigned long n = r.u32(); for (unsigned long i = 0; i < n && r.ok(); ++i) rec.index(r.u32(), "an argument"); break; }
        case 0x1203: fieldList(r, at + 4, rec); break;
        case 0x1205: rec.index(r.u32(), "a bit-field's type"); r.u8(); r.u8(); break;
        case 0x1503: rec.index(r.u32(), "the element"); rec.index(r.u32(), "the index type"); r.numeric(); r.name(); break;
        case 0x1504: case 0x1505: {
            r.u16(); const unsigned long properties = r.u16();
            rec.index(r.u32(), "the field list"); rec.index(r.u32(), "the derivation list"); rec.index(r.u32(), "the vftable shape"); r.numeric();
            classNames(r, properties, rec);
            break;
        }
        case 0x1506: { r.u16(); const unsigned long properties = r.u16(); rec.index(r.u32(), "the field list"); r.numeric(); classNames(r, properties, rec); break; }
        case 0x1507: { r.u16(); const unsigned long properties = r.u16(); rec.index(r.u32(), "the underlying type"); rec.index(r.u32(), "the field list"); classNames(r, properties, rec); break; }
        default: rec.problem("a record of unknown leaf " + hexText(leaf)); break;
        }
        if (!r.ok()) rec.problem("a record that is shorter than its fields");
        at = end;
        ++count;
    }
    return count;
}

void CvCheck::symbols(const std::string& records, unsigned long typeCount, std::vector<std::string>& problems)
{
    size_t at = 0;
    while (at + 4 <= records.size()) {
        Reader head(records, at, records.size());
        const unsigned long length = head.u16(), kind = head.u16();
        if (length < 2 || at + 2 + length > records.size()) { problems.push_back("a symbol record that passes the end of its subsection"); return; }
        Reader r(records, at + 4, at + 2 + length);
        unsigned long type = 0;
        switch (kind) {
        case 0x110F: case 0x1110: r.skip(24); type = r.u32(); break;     // S_LPROC32, S_GPROC32
        case 0x110C: case 0x110D: case 0x1112: case 0x1113: case 0x1107: case 0x1108: type = r.u32(); break;   // S_LDATA32, S_GDATA32, S_LTHREAD32, S_GTHREAD32, S_CONSTANT, S_UDT
        case 0x113E: type = r.u32(); break;                                          // S_LOCAL
        case 0x1111: r.u32(); type = r.u32(); break;                                 // S_REGREL32
        default: break;
        }
        if (type >= kFirstIndex + typeCount) problems.push_back("symbol record " + hexText(kind) + ": type " + hexText(type) + " is not one of the object's " + hexText(typeCount) + " type records");
        at += 2 + length;
    }
}

namespace {

// a procedure type's parameter records: LF_PROCEDURE's or LF_MFUNCTION's count, less a "..." (an argument of type 0,
// which has no record), with "this" for a member function that has one; -1: not a procedure type of the section
struct ProcedureTypes {
    std::vector<long> params;   // by type index - 1000h
    explicit ProcedureTypes(const std::string& section)
    {
        std::vector<std::pair<unsigned long, size_t> > records;   // leaf, offset of the body
        size_t at = 4;
        while (section.size() >= 4 && at + 4 <= section.size()) {
            Reader head(section, at, section.size());
            const unsigned long length = head.u16(), leaf = head.u16();
            if (length < 2 || at + 2 + length > section.size()) break;
            records.push_back(std::make_pair(leaf, at + 4));
            at += 2 + length;
        }
        params.assign(records.size(), -1);
        for (size_t k = 0; k < records.size(); ++k) {
            const unsigned long leaf = records[k].first;
            if (leaf != 0x1008 && leaf != 0x1009) continue;
            Reader r(section, records[k].second, section.size());
            r.u32();                                         // the result
            unsigned long thisType = 0;
            if (leaf == 0x1009) { r.u32(); thisType = r.u32(); }
            r.u8(); r.u8();
            long count = static_cast<long>(r.u16());
            const unsigned long list = r.u32();
            if (list >= kFirstIndex && list - kFirstIndex < records.size() && records[list - kFirstIndex].first == 0x1201) {
                Reader a(section, records[list - kFirstIndex].second, section.size());
                const unsigned long n = a.u32();
                unsigned long last = 1;
                for (unsigned long i = 0; i < n && a.ok(); ++i) last = a.u32();
                if (n > 0 && last == 0) --count;
            }
            params[k] = count + (thisType != 0 ? 1 : 0);
        }
    }
    long of(unsigned long type) const { return type >= kFirstIndex && type - kFirstIndex < params.size() ? params[type - kFirstIndex] : -1; }
};

struct OpenScope {
    unsigned long start, end;                                 // within the procedure
    std::vector<std::pair<unsigned long, unsigned long> > children;
};

} // namespace

void CvCheck::procedures(const std::string& records, const std::string& typesSection, std::vector<std::string>& problems)
{
    const ProcedureTypes types(typesSection);
    std::vector<OpenScope> open;
    std::string name;
    long expected = -1;
    bool hasLocal = false, hasRegRel = false, frameProc = false;
    size_t direct = 0, params = 0, paramLocals = 0;   // variable records directly in the procedure; how many of the first are parameters
    bool firstAreVariables = true;
    unsigned long lastRangeEnd = 0;
    size_t at = 0;
    while (at + 4 <= records.size()) {
        Reader head(records, at, records.size());
        const unsigned long length = head.u16(), kind = head.u16();
        if (length < 2 || at + 2 + length > records.size()) return;   // (symbols() reports it)
        Reader r(records, at + 4, at + 2 + length);
        const std::string where = "procedure " + name + ": ";
        switch (kind) {
        case 0x110F: case 0x1110: {                              // S_LPROC32, S_GPROC32
            r.skip(12); const unsigned long len = r.u32(); r.skip(8); const unsigned long type = r.u32(); r.skip(7);
            const size_t start = r.at(); const size_t n = r.name();
            name = records.substr(start, n);
            OpenScope s; s.start = 0; s.end = len;
            open.clear(); open.push_back(s);
            expected = types.of(type);
            hasLocal = hasRegRel = frameProc = false; direct = params = paramLocals = 0; firstAreVariables = true;
            break;
        }
        case 0x1012: frameProc = true; break;                   // S_FRAMEPROC
        case 0x1103: {                                          // S_BLOCK32
            r.skip(8); const unsigned long len = r.u32(), start = r.u32();
            if (open.empty()) { problems.push_back(where + "a block outside a procedure"); break; }
            OpenScope& parent = open.back();
            if (len == 0 || start < parent.start || start + len > parent.end) problems.push_back(where + "a block [" + hexText(start) + ", " + hexText(start + len) + ") outside its parent");
            for (size_t c = 0; c < parent.children.size(); ++c)
                if (start < parent.children[c].second && parent.children[c].first < start + len) problems.push_back(where + "two blocks of one parent overlap at " + hexText(start));
            parent.children.push_back(std::make_pair(start, start + len));
            OpenScope s; s.start = start; s.end = start + len;
            open.push_back(s);
            if (open.size() == 2) firstAreVariables = firstAreVariables && direct >= static_cast<size_t>(expected < 0 ? 0 : expected);
            break;
        }
        case 0x0006:                                            // S_END
            if (open.size() == 1) {
                // the procedure: its parameters first, as many as its type says (docs/design-codeview.md 5.5/4 (b))
                if (hasLocal && hasRegRel) problems.push_back(where + "S_REGREL32 and S_LOCAL in one procedure");
                if (expected >= 0 && (frameProc || hasLocal || hasRegRel)) {
                    if (hasLocal && static_cast<long>(paramLocals) != expected) problems.push_back(where + "parameter records: " + hexText(static_cast<unsigned long>(paramLocals)) + ", its type has " + hexText(static_cast<unsigned long>(expected)));
                    if (hasLocal && static_cast<long>(params) != expected) problems.push_back(where + "the parameter records do not come first");
                    if (!hasLocal && (static_cast<long>(direct) < expected || !firstAreVariables)) problems.push_back(where + "fewer variable records before its first block than its type has parameters");
                }
            }
            if (!open.empty()) open.pop_back();
            break;
        case 0x1111: {                                          // S_REGREL32
            hasRegRel = true;
            r.u32(); r.u32(); const unsigned long reg = r.u16();
            if (reg != 334 && reg != 335) problems.push_back(where + "S_REGREL32 off register " + hexText(reg));
            if (open.size() == 1) { if (direct == params) ++params; ++direct; }
            break;
        }
        case 0x113E: {                                          // S_LOCAL
            hasLocal = true;
            r.u32(); const unsigned long flags = r.u16();
            const bool param = (flags & 1) != 0;
            if (param) ++paramLocals;
            if (open.size() == 1) { if (param && direct == params) ++params; ++direct; }
            else if (param) problems.push_back(where + "a parameter inside a block");
            lastRangeEnd = 0;
            break;
        }
        case 0x1145: {                                          // S_DEFRANGE_REGISTER_REL
            const unsigned long reg = r.u16(); r.u16(); r.u32();
            const unsigned long start = r.u32(); r.u16(); const unsigned long len = r.u16();
            if (reg != 334) problems.push_back(where + "a frame range off register " + hexText(reg));
            if (open.empty() || len == 0 || start + len > open.front().end) problems.push_back(where + "a range [" + hexText(start) + ", " + hexText(start + len) + ") outside the procedure");
            if (start < lastRangeEnd) problems.push_back(where + "ranges of one variable that overlap or descend at " + hexText(start));
            lastRangeEnd = start + len;
            while (r.ok() && !r.atEnd()) {
                const unsigned long gap = r.u16(), gapLen = r.u16();
                if (r.ok() && (gap == 0 || gap + gapLen > len)) problems.push_back(where + "a gap outside its range at " + hexText(start + gap));
            }
            break;
        }
        default: break;
        }
        at += 2 + length;
    }
}

void CvCheck::frameRanges(const std::string& fn, const std::vector<std::pair<unsigned long, unsigned long> >& ranges,
                          const std::vector<std::pair<unsigned long, unsigned long> >& windows, std::vector<std::string>& problems)
{
    for (size_t r = 0; r < ranges.size(); ++r)
        for (size_t w = 0; w < windows.size(); ++w)
            if (windows[w].first < windows[w].second && ranges[r].first < windows[w].second && windows[w].first < ranges[r].second)
                problems.push_back(fn + ": a frame range [" + hexText(ranges[r].first) + ", " + hexText(ranges[r].second) + ") holds code of a prologue or an epilogue [" + hexText(windows[w].first) + ", " + hexText(windows[w].second) + ")");
}

} // namespace cppparse
