// types.h - sample-dbg: what types.cpp and types2.cpp share (test 20, CodeView types under -g).
// The classes of namespace shared are defined by both units, so that one program has each of them from two
// objects (two of cppparse's, or one of cppparse's and one of cl's); the fact functions print what a unit knows of a
// type by itself, in the lines sample-dbg-driver\dbgtypes.cpp prints for the same type from the PDB.
#ifndef SAMPLE_DBG_TYPES_H
#define SAMPLE_DBG_TYPES_H

namespace shared {
struct Pair { int first; double second; };
class Counter {
public:
    Counter() : m_n(0) {}
    virtual ~Counter() {}
    virtual int next() { return ++m_n; }
    int m_n;
};
template <class T, int N> struct Arr { T a[N]; int count; };
enum Mode { Off, On = 4, Auto = -2 };
}

// "size <type> <bytes>", "member <type> <member> <offset>", "bits <type> <member> <first bit in the object> <width>",
// "base <type> <base> <offset>", "enumerator <enumeration> <name> <value>", "global <variable> <bytes>"
void factSize(const char* type, unsigned long long bytes);
void factMember(const char* type, const char* member, long long offset);
void factBits(const char* type, const char* member, const void* object, unsigned long long bytes);
void factBase(const char* type, const char* base, long long offset);
void factEnumerator(const char* type, const char* name, long long value);
void factGlobal(const char* name, unsigned long long bytes);
// "local <function> <variable> <bytes>": a static local, whose type may be a class local to the function
void factLocal(const char* function, const char* variable, unsigned long long bytes);

#define SIZE(name, T) factSize(name, sizeof(T))
#define MEMBER(name, object, m) factMember(name, #m, (const char*)&(object).m - (const char*)&(object))
#define BASE(name, object, B, baseName) factBase(name, baseName, (const char*)static_cast<B*>(&(object)) - (const char*)&(object))
#define GLOBAL(v) factGlobal(#v, sizeof(v))

// what types2.cpp reports
void reportSecondUnit();

#endif
