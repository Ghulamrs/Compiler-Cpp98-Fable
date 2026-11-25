// CvModule.hpp - what a module's CodeView types and symbols are made from (docs/design-codeview.md 5.4).
//
// With -g the front end's types and variables are described here, free of the front end's own classes (CvFromSema,
// in middle/, fills it while IrGen runs), and the object writer receives the description beside the listing: the
// listing's markers name a type by its number here ("T12"), CvTypes turns the descriptions into the records of
// .debug$T, CvSymbols the variables into records of .debug$S.
//     CvType      one type by number (1, 2, ...; 0: none): a fundamental type, a qualified one, a pointer, a
//                 reference, a pointer to member, an array, a function type, or a class
//     CvClass    a class, struct, union or enumeration: its names, and once it is complete its size, bases,
//                 members, nested types and enumerators
//     CvVariable  a variable with static storage: the name it shows, the object's symbol, its type; a constant's value
//     CvTypedef   a typedef name of namespace scope
// Numbers are given in the order the front end is asked, which is the order of the source; nothing here is ordered
// by an address in memory, so the same source gives the same description (5.5/2).
//
// Strict C++98.
#ifndef CPPPARSE_CVMODULE_HPP
#define CPPPARSE_CVMODULE_HPP

#include "Int64.hpp"

#include <string>
#include <vector>

namespace cppparse {

struct CvType {
    enum Kind { Basic, Modifier, Pointer, Reference, MemberPointer, Array, Procedure, MemberFunction, Class };
    Kind             kind;
    unsigned long    basic;         // Basic: CodeView's own index of the fundamental type (74h: int)
    int              to;            // Modifier: the type qualified; Pointer, Reference, MemberPointer: the pointee;
                                    // Array: the element; Procedure, MemberFunction: the result
    bool             isConst, isVolatile;   // Modifier; of a Pointer or MemberPointer itself
    unsigned long    bytes;         // Array: its size; MemberPointer: its size
    int              classNumber;   // Class: the class or enumeration, a number in CvModule's classes
    int              ofClass;       // MemberPointer, MemberFunction: the class, a type number
    int              model;         // MemberPointer: CodeView's representation (CV_pmtype): to data 1, 2, 3 for
                                    // single, multiple, virtual inheritance and 4 unspecified; to a function 5 to 8
    bool             toFunction;    // MemberPointer: to a member function
    int              thisType;      // MemberFunction: the type of this (0: a static member function)
    std::vector<int> params;        // Procedure, MemberFunction: the parameters' types
    bool             variadic;      // ... and "...": a last argument of type 0, as cl counts it
    CvType() : kind(Basic), basic(0), to(0), isConst(false), isVolatile(false), bytes(0), classNumber(0), ofClass(0), model(0), toFunction(false), thisType(0), variadic(false) {}
};

struct CvMember {
    enum Kind { Data, Static, Nested };
    Kind        kind;
    std::string name;
    int         type;               // Nested: the nested class or enumeration, or the type a member typedef names
    int         access;             // 1 private, 2 protected, 3 public (CodeView's CV_access)
    long        offset;             // Data
    int         bitWidth, bitPosition;   // Data: a bit-field's width (-1: none) and first bit in its unit
    CvMember() : kind(Data), type(0), access(3), offset(0), bitWidth(-1), bitPosition(0) {}
};

struct CvBase {
    int  type;
    int  access;
    bool isVirtual, indirect;       // a virtual base; one inherited from a base, not named by the class itself
    long offset;                    // a non-virtual base: where it lies
    long vbptrOffset, vbIndex;      // a virtual base: the vbptr through which it is found, its entry in that vbtable
    CvBase() : type(0), access(3), isVirtual(false), indirect(false), offset(0), vbptrOffset(0), vbIndex(0) {}
};

struct CvEnumerator {
    std::string name;
    int64       value;
};

struct CvClass {
    enum Kind { Class, Struct, Union, Enum };
    Kind        kind;
    std::string name;               // as cl shows it: "geom::Circle", "Arr<int,4>"
    std::string uniqueName;         // the type descriptor's decorated name, ".?AVCircle@geom@@"
    std::string udtName;            // the name its S_UDT shows where that is not 'name': a class of an unnamed
                                    // namespace is "?A0x<unit id>::Hidden" there, as cl writes it (empty: name)
    bool        complete;           // the definition is described below (else the record is a forward reference only)
    bool        nested, scoped;     // declared in a class; in a function
    bool        hasNested, hasConstructor;
    unsigned long size;
    int         underlying;         // Enum: its underlying type, a type number
    int         vtableSlots;        // a class with virtual functions: the slots of its vftable at offset 0 (-1: none)
    bool        ownVfptr;           // ... whose pointer is its own member, not a base's
    std::vector<CvBase>       bases;
    std::vector<CvMember>     members;
    std::vector<CvEnumerator> enumerators;
    CvClass() : kind(Struct), complete(false), nested(false), scoped(false), hasNested(false), hasConstructor(false), size(0), underlying(0), vtableSlots(-1), ownVfptr(false) {}
};

struct CvVariable {
    std::string name;               // "geom::Shape::count"
    std::string symbol;             // the object's symbol of it; empty: none (a constant that has no storage)
    int         type;
    bool        internal;           // internal linkage: S_LDATA32
    bool        thread;             // __declspec(thread): S_GTHREAD32 / S_LTHREAD32, its offset in .tls$
    bool        hasValue;           // a const of integral or enumeration type with a constant value: an S_CONSTANT
    int64       value;              // where the object does not define the symbol
    CvVariable() : type(0), internal(false), thread(false), hasValue(false), value(0) {}
};

struct CvTypedef {
    std::string name;
    int         type;
};

class CvModule {
public:
    // each returns the number of what it adds
    int add(const CvType& t)     { m_types.push_back(t); return static_cast<int>(m_types.size()); }
    int add(const CvClass& r)   { m_classes.push_back(r); return static_cast<int>(m_classes.size()); }
    void add(const CvVariable& v) { m_variables.push_back(v); }
    void add(const CvTypedef& t)  { m_typedefs.push_back(t); }

    int typeCount() const   { return static_cast<int>(m_types.size()); }
    int classCount() const { return static_cast<int>(m_classes.size()); }
    const CvType&   type(int number) const   { return m_types[static_cast<size_t>(number - 1)]; }
    const CvClass& classAt(int number) const { return m_classes[static_cast<size_t>(number - 1)]; }
    CvClass&       classAt(int number)       { return m_classes[static_cast<size_t>(number - 1)]; }
    const std::vector<CvVariable>& variables() const { return m_variables; }
    const std::vector<CvTypedef>&  typedefs() const  { return m_typedefs; }

    // the description as text (<name>-cv6.txt): one line per type, class, member, variable
    std::string dump() const;

private:
    std::vector<CvType>     m_types;
    std::vector<CvClass>   m_classes;
    std::vector<CvVariable> m_variables;
    std::vector<CvTypedef>  m_typedefs;
};

} // namespace cppparse

#endif
