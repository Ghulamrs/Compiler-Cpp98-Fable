// EhTable.hpp - the table model (--eh=table) in the code generator: the code ranges of the regions, and the landing
// pads' protocol with cppparse's runtime, which the setjmp model shares.
//
// A try region costs no code: the code it covers is listed as address ranges in the function's unwind info, each
// with the landing pad (the dispatch stub of its innermost region) where the runtime resumes this frame when an
// exception passes through a call in that range. EhRanges opens and closes the ranges as the emitter goes through
// the blocks and writes them as ";@ehrange begin, end, pad" lines, which the object writer turns into the range
// table (EhRangeTable; comments to ml64).
//
// A dispatch stub tries the clauses of its region in order:
//     lea rcx, "type"   ; call cppparse_match ; test eax, eax ; jne handler   (per catch clause)
//     call cppparse_rethrow                                                    (no clause matched)
//   handler:
//     call cppparse_catch_begin   -> rax = address of the thrown object (a copy owned by the runtime)
//     ... handler body ...
//     call cppparse_catch_end
//
// An object's landing pad whose cleanup is one destructor call ends with one call of the runtime, which destroys the
// object while the exception stays in flight. Where the pad's own code lies in a region of its function (an object
// built before this one is alive, or a try block is around it), the exception goes on at that region's landing pad,
// and the code generator knows which one that is: the pad calls the entry that returns and jumps there itself,
//     call cppparse_unwind_destroy_next ; jmp <the region's landing pad>
// so the pads of a frame run one after another without a walk of the stack, and every return goes to the
// instruction after its call: the runtime writes no return address (row G30). In no region of its function the
// pad ends with "call cppparse_unwind_destroy", which does not return: the exception leaves the frame.
//
// Strict C++98.
#ifndef CPPPARSE_EHTABLE_HPP
#define CPPPARSE_EHTABLE_HPP

#include "EhEmitter.hpp"

#include <map>
#include <string>
#include <vector>

namespace cppparse {

class EhRanges {
public:
    EhRanges() : m_open(-1) {}
    void begin(const std::string& functionLabel) { m_label = functionLabel; }
    // the regions open from here on (innermost last): closes / opens a range where the innermost region changes
    void set(const std::vector<long>& now, EhEmitter& out);
    const std::vector<long>& now() const { return m_now; }
    // the ";@ehrange" lines of the function: each range with its region's landing pad, the block named for the
    // region in 'padLabel' or else its dispatch stub
    void write(EhEmitter& out, const std::map<long, std::string>& padLabel) const;
    static std::string stubLabel(const std::string& functionLabel, long region);

private:
    struct Range { std::string begin, end; long region; };
    std::string        m_label;
    std::vector<long>  m_now;
    std::vector<Range> m_ranges;
    long               m_open;    // innermost region of the open range, -1 when none is open
};

class EhPads {
public:
    // a dispatch stub's label line and what it refers to
    static void stubBegin(EhEmitter& out, const std::string& functionLabel, long region);
    // one clause: "catch (...)" jumps to its handler and ends the stub (true is returned); any other asks the runtime
    static bool clause(EhEmitter& out, const std::string& type, const std::string& handlerLabel);
    // no clause matched: the exception goes on to the frame's next landing pad or to the caller
    static void noMatch(EhEmitter& out);
    // a handler's first and last instruction
    static void catchBegin(EhEmitter& out, const std::string& type);
    static void catchEnd(EhEmitter& out);
    // an object's landing pad (see above): whether a call is the one that destroys the pad's object, the entry of the
    // runtime that returns from it, and the jump to the frame's next landing pad that follows that entry's call
    static bool        isDestroy(const std::string& callee);
    static const char* destroyNext();
    static void        next(EhEmitter& out, const std::string& padLabel);
};

} // namespace cppparse

#endif
