// layout.cpp - compiled by cppparse (--compile6): the values of layout.hpp's expressions, and objects the
// cl-compiled driver reads through its own view of the classes.
#include "layout.hpp"

EM g_em;
NE g_ne;

namespace {

const int s_values[] = {
#define LAYOUT_ENTRY(e) (int)(e),
    LAYOUT_VALUES(LAYOUT_ENTRY)
#undef LAYOUT_ENTRY
    0
};

struct Init {
    Init() { g_em.w = 41; g_ne.n = 7; g_ne.w = 9; }
};
Init s_init;

} // namespace

int layoutValue(int k)
{
    if (k < 0 || k >= (int)(sizeof(s_values) / sizeof(s_values[0])) - 1) return -1;
    return s_values[k];
}

int emSum(const EM* p)
{
    return p->w + 1;
}

int evalGet(EVal& v, int set)
{
    v.v = set;
    E* e = &v;
    E other;
    *e = other;                            // copies nothing: E's byte is the low byte of v
    EVal* back = static_cast<EVal*>(e);
    return back->get();
}

