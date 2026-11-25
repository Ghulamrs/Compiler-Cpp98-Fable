// CvScopes.cpp - see CvScopes.hpp.
#include "CvScopes.hpp"

#include <algorithm>
#include <cstdlib>

namespace cppparse {

namespace {

const int kRbp = 334;   // CV_AMD64_RBP

// runs of offsets in increasing order, adjacent and overlapping ones joined
void mergePieces(std::vector<std::pair<unsigned long, unsigned long> >& p)
{
    std::sort(p.begin(), p.end());
    std::vector<std::pair<unsigned long, unsigned long> > out;
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i].second <= p[i].first) continue;
        if (!out.empty() && p[i].first <= out.back().second) { if (p[i].second > out.back().second) out.back().second = p[i].second; }
        else out.push_back(p[i]);
    }
    p.swap(out);
}

} // namespace

CvScopes::CvScopes(const CvProcedureMarks& marks, unsigned long codeSize, bool hasFrame)
    : m_marks(marks), m_codeSize(codeSize), m_dbgStart(0), m_dbgEnd(codeSize), m_hasFrame(hasFrame), m_ranged(false), m_inPieces(0)
{
    const size_t n = marks.blockParents.size();
    m_parent.assign(n + 1, 0);
    for (size_t k = 1; k <= n; ++k) {
        const int p = marks.blockParents[k - 1];
        m_parent[k] = p >= 0 && static_cast<size_t>(p) < k ? p : 0;   // a parent is met before its children
    }
    m_pieces.assign(n + 1, Pieces());
    m_pieces[0].push_back(std::make_pair(0UL, codeSize));
    // DbgStart behind the entry sequence, DbgEnd at the function's own epilogue (the first: a funclet's come after it)
    bool started = false, ended = false;
    for (size_t i = 0; i < marks.positions.size(); ++i) {
        const unsigned long at = marks.positions[i].first;
        if (marks.positions[i].second == ";@prologend" && !started) { m_dbgStart = at; started = true; }
        if (marks.positions[i].second == ";@epilog" && !ended) { m_dbgEnd = at; ended = true; }
    }
    if (m_dbgEnd < m_dbgStart) m_dbgEnd = m_dbgStart;
    // the runs of code of each scope, from the ;@scope markers (the code before the first one is the entry sequence)
    int current = -1;
    unsigned long runStart = 0;
    for (size_t i = 0; i <= marks.positions.size(); ++i) {
        const bool last = i == marks.positions.size();
        if (!last && marks.positions[i].second.compare(0, 8, ";@scope ") != 0) continue;
        const unsigned long at = last ? codeSize : (marks.positions[i].first < codeSize ? marks.positions[i].first : codeSize);
        if (at > runStart) {
            if (current >= 1 && static_cast<size_t>(current) <= n)
                for (int b = current; b >= 1; b = m_parent[static_cast<size_t>(b)]) m_pieces[static_cast<size_t>(b)].push_back(std::make_pair(runStart, at));
            else if (current < 0 && runStart >= m_dbgStart) m_outside.push_back(std::make_pair(runStart, at));
            runStart = at;
        }
        if (!last) current = atoi(marks.positions[i].second.c_str() + 8);
    }
    for (size_t k = 1; k <= n; ++k) { mergePieces(m_pieces[k]); if (m_pieces[k].size() > 1) ++m_inPieces; }
    // the form: plain when every variable has its slot and lies in a block of one piece
    for (size_t v = 0; v < marks.variables.size(); ++v) {
        const CvFrameVariable& var = marks.variables[v];
        const size_t b = var.block >= 0 && static_cast<size_t>(var.block) <= n ? static_cast<size_t>(var.block) : 0;
        if (b != 0 && m_pieces[b].empty()) continue;   // a block without code: not described
        if (!var.placed || !m_hasFrame || m_pieces[b].size() != 1) m_ranged = true;
    }
    if (m_ranged)
        for (size_t v = 0; v < marks.variables.size(); ++v) {
            const CvFrameVariable& var = marks.variables[v];
            const size_t b = var.block >= 0 && static_cast<size_t>(var.block) <= n ? static_cast<size_t>(var.block) : 0;
            if (var.placed && m_hasFrame && b != 0) m_offsetRanges.insert(m_offsetRanges.end(), m_pieces[b].begin(), m_pieces[b].end());
        }
}

