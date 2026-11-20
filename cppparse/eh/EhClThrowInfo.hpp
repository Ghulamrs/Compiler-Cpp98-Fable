// EhClThrowInfo.hpp - cl's exception model (--eh=cl): what a throw hands to _CxxThrowException.
//
//   ThrowInfo           { attributes (1: the object, or a thrown pointer's pointee, is const; 2: volatile),
//                         RVA of the destructor or 0, 0, RVA of the CatchableTypeArray }
//   CatchableTypeArray  { count, RVA of a CatchableType each }: the thrown type itself, then what else a handler may
//                         take it as (the accessible unambiguous bases; for a pointer the pointers to them and void*)
//   CatchableType       { properties (1: a scalar, copied bit by bit; 4: the class has virtual bases, its copy
//                         constructor takes the most-derived flag), RVA of the type descriptor, where the type lies in
//                         the thrown object { mdisp, pdisp (-1: not through a vbtable), vdisp }, the size, RVA of the
//                         copy constructor or 0 }
// cl's runtime reads them (vcruntime's frame.cpp; the layouts are ehdata.h's). The descriptors of pointers are those of
// the pointer to the unqualified pointee: the qualifiers travel in the ThrowInfo's attributes and in a handler's
// adjectives, as cl's do.
//
// The tables are COMDAT data that the units of a program share, so a name must stand for one content: every field is
// in the name, spelled out or as a hash (FNV-1a, 32 bits) of the rest. The descriptor of a thrown `const char*` is
// that of `char*` (the attributes tell them apart), and one base lies at different offsets in different classes.
//     $cppCT$<type>$<mdisp>$<pdisp>$<vdisp>$<hash of properties, size, copy constructor>
//     $cppCTA$<thrown type>$<hash of the CatchableType names>
//     $cppTI$<thrown type>$<hash of attributes, destructor, array name>
// IrGen describes the thrown type from the semantic types (EhThrown); this class lays the tables out.
//
// Strict C++98.
#ifndef CPPPARSE_EHCLTHROWINFO_HPP
#define CPPPARSE_EHCLTHROWINFO_HPP

#include "X86.hpp"

#include <string>
#include <vector>

namespace cppparse {

// one type a handler may catch the thrown object as
struct EhCatchable {
    std::string typeName;      // the type's decorated name as in its descriptor, without the leading '.' ("?AUErr@@", "PEAD")
    std::string descriptor;    // the IR name of its type descriptor global
    long        mdisp, pdisp, vdisp;
    long        size;
    bool        scalar;        // copied bit by bit: not a class
    bool        virtualBases;  // a class with virtual bases
    std::string copyFunction;  // the IR name of the copy constructor, "" when the object is copied bit by bit
    EhCatchable() : mdisp(0), pdisp(-1), vdisp(0), size(0), scalar(false), virtualBases(false) {}
};

struct EhThrown {
    std::string              typeName;     // the thrown type's decorated name (for a pointer: to the unqualified pointee)
    bool                     isConst, isVolatile;   // the object's, or the thrown pointer's pointee's
    std::string              destructor;   // the IR name of the destructor the runtime calls, "" for none
    std::vector<EhCatchable> catchables;   // the type itself first
    EhThrown() : isConst(false), isVolatile(false) {}
};

// one table as a global: its IR name, its decorated name, its contents
struct EhThrowTable {
    std::string             name, decorated;
    std::vector<IrDataItem> items;
};

class EhClThrowInfo {
public:
    // the IR name of the ThrowInfo of 't' (a function of its contents alone)
    static std::string name(const EhThrown& t);
    // the tables of 't', each to be emitted once: its CatchableTypes, the array, the ThrowInfo last
    static void build(const EhThrown& t, std::vector<EhThrowTable>& out);
    static unsigned long hash(const std::string& text);   // FNV-1a, 32 bits
    static std::string   hex8(unsigned long v);

private:
    static std::string catchableName(const EhCatchable& c, bool decorated);
    static std::string arrayName(const EhThrown& t, bool decorated);
    static std::string infoName(const EhThrown& t, bool decorated);
};

} // namespace cppparse

#endif
