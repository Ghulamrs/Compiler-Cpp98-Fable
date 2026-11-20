// EhClFunclets.cpp - see EhClFunclets.hpp.
#include "EhClFunclets.hpp"
#include "PathUtil.hpp"

#include <algorithm>

namespace cppparse {

// every block gets its owner: the entry block the function, a region's handler block a funclet of its own, a branch
// target its source's, and the target of the branch after a catchend the funclet around the handler's. A block
// reached under two owners would need two copies of its code: refused. Then the states
bool EhClFunclets::compute(const IrFunction& f, EhRegions& regions, const std::string& functionLabel)
{
    m_label = functionLabel;
    const size_t n = f.blocks.size();
    m_owner.assign(n, -2);
    if (n == 0) return true;
    std::map<int, int> funcletOf;   // handler block -> its funclet
    std::vector<int> work;
    m_owner[0] = -1;
    work.push_back(0);
    while (!work.empty()) {
        const size_t b = static_cast<size_t>(work.back());
        work.pop_back();
        const int o = m_owner[b];
        const IrBlock& blk = f.blocks[b];
        std::vector<long> stack = regions.in(b);
        for (size_t i = 0; i < blk.insts.size(); ++i) {
            const IrInst& in = blk.insts[i];
            std::vector<int> targets;
            int targetOwner = o;
            if (in.op == IrInst::TryBegin) {
                std::vector<long>& opened = m_openings[Key(o, stack)];
                if (std::find(opened.begin(), opened.end(), in.extra) == opened.end()) opened.push_back(in.extra);
                for (size_t a = 0; a < in.args.size(); ++a) {
                    if (in.args[a].kind != IrOperand::Block) continue;
                    const int h = in.args[a].id;
                    if (h < 0 || static_cast<size_t>(h) >= n) return fail("a handler block out of range");
                    std::map<int, int>::const_iterator known = funcletOf.find(h);
                    if (known != funcletOf.end()) {
                        if (m_funclets[static_cast<size_t>(known->second)].parent != o || m_funclets[static_cast<size_t>(known->second)].region != in.extra) return fail("a handler block serves two regions");
                        continue;
                    }
                    if (m_owner[static_cast<size_t>(h)] != -2) return fail("a handler block is also a branch target");
                    Funclet fc;
                    fc.root = h; fc.parent = o; fc.region = in.extra;
                    const std::vector<IrInst>& hi = f.blocks[static_cast<size_t>(h)].insts;
                    size_t first = 0;
                    while (first < hi.size() && hi[first].op == IrInst::Nop) ++first;
                    fc.isCatch = first < hi.size() && hi[first].op == IrInst::CatchParam;
                    const int index = static_cast<int>(m_funclets.size());
                    fc.label = m_label + "_F" + intToString(index);
                    m_funclets.push_back(fc);
                    funcletOf[h] = index;
                    m_regionFunclets[in.extra].push_back(index);
                    m_owner[static_cast<size_t>(h)] = index;
                    work.push_back(h);
                }
                stack.push_back(in.extra);
                continue;
            }
            if (in.op == IrInst::TryEnd) { if (!stack.empty()) stack.pop_back(); continue; }
            if (in.op == IrInst::CatchEnd) {
                if (o < 0 || !m_funclets[static_cast<size_t>(o)].isCatch) return fail("a catchend outside a handler");
                if (i + 2 != blk.insts.size() || blk.insts[i + 1].op != IrInst::Br || blk.insts[i + 1].args.empty()) return fail("a catchend that does not end its block");
                targets.push_back(blk.insts[i + 1].args[0].id);
                targetOwner = m_funclets[static_cast<size_t>(o)].parent;
            } else if (in.op == IrInst::Br || in.op == IrInst::CondBr) {
                for (size_t a = 0; a < in.args.size(); ++a) if (in.args[a].kind == IrOperand::Block) targets.push_back(in.args[a].id);
            } else if (in.op == IrInst::Ret && o >= 0) return fail("a return inside a handler or a cleanup");
            for (size_t t = 0; t < targets.size(); ++t) {
                if (targets[t] < 0 || static_cast<size_t>(targets[t]) >= n) return fail("a branch target out of range");
                int& to = m_owner[static_cast<size_t>(targets[t])];
                if (to == -2) { to = targetOwner; work.push_back(targets[t]); }
                else if (to != targetOwner) return fail("block " + intToString(targets[t]) + " is reached from two funclets");
            }
            if (in.op == IrInst::CatchEnd) break;
        }
    }
    // a cleanup whose code is one direct call of a destructor with the address of a local: it needs no frame
    for (size_t k = 0; k < m_funclets.size(); ++k) {
        Funclet& fc = m_funclets[k];
        if (fc.isCatch) continue;
        size_t blocks = 0;
        for (size_t b = 0; b < n; ++b) if (m_owner[b] == static_cast<int>(k)) ++blocks;
        if (blocks != 1) continue;
        std::vector<const IrInst*> code;
        const std::vector<IrInst>& hi = f.blocks[static_cast<size_t>(fc.root)].insts;
        for (size_t i = 0; i < hi.size(); ++i) if (hi[i].op != IrInst::Nop) code.push_back(&hi[i]);
        if (code.size() != 3) continue;
        const IrInst& ad = *code[0];
        const IrInst& call = *code[1];
        const IrInst& end = *code[2];
        if (ad.op != IrInst::AddrLocal || ad.dst < 0 || ad.args.empty() || ad.args[0].kind != IrOperand::Local) continue;
        if (call.op != IrInst::Call || call.dst >= 0 || call.sub == "(indirect)" || call.args.size() != 1 || call.args[0].kind != IrOperand::Temp || call.args[0].id != ad.dst) continue;
        if (end.op != IrInst::Throw || end.sub != "@cleanupret") continue;
        if (f.variadic && ad.args[0].id < f.paramCount) continue;
        fc.jump = true;
    }
    if (!number(-1, std::vector<long>(), -1)) return false;
    for (std::map<Key, std::vector<long> >::const_iterator o = m_openings.begin(); o != m_openings.end(); ++o)
        for (size_t k = 0; k < o->second.size(); ++k) {
            std::vector<long> stack = o->first.second;
            stack.push_back(o->second[k]);
            if (!m_stackState.count(Key(o->first.first, stack))) return fail("region " + intToString(static_cast<int>(o->second[k])) + " has no state (it is opened in a cleanup)");
        }
    return true;
}

// the states, as __CxxFrameHandler3 wants them: a stack's state, then those of the stacks that extend it; for a try,
// after all of its body's states one state per handler, each followed by the states of the regions that handler
// opens. So tryLow..tryHigh is the try with its body, tryHigh+1..catchHigh its handlers, and a try inside either comes
// before it in the TryBlockMap
bool EhClFunclets::number(int owner, const std::vector<long>& stack, int parentState)
{
    std::map<Key, std::vector<long> >::const_iterator o = m_openings.find(Key(owner, stack));
    if (o == m_openings.end()) return true;
    std::vector<long> regions = o->second;
    std::sort(regions.begin(), regions.end());
    for (size_t r = 0; r < regions.size(); ++r) {
        const long region = regions[r];
        const std::vector<int> funclets = m_regionFunclets.count(region) ? m_regionFunclets[region] : std::vector<int>();
        std::string action = "0";
        bool handlers = false;
        for (size_t k = 0; k < funclets.size(); ++k) {
            if (m_funclets[static_cast<size_t>(funclets[k])].isCatch) handlers = true;
            else action = m_funclets[static_cast<size_t>(funclets[k])].label;
        }
        const int state = static_cast<int>(m_states.size());
        m_states.push_back(std::make_pair(parentState, action));
        std::vector<long> inner = stack;
        inner.push_back(region);
        m_stackState[Key(owner, inner)] = state;
        if (!number(owner, inner, state)) return false;
        if (!handlers) continue;
        // a try's handlers get their states once: a try opened under two stacks would need two sets of handlers
        if (!m_triesDone.insert(region).second) return fail("try region " + intToString(static_cast<int>(region)) + " is opened under two stacks of regions");
        Try t;
        t.low = state; t.high = static_cast<int>(m_states.size()) - 1;
        for (size_t k = 0; k < funclets.size(); ++k) {
            Funclet& fc = m_funclets[static_cast<size_t>(funclets[k])];
            if (!fc.isCatch) continue;
            fc.state = static_cast<int>(m_states.size());
            m_states.push_back(std::make_pair(parentState, std::string("0")));
            m_stackState[Key(funclets[k], stack)] = fc.state;   // the handler runs in the regions around its try
            if (!number(funclets[k], stack, fc.state)) return false;
            t.handlers.push_back(funclets[k]);
        }
        t.catchHigh = static_cast<int>(m_states.size()) - 1;
        m_tries.push_back(t);
    }
    return true;
}

void EhClFunclets::layout(const IrFunction& f, std::vector<int>& order, std::vector<bool>& emitted, std::vector<int>& next, size_t& ownCount) const
{
    const size_t n = f.blocks.size();
    std::vector<bool> placed(n, false);
    order.clear();
    for (int owner = -1; owner < static_cast<int>(m_funclets.size()); ++owner) {
        // in chains, as the code generator lays out any group of blocks: after a block, one of its branch targets still
        // to place, by preference the one that only this block enters, then the one made right after this block, then
        // the true target before the false; a chain that ends continues with the group's lowest-numbered block
        int cur = owner < 0 ? 0 : m_funclets[static_cast<size_t>(owner)].root;
        while (cur >= 0) {
            placed[static_cast<size_t>(cur)] = true;
            order.push_back(cur);
            int following = -1, bestRank = 9;
            const std::vector<IrInst>& insts = f.blocks[static_cast<size_t>(cur)].insts;
            for (size_t i = 0; i < insts.size(); ++i) {
                const IrInst& in = insts[i];
                if (in.op != IrInst::Br && in.op != IrInst::CondBr) continue;
                for (size_t a = 0; a < in.args.size(); ++a) {
                    const int t = in.args[a].id;
                    if (in.args[a].kind != IrOperand::Block || t < 0 || static_cast<size_t>(t) >= n || m_owner[static_cast<size_t>(t)] != owner || placed[static_cast<size_t>(t)]) continue;
                    const int rank = (f.blocks[static_cast<size_t>(t)].preds.size() == 1 ? 0 : 4) + (t == cur + 1 ? 0 : 2) + (following < 0 ? 0 : 1);
                    if (rank < bestRank) { bestRank = rank; following = t; }
                }
            }
            if (following < 0) for (size_t b = 0; b < n && following < 0; ++b) if (m_owner[b] == owner && !placed[b]) following = static_cast<int>(b);
            cur = following;
        }
        if (owner < 0) ownCount = order.size();
    }
    emitted = placed;
    next.assign(n, -1);
    for (size_t k = 0; k + 1 < order.size(); ++k)
        if (m_owner[static_cast<size_t>(order[k])] == m_owner[static_cast<size_t>(order[k + 1])]) next[static_cast<size_t>(order[k])] = order[k + 1];
}

// a funclet is entered with none of the function's registers, and what it leaves in one is gone when the runtime has
// cut the stack back: a value defined or used in the code of more than one owner must live in the frame. A value the
// function holds across a try and its handlers, which no funclet touches, may keep a callee-saved register: the
// runtime restores those to their values at the call that threw
void EhClFunclets::crossing(const IrFunction& f, std::set<int>& temps) const
{
    std::map<int, int> ownerOf;   // temp -> its owner, -9: more than one
    for (size_t b = 0; b < f.blocks.size(); ++b) {
        if (m_owner[b] == -2) continue;
        for (size_t i = 0; i < f.blocks[b].insts.size(); ++i) {
            const IrInst& in = f.blocks[b].insts[i];
            std::vector<int> used;
            if (in.dst >= 0) used.push_back(in.dst);
            for (size_t a = 0; a < in.args.size(); ++a) if (in.args[a].kind == IrOperand::Temp) used.push_back(in.args[a].id);
            for (size_t t = 0; t < used.size(); ++t) {
                std::map<int, int>::iterator o = ownerOf.find(used[t]);
                if (o == ownerOf.end()) ownerOf[used[t]] = m_owner[b];
                else if (o->second != m_owner[b]) o->second = -9;
            }
        }
    }
    for (std::map<int, int>::const_iterator o = ownerOf.begin(); o != ownerOf.end(); ++o) if (o->second == -9) temps.insert(o->first);
}

void EhClFunclets::setFrame(size_t k, const std::vector<std::string>& pushed, const std::vector<std::string>& xmms, int outgoingArgs)
{
    Funclet& fc = m_funclets[k];
    fc.pushed = pushed;
    fc.xmms = xmms;
    fc.xmmAt = (32 + 8 * outgoingArgs + 15) / 16 * 16;
    fc.alloc = fc.xmmAt + 16 * static_cast<int>(xmms.size());
    if ((8 * static_cast<int>(pushed.size()) + fc.alloc) % 16) fc.alloc += 8;   // rsp is 16-aligned after "push rbp"
}

// rdx's home slot, 16 above the funclet's rsp on entry: above its allocation, the pushes and rbp
int EhClFunclets::dispFrame(size_t k) const
{
    const Funclet& fc = m_funclets[k];
    return fc.alloc + 8 * (static_cast<int>(fc.pushed.size()) + 1) + 16;
}

void EhClFunclets::state(int s, bool force, EhEmitter& out)
{
    if (s == m_stateNow && !force) return;
    m_stateNow = s;
    const std::string label = m_label + "_S" + intToString(static_cast<int>(m_ips.size()));
    out.ehText(label + ":\n");
    m_ips.push_back(std::make_pair(label, s));
}

void EhClFunclets::setState(const std::vector<long>& stack, int funclet, EhEmitter& out)
{
    std::map<Key, int>::const_iterator s = m_stackState.find(Key(funclet, stack));
    state(s != m_stackState.end() ? s->second : -1, false, out);
}

// a funclet's first lines: the runtime calls it with the function's establisher frame in rdx. A handler keeps rdx in
// its home slot, where the runtime finds the function's frame again when an exception passes through the handler (the
// HandlerType's dispFrame); rbp becomes the function's own, so the code addresses the frame as the function does.
// afterCall: the code before ends with a call (one that does not return): int 3 keeps its return address in that
// function table range
void EhClFunclets::entry(size_t k, int frameSize, bool afterCall, EhEmitter& out)
{
    const Funclet& fc = m_funclets[k];
    if (afterCall) out.ehLine("int 3");
    out.ehText(";@funclet " + std::string(fc.jump ? "0" : fc.isCatch ? "2" : "1") + "\n" + fc.label + ":        ; " + (fc.isCatch ? "handler" : "cleanup") + " of region " + intToString(static_cast<int>(fc.region)) + "\n");
    state(fc.isCatch ? fc.state : -1, true, out);
    if (fc.jump) return;
    if (fc.isCatch) out.ehLine("mov qword ptr [rsp+16], rdx");
    out.ehLine("push rbp");
    out.ehText(";@pushreg rbp\n");
    for (size_t r = 0; r < fc.pushed.size(); ++r) { out.ehLine("push " + fc.pushed[r]); out.ehText(";@pushreg " + fc.pushed[r] + "\n"); }
    out.ehLine("sub rsp, " + intToString(fc.alloc));
    out.ehText(";@allocstack " + intToString(fc.alloc) + "\n");
    for (size_t r = 0; r < fc.xmms.size(); ++r) {
        const std::string at = intToString(fc.xmmAt + 16 * static_cast<int>(r));
        out.ehLine("movdqu xmmword ptr [rsp+" + at + "], " + fc.xmms[r]);
        out.ehText(";@savexmm128 " + fc.xmms[r] + ", " + at + "\n");
    }
    out.ehText(";@endprolog\n");
    out.ehLine("lea rbp, [rdx+" + intToString(frameSize) + "]");
}

// a funclet returns to the runtime: a handler with the address the function (or the handler around it) goes on at
void EhClFunclets::exit(size_t k, const std::string& continueAt, EhEmitter& out) const
{
    const Funclet& fc = m_funclets[k];
    if (!continueAt.empty()) out.ehLine("lea rax, " + continueAt);
    for (size_t r = 0; r < fc.xmms.size(); ++r) out.ehLine("movdqu " + fc.xmms[r] + ", xmmword ptr [rsp+" + intToString(fc.xmmAt + 16 * static_cast<int>(r)) + "]");
    out.ehLine("add rsp, " + intToString(fc.alloc));
    for (size_t r = fc.pushed.size(); r-- > 0;) out.ehLine("pop " + fc.pushed[r]);
    out.ehLine("pop rbp");
    out.ehLine("ret");
}

// the FuncInfo's lines for the object writer (comments to ml64): Coff.cpp hands them to EhClFuncInfo
void EhClFunclets::writeTables(EhEmitter& out, int unwindHelp, const std::vector<Handler>& handlers) const
{
    out.ehText(";@ehcl " + intToString(unwindHelp) + "\n");
    for (size_t s = 0; s < m_states.size(); ++s) out.ehText(";@ehstate " + intToString(m_states[s].first) + ", " + m_states[s].second + "\n");
    for (size_t t = 0; t < m_tries.size(); ++t) {
        const Try& te = m_tries[t];
        out.ehText(";@ehtry " + intToString(te.low) + ", " + intToString(te.high) + ", " + intToString(te.catchHigh) + "\n");
        for (size_t h = 0; h < te.handlers.size(); ++h) {
            const size_t k = static_cast<size_t>(te.handlers[h]);
            const Handler& hd = handlers[k];
            out.ehText(";@ehcatch " + intToString(static_cast<int>(hd.adjectives)) + ", " + (hd.type.empty() ? std::string("0") : hd.type) + ", " + intToString(hd.object) + ", " + m_funclets[k].label + ", " + intToString(dispFrame(k)) + "\n");
        }
    }
    for (size_t k = 0; k < m_ips.size(); ++k) out.ehText(";@ehip " + m_ips[k].first + ", " + intToString(m_ips[k].second) + "\n");
}

} // namespace cppparse
