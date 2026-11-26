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

// the pieces as records' ranges: each a start, a length of at most 65,535 bytes (the field is 16 bits) and the holes
// between the pieces it spans as gaps {from the start, length}; a piece longer than a range goes on in the next
struct Span { unsigned long start, length; std::vector<std::pair<unsigned long, unsigned long> > gaps; };

void spansOf(const std::vector<std::pair<unsigned long, unsigned long> >& p, std::vector<Span>& out)
{
    size_t i = 0;
    unsigned long from = p.empty() ? 0 : p[0].first;
    while (i < p.size()) {
        Span s;
        s.start = from;
        unsigned long end = s.start;
        while (i < p.size()) {
            const unsigned long b = p[i].first > s.start ? p[i].first : s.start;
            if (b - s.start >= 0xFFFFUL) break;
            if (b > end && end > s.start) s.gaps.push_back(std::make_pair(end - s.start, b - end));
            const unsigned long e = p[i].second - s.start > 0xFFFFUL ? s.start + 0xFFFFUL : p[i].second;
            end = e;
            if (e < p[i].second) { from = e; break; }   // a piece longer than a range: the rest starts the next
            ++i;
            if (i < p.size()) from = p[i].first;
        }
        s.length = end - s.start;
        out.push_back(s);
    }
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
        if (var.ranged >= 0) m_ranged = true;          // a place over ranges of the code
        if (b != 0 && m_pieces[b].empty()) continue;   // a block without code: not described
        if (!var.placed || !m_hasFrame || m_pieces[b].size() != 1) m_ranged = true;
    }
    if (m_ranged)
        for (size_t v = 0; v < marks.variables.size(); ++v) {
            const CvFrameVariable& var = marks.variables[v];
            const size_t b = var.block >= 0 && static_cast<size_t>(var.block) <= n ? static_cast<size_t>(var.block) : 0;
            if (var.placed && m_hasFrame && b != 0) m_offsetRanges.insert(m_offsetRanges.end(), m_pieces[b].begin(), m_pieces[b].end());
        }
    placeRanges();
}

