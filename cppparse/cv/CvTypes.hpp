// CvTypes.hpp - the type records of an object's .debug$T section (cl's /Z7 form: the types are in the object).
//
// CV_SIGNATURE_C13 (4), then records of {length of what follows (2 bytes), leaf (2), data, LF_PAD bytes up to a
// multiple of four}; a record's type index is 1000h plus its position. CvTypes writes the records of a CvModule's
// descriptions as they are asked for, each record once (an equal one already written is found by its bytes), so the
// indices follow the order of first use:
//     a fundamental type              no record: CodeView's own index (74h int); a plain pointer to one 600h + index
//     LF_MODIFIER (1001h)             {type, 1 const | 2 volatile}
//     LF_POINTER (1002h)              {type, attributes: 64-bit (Ch), 8 bytes (10000h); 20h a reference; 400h const,
//                                     200h volatile; a pointer to member 40h (data) or 60h (function) with its own
//                                     size, then {the class, the representation}}
//     LF_ARRAY (1503h)                {element, index type 23h, size in bytes, ""}
//     LF_PROCEDURE (1008h)            {result, convention 0, attributes 0, parameter count, LF_ARGLIST}
//     LF_MFUNCTION (1009h)            {result, class, type of this, 0, 0, parameter count, LF_ARGLIST, this adjustment 0}
//     LF_ARGLIST (1201h)              {count, types}; "..." is a last type 0
//     LF_ENUM (1507h)                 {count, properties, underlying type, LF_FIELDLIST, name, unique name}
//     LF_CLASS (1504h), LF_STRUCTURE (1505h)   {member count, properties, LF_FIELDLIST, derived 0, LF_VTSHAPE, size,
//                                     name, unique name}; LF_UNION (1506h) the same without derived and shape
//     LF_FIELDLIST (1203h)            LF_BCLASS (1400h) {access, base, offset}; LF_VBCLASS (1401h), LF_IVBCLASS (1402h)
//                                     {access, base, type of the vbptr, vbptr offset, index in the vbtable};
//                                     LF_VFUNCTAB (1409h) {0, pointer to the LF_VTSHAPE}; LF_MEMBER (150Dh) {access,
//                                     type, offset, name}, a bit-field's type an LF_BITFIELD (1205h) {type, width,
//                                     position}; LF_STMEMBER (150Eh) {access, type, name}; LF_NESTTYPE (1510h) {0, type,
//                                     name}; LF_ENUMERATE (1502h) {access, value, name}; a list that would pass FF00h
//                                     bytes continues in another through LF_INDEX (1404h) {0, list}
//     LF_VTSHAPE (000Ah)              {slot count, one 4-bit descriptor per slot: 5, a 64-bit pointer}
// Inside a record a class is named by its forward reference (properties 80h, no members, size 0), which is written
// before the first record that names it; so every index a record holds is a fundamental type or below the record's
// own (CvCheck). A symbol names the definition when the module has the class complete (symbolIndex). finish()
// writes the definition of every class named so far that is complete, and of those the definitions name in turn:
// a pointer then leads to its members in whichever object the debugger stops.
// Properties, as cl sets them: 80h forward reference, 200h has a unique name (always: linkers and readers resolve a
// forward reference by name, and two units may each have a class of one name), 8h nested, 10h contains nested
// types, 2h has a constructor or destructor, 100h local to a function, 400h on a union's definition.
//
// Strict C++98.
#ifndef CPPPARSE_CVTYPES_HPP
#define CPPPARSE_CVTYPES_HPP

#include "CvModule.hpp"
#include "CvRecord.hpp"

#include <map>
#include <utility>

namespace cppparse {

class CvTypes {
public:
    explicit CvTypes(const CvModule& module);

    // the type index of the module's type 'number' (0: none, T_NOTYPE); a class by its forward reference
    unsigned long index(int number);
    // the same for a symbol's type field: a class by its definition where the module has one
    unsigned long symbolIndex(int number);
    // the definitions still owed (see above); to be called once, behind the last index() and symbolIndex()
    void finish();

    // the section's bytes: the signature and the records written so far
    std::string section() const;
    unsigned long count() const { return static_cast<unsigned long>(m_sizes.size()); }
    // the classes and enumerations whose definition was written, with its index, in the order of writing
    const std::vector<std::pair<int, unsigned long> >& definitions() const { return m_definitions; }

private:
    unsigned long record(unsigned long leaf, const CvBytes& body);
    unsigned long forward(int classNumber);
    unsigned long definition(int classNumber);
    unsigned long fieldList(const std::vector<std::string>& members);
    unsigned long argList(const CvType& t);
    unsigned long vtShape(int slots);
    unsigned long properties(const CvClass& c) const;
    static void names(CvBytes& body, const CvClass& c);

    const CvModule&                      m_module;
    std::string                          m_records;       // the records, one behind the other
    std::vector<unsigned long>           m_sizes;         // each record's bytes
    std::map<std::string, unsigned long> m_known;         // a record's bytes -> its index
    std::vector<unsigned long>           m_index;         // per type number: its index, 0 while not asked for
    std::vector<unsigned long>           m_forward, m_definition;   // per class number
    std::vector<std::pair<int, unsigned long> > m_definitions;
};

} // namespace cppparse

#endif
