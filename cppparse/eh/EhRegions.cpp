// EhRegions.cpp - see EhRegions.hpp.
#include "EhRegions.hpp"

#include <algorithm>

namespace cppparse {

bool EhRegions::compute(const IrFunction& f)
{
    const size_t n = f.blocks.size();
    m_in.assign(n, std::vector<long>());
    std::vector<bool> seen(n, false);
    std::vector<int> work;
    bool ok = true;
    seen[0] = true;
    work.push_back(0);
    while (!work.empty() && ok) {
        const int b = work.back();
        work.pop_back();
        std::vector<long> cur = m_in[static_cast<size_t>(b)];
        const IrBlock& blk = f.blocks[static_cast<size_t>(b)];
        for (size_t i = 0; i < blk.insts.size() && ok; ++i) {
            const IrInst& in = blk.insts[i];
            std::vector<int> targets;
            if (in.op == IrInst::TryBegin) {
                m_outer[in.extra] = cur;
                for (size_t a = 0; a < in.args.size(); ++a) if (in.args[a].kind == IrOperand::Block) targets.push_back(in.args[a].id);
            } else if (in.op == IrInst::Br || in.op == IrInst::CondBr) {
                for (size_t a = 0; a < in.args.size(); ++a) if (in.args[a].kind == IrOperand::Block) targets.push_back(in.args[a].id);
            }
            // the handlers of a region run in the regions around it; a branch keeps the current ones
            for (size_t t = 0; t < targets.size(); ++t) {
                const size_t h = static_cast<size_t>(targets[t]);
                if (h >= n) { ok = false; break; }
                if (!seen[h]) { seen[h] = true; m_in[h] = cur; work.push_back(targets[t]); }
                else if (m_in[h] != cur) ok = false;
            }
            if (in.op == IrInst::TryBegin) cur.push_back(in.extra);
            else if (in.op == IrInst::TryEnd) {
                if (cur.empty() || cur.back() != in.extra) ok = false;
                else cur.pop_back();
            }
        }
    }
    return ok;
}

void EhRegions::computeEdges(const IrFunction& f)
{
    const size_t n = f.blocks.size();
    m_edges.assign(n, std::vector<int>());
    std::map<long, std::vector<int> > handlersOf;   // region -> its handler entries (the TryBegin's block operands)
    for (size_t b = 0; b < n; ++b)
        for (size_t i = 0; i < f.blocks[b].insts.size(); ++i) {
            const IrInst& in = f.blocks[b].insts[i];
            if (in.op != IrInst::TryBegin) continue;
            for (size_t a = 0; a < in.args.size(); ++a) if (in.args[a].kind == IrOperand::Block) handlersOf[in.extra].push_back(in.args[a].id);
        }
    for (size_t b = 0; b < n; ++b) {
        std::vector<long> cur = m_in[b];
        std::vector<int>& out = m_edges[b];
        for (size_t i = 0; i < f.blocks[b].insts.size(); ++i) {
            const IrInst& in = f.blocks[b].insts[i];
            if (in.op == IrInst::TryBegin) { cur.push_back(in.extra); continue; }
            if (in.op == IrInst::TryEnd) { if (!cur.empty() && cur.back() == in.extra) cur.pop_back(); continue; }
            if (!in.mayThrow()) continue;   // a call to a function with C linkage reaches no handler (cl /EHsc)
            // a clause that does not match rethrows from the landing pad to the regions around: all of them count
            for (size_t r = 0; r < cur.size(); ++r) {
                const std::vector<int>& hs = handlersOf[cur[r]];
                for (size_t h = 0; h < hs.size(); ++h)
                    if (hs[h] >= 0 && static_cast<size_t>(hs[h]) < n && std::find(out.begin(), out.end(), hs[h]) == out.end()) out.push_back(hs[h]);
            }
        }
    }
}

} // namespace cppparse
