// EhTable.cpp - see EhTable.hpp.
#include "EhTable.hpp"
#include "PathUtil.hpp"

namespace cppparse {

void EhRanges::set(const std::vector<long>& now, EhEmitter& out)
{
    m_now = now;
    const long inner = now.empty() ? -1 : now.back();
    if (inner == m_open) return;
    if (m_open >= 0) {
        m_ranges.back().end = m_label + "_R" + intToString(static_cast<int>(m_ranges.size())) + "e";
        out.ehText(m_ranges.back().end + ":\n");
    }
    m_open = inner;
    if (inner >= 0) {
        Range r;
        r.begin = m_label + "_R" + intToString(static_cast<int>(m_ranges.size()) + 1) + "b";
        r.region = inner;
        m_ranges.push_back(r);
        out.ehText(r.begin + ":\n");
    }
}

std::string EhRanges::stubLabel(const std::string& functionLabel, long region)
{
    return functionLabel + "_T" + intToString(static_cast<int>(region));
}

void EhRanges::write(EhEmitter& out, const std::map<long, std::string>& padLabel) const
{
    // read by the COFF writer (a comment to ml64): code range, landing pad
    for (size_t r = 0; r < m_ranges.size(); ++r) {
        const std::map<long, std::string>::const_iterator pad = padLabel.find(m_ranges[r].region);
        out.ehText(";@ehrange " + m_ranges[r].begin + ", " + m_ranges[r].end + ", " + (pad != padLabel.end() ? pad->second : stubLabel(m_label, m_ranges[r].region)) + "\n");
    }
}

void EhPads::stubBegin(EhEmitter& out, const std::string& functionLabel, long region)
{
    const std::string id = intToString(static_cast<int>(region));
    out.ehText(functionLabel + "_T" + id + ":        ; handlers of try region " + id + "\n");
    out.ehExtern("cppparse_match");
    out.ehExtern("cppparse_rethrow");
}

bool EhPads::clause(EhEmitter& out, const std::string& type, const std::string& handlerLabel)
{
    if (type == "...") { out.ehLine("jmp " + handlerLabel + "        ; catch (...)"); return true; }
    out.ehLine("lea rcx, " + out.ehString("\"" + type + "\""));
    out.ehLine("call cppparse_match");
    out.ehLine("test eax, eax");
    out.ehLine("jne " + handlerLabel + "        ; catch (" + type + ")");
    return false;
}

void EhPads::noMatch(EhEmitter& out)
{
    out.ehLine("call cppparse_rethrow            ; no clause matched: outer frame");
    // the call does not return; int 3 keeps its return address inside this function, where the stack walk of the
    // exception looks it up (as cl and clang end a function with a call that does not return)
    out.ehLine("int 3");
}

void EhPads::catchBegin(EhEmitter& out, const std::string& type)
{
    out.ehExtern("cppparse_catch_begin");
    out.ehLine("call cppparse_catch_begin       ; catch (" + type + "): rax = object");
}

void EhPads::catchEnd(EhEmitter& out)
{
    out.ehExtern("cppparse_catch_end");
    out.ehLine("call cppparse_catch_end");
}

bool EhPads::isDestroy(const std::string& callee)
{
    return callee == "cppparse_unwind_destroy";   // IrGen::emitPads
}

const char* EhPads::destroyNext()
{
    return "cppparse_unwind_destroy_next";
}

void EhPads::next(EhEmitter& out, const std::string& padLabel)
{
    out.ehLine("jmp " + padLabel + "        ; the exception goes on at the frame's next landing pad");
}

} // namespace cppparse
