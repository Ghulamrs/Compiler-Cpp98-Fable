// MapFile.cpp - the object map in the layout of a Visual Studio linker map.
#include "MapFile.hpp"

#include <map>
#include <sstream>
#include <vector>

namespace cppparse {

namespace {

std::string hexText(unsigned long v, int digits)
{
    static const char* const kDigits = "0123456789abcdef";
    std::string s(static_cast<size_t>(digits), '0');
    for (int i = digits - 1; i >= 0 && v; --i) { s[static_cast<size_t>(i)] = kDigits[v & 0xF]; v >>= 4; }
    return s;
}

std::string address(int section, unsigned long offset)
{
    return hexText(static_cast<unsigned long>(section), 4) + ":" + hexText(offset, 8);
}

std::string pad(const std::string& s, size_t width)
{
    return s.size() >= width ? s : s + std::string(width - s.size(), ' ');
}

std::string typeOf(const CoffSymbolInfo& y, const std::string& readable)
{
    if (y.isFunction) return "function";
    if (readable.compare(0, 14, "string literal") == 0) return "string";
    if (readable.compare(0, 15, "double constant") == 0 || y.name == "sign_mask") return "constant";
    const std::string vftable = "`vftable'";
    if (readable.size() > vftable.size() && readable.compare(readable.size() - vftable.size(), vftable.size(), vftable) == 0) return "vtable";
    return "data";
}

void symbolLines(std::ostringstream& s, const CoffResult& r, const std::map<std::string, std::string>& names, bool external)
{
    s << "  Address         Size        Type      Symbol                                    Name\n\n";
    int count = 0;
    for (size_t i = 0; i < r.symbolList.size(); ++i) {
        const CoffSymbolInfo& y = r.symbolList[i];
        if (y.external != external) continue;
        std::map<std::string, std::string>::const_iterator n = names.find(y.name);
        const std::string readable = n == names.end() ? std::string() : n->second;
        s << " " << address(y.section, y.offset) << "   " << hexText(y.size, 8) << "H   " << pad(typeOf(y, readable), 9) << " "
          << pad(y.name, 41) << (readable == y.name ? std::string() : " " + readable) << "\n";
        ++count;
    }
    if (count == 0) s << "  (none)\n";
}

} // namespace

std::string renderObjectMap(const std::string& title, const std::string& object, const CoffResult& r,
                            const std::map<std::string, std::string>& names)
{
    std::ostringstream s;
    s << " " << title << "\n\n";
    s << " cppparse object map of " << object << "\n";
    s << " Addresses are section:offset in the object; sizes are the encoded bytes of each definition.\n\n";
    if (!r.errors.empty()) {
        s << " not available: " << r.errors.size() << " line(s) of the assembly could not be encoded\n";
        for (size_t i = 0; i < r.errors.size() && i < 10; ++i) s << "   " << r.errors[i] << "\n";
        return s.str();
    }

    // the plain sections one per line; COMDAT sections (one per inline function, template instance or vtable)
    // grouped by name and class: the first and last section number, how many, and their total size
    s << " Start         Length     Name                   Class\n";
    unsigned long code = 0, data = 0;
    for (size_t i = 0; i < r.sectionList.size(); ++i) {
        const CoffSectionInfo& c = r.sectionList[i];
        if (c.isCode) code += c.size; else data += c.size;
        if (!c.comdat) s << " " << address(c.number, 0) << " " << hexText(c.size, 8) << "H  " << pad(c.name, 22) << " " << (c.isCode ? "CODE" : "DATA") << "\n";
    }
    std::vector<std::string> groupOrder;
    std::map<std::string, std::pair<int, int> > groupRange;      // name/class -> first, last section number
    std::map<std::string, std::pair<int, unsigned long> > groupSize;   // count, bytes
    for (size_t i = 0; i < r.sectionList.size(); ++i) {
        const CoffSectionInfo& c = r.sectionList[i];
        if (!c.comdat) continue;
        std::string key = pad(c.name, 22) + " " + (c.isCode ? "CODE" : "DATA");
        if (!groupRange.count(key)) { groupOrder.push_back(key); groupRange[key] = std::make_pair(c.number, c.number); groupSize[key] = std::make_pair(0, 0UL); }
        groupRange[key].second = c.number;
        groupSize[key].first++;
        groupSize[key].second += c.size;
    }
    for (size_t g = 0; g < groupOrder.size(); ++g) {
        const std::string& key = groupOrder[g];
        s << " " << hexText(static_cast<unsigned long>(groupRange[key].first), 4) << "-" << hexText(static_cast<unsigned long>(groupRange[key].second), 4)
          << "     " << hexText(groupSize[key].second, 8) << "H  " << key << "  COMDAT, " << groupSize[key].first << " section(s)\n";
    }
    int functions = 0, publics = 0;
    for (size_t i = 0; i < r.symbolList.size(); ++i) { if (r.symbolList[i].isFunction) ++functions; if (r.symbolList[i].external) ++publics; }
    s << "\n Totals: code " << code << " bytes, data " << data << " bytes; " << r.symbolList.size() << " symbols ("
      << functions << " functions), " << publics << " public, " << (r.symbolList.size() - static_cast<size_t>(publics)) << " static\n\n";

    s << " Publics by Value\n\n";
    symbolLines(s, r, names, true);
    s << "\n Static symbols\n\n";
    symbolLines(s, r, names, false);

    for (size_t i = 0; i < r.symbolList.size(); ++i)
        if (r.symbolList[i].isFunction && r.symbolList[i].name == "main") {
            s << "\n entry point at        " << address(r.symbolList[i].section, r.symbolList[i].offset) << "\n";
            break;
        }
    return s.str();
}

} // namespace cppparse
