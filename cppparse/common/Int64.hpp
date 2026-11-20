// Int64.hpp - the 64-bit integer types of cppparse's own sources. long long is the one extension of C++98 the
// sources use (C99 and C++11 have it; every compiler that builds cppparse does): the target's 64-bit constants,
// sizes and offsets need it, as does Win64's size_t, where long has 32 bits. The sources name it through these
// two typedefs alone, so that clang -std=c++98 -pedantic reports it here only (sandbox/scripts/strict98.ps1);
// the library headers in include/ have their own pair, __cppparse_int64 and __cppparse_uint64 in <__config>,
// since cl builds the sources against its own headers. A 64-bit constant is written without the LL suffix, from
// these types (~static_cast<uint64>(0), static_cast<uint64>(1) << 63) or from the limits below.
#ifndef CPPPARSE_INT64_HPP
#define CPPPARSE_INT64_HPP

typedef long long          int64;
typedef unsigned long long uint64;

const uint64 uint64Max = ~static_cast<uint64>(0);
const int64  int64Max  = static_cast<int64>(uint64Max >> 1);
const int64  int64Min  = -int64Max - 1;

#endif
