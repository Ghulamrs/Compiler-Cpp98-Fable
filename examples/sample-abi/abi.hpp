// abi.hpp - classes of every size class the Windows x64 convention distinguishes, shared by abi.cpp (compiled
// by cppparse) and driver-abi.cpp (compiled by cl). Classes of 1, 2, 4 or 8 bytes travel in an integer
// register (floats too); a C++03 aggregate of those sizes is returned in rax by a non-member function; others
// go by address of a copy and come back through a hidden result pointer. C4 has constructors (register
// argument, hidden result pointer), K4 a copy constructor (argument by address too).
#ifndef SAMPLE_ABI_HPP
#define SAMPLE_ABI_HPP

struct S1 { char a; };
struct S2 { short a; };
struct S4 { int a; };
struct S8 { int a, b; };
struct F2 { float x, y; };
struct D1 { double d; };
struct S3 { char a, b, c; };
struct S12 { int a, b, c; };
struct S16 { long long a, b; };
struct C4 { int a; C4() : a(0) {} C4(int v) : a(v) {} };
struct K4 { int a; K4() : a(0) {} K4(const K4& o) : a(o.a) {} };

inline void fill(S1& r, int s) { r.a = (char)s; }
inline void fill(S2& r, int s) { r.a = (short)(s * 300); }
inline void fill(S4& r, int s) { r.a = s * 100000; }
inline void fill(S8& r, int s) { r.a = s; r.b = -s * 7; }
inline void fill(F2& r, int s) { r.x = (float)s + 0.5f; r.y = (float)(s * 2) + 0.25f; }
inline void fill(D1& r, int s) { r.d = (double)s + 0.125; }
inline void fill(S3& r, int s) { r.a = (char)s; r.b = (char)(s + 1); r.c = (char)(s + 2); }
inline void fill(S12& r, int s) { r.a = s; r.b = s * 3; r.c = s * 5; }
inline void fill(S16& r, int s) { r.a = (long long)s << 33; r.b = s * 11; }
inline void fill(C4& r, int s) { r.a = s * 13; }
inline void fill(K4& r, int s) { r.a = s * 17; }

inline int val(const S1& r) { return r.a; }
inline int val(const S2& r) { return r.a; }
inline int val(const S4& r) { return r.a; }
inline int val(const S8& r) { return r.a * 1000 + r.b; }
inline int val(const F2& r) { return (int)(r.x * 8) + (int)(r.y * 8) * 1000; }
inline int val(const D1& r) { return (int)(r.d * 8); }
inline int val(const S3& r) { return r.a + r.b * 7 + r.c * 49; }
inline int val(const S12& r) { return r.a + r.b * 7 + r.c * 49; }
inline int val(const S16& r) { return (int)(r.a >> 33) * 1000 + (int)r.b; }
inline int val(const C4& r) { return r.a; }
inline int val(const K4& r) { return r.a; }

// X(T) for every class above
#define ABI_TYPES(X) X(S1) X(S2) X(S4) X(S8) X(F2) X(D1) X(S3) X(S12) X(S16) X(C4) X(K4)

// c_*: defined by cppparse (abi.cpp), k_*: defined by cl (driver-abi.cpp)
#define ABI_DECLARE(T) T c_mk##T(int s); int c_use##T(int k, T a, T b); T k_mk##T(int s); int k_use##T(int k, T a, T b);
ABI_TYPES(ABI_DECLARE)

// eight arguments: the last four in stack slots
int c_many(S1 a, S2 b, S4 c, S8 d, F2 e, D1 f, S3 g, C4 h);
int k_many(S1 a, S2 b, S4 c, S8 d, F2 e, D1 f, S3 g, C4 h);

// member functions return every class through the hidden pointer (after this); static ones like free functions
struct M {
    int base;
    S4 get(int s) const;
    S8 pair(S4 a) const;
    static S8 make(int s);
};
struct N {
    int base;
    S4 get(int s) const;
    S8 pair(S4 a) const;
    static S8 make(int s);
};

int c_callsK();   // cppparse calling cl: the number of failed checks

#endif