int CvScopes::recordOf(int block) const
{
    // the ranged form has no S_BLOCK32: dbghelp does not look into a block of a procedure that has an S_LOCAL (stage 2's
    // measurement, docs/design-codeview.md section 11), so there every variable stands in the procedure with its ranges
    if (m_ranged) return 0;
    int b = block >= 0 && static_cast<size_t>(block) < m_pieces.size() ? block : 0;
    while (b > 0 && m_pieces[static_cast<size_t>(b)].size() != 1) b = m_parent[static_cast<size_t>(b)];
    return b;
}

void CvScopes::write(CvSymbols& out, CvTypes& types, const std::string& fn, const std::set<std::string>& defined) const
{
    if (m_hasFrame) out.addFrameProc(m_marks.frameBytes, m_marks.savedBytes, m_marks.frameFlags);
    writeScope(out, types, fn, defined, 0);
}

void CvScopes::writeScope(CvSymbols& out, CvTypes& types, const std::string& fn, const std::set<std::string>& defined, int scope) const
{
    // its variables (the parameters first, as the markers list them), then those of its blocks in pieces
    for (size_t v = 0; v < m_marks.variables.size(); ++v) {
        const CvFrameVariable& var = m_marks.variables[v];
        const int b = var.block >= 0 && static_cast<size_t>(var.block) < m_pieces.size() ? var.block : 0;
        if (b != 0 && m_pieces[static_cast<size_t>(b)].empty()) continue;
        if (recordOf(b) != scope) continue;
        const unsigned long type = types.symbolIndex(var.type);
        if (!m_ranged) { out.addRegRel(var.offset, type, kRbp, var.name); continue; }
        const int param = var.param ? 1 : 0;
        if (!var.placed || !m_hasFrame) { out.addLocal(type, 0x100 | param, var.name); continue; }
        out.addLocal(type, param, var.name);
        if (b == 0) { out.addFramePointerFullScope(var.offset); continue; }
        // a block's variable: over the block's pieces, the hull of each group of pieces that fits a range (65,535
        // bytes), the holes as gaps
        const Pieces& p = m_pieces[static_cast<size_t>(b)];
        size_t i = 0;
        unsigned long from = p.empty() ? 0 : p[0].first;
        while (i < p.size()) {
            const unsigned long start = from;
            std::vector<std::pair<unsigned long, unsigned long> > gaps;
            unsigned long end = start;
            while (i < p.size()) {
                const unsigned long s = p[i].first > start ? p[i].first : start;
                if (s - start >= 0xFFFFUL) break;
                if (s > end && end > start) gaps.push_back(std::make_pair(end - start, s - end));
                const unsigned long e = p[i].second - start > 0xFFFFUL ? start + 0xFFFFUL : p[i].second;
                end = e;
                if (e < p[i].second) { from = e; break; }   // a piece longer than a range: the rest starts the next
                ++i;
                if (i < p.size()) from = p[i].first;
            }
            out.addRegisterRel(kRbp, var.offset, fn, start, end - start, gaps);
        }
    }
    for (size_t s = 0; s < m_marks.statics.size(); ++s)
        if (recordOf(m_marks.statics[s].block) == scope && defined.count(m_marks.statics[s].symbol))
            out.addData(m_marks.statics[s].symbol, m_marks.statics[s].name, types.symbolIndex(m_marks.statics[s].type), true);
    for (size_t k = 1; k < m_pieces.size(); ++k) {
        if (m_ranged || m_pieces[k].size() != 1 || recordOf(m_parent[k]) != scope) continue;
        out.beginBlock(fn, m_pieces[k][0].first, m_pieces[k][0].second - m_pieces[k][0].first);
        writeScope(out, types, fn, defined, static_cast<int>(k));
        out.endBlock();
    }
}

} // namespace cppparse
