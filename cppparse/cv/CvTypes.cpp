// CvTypes.cpp - see CvTypes.hpp.
#include "CvTypes.hpp"
#include "CvNames.hpp"

namespace cppparse {

namespace {

enum {
    LfVtShape = 0x000A, LfModifier = 0x1001, LfPointer = 0x1002, LfProcedure = 0x1008, LfMFunction = 0x1009, LfArgList = 0x1201,
    LfFieldList = 0x1203, LfBitfield = 0x1205, LfBClass = 0x1400, LfVBClass = 0x1401, LfIVBClass = 0x1402, LfIndex = 0x1404,
    LfVFuncTab = 0x1409, LfEnumerate = 0x1502, LfArray = 0x1503, LfClass = 0x1504, LfStructure = 0x1505, LfUnion = 0x1506,
    LfEnum = 0x1507, LfMember = 0x150D, LfStMember = 0x150E, LfNestType = 0x1510
};
enum { PropConstructor = 0x2, PropNested = 0x8, PropHasNested = 0x10, PropForward = 0x80, PropScoped = 0x100, PropUniqueName = 0x200, PropSealed = 0x400 };
const unsigned long kFirstIndex = 0x1000;
const unsigned long kMaxRecord = 0xFF00;       // a record's bytes: its length field has 16 bits
const unsigned long kPointer64 = 0x1000C;      // LF_POINTER's attributes: a 64-bit pointer of 8 bytes

// a member of a field list: its bytes padded to four with LF_PAD
std::string fieldBytes(CvBytes& b)
{
    b.padLeaf();
    return b.bytes();
}

} // namespace

CvTypes::CvTypes(const CvModule& module)
    : m_module(module), m_index(static_cast<size_t>(module.typeCount()), 0UL),
      m_forward(static_cast<size_t>(module.classCount()), 0UL), m_definition(static_cast<size_t>(module.classCount()), 0UL)
{
}

unsigned long CvTypes::record(unsigned long leaf, const CvBytes& body)
{
    CvBytes r;
    r.typeRecord(leaf, body);
    std::map<std::string, unsigned long>::const_iterator it = m_known.find(r.bytes());
    if (it != m_known.end()) return it->second;
    const unsigned long idx = kFirstIndex + count();
    m_known[r.bytes()] = idx;
    m_records += r.bytes();
    m_sizes.push_back(r.size());
    return idx;
}

std::string CvTypes::section() const
{
    CvBytes b;
    b.put32(4);                                      // CV_SIGNATURE_C13
    return b.bytes() + m_records;
}

unsigned long CvTypes::argList(const CvType& t)
{
    CvBytes a;
    a.put32(static_cast<unsigned long>(t.params.size()) + (t.variadic ? 1 : 0));
    for (size_t i = 0; i < t.params.size(); ++i) a.put32(index(t.params[i]));
    if (t.variadic) a.put32(0);
    return record(LfArgList, a);
}

unsigned long CvTypes::vtShape(int slots)
{
    CvBytes b;
    b.put16(static_cast<unsigned long>(slots));
    for (int s = 0; s < slots; s += 2) b.put8(s + 1 < slots ? 0x55 : 0x05);
    return record(LfVtShape, b);
}

unsigned long CvTypes::index(int number)
{
    if (number <= 0 || number > m_module.typeCount()) return 0;
    unsigned long& known = m_index[static_cast<size_t>(number - 1)];
    if (known != 0) return known;
    const CvType& t = m_module.type(number);
    unsigned long idx = 0;
    CvBytes b;
    switch (t.kind) {
    case CvType::Basic: idx = t.basic; break;
    case CvType::Modifier:
        b.put32(index(t.to)); b.put16((t.isConst ? 1UL : 0UL) | (t.isVolatile ? 2UL : 0UL));
        idx = record(LfModifier, b);
        break;
    case CvType::Pointer: case CvType::Reference: {
        const unsigned long to = index(t.to);
        if (t.kind == CvType::Pointer && to != 0 && to < 0x100 && !t.isConst && !t.isVolatile) { idx = 0x600 + to; break; }
        b.put32(to); b.put32(kPointer64 | (t.kind == CvType::Reference ? 0x20UL : 0UL) | (t.isConst ? 0x400UL : 0UL) | (t.isVolatile ? 0x200UL : 0UL));
        idx = record(LfPointer, b);
        break;
    }
    case CvType::MemberPointer:
        b.put32(index(t.to));
        b.put32((t.bytes << 13) | (t.toFunction ? 0x60UL : 0x40UL) | 0xCUL | (t.isConst ? 0x400UL : 0UL) | (t.isVolatile ? 0x200UL : 0UL));
        b.put32(index(t.ofClass)); b.put16(static_cast<unsigned long>(t.model));
        idx = record(LfPointer, b);
        break;
    case CvType::Array:
        b.put32(index(t.to)); b.put32(0x23); b.numeric(static_cast<int64>(t.bytes), true); b.name(std::string());
        idx = record(LfArray, b);
        break;
    case CvType::Procedure: {
        const unsigned long result = index(t.to), args = argList(t);
        b.put32(result); b.put8(0); b.put8(0); b.put16(static_cast<unsigned long>(t.params.size()) + (t.variadic ? 1 : 0)); b.put32(args);
        idx = record(LfProcedure, b);
        break;
    }
    case CvType::MemberFunction: {
        const unsigned long result = index(t.to), cls = index(t.ofClass), self = index(t.thisType), args = argList(t);
        b.put32(result); b.put32(cls); b.put32(self); b.put8(0); b.put8(0);
        b.put16(static_cast<unsigned long>(t.params.size()) + (t.variadic ? 1 : 0)); b.put32(args); b.put32(0);
        idx = record(LfMFunction, b);
        break;
    }
    case CvType::Class:
        if (t.classNumber <= 0 || t.classNumber > m_module.classCount()) return 0;
        // an enumeration has no forward reference: it is complete wherever it is named
        idx = m_module.classAt(t.classNumber).kind == CvClass::Enum ? definition(t.classNumber) : forward(t.classNumber);
        break;
    }
    // (a type's index is noted once its record is written: the records it names were asked for first, above)
    m_index[static_cast<size_t>(number - 1)] = idx;
    return idx;
}

unsigned long CvTypes::symbolIndex(int number)
{
    if (number <= 0 || number > m_module.typeCount()) return 0;
    const CvType& t = m_module.type(number);
    if (t.kind == CvType::Class && t.classNumber > 0 && t.classNumber <= m_module.classCount()) return definition(t.classNumber);
    return index(number);
}

unsigned long CvTypes::properties(const CvClass& c) const
{
    return PropUniqueName | (c.nested ? PropNested : 0) | (c.scoped ? PropScoped : 0);
}

void CvTypes::names(CvBytes& body, const CvClass& c)
{
    body.name(CvNames::cutName(c.name));
    body.name(CvNames::cutUniqueName(c.uniqueName));
}

unsigned long CvTypes::forward(int classNumber)
{
    unsigned long& known = m_forward[static_cast<size_t>(classNumber - 1)];
    if (known != 0) return known;
    const CvClass& c = m_module.classAt(classNumber);
    CvBytes b;
    b.put16(0); b.put16(properties(c) | PropForward); b.put32(0);
    if (c.kind != CvClass::Union) { b.put32(0); b.put32(0); }
    b.numeric(0, true);
    names(b, c);
    const unsigned long idx = record(c.kind == CvClass::Union ? LfUnion : c.kind == CvClass::Class ? LfClass : LfStructure, b);
    m_forward[static_cast<size_t>(classNumber - 1)] = idx;
    return idx;
}

// the members as field lists: one, or where their bytes would pass a record's limit several, each but the last
// ending in an LF_INDEX to the next; the last is written first, so that an index is below the record that holds it
unsigned long CvTypes::fieldList(const std::vector<std::string>& members)
{
    std::vector<std::pair<size_t, size_t> > pieces;   // [first, end) of the members in each list
    size_t first = 0;
    unsigned long bytes = 0;
    for (size_t i = 0; i < members.size(); ++i) {
        if (i > first && bytes + members[i].size() > kMaxRecord - 16) { pieces.push_back(std::make_pair(first, i)); first = i; bytes = 0; }
        bytes += static_cast<unsigned long>(members[i].size());
    }
    pieces.push_back(std::make_pair(first, members.size()));
    unsigned long next = 0;
    for (size_t p = pieces.size(); p > 0; --p) {
        CvBytes list;
        for (size_t i = pieces[p - 1].first; i < pieces[p - 1].second; ++i) list.raw(members[i]);
        if (p < pieces.size()) { list.put16(LfIndex); list.put16(0); list.put32(next); }
        next = record(LfFieldList, list);
    }
    return next;
}

unsigned long CvTypes::definition(int classNumber)
{
    if (m_definition[static_cast<size_t>(classNumber - 1)] != 0) return m_definition[static_cast<size_t>(classNumber - 1)];
    const CvClass& c = m_module.classAt(classNumber);
    if (!c.complete) return forward(classNumber);
    std::vector<std::string> members;
    unsigned long idx = 0;
    if (c.kind == CvClass::Enum) {
        for (size_t i = 0; i < c.enumerators.size(); ++i) {
            CvBytes m;
            m.put16(LfEnumerate); m.put16(3); m.numeric(c.enumerators[i].value, false); m.name(c.enumerators[i].name);
            members.push_back(fieldBytes(m));
        }
        const unsigned long underlying = index(c.underlying), fields = fieldList(members);
        CvBytes b;
        b.put16(static_cast<unsigned long>(members.size())); b.put16(properties(c)); b.put32(underlying); b.put32(fields);
        names(b, c);
        idx = record(LfEnum, b);
    }
    else {
        // the forward reference first, as cl has it: the class's own members may name it (a pointer to itself)
        forward(classNumber);
        for (size_t i = 0; i < c.bases.size(); ++i) {
            const CvBase& base = c.bases[i];
            CvBytes m;
            if (!base.isVirtual) { m.put16(LfBClass); m.put16(static_cast<unsigned long>(base.access)); m.put32(index(base.type)); m.numeric(base.offset, false); }
            else {
                // the vbptr's type as cl writes it: a pointer to const int
                CvBytes ci; ci.put32(0x74); ci.put16(1);
                CvBytes pi; pi.put32(record(LfModifier, ci)); pi.put32(kPointer64);
                const unsigned long vbptr = record(LfPointer, pi);
                m.put16(base.indirect ? LfIVBClass : LfVBClass); m.put16(static_cast<unsigned long>(base.access)); m.put32(index(base.type)); m.put32(vbptr);
                m.numeric(base.vbptrOffset, false); m.numeric(base.vbIndex, false);
            }
            members.push_back(fieldBytes(m));
        }
        unsigned long shape = 0;
        if (c.vtableSlots >= 0 && c.kind != CvClass::Union) {
            shape = vtShape(c.vtableSlots);
            if (c.ownVfptr) {
                CvBytes p; p.put32(shape); p.put32(kPointer64);
                CvBytes m; m.put16(LfVFuncTab); m.put16(0); m.put32(record(LfPointer, p));
                members.push_back(fieldBytes(m));
            }
        }
        for (size_t i = 0; i < c.members.size(); ++i) {
            const CvMember& mem = c.members[i];
            CvBytes m;
            unsigned long type = index(mem.type);
            if (mem.kind == CvMember::Data) {
                if (mem.bitWidth >= 0) { CvBytes bf; bf.put32(type); bf.put8(mem.bitWidth); bf.put8(mem.bitPosition); type = record(LfBitfield, bf); }
                m.put16(LfMember); m.put16(static_cast<unsigned long>(mem.access)); m.put32(type); m.numeric(mem.offset, false); m.name(mem.name);
            }
            else if (mem.kind == CvMember::Static) { m.put16(LfStMember); m.put16(static_cast<unsigned long>(mem.access)); m.put32(type); m.name(mem.name); }
            else { m.put16(LfNestType); m.put16(0); m.put32(type); m.name(mem.name); }
            members.push_back(fieldBytes(m));
        }
        const unsigned long fields = fieldList(members);
        CvBytes b;
        b.put16(static_cast<unsigned long>(members.size() > 0xFFFF ? 0xFFFF : members.size()));
        b.put16(properties(c) | (c.hasNested ? PropHasNested : 0) | (c.hasConstructor ? PropConstructor : 0) | (c.kind == CvClass::Union ? PropSealed : 0));
        b.put32(fields);
        if (c.kind != CvClass::Union) { b.put32(0); b.put32(shape); }
        b.numeric(static_cast<int64>(c.size), true);
        names(b, c);
        idx = record(c.kind == CvClass::Union ? LfUnion : c.kind == CvClass::Class ? LfClass : LfStructure, b);
    }
    m_definition[static_cast<size_t>(classNumber - 1)] = idx;
    m_definitions.push_back(std::make_pair(classNumber, idx));
    return idx;
}

void CvTypes::finish()
{
    // a definition names classes by forward references, which may be new ones: until a pass adds none
    for (bool more = true; more;) {
        more = false;
        for (int c = 1; c <= m_module.classCount(); ++c) {
            if (m_forward[static_cast<size_t>(c - 1)] == 0 || m_definition[static_cast<size_t>(c - 1)] != 0 || !m_module.classAt(c).complete) continue;
            definition(c);
            more = true;
        }
    }
}

} // namespace cppparse
