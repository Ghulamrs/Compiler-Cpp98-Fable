// CvModule.cpp - see CvModule.hpp.
#include "CvModule.hpp"

#include "CvSha256.hpp"

#include <sstream>

namespace cppparse {

namespace {

const char* accessText(int access)
{
    return access == 1 ? "private" : access == 2 ? "protected" : "public";
}

const char* methodText(CvMethod::Kind kind)
{
    switch (kind) {
    case CvMethod::Virtual: return "virtual ";
    case CvMethod::Static: return "static ";
    case CvMethod::IntroVirtual: return "introducing virtual ";
    case CvMethod::PureVirtual: return "pure virtual ";
    case CvMethod::PureIntroVirtual: return "pure introducing virtual ";
    default: return "";
    }
}

// a file name with its letters A to Z in lower case: what two spellings of one file's name have in common
std::string sourceKey(const std::string& name)
{
    std::string key = name;
    for (size_t i = 0; i < key.size(); ++i) if (key[i] >= 'A' && key[i] <= 'Z') key[i] = static_cast<char>(key[i] - 'A' + 'a');
    return key;
}

} // namespace

void CvModule::add(const CvSource& s)
{
    const std::string key = sourceKey(s.name);
    if (m_sourceByName.count(key)) return;
    m_sourceByName[key] = m_sources.size();
    m_sources.push_back(s);
}

const CvSource* CvModule::source(const std::string& name) const
{
    const std::map<std::string, size_t>::const_iterator it = m_sourceByName.find(sourceKey(name));
    return it == m_sourceByName.end() ? 0 : &m_sources[it->second];
}

std::string CvModule::dump() const
{
    std::ostringstream o;
    o << "CodeView description (-g): " << m_types.size() << " types, " << m_classes.size() << " classes and enumerations, "
      << m_variables.size() << " variables, " << m_typedefs.size() << " typedef names\n\n";
    for (size_t i = 0; i < m_types.size(); ++i) {
        const CvType& t = m_types[i];
        o << "T" << (i + 1) << " ";
        switch (t.kind) {
        case CvType::Basic: o << "basic " << std::hex << t.basic << std::dec << "h"; break;
        case CvType::Modifier: o << (t.isConst ? "const " : "") << (t.isVolatile ? "volatile " : "") << "T" << t.to; break;
        case CvType::Pointer: o << "pointer to T" << t.to << (t.isConst ? " const" : "") << (t.isVolatile ? " volatile" : ""); break;
        case CvType::Reference: o << "reference to T" << t.to; break;
        case CvType::MemberPointer: o << "pointer to member " << (t.toFunction ? "function " : "") << "T" << t.to << " of T" << t.ofClass << ", " << t.bytes << " bytes, model " << t.model; break;
        case CvType::Array: o << "array of T" << t.to << ", " << t.bytes << " bytes"; break;
        case CvType::Procedure: case CvType::MemberFunction:
            o << (t.kind == CvType::Procedure ? "function" : "member function") << " (";
            for (size_t p = 0; p < t.params.size(); ++p) o << (p ? ", " : "") << "T" << t.params[p];
            if (t.variadic) o << (t.params.empty() ? "..." : ", ...");
            o << ") returning T" << t.to;
            if (t.kind == CvType::MemberFunction) { o << ", of T" << t.ofClass << ", this T" << t.thisType; if (t.thisAdjust != 0) o << " adjusted by " << t.thisAdjust; }
            if (t.attributes & CvType::FnConstructor) o << ((t.attributes & CvType::FnConstructorVbases) ? ", a constructor with virtual bases" : ", a constructor");
            if (t.attributes & CvType::FnReturnsUdt) o << ", the result the C++ way";
            break;
        case CvType::Class: o << "class C" << t.classNumber; break;
        }
        o << "\n";
    }
    o << "\n";
    for (size_t i = 0; i < m_classes.size(); ++i) {
        const CvClass& c = m_classes[i];
        o << "C" << (i + 1) << " " << (c.kind == CvClass::Class ? "class" : c.kind == CvClass::Struct ? "struct" : c.kind == CvClass::Union ? "union" : "enum")
          << " " << c.name << "  [" << c.uniqueName << "]";
        if (!c.complete) { o << "  incomplete\n"; continue; }
        o << "  " << c.size << " bytes" << (c.nested ? ", nested" : "") << (c.scoped ? ", local" : "") << (c.hasNested ? ", has nested types" : "") << (c.hasConstructor ? ", has a constructor or destructor" : "")
          << (c.hasOperator ? ", has an operator" : "") << (c.hasAssignment ? ", has a copy assignment" : "") << (c.hasConversion ? ", has a conversion" : "");
        if (c.vtableSlots >= 0) o << ", vftable of " << c.vtableSlots << " slots" << (c.ownVfptr ? " (own vfptr)" : "");
        o << "\n";
        for (size_t b = 0; b < c.bases.size(); ++b) {
            const CvBase& base = c.bases[b];
            o << "    " << accessText(base.access) << (base.isVirtual ? (base.indirect ? " indirect virtual base T" : " virtual base T") : " base T") << base.type;
            if (base.isVirtual) o << ", vbptr at " << base.vbptrOffset << ", vbtable entry " << base.vbIndex; else o << " at " << base.offset;
            o << "\n";
        }
        for (size_t m = 0; m < c.members.size(); ++m) {
            const CvMember& mem = c.members[m];
            if (mem.kind == CvMember::Nested) { o << "    nested type " << mem.name << " T" << mem.type << "\n"; continue; }
            if (mem.kind == CvMember::Method) {
                for (size_t f = 0; f < mem.methods.size(); ++f) {
                    const CvMethod& fn = mem.methods[f];
                    o << "    " << accessText(fn.access) << " " << methodText(fn.kind) << "function " << mem.name << " T" << fn.type;
                    if (fn.kind == CvMethod::IntroVirtual || fn.kind == CvMethod::PureIntroVirtual) o << ", vftable slot at " << fn.vftableOffset;
                    o << "\n";
                }
                continue;
            }
            o << "    " << accessText(mem.access) << (mem.kind == CvMember::Static ? " static " : " ") << mem.name << " T" << mem.type;
            if (mem.kind == CvMember::Data) { o << " at " << mem.offset; if (mem.bitWidth >= 0) o << ", bits " << mem.bitPosition << " to " << (mem.bitPosition + mem.bitWidth - 1); }
            o << "\n";
        }
        for (size_t e = 0; e < c.enumerators.size(); ++e) o << "    " << c.enumerators[e].name << " = " << c.enumerators[e].value << "\n";
    }
    o << "\n";
    for (size_t i = 0; i < m_variables.size(); ++i) {
        const CvVariable& v = m_variables[i];
        o << (v.internal ? "internal " : "") << "variable " << v.name << " T" << v.type;
        if (!v.symbol.empty()) o << "  [" << v.symbol << "]";
        if (v.hasValue) o << "  constant " << v.value;
        o << "\n";
    }
    for (size_t i = 0; i < m_typedefs.size(); ++i) o << "typedef " << m_typedefs[i].name << " T" << m_typedefs[i].type << "\n";
    for (size_t i = 0; i < m_sources.size(); ++i) o << "source " << m_sources[i].name << "  SHA-256 " << CvSha256::hex(m_sources[i].checksum) << "\n";
    return o.str();
}

} // namespace cppparse