// the code each ;@var variable is in its place over: from a ;@loc marker that names a place to the variable's next
// marker (or the end of the code), cut to the pieces of the variable's block (a variable is listed in its block only);
// neighbours in one place joined
void CvScopes::placeRanges()
{
    m_ranges.assign(m_marks.variables.size(), std::vector<CvPlaceRange>());
    std::vector<int> varOf;                       // ;@var's number -> the variable's index
    for (size_t v = 0; v < m_marks.variables.size(); ++v) {
        const int id = m_marks.variables[v].ranged;
        if (id < 0) continue;
        if (static_cast<size_t>(id) >= varOf.size()) varOf.resize(static_cast<size_t>(id) + 1, -1);
        varOf[static_cast<size_t>(id)] = static_cast<int>(v);
    }
    if (varOf.empty()) return;
    std::vector<CvPlaceRange> open(varOf.size());   // end 1: a place is open from 'start'
    for (size_t i = 0; i <= m_marks.positions.size(); ++i) {
        const bool last = i == m_marks.positions.size();
        int id = -1;
        std::string place;
        if (!last) {
            const std::string& text = m_marks.positions[i].second;
            if (text.compare(0, 6, ";@loc ") != 0) continue;
            const size_t sp = text.find(' ', 6);
            id = atoi(text.c_str() + 6);
            place = sp == std::string::npos ? std::string("-") : text.substr(sp + 1);
            if (id < 0 || static_cast<size_t>(id) >= varOf.size() || varOf[static_cast<size_t>(id)] < 0) continue;
        }
        const unsigned long at = last || m_marks.positions[i].first > m_codeSize ? m_codeSize : m_marks.positions[i].first;
        for (size_t k = 0; k < open.size(); ++k) {
            if (!last && static_cast<int>(k) != id) continue;
            CvPlaceRange& o = open[k];
            if (o.end == 1 && at > o.start && varOf[k] >= 0) {
                CvPlaceRange r = o;
                r.end = at;
                m_ranges[static_cast<size_t>(varOf[k])].push_back(r);
            }
            o.end = 0;
        }
        if (last || place == "-") continue;
        CvPlaceRange& o = open[static_cast<size_t>(id)];
        if (place.compare(0, 4, "[rbp") == 0 && place.size() > 5) { o.reg = 0; o.offset = (place[4] == '-' ? -1 : 1) * atol(place.c_str() + 5); }
        else {
            const size_t sp = place.find(' ');
            o.reg = sp == std::string::npos ? 0 : CvRegisters::number(CvRegisters::family(place.substr(0, sp)), atoi(place.c_str() + sp + 1));
            o.offset = 0;
            if (o.reg == 0) continue;             // not a register of the table: no place
        }
        o.start = at;
        o.end = 1;
    }
    for (size_t v = 0; v < m_ranges.size(); ++v) {
        std::vector<CvPlaceRange>& rs = m_ranges[v];
        if (rs.empty()) continue;
        const CvFrameVariable& var = m_marks.variables[v];
        const size_t b = var.block >= 0 && static_cast<size_t>(var.block) < m_pieces.size() ? static_cast<size_t>(var.block) : 0;
        std::vector<CvPlaceRange> cut;
        for (size_t r = 0; r < rs.size(); ++r) {
            if (rs[r].reg == 0 && !m_hasFrame) continue;
            if (rs[r].start < m_dbgStart) rs[r].start = m_dbgStart;   // (the entry sequence is no part of any range)
            if (rs[r].start >= rs[r].end) continue;
            if (b == 0) { cut.push_back(rs[r]); continue; }
            for (size_t p = 0; p < m_pieces[b].size(); ++p) {
                CvPlaceRange c = rs[r];
                if (m_pieces[b][p].first > c.start) c.start = m_pieces[b][p].first;
                if (m_pieces[b][p].second < c.end) c.end = m_pieces[b][p].second;
                if (c.start < c.end) cut.push_back(c);
            }
        }
        rs.clear();
        for (size_t r = 0; r < cut.size(); ++r) {
            if (!rs.empty() && rs.back().end == cut[r].start && rs.back().reg == cut[r].reg && rs.back().offset == cut[r].offset) rs.back().end = cut[r].end;
            else rs.push_back(cut[r]);
        }
        for (size_t r = 0; r < rs.size(); ++r) {
            if (rs[r].reg == 0) m_offsetRanges.push_back(std::make_pair(rs[r].start, rs[r].end));
            else { CvRegisterRange g; g.start = rs[r].start; g.end = rs[r].end; g.reg = rs[r].reg; m_registerRanges.push_back(g); }
        }
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
        if (var.ranged >= 0) {
            // in its temporary's place over the ranges its markers gave: the runs of ranges in one place, each as
            // records with the holes as gaps
            const std::vector<CvPlaceRange>& rs = m_ranges[v];
            out.addLocal(type, (rs.empty() ? 0x100 : 0) | param, var.name);
            for (size_t r = 0; r < rs.size();) {
                Pieces run;
                size_t e = r;
                while (e < rs.size() && rs[e].reg == rs[r].reg && rs[e].offset == rs[r].offset) { run.push_back(std::make_pair(rs[e].start, rs[e].end)); ++e; }
                std::vector<Span> spans;
                spansOf(run, spans);
                for (size_t s = 0; s < spans.size(); ++s) {
                    if (rs[r].reg == 0) out.addRegisterRel(kRbp, rs[r].offset, fn, spans[s].start, spans[s].length, spans[s].gaps);
                    else out.addRegister(rs[r].reg, fn, spans[s].start, spans[s].length, spans[s].gaps);
                }
                r = e;
            }
            continue;
        }
        if (!var.placed || !m_hasFrame) { out.addLocal(type, 0x100 | param, var.name); continue; }
        out.addLocal(type, param, var.name);
        if (b == 0) { out.addFramePointerFullScope(var.offset); continue; }
        // a block's variable: over the block's pieces, the hull of each group of pieces that fits a range (65,535
        // bytes), the holes as gaps
        std::vector<Span> spans;
        spansOf(m_pieces[static_cast<size_t>(b)], spans);
        for (size_t s = 0; s < spans.size(); ++s) out.addRegisterRel(kRbp, var.offset, fn, spans[s].start, spans[s].length, spans[s].gaps);
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
