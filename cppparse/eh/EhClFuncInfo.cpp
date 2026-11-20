// EhClFuncInfo.cpp - see EhClFuncInfo.hpp.
#include "EhClFuncInfo.hpp"
#include "PathUtil.hpp"

#include <cstddef>
#include <utility>

namespace cppparse {

namespace {

void put32(std::vector<unsigned char>& out, unsigned long v) { for (int b = 0; b < 4; ++b) out.push_back(static_cast<unsigned char>((v >> (8 * b)) & 0xFF)); }

// the fields of the tables: a plain value, or an RVA (a relocation against 'symbol', the value its addend)
struct Fields {
    std::vector<std::pair<std::string, unsigned long> > list;
    void value(long v) { list.push_back(std::make_pair(std::string(), static_cast<unsigned long>(v))); }
    void rva(const std::string& symbol, unsigned long addend) { list.push_back(std::make_pair(symbol, addend)); }
};

} // namespace

void EhClFuncInfo::ipMap(const EhClFunction& f, std::vector<EhClIp>& out)
{
    out.clear();
    for (size_t k = 0; k < f.ips.size(); ++k) {
        if (!out.empty() && out.back().at == f.ips[k].at) out.back().state = f.ips[k].state;
        else out.push_back(f.ips[k]);
    }
    for (size_t k = 0; k + 1 < out.size();) { if (out[k].state == out[k + 1].state) out.erase(out.begin() + static_cast<std::ptrdiff_t>(k + 1)); else ++k; }
    while (!out.empty() && out[0].state == -1) out.erase(out.begin());   // before the first entry the state is -1 anyway
    for (size_t k = 0; k + 1 < out.size();) { if (out[k].state == out[k + 1].state) out.erase(out.begin() + static_cast<std::ptrdiff_t>(k + 1)); else ++k; }
}

bool EhClFuncInfo::write(const EhClFunction& f, const std::string& fn, const std::string& self, const std::string& handler,
                         std::vector<unsigned char>& out, std::vector<EhReloc>& relocs, std::vector<EhFrameEntry>& entries, std::string& err)
{
    out.clear(); relocs.clear(); entries.clear();
    std::vector<size_t> infoFields;   // where the RVA of the FuncInfo goes
    // the function: EHANDLER | UHANDLER with a try, UHANDLER alone with cleanups only (the search phase passes it by)
    EhFrameEntry own;
    own.begin = 0; own.end = f.funclets.empty() ? f.codeSize : f.funclets[0].start; own.unwindAt = 0;
    if (!EhUnwindInfo::write(f.unwind, f.prologEnd, 0, f.tries.empty() ? EhUnwindInfo::FlagUHandler : EhUnwindInfo::FlagEHandler | EhUnwindInfo::FlagUHandler, out, err)) return false;
    { EhReloc rl; rl.offset = static_cast<unsigned long>(out.size()); rl.symbol = handler; relocs.push_back(rl); put32(out, 0); }
    infoFields.push_back(out.size()); put32(out, 0);
    entries.push_back(own);
    for (size_t k = 0; k < f.funclets.size(); ++k) {
        const EhClFunclet& fc = f.funclets[k];
        if (fc.kind == EhClFunclet::Jump) continue;
        EhFrameEntry e;
        e.begin = fc.start; e.end = k + 1 < f.funclets.size() ? f.funclets[k + 1].start : f.codeSize; e.unwindAt = static_cast<unsigned long>(out.size());
        if (!EhUnwindInfo::write(fc.unwind, fc.prologEnd, fc.start, fc.kind == EhClFunclet::Catch ? EhUnwindInfo::FlagEHandler | EhUnwindInfo::FlagUHandler : 0, out, err)) { err = "funclet: " + err; return false; }
        if (fc.kind == EhClFunclet::Catch) {
            EhReloc rl; rl.offset = static_cast<unsigned long>(out.size()); rl.symbol = handler; relocs.push_back(rl); put32(out, 0);
            infoFields.push_back(out.size()); put32(out, 0);
        }
        entries.push_back(e);
    }
    std::vector<EhClIp> ips;
    ipMap(f, ips);
    const unsigned long nStates = static_cast<unsigned long>(f.states.size()), nTries = static_cast<unsigned long>(f.tries.size());
    unsigned long nHandlers = 0;
    for (size_t t = 0; t < f.tries.size(); ++t) nHandlers += static_cast<unsigned long>(f.tries[t].catches.size());
    const unsigned long infoAt = static_cast<unsigned long>(out.size());
    const unsigned long unwindMapAt = infoAt + 40, tryMapAt = unwindMapAt + 8 * nStates;
    unsigned long handlersAt = tryMapAt + 20 * nTries;
    const unsigned long ipMapAt = handlersAt + 20 * nHandlers;
    for (size_t k = 0; k < infoFields.size(); ++k) {
        EhReloc rl; rl.offset = static_cast<unsigned long>(infoFields[k]); rl.symbol = self; relocs.push_back(rl);
        for (int b = 0; b < 4; ++b) out[infoFields[k] + static_cast<size_t>(b)] = static_cast<unsigned char>((infoAt >> (8 * b)) & 0xFF);
    }
    Fields fl;
    fl.value(0x19930522L);
    fl.value(static_cast<long>(nStates)); if (nStates) fl.rva(self, unwindMapAt); else fl.value(0);
    fl.value(static_cast<long>(nTries)); if (nTries) fl.rva(self, tryMapAt); else fl.value(0);
    fl.value(static_cast<long>(ips.size())); if (!ips.empty()) fl.rva(self, ipMapAt); else fl.value(0);
    fl.value(f.unwindHelp);
    fl.value(0);   // no exception specification
    fl.value(1);   // /EHs: catch (...) takes C++ exceptions only, others pass without unwinding
    for (size_t s = 0; s < f.states.size(); ++s) {
        fl.value(f.states[s].to);
        if (f.states[s].action < 0) fl.value(0); else fl.rva(fn, static_cast<unsigned long>(f.states[s].action));
    }
    for (size_t t = 0; t < f.tries.size(); ++t) {
        const EhClTry& tb = f.tries[t];
        fl.value(tb.low); fl.value(tb.high); fl.value(tb.catchHigh); fl.value(static_cast<long>(tb.catches.size()));
        fl.rva(self, handlersAt);
        handlersAt += 20 * static_cast<unsigned long>(tb.catches.size());
    }
    for (size_t t = 0; t < f.tries.size(); ++t)
        for (size_t h = 0; h < f.tries[t].catches.size(); ++h) {
            const EhClCatch& c = f.tries[t].catches[h];
            fl.value(static_cast<long>(c.adjectives));
            if (c.type.empty()) fl.value(0); else fl.rva(c.type, 0);
            fl.value(c.dispObject);
            fl.rva(fn, c.handler);
            fl.value(c.dispFrame);
        }
    for (size_t k = 0; k < ips.size(); ++k) { fl.rva(fn, ips[k].at + 1); fl.value(ips[k].state); }
    for (size_t k = 0; k < fl.list.size(); ++k) {
        if (!fl.list[k].first.empty()) { EhReloc rl; rl.offset = static_cast<unsigned long>(out.size()); rl.symbol = fl.list[k].first; relocs.push_back(rl); }
        put32(out, fl.list[k].second);
    }
    return true;
}

// (i)   a state unwinds to a lower one (the runtime loops "while the current state is above the target")
// (ii)  a try's body is exactly tryLow..tryHigh: every state in it unwinds, through states of the body, to tryLow, and
//       tryLow unwinds to a state below the body; its handlers' states are exactly tryHigh+1..catchHigh, each
//       unwinding inside that range or to what tryLow unwinds to
// (iii) the TryBlockMap is in post-order: a try whose states lie in the body or the handlers of T comes before T
// (iv)  the IP map ascends and names no state at or above the state count
// (v)   every address of a handler funclet has a state in (tryHigh, catchHigh] of the try it handles, and the
//       HandlerType's address is the funclet's first byte, where its function table entry begins (the runtime finds
//       the function's frame from a handler's frame only then, and takes the handler's own rsp for it otherwise)
// (vi)  no function table range ends with a call: its return address would be the next range's first byte
bool EhClCheck::check(const EhClFunction& f, std::string& err)
{
    const long n = static_cast<long>(f.states.size());
    for (long s = 0; s < n; ++s)
        if (f.states[static_cast<size_t>(s)].to >= s || f.states[static_cast<size_t>(s)].to < -1) { err = "state " + intToString(static_cast<int>(s)) + " unwinds to " + intToString(static_cast<int>(f.states[static_cast<size_t>(s)].to)); return false; }
    for (size_t t = 0; t < f.tries.size(); ++t) {
        const EhClTry& tb = f.tries[t];
        const std::string name = "try " + intToString(static_cast<int>(t)) + " [" + intToString(static_cast<int>(tb.low)) + ", " + intToString(static_cast<int>(tb.high)) + ", " + intToString(static_cast<int>(tb.catchHigh)) + "]";
        if (tb.low < 0 || tb.low > tb.high || tb.high >= tb.catchHigh || tb.catchHigh >= n) { err = name + ": states out of order or out of range"; return false; }
        const long outside = f.states[static_cast<size_t>(tb.low)].to;
        for (long s = tb.low + 1; s <= tb.high; ++s)
            if (f.states[static_cast<size_t>(s)].to < tb.low) { err = name + ": state " + intToString(static_cast<int>(s)) + " of the body does not unwind through tryLow"; return false; }
        for (long s = tb.high + 1; s <= tb.catchHigh; ++s) {
            const long to = f.states[static_cast<size_t>(s)].to;
            if (!(to == outside || (to > tb.high && to < s))) { err = name + ": handler state " + intToString(static_cast<int>(s)) + " unwinds to " + intToString(static_cast<int>(to)); return false; }
        }
        if (tb.catches.empty()) { err = name + ": no handlers"; return false; }
        for (size_t u = t + 1; u < f.tries.size(); ++u) {
            // a later try must not lie inside this one: inside tries come first
            const EhClTry& o = f.tries[u];
            if (o.low >= tb.low && o.catchHigh <= tb.catchHigh) { err = name + ": try " + intToString(static_cast<int>(u)) + " lies inside it and comes after it"; return false; }
        }
        for (size_t u = 0; u < t; ++u) {
            const EhClTry& o = f.tries[u];
            const bool inside = o.low >= tb.low && o.catchHigh <= tb.catchHigh, apart = o.catchHigh < tb.low || o.low > tb.catchHigh;
            if (!inside && !apart) { err = name + ": overlaps try " + intToString(static_cast<int>(u)); return false; }
            if (inside && o.low <= tb.high && o.catchHigh > tb.high) { err = name + ": try " + intToString(static_cast<int>(u)) + " straddles its body and its handlers"; return false; }
        }
    }
    std::vector<EhClIp> ips;
    EhClFuncInfo::ipMap(f, ips);
    for (size_t k = 0; k < f.ips.size(); ++k) {
        if (k > 0 && f.ips[k].at < f.ips[k - 1].at) { err = "the IP map does not ascend"; return false; }
        if (f.ips[k].state < -1 || f.ips[k].state >= n) { err = "the IP map names state " + intToString(static_cast<int>(f.ips[k].state)); return false; }
        if (f.ips[k].at >= f.codeSize && f.ips[k].state != -1) { err = "an IP entry behind the function's code"; return false; }
    }
    // the function table ranges: the function's own code, then each funclet up to the next
    std::vector<unsigned long> rangeEnds;
    for (size_t k = 0; k < f.funclets.size(); ++k) {
        if (k > 0 && f.funclets[k].start < f.funclets[k - 1].start) { err = "the funclets are not in address order"; return false; }
        rangeEnds.push_back(f.funclets[k].start);
    }
    rangeEnds.push_back(f.codeSize);
    for (size_t e = 0; e < rangeEnds.size(); ++e)
        for (size_t c = 0; c < f.callEnds.size(); ++c)
            if (f.callEnds[c] == rangeEnds[e]) { err = "a call ends a function table range at offset " + intToString(static_cast<int>(rangeEnds[e])); return false; }
    for (size_t t = 0; t < f.tries.size(); ++t)
        for (size_t h = 0; h < f.tries[t].catches.size(); ++h) {
            const EhClCatch& c = f.tries[t].catches[h];
            size_t k = 0;
            while (k < f.funclets.size() && f.funclets[k].start != c.handler) ++k;
            if (k == f.funclets.size() || f.funclets[k].kind != EhClFunclet::Catch) { err = "a handler's address is not the start of a handler funclet"; return false; }
            const unsigned long begin = f.funclets[k].start, end = k + 1 < f.funclets.size() ? f.funclets[k + 1].start : f.codeSize;
            // the states over [begin, end): the one in force at begin + 1 (a return address is never the first byte), then every entry inside
            long state = -1;
            for (size_t i = 0; i < ips.size() && ips[i].at <= begin; ++i) state = ips[i].state;
            if (state <= f.tries[t].high || state > f.tries[t].catchHigh) { err = "handler funclet at " + intToString(static_cast<int>(begin)) + " starts in state " + intToString(static_cast<int>(state)) + ", outside its try's handler states"; return false; }
            for (size_t i = 0; i < ips.size(); ++i)
                if (ips[i].at > begin && ips[i].at < end && (ips[i].state <= f.tries[t].high || ips[i].state > f.tries[t].catchHigh)) { err = "handler funclet at " + intToString(static_cast<int>(begin)) + " has code in state " + intToString(static_cast<int>(ips[i].state)) + ", outside its try's handler states"; return false; }
        }
    for (long s = 0; s < n; ++s) {
        const long a = f.states[static_cast<size_t>(s)].action;
        if (a < 0) continue;
        size_t k = 0;
        while (k < f.funclets.size() && f.funclets[k].start != static_cast<unsigned long>(a)) ++k;
        if (k == f.funclets.size() || f.funclets[k].kind == EhClFunclet::Catch) { err = "the action of state " + intToString(static_cast<int>(s)) + " is not a cleanup funclet"; return false; }
    }
    return true;
}

} // namespace cppparse
