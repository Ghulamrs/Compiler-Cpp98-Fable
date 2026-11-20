// EhClThrowInfo.cpp - see EhClThrowInfo.hpp.
#include "EhClThrowInfo.hpp"
#include "PathUtil.hpp"

namespace cppparse {

namespace {

IrDataItem ehInt(long v) { IrDataItem it; it.kind = IrDataItem::Int; it.size = 4; it.value = v; return it; }
IrDataItem ehRva(const std::string& name) { IrDataItem it; it.kind = IrDataItem::Rva; it.size = 4; it.text = name; return it; }

// a displacement in a name: digits, 'm' for the minus sign
std::string dispText(long v) { return v < 0 ? "m" + intToString(static_cast<int>(-v)) : intToString(static_cast<int>(v)); }

// an IR global name holds letters, digits and underscores only; the hash of the exact text keeps two names apart that
// differ in other characters alone
std::string irKey(const std::string& text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        out += (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80 ? text[i] : '_';
    }
    return out + "_" + EhClThrowInfo::hex8(EhClThrowInfo::hash(text));
}

} // namespace

unsigned long EhClThrowInfo::hash(const std::string& text)
{
    unsigned long h = 2166136261UL;
    for (size_t i = 0; i < text.size(); ++i) h = ((h ^ static_cast<unsigned char>(text[i])) * 16777619UL) & 0xFFFFFFFFUL;
    return h;
}

std::string EhClThrowInfo::hex8(unsigned long v)
{
    static const char digits[] = "0123456789abcdef";
    std::string s(8, '0');
    for (int i = 7; i >= 0; --i) { s[static_cast<size_t>(i)] = digits[v & 15]; v >>= 4; }
    return s;
}

std::string EhClThrowInfo::catchableName(const EhCatchable& c, bool decorated)
{
    const std::string rest = intToString(c.scalar ? 1 : c.virtualBases ? 4 : 0) + ";" + intToString(static_cast<int>(c.size)) + ";" + c.copyFunction;
    const std::string text = "$cppCT$" + c.typeName + "$" + dispText(c.mdisp) + "$" + dispText(c.pdisp) + "$" + dispText(c.vdisp) + "$" + hex8(hash(rest));
    return decorated ? text : "eh_ct_" + irKey(text);
}

std::string EhClThrowInfo::arrayName(const EhThrown& t, bool decorated)
{
    std::string list;
    for (size_t k = 0; k < t.catchables.size(); ++k) list += catchableName(t.catchables[k], true) + ";";
    const std::string text = "$cppCTA$" + t.typeName + "$" + hex8(hash(list));
    return decorated ? text : "eh_cta_" + irKey(text);
}

std::string EhClThrowInfo::infoName(const EhThrown& t, bool decorated)
{
    const std::string rest = std::string(t.isConst ? "c" : "") + (t.isVolatile ? "v" : "") + ";" + t.destructor + ";" + arrayName(t, true);
    const std::string text = "$cppTI$" + t.typeName + "$" + hex8(hash(rest));
    return decorated ? text : "eh_ti_" + irKey(text);
}

std::string EhClThrowInfo::name(const EhThrown& t) { return infoName(t, false); }

void EhClThrowInfo::build(const EhThrown& t, std::vector<EhThrowTable>& out)
{
    EhThrowTable array;
    array.name = arrayName(t, false);
    array.decorated = arrayName(t, true);
    array.items.push_back(ehInt(static_cast<long>(t.catchables.size())));
    for (size_t k = 0; k < t.catchables.size(); ++k) {
        const EhCatchable& c = t.catchables[k];
        EhThrowTable ct;
        ct.name = catchableName(c, false);
        ct.decorated = catchableName(c, true);
        ct.items.push_back(ehInt(c.scalar ? 1 : c.virtualBases ? 4 : 0));
        ct.items.push_back(ehRva(c.descriptor));
        ct.items.push_back(ehInt(c.mdisp));
        ct.items.push_back(ehInt(c.pdisp));
        ct.items.push_back(ehInt(c.vdisp));
        ct.items.push_back(ehInt(c.size > 0 ? c.size : 1));
        ct.items.push_back(c.copyFunction.empty() ? ehInt(0) : ehRva(c.copyFunction));
        out.push_back(ct);
        array.items.push_back(ehRva(ct.name));
    }
    out.push_back(array);
    EhThrowTable info;
    info.name = infoName(t, false);
    info.decorated = infoName(t, true);
    info.items.push_back(ehInt((t.isConst ? 1 : 0) | (t.isVolatile ? 2 : 0)));
    info.items.push_back(t.destructor.empty() ? ehInt(0) : ehRva(t.destructor));
    info.items.push_back(ehInt(0));
    info.items.push_back(ehRva(array.name));
    out.push_back(info);
}

} // namespace cppparse
