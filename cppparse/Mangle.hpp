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
std::string msvcMangleVariableSymbol(const sema::Symbol& v);
std::string msvcMangleVtableSymbol(const sema::Symbol& c);

} // namespace cppparse

#endif
