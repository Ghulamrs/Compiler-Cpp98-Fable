// EhRegions.hpp - the try and cleanup regions of an IR function, as the back end needs them.
//
// IrGen brackets a region with trybegin (its id, its handler blocks) and tryend. The table model lists the code of a
// region as address ranges, so every block must be entered with the same regions open whichever path reaches it:
// compute() finds that stack per block, or reports that the function has none. The exception edges say where a call
// inside a region may continue: at the handlers of that region and of every region around it.
//
// Strict C++98.
#ifndef CPPPARSE_EHREGIONS_HPP
#define CPPPARSE_EHREGIONS_HPP

#include "Ir.hpp"

#include <map>
#include <vector>

namespace cppparse {

class EhRegions {
public:
    // the regions open on entry to each block (innermost last) and around each region's trybegin; false when the
    // regions are not nested the same way along every path
    bool compute(const IrFunction& f);
    // after compute(): the exception edges of the regions: a call or throw inside a region continues at the handlers
    // of that region and of every region around it, with each register as it was at the call; what a handler reads
    // (and what the code after it reads, a loop that goes round again) is live across every such call
    void computeEdges(const IrFunction& f);

    const std::vector<long>& in(size_t block) const { return m_in[block]; }
    const std::vector<long>& outer(long region) { return m_outer[region]; }
    const std::vector<std::vector<int> >& edges() const { return m_edges; }   // block -> handler blocks its calls and throws may continue at

private:
    std::vector<std::vector<long> >    m_in;      // regions active on entry to each block, innermost last
    std::map<long, std::vector<long> > m_outer;   // region -> the regions active at its trybegin
    std::vector<std::vector<int> >     m_edges;
};

} // namespace cppparse

#endif
