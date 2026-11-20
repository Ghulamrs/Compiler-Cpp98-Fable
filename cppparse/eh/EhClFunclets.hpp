// EhClFunclets.hpp - cl's exception model (--eh=cl) for one function of the code generator.
//
// The regions are the table model's (EhRegions). A region's handler blocks, with what they alone reach, are funclets:
// code that cl's runtime calls with the function's frame (the establisher frame, rsp after the prologue) in rdx, while
// the frames below are still on the stack. A cleanup funclet runs its landing pad's code and returns; a handler funclet
// runs the handler up to its catchend and returns the address the function continues at. This class finds
//   the owner of every block: the function itself, or one funclet (a block reached under two owners is refused);
//   the states: one per distinct stack of regions as its owner sees it (the trie of the stacks, in pre-order), and at
//   a try, after its body's states, one state per handler followed by the states of the regions that handler opens;
//   the TryBlockMap, inner tries first;
//   the order of the blocks: the function's, then each funclet's;
//   the values used on both sides of a funclet's border, which must live in the frame;
// and writes the funclets' prologues and epilogues, the labels of the IP-to-state map and the ";@" lines from which the
// object writer builds the FuncInfo (EhClFuncInfo). docs/design-cl-exceptions.md, section 5.2.
//
// Strict C++98.
#ifndef CPPPARSE_EHCLFUNCLETS_HPP
#define CPPPARSE_EHCLFUNCLETS_HPP

#include "EhEmitter.hpp"
#include "EhRegions.hpp"
#include "Ir.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace cppparse {

class EhClFunclets {
public:
    struct Funclet {
        int  root, parent;                // its entry block; the funclet whose code its region lies in (-1: the function's)
        bool isCatch;                     // a handler (its entry block starts with a catchparam), else a cleanup
        long region;                      // the region it serves
        int  state;                       // a handler's own state: its code outside the regions it opens
        bool jump;                        // a cleanup that is one destructor call on a local: "lea rcx, [rdx+n] / jmp", no frame
        std::string label;
        std::vector<std::string> pushed;  // the callee-saved registers its code writes: pushed in its prologue
        std::vector<std::string> xmms;    // and the xmm ones, saved in its own frame
        int  alloc, xmmAt;                // its frame below the pushes; where the xmm saves begin in it
        Funclet() : root(-1), parent(-1), isCatch(false), region(-1), state(-1), jump(false), alloc(0), xmmAt(0) {}
    };
    struct Try { int low, high, catchHigh; std::vector<int> handlers; };
    // a handler's HandlerType: the code generator knows the frame offset of its variable
    struct Handler { long adjectives; std::string type; int object; Handler() : adjectives(0x40), object(0) {} };

    EhClFunclets() : m_stateNow(-1) {}
    // 'regions' computed for f; false when the blocks do not divide into the function's and the funclets', or a
    // region has no state: error() says why
    bool compute(const IrFunction& f, EhRegions& regions, const std::string& functionLabel);
    const std::string& error() const { return m_error; }
    bool fail(const std::string& what) { if (m_error.empty()) m_error = what; return false; }   // the function cannot be emitted in this model

    int owner(size_t block) const { return m_owner[block]; }   // -1: the function's own code, -2: not reached
    size_t count() const { return m_funclets.size(); }
    Funclet& at(size_t k) { return m_funclets[k]; }
    const Funclet& at(size_t k) const { return m_funclets[k]; }
    size_t stateCount() const { return m_states.size(); }

    // the blocks in emission order: the function's own first (ownCount of them), then each funclet's; next[b] is the
    // block laid out after b when it has the same owner, else -1
    void layout(const IrFunction& f, std::vector<int>& order, std::vector<bool>& emitted, std::vector<int>& next, size_t& ownCount) const;
    // the temporaries defined or used in the code of more than one owner
    void crossing(const IrFunction& f, std::set<int>& temps) const;
    // a funclet's frame: what it saves, the stack arguments its calls need
    void setFrame(size_t k, const std::vector<std::string>& pushed, const std::vector<std::string>& xmms, int outgoingArgs);
    int  dispFrame(size_t k) const;   // where a handler keeps the function's establisher frame, from its own

    // ---- emission ----
    // the regions open in the code that follows, in 'funclet' (-1: the function): a label where the state changes
    void setState(const std::vector<long>& stack, int funclet, EhEmitter& out);
    void endStates(EhEmitter& out) { state(-1, false, out); }
    void entry(size_t k, int frameSize, bool afterCall, EhEmitter& out);
    void exit(size_t k, const std::string& continueAt, EhEmitter& out) const;
    // the FuncInfo's lines; handlers[k] describes funclet k when it is a handler
    void writeTables(EhEmitter& out, int unwindHelp, const std::vector<Handler>& handlers) const;

private:
    typedef std::pair<int, std::vector<long> > Key;   // an owner and a stack of regions
    bool number(int owner, const std::vector<long>& stack, int parentState);
    void state(int s, bool force, EhEmitter& out);

    std::string          m_label, m_error;
    std::vector<int>     m_owner;
    std::vector<Funclet> m_funclets;
    std::map<long, std::vector<int> > m_regionFunclets;   // region -> its funclets, in handler order
    std::map<Key, std::vector<long> > m_openings;         // the regions an owner opens under a stack
    std::map<Key, int>   m_stackState;
    std::set<long>       m_triesDone;
    std::vector<std::pair<int, std::string> > m_states;   // the UnwindMap: the state it unwinds to, the action's label ("0": none)
    std::vector<Try>     m_tries;                         // the TryBlockMap: inner tries first
    std::vector<std::pair<std::string, int> > m_ips;      // the IpToStateMap: a label, the state from there on
    int                  m_stateNow;
};

} // namespace cppparse

#endif
