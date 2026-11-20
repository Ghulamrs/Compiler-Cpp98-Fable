// EhClFuncInfo.hpp - cl's exception model (--eh=cl): the tables __CxxFrameHandler3 reads for one function, as bytes
// for the object writer (Coff.cpp), and the check of what the runtime takes for granted.
//
// A function with regions has, behind its UNWIND_INFO, the handler's address and the RVA of its FuncInfo:
//   FuncInfo      { 19930522h, states, RVA UnwindMap, tries, RVA TryBlockMap, IP entries, RVA IpToStateMap,
//                   the offset of the unwind-help slot from the establisher frame, 0, 1 (/EHs) }
//   UnwindMap     per state { the state it unwinds to, RVA of the action or 0 }
//   TryBlockMap   per try { tryLow, tryHigh, catchHigh, handlers, RVA HandlerTypes }
//   HandlerType   { adjectives (1 const, 2 volatile, 8 reference, 40h catch (...)), RVA type descriptor or 0, the
//                   offset of the handler's variable from the establisher frame (0: none), RVA of the handler funclet,
//                   the offset in the funclet's frame where it keeps the function's establisher frame }
//   IpToStateMap  { RVA, state } ascending: an address has the state of the last entry not above it
// A funclet (a handler, or a cleanup with a frame) has a function table entry and UNWIND_INFO of its own; a handler's
// names the same handler and FuncInfo. An IP entry is written as its address + 1: the runtime looks up return
// addresses, and the one of a call that ends a range is the next range's first byte (cl puts a nop there instead).
// docs/design-cl-exceptions.md, sections 3 and 5.2.
//
// Strict C++98.
#ifndef CPPPARSE_EHCLFUNCINFO_HPP
#define CPPPARSE_EHCLFUNCINFO_HPP

#include "EhUnwindInfo.hpp"

#include <string>
#include <vector>

namespace cppparse {

// a funclet of the function: code that cl's runtime calls with the function's frame in rdx
struct EhClFunclet {
    enum Kind { Jump, Cleanup, Catch };   // one jump to a destructor (no frame, no table entry); a cleanup with a frame; a handler
    unsigned long             start;      // code offset
    Kind                      kind;
    std::vector<EhUnwindStep> unwind;     // its prologue's steps, their offsets from the start of the function
    long                      prologEnd;  // code offset, -1 when absent
    EhClFunclet() : start(0), kind(Jump), prologEnd(-1) {}
};

struct EhClCatch {
    unsigned long adjectives;
    std::string   type;         // the type descriptor's symbol, "" for catch (...)
    long          dispObject;
    unsigned long handler;      // code offset of its funclet
    long          dispFrame;
};

struct EhClTry { long low, high, catchHigh; std::vector<EhClCatch> catches; };
struct EhClState { long to; long action; };   // action: code offset of the funclet, -1 for none
struct EhClIp { unsigned long at; long state; };

// everything of one function, in code offsets
struct EhClFunction {
    std::vector<EhUnwindStep> unwind;     // the function's own prologue
    long                      prologEnd;
    unsigned long             codeSize;
    long                      unwindHelp;
    std::vector<EhClState>    states;
    std::vector<EhClTry>      tries;
    std::vector<EhClIp>       ips;        // as the listing gives them, in address order
    std::vector<EhClFunclet>  funclets;   // in address order
    std::vector<unsigned long> callEnds;  // the code offsets just behind every call instruction
    EhClFunction() : prologEnd(-1), codeSize(0), unwindHelp(0) {}
};

// one function table entry (RUNTIME_FUNCTION): code offsets, and where its UNWIND_INFO lies in the bytes built
struct EhFrameEntry { unsigned long begin, end, unwindAt; };

class EhClFuncInfo {
public:
    // the bytes of the function's .xdata contribution: its UNWIND_INFO, the funclets', the FuncInfo and its maps.
    // 'fn' is the function's symbol (code addresses are relocations against it), 'self' the symbol of these bytes,
    // 'handler' __CxxFrameHandler3. 'entries' gets the function table entries, in address order
    static bool write(const EhClFunction& f, const std::string& fn, const std::string& self, const std::string& handler,
                      std::vector<unsigned char>& out, std::vector<EhReloc>& relocs, std::vector<EhFrameEntry>& entries, std::string& err);
    // the IP entries as the map holds them: one per address (the last one given for it), no entry that repeats the
    // state before it, none with state -1 at the head
    static void ipMap(const EhClFunction& f, std::vector<EhClIp>& out);
};

// What the runtime relies on and checks only by failing fast (0xC0000409, no message): see the .cpp. Run on every
// function the object writer gets in this model, so a wrong table is a compile error
class EhClCheck {
public:
    static bool check(const EhClFunction& f, std::string& err);
};

} // namespace cppparse

#endif
