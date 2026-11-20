// EhUnwindInfo.cpp - see EhUnwindInfo.hpp.
#include "EhUnwindInfo.hpp"

#include <algorithm>
#include <map>

namespace cppparse {

bool EhUnwindInfo::write(const std::vector<EhUnwindStep>& steps, long prologEnd, unsigned long start, int flags, std::vector<unsigned char>& out, std::string& err)
{
    if (prologEnd < 0) { err = "PROC FRAME without .endprolog"; return false; }
    if (prologEnd - static_cast<long>(start) > 255) { err = "prologue longer than 255 bytes"; return false; }
    std::vector<unsigned long> slots;   // 16-bit each
    for (size_t k = steps.size(); k-- > 0;) {
        const EhUnwindStep& s = steps[k];
        const unsigned long at = s.offset - start;
        if (s.op == EhUnwindStep::Push) slots.push_back(at | (static_cast<unsigned long>(s.reg) << 12));                      // UWOP_PUSH_NONVOL
        else if (s.op == EhUnwindStep::Alloc) {
            if (s.value <= 128) slots.push_back(at | (2UL << 8) | (((s.value - 8) / 8) << 12));                             // UWOP_ALLOC_SMALL
            else if (s.value <= 524280UL) { slots.push_back(at | (1UL << 8)); slots.push_back(s.value / 8); }               // UWOP_ALLOC_LARGE, size/8
            else { slots.push_back(at | (1UL << 8) | (1UL << 12)); slots.push_back(s.value & 0xFFFFUL); slots.push_back(s.value >> 16); }
        } else {
            if (s.value / 16 <= 0xFFFFUL) { slots.push_back(at | (8UL << 8) | (static_cast<unsigned long>(s.reg) << 12)); slots.push_back(s.value / 16); }   // UWOP_SAVE_XMM128
            else { slots.push_back(at | (9UL << 8) | (static_cast<unsigned long>(s.reg) << 12)); slots.push_back(s.value & 0xFFFFUL); slots.push_back(s.value >> 16); }
        }
    }
    if (slots.size() > 255) { err = "too many unwind codes"; return false; }
    out.push_back(static_cast<unsigned char>(1 | (flags << 3)));   // version 1, the flags (UNW_FLAG_EHANDLER with a handler)
    out.push_back(static_cast<unsigned char>(prologEnd - static_cast<long>(start)));
    out.push_back(static_cast<unsigned char>(slots.size()));
    out.push_back(0);                                              // no frame register
    if (slots.size() % 2) slots.push_back(0);
    for (size_t k = 0; k < slots.size(); ++k) { out.push_back(static_cast<unsigned char>(slots[k] & 0xFF)); out.push_back(static_cast<unsigned char>((slots[k] >> 8) & 0xFF)); }
    return true;
}

const unsigned long EhRangeTable::kMagic = 0x58505043UL;   // "CPPX": the range table that follows the handler is cppparse's

bool EhRangeTable::write(const std::string& fn, const std::string& handler, const std::vector<EhRange>& given,
                         std::vector<unsigned char>& out, std::vector<EhReloc>& relocs, std::string& err)
{
    std::vector<std::pair<unsigned long, std::pair<unsigned long, unsigned long> > > ranges;   // begin -> (end, pad)
    for (size_t r = 0; r < given.size(); ++r)
        if (given[r].begin < given[r].end) ranges.push_back(std::make_pair(given[r].begin, std::make_pair(given[r].end, given[r].pad)));   // (an empty range holds no code)
    std::sort(ranges.begin(), ranges.end());
    std::vector<unsigned long> entries, pads;
    std::map<unsigned long, unsigned long> padIndex;
    for (size_t r = 0; r < ranges.size(); ++r) {
        const unsigned long pad = ranges[r].second.second;
        std::map<unsigned long, unsigned long>::iterator p = padIndex.find(pad);
        if (p == padIndex.end()) { pads.push_back(pad); p = padIndex.insert(std::make_pair(pad, static_cast<unsigned long>(pads.size()))).first; }
        entries.push_back((ranges[r].first << 12) | p->second);
        const unsigned long end = ranges[r].second.first;
        if (r + 1 == ranges.size() || ranges[r + 1].first != end) entries.push_back(end << 12);
    }
    if (pads.size() > 4095 || (!ranges.empty() && ranges.back().second.first >= (1UL << 20))) { err = "too many landing pads or too much code for the range table of " + fn; return false; }
    std::vector<std::pair<std::string, unsigned long> > fields;   // symbol (empty: a plain value) and value / addend
    fields.push_back(std::make_pair(handler, 0UL));
    fields.push_back(std::make_pair(std::string(), kMagic));
    fields.push_back(std::make_pair(std::string(), static_cast<unsigned long>(entries.size())));
    fields.push_back(std::make_pair(std::string(), static_cast<unsigned long>(pads.size())));
    for (size_t e = 0; e < entries.size(); ++e) fields.push_back(std::make_pair(std::string(), entries[e]));
    for (size_t p = 0; p < pads.size(); ++p) fields.push_back(std::make_pair(fn, pads[p]));
    for (size_t f = 0; f < fields.size(); ++f) {
        if (!fields[f].first.empty()) { EhReloc rl; rl.offset = static_cast<unsigned long>(out.size()); rl.symbol = fields[f].first; relocs.push_back(rl); }
        for (int b = 0; b < 4; ++b) out.push_back(static_cast<unsigned char>((fields[f].second >> (8 * b)) & 0xFF));
    }
    return true;
}

} // namespace cppparse
