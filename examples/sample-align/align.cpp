// align.cpp - compiled by cppparse (--compile6): the layouts it computes for align.hpp, and the globals the
// cl-compiled driver reads. Offsets are measured on objects, without offsetof.
#include "align.hpp"

Pack1                         g_pack1 = { 'p', 2.5, 7 };
__declspec(align(64)) double  g_align64 = 6.25;
Align32                       g_align32 = { 32 };
char                          g_char = 'c';
int                           g_int = 41;

namespace {

Pack1              s_p1;
Pack2              s_p2;
Pack4              s_p4;
HoldsAlign32       s_h;
PackedHoldsAlign32 s_ph;
MemberAligned      s_m;
AfterNamedPop      s_a;

int distance(const void* from, const void* to)
{
    return static_cast<int>(static_cast<const char*>(to) - static_cast<const char*>(from));
}

} // namespace

int layoutValue(int which)
{
    switch (which) {
    case 0:  return static_cast<int>(sizeof(Pack1));
    case 1:  return distance(&s_p1, &s_p1.d);
    case 2:  return distance(&s_p1, &s_p1.i);
    case 3:  return static_cast<int>(sizeof(Pack2));
    case 4:  return distance(&s_p2, &s_p2.d);
    case 5:  return static_cast<int>(sizeof(Pack4));
    case 6:  return distance(&s_p4, &s_p4.d);
    case 7:  return static_cast<int>(sizeof(Natural));
    case 8:  return static_cast<int>(sizeof(Align32));
    case 9:  return distance(&s_h, &s_h.x);
    case 10: return static_cast<int>(sizeof(HoldsAlign32));
    case 11: return distance(&s_ph, &s_ph.x);
    case 12: return static_cast<int>(sizeof(PackedHoldsAlign32));
    case 13: return distance(&s_m, &s_m.x);
    case 14: return static_cast<int>(sizeof(MemberAligned));
    case 15: return distance(&s_a, &s_a.d);
    case 16: return static_cast<int>(sizeof(AfterNamedPop));
    }
    return -1;
}
