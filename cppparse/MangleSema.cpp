// MangleSema.cpp - MSVC x64 name decoration from the version 6 symbols and types (same encoding as Mangle.cpp).
#include "Mangle.hpp"
#include "PathUtil.hpp"
#include "Sema.hpp"

#include <vector>

namespace cppparse {

namespace {

class SemaMangler {
public:
    std::string function(const sema::Symbol& f);
    std::string variable(const sema::Symbol& v);
    std::string vtable(const sema::Symbol& c);
private:
    std::string number(long v);
    std::string name(const std::string& component);
    std::string symbolName(const sema::Symbol* s);
    std::string templateName(const sema::Symbol* inst, const std::string& base);
    std::string scopesOf(const sema::Scope* s);
    std::string type(const sema::Type* t);
    std::string paramType(const sema::Type* t);
    std::string classType(const sema::Type* t);
    std::string operatorCode(const std::string& fname, bool& ok);
    std::vector<std::string> m_names;
    std::vector<std::string> m_argTypes;
};

std::string SemaMangler::number(long v)
{
    std::string out;
    if (v < 0) { out += '?'; v = -v; }
    if (v == 0) return out + "A@";
    if (v <= 10) { out += static_cast<char>('0' + (v - 1)); return out; }
    std::string hex;
    unsigned long u = static_cast<unsigned long>(v);
    while (u) { hex.insert(hex.begin(), static_cast<char>('A' + (u & 15))); u >>= 4; }
    return out + hex + "@";
}

std::string SemaMangler::name(const std::string& component)
{
    for (size_t i = 0; i < m_names.size(); ++i)
        if (m_names[i] == component) return std::string(1, static_cast<char>('0' + i));
    if (m_names.size() < 10) m_names.push_back(component);
    return component + "@";
}

// "?$Buffer@H$03@": the arguments use their own back-reference tables, the whole name takes one slot
std::string SemaMangler::templateName(const sema::Symbol* inst, const std::string& base)
{
    std::vector<std::string> savedNames = m_names, savedArgs = m_argTypes;
    m_names.clear(); m_argTypes.clear();
    std::string enc = "?$" + base + "@";
    for (size_t i = 0; i < inst->templateArgs.size(); ++i) {
        if (inst->templateArgs[i]) enc += type(inst->templateArgs[i]);
        else enc += "$0" + number(i < inst->templateIntArgs.size() ? inst->templateIntArgs[i] : 0);
    }
    enc += "@";
    m_names = savedNames; m_argTypes = savedArgs;
    for (size_t i = 0; i < m_names.size(); ++i)
        if (m_names[i] == enc) return std::string(1, static_cast<char>('0' + i));
    if (m_names.size() < 10) m_names.push_back(enc);
    return enc;
}

std::string SemaMangler::symbolName(const sema::Symbol* s)
{
    if (s->kind == sema::Symbol::Class && s->instantiatedFrom) return templateName(s, s->instantiatedFrom->name);
    if (s->kind == sema::Symbol::Namespace && s->name.empty()) return "?A0x00000000@";
    return name(s->name);
}

std::string SemaMangler::scopesOf(const sema::Scope* s)
{
    std::string out;
    for (; s; s = s->parent) {
        if ((s->kind == sema::Scope::Namespace || s->kind == sema::Scope::Class) && s->owner) out += symbolName(s->owner);
        if (s->kind == sema::Scope::Function || s->kind == sema::Scope::Block) break;
    }
    return out;
}

std::string SemaMangler::classType(const sema::Type* t)
{
    const sema::Symbol* c = t->decl;
    char letter = c && c->isUnion ? 'T' : c && c->isStruct ? 'U' : 'V';
    if (!c) return std::string(1, letter) + "unknown@@";
    return letter + symbolName(c) + scopesOf(c->scope) + "@";
}

std::string SemaMangler::type(const sema::Type* t)
{
    if (!t) return "X";
    switch (t->kind) {
    case sema::Type::Void: return "X";
    case sema::Type::Bool: return "_N";
    case sema::Type::Char: return "D";
    case sema::Type::SChar: return "C";
    case sema::Type::UChar: return "E";
    case sema::Type::WChar: return "_W";
    case sema::Type::Short: return "F";
    case sema::Type::UShort: return "G";
    case sema::Type::Int: return "H";
    case sema::Type::UInt: return "I";
    case sema::Type::Long: return "J";
    case sema::Type::ULong: return "K";
    case sema::Type::LongLong: return "_J";
    case sema::Type::ULongLong: return "_K";
    case sema::Type::Float: return "M";
    case sema::Type::Double: return "N";
    case sema::Type::LongDouble: return "O";
    case sema::Type::NullPtr: return "PEAX";
    case sema::Type::Pointer: case sema::Type::Array: {
        const sema::Type* in = t->inner;
        if (in && in->kind == sema::Type::Function) {
            std::string s = "P6A" + type(in->inner);
            if (in->params.empty()) s += "X"; else { for (size_t i = 0; i < in->params.size(); ++i) s += type(in->params[i]); s += "@"; }
            return s + "Z";
        }
        std::string pcv = in && in->isConst ? (in->isVolatile ? "D" : "B") : (in && in->isVolatile ? "C" : "A");
        return std::string(t->kind == sema::Type::Pointer && t->isConst ? "Q" : "P") + "E" + pcv + type(in);
    }
    case sema::Type::Reference: {
        const sema::Type* in = t->inner;
        std::string pcv = in && in->isConst ? (in->isVolatile ? "D" : "B") : (in && in->isVolatile ? "C" : "A");
        return "AE" + pcv + type(in);
    }
    case sema::Type::Class: return classType(t);
    case sema::Type::Enum: return "W4" + (t->decl ? symbolName(t->decl) + scopesOf(t->decl->scope) : std::string("enum@")) + "@";
    case sema::Type::Function: {
        std::string s = "$$A6A" + type(t->inner);
        if (t->params.empty()) s += "X"; else { for (size_t i = 0; i < t->params.size(); ++i) s += type(t->params[i]); s += "@"; }
        return s + "Z";
    }
    case sema::Type::MemberPointer: return "PEAX";
    default: return "X";
    }
}

std::string SemaMangler::paramType(const sema::Type* t)
{
    std::string enc = type(t);
    bool primitive = enc.size() == 1 || (enc.size() == 2 && enc[0] == '_');
    if (primitive) return enc;
    for (size_t i = 0; i < m_argTypes.size(); ++i)
        if (m_argTypes[i] == enc) return std::string(1, static_cast<char>('0' + i));
    if (m_argTypes.size() < 10) m_argTypes.push_back(enc);
    return enc;
}

std::string SemaMangler::operatorCode(const std::string& fname, bool& ok)
{
    static const char* const table[][2] = {
        {"=", "4"}, {"+", "H"}, {"-", "G"}, {"*", "D"}, {"/", "K"}, {"%", "L"}, {"==", "8"}, {"!=", "9"},
        {"<", "M"}, {">", "O"}, {"<=", "N"}, {">=", "P"}, {"[]", "A"}, {"()", "R"}, {"->", "C"}, {"++", "E"},
        {"--", "F"}, {"!", "7"}, {"|", "U"}, {"&", "I"}, {"^", "T"}, {"~", "S"}, {">>", "5"}, {"<<", "6"},
        {"+=", "Y"}, {"-=", "Z"}, {"*=", "X"}, {"/=", "_0"}, {"%=", "_1"}, {">>=", "_2"}, {"<<=", "_3"},
        {"&=", "_4"}, {"|=", "_5"}, {"^=", "_6"}, {"&&", "V"}, {"||", "W"}, {",", "Q"}, {"->*", "J"},
        {"new", "2"}, {"delete", "3"}, {"new[]", "_U"}, {"delete[]", "_V"}
    };
    std::string op = trim(fname.substr(8));
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
        if (op == table[i][0]) { ok = true; return table[i][1]; }
    ok = false;
    return "";
}

std::string SemaMangler::function(const sema::Symbol& f)
{
    const sema::Symbol* owner = f.ownerClass;
    if (f.isExternC) return f.name;
    if (f.name == "main" && !owner && f.scope && f.scope->kind == sema::Scope::Global) return "main";
    std::string out = "?";
    if (f.isConstructor) out += "?0";
    else if (f.isDestructor) out += "?1";
    else if (f.isConversion) out += "?B";
    else if (f.isOperator) { bool ok; std::string code = operatorCode(f.name, ok); out += ok ? "?" + code : name(f.name); }
    else if (f.instantiatedFrom && !f.templateArgs.empty()) out += templateName(&f, f.instantiatedFrom->name);
    else out += name(f.name);
    out += scopesOf(f.scope) + "@";
    const sema::Type* ft = f.type;
    if (owner && !f.isStatic) {
        char acc = f.access == cxx::AccessPrivate ? 'A' : f.access == cxx::AccessProtected ? 'I' : 'Q';
        if (f.isVirtual) acc = static_cast<char>(acc + 4);
        out += acc;
        out += "E";
        out += ft && ft->constMethod ? "B" : "A";
        out += "A";
    } else if (owner) {
        out += f.access == cxx::AccessPrivate ? "C" : f.access == cxx::AccessProtected ? "K" : "S";
        out += "A";
    } else out += "YA";
    if (f.isConstructor || f.isDestructor) out += "@";
    else out += type(ft ? ft->inner : 0);
    if (!ft || ft->params.empty()) out += "X";
    else {
        for (size_t i = 0; i < ft->params.size(); ++i) out += paramType(ft->params[i]);
        out += ft->variadic ? "Z" : "@";
    }
    return out + "Z";
}

std::string SemaMangler::variable(const sema::Symbol& v)
{
    std::string out = "?" + name(v.name) + scopesOf(v.scope) + "@";
    if (v.isMember) out += v.access == cxx::AccessPrivate ? "0" : v.access == cxx::AccessProtected ? "1" : "2";
    else out += "3";
    out += type(v.type);
    out += v.type && v.type->isConst ? "B" : "A";
    return out;
}

std::string SemaMangler::vtable(const sema::Symbol& c)
{
    return "??_7" + symbolName(&c) + scopesOf(c.scope) + "@6B@";
}

} // namespace

std::string msvcMangleSymbol(const sema::Symbol& fn) { SemaMangler m; return m.function(fn); }
std::string msvcMangleVariableSymbol(const sema::Symbol& v) { SemaMangler m; return m.variable(v); }
std::string msvcMangleVtableSymbol(const sema::Symbol& c) { SemaMangler m; return m.vtable(c); }

} // namespace cppparse
