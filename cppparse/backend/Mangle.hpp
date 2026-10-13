// Mangle.hpp - version 5: Microsoft Visual C++ compatible name mangling (x64).
//
// Produces the decorated names cl.exe would give the same declarations, so
// that objects written by cppparse link against code compiled by MSVC with
// ordinary C++ declarations (no extern "C" needed):
//
//   int square(int)                   ?square@@YAHH@Z
//   int Point::sum() const            ?sum@Point@@QEBAHXZ
//   Point::Point(int, int)            ??0Point@@QEAA@HH@Z
//   void Buffer<int,4>::add(const int&)  ?add@?$Buffer@H$03@@QEAAXAEBH@Z
//   int maxOf<int>(const int&, const int&)  ??$maxOf@H@@YAHAEBH0@Z
//   int g_shapeVersion                ?g_shapeVersion@@3HA
//   vtable of geom::Circle            ??_7Circle@geom@@6B@
//
// Covered: namespaces and nested classes, access and static/virtual kinds,
// const methods, constructors/destructors/operators, the fundamental types,
// pointers/references with cv-qualifiers, class/struct/union/enum types,
// template instances with type and integer arguments, name and type
// back-references. Not covered: function pointer parameters beyond the
// simple case, member pointers, varargs.
//
// Strict C++98.
#ifndef CPPPARSE_MANGLE_HPP
#define CPPPARSE_MANGLE_HPP

#include "Types.hpp"

#include <string>

namespace cppparse {
namespace sema { struct Symbol; }

std::string msvcMangleFunction(const FunctionInfo& f, TypeTable& types);
std::string msvcMangleVariable(const VariableInfo& v, TypeTable& types);   // globals and static data members
std::string msvcMangleVtable(const std::string& qualifiedClassName);

// the same decoration from the version 6 symbols (MangleSema.cpp)
std::string msvcMangleSymbol(const sema::Symbol& fn);
// the id that distinguishes this translation unit's anonymous namespace ("?A0x<id>@"), set per unit
void        setMangleUnitId(unsigned long id);
extern unsigned long g_mangleUnitId;
std::string msvcMangleVariableSymbol(const sema::Symbol& v);
// a static local: "?n@?1??counterA@@YAHXZ@4HA", the variable in block <discriminator> (2: the function's body)
std::string msvcMangleStaticLocal(const sema::Symbol& v, const sema::Symbol& fn, long discriminator);
// a vftable; forBase names one of several (??_7D@@6BPB@@@: D's vftable for PB), 0 for a class with one
std::string msvcMangleVtableSymbol(const sema::Symbol& c, const sema::Symbol* forBase = 0);
// an adjustor thunk of a virtual function: ?f@D@@WBA@EAAHXZ, D::f`adjustor{16}'
std::string msvcMangleThunkSymbol(const sema::Symbol& fn, long adjust);

} // namespace cppparse

#endif
