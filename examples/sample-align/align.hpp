// align.hpp - structure packing and declared alignment, shared by the unit cppparse compiles (align.cpp) and the
// driver cl compiles (sample-align-driver): both must agree on every size and offset below.
// Strict C++98 plus the Microsoft extensions #pragma pack and __declspec(align(N)).
#ifndef SAMPLE_ALIGN_HPP_INCLUDED
#define SAMPLE_ALIGN_HPP_INCLUDED

#pragma pack(push, 1)
struct Pack1 { char c; double d; int i; };                   // d at 1, i at 9, size 13
#pragma pack(pop)

#pragma pack(push, 2)
struct Pack2 { char c; int i; double d; };                   // i at 2, d at 6, size 14
#pragma pack(pop)

#pragma pack(4)
struct Pack4 { char c; double d; };                          // d at 4, size 12
#pragma pack()

struct Natural { char c; double d; };                        // d at 8, size 16

__declspec(align(32)) struct Align32 { int a; };            // size 32, alignment 32

struct HoldsAlign32 { char c; Align32 x; };                  // x at 32, size 64

#pragma pack(push, 1)
struct PackedHoldsAlign32 { char c; Align32 x; };            // pack cannot lower a declared alignment: x at 32
#pragma pack(pop)

struct MemberAligned { char c; __declspec(align(16)) int x; };   // x at 16, size 32

// named records: pop to r1 restores what was in force at its push (the default)
#pragma pack(push, r1, 2)
#pragma pack(push, 8)
#pragma pack(pop, r1)
struct AfterNamedPop { char c; double d; };                  // the default again: d at 8, size 16

// the globals defined by align.cpp (compiled by cppparse), read by the driver (compiled by cl)
extern Pack1                         g_pack1;
extern __declspec(align(64)) double  g_align64;
extern Align32                       g_align32;
extern char                          g_char;
extern int                           g_int;

// what cppparse computed: 0 sizeof(Pack1), 1 offset of Pack1::d, 2 offset of Pack1::i, 3 sizeof(Pack2),
// 4 offset of Pack2::d, 5 sizeof(Pack4), 6 offset of Pack4::d, 7 sizeof(Natural), 8 sizeof(Align32),
// 9 offset of HoldsAlign32::x, 10 sizeof(HoldsAlign32), 11 offset of PackedHoldsAlign32::x,
// 12 sizeof(PackedHoldsAlign32), 13 offset of MemberAligned::x, 14 sizeof(MemberAligned),
// 15 offset of AfterNamedPop::d, 16 sizeof(AfterNamedPop); -1 past the end
int layoutValue(int which);

#endif
