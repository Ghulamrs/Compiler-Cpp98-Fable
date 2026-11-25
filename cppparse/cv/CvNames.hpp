// CvNames.hpp - the names CodeView records show, in cl's spelling.
//
// A debugger shows a procedure, and finds a type, by the name its record carries; cl's and cppparse's records of one
// function or class are one thing to it only when the names are the same characters (docs/design-codeview.md 3.4).
// CvNames holds cl's spelling rules, measured on cl 19.52's /Z7 objects; who has the declarations (CvFromSema, in
// middle/) describes a name piece by piece and gets the text:
//     a type inside a name            int *, char const *, int * const, ns::In const &, enum ns::E, __int64,
//                                     int [4], int (*)[3], int (__cdecl*)(int,char), int ns::In::*,
//                                     int (__cdecl ns::In::*)(int)const , void (__cdecl*)(void)
//     a template-id                   Pair<int,ns::Box<double> >: no space after a comma, "> >" never ">>"
//     a non-type argument             its value in decimal as a signed 64-bit number (bool 1, 'a' 97), &inc for a
//                                     function or a variable, a pointer to a data member its offset
//     a scope                         ns::Box<int>::get; an unnamed namespace `anonymous namespace'; a class local
//                                     to a function `use'::`2'::Local, the number the block's (2: the body)
//     a conversion function           operator int *
// A type is put together as C declarators are read, from the inside out, so it is built in two halves (CvTypeText):
// what stands left of the declarator and what stands right of it ("int (" and ")[3]" around the "*" of a pointer
// to an array).
//
// Strict C++98.
#ifndef CPPPARSE_CVNAMES_HPP
#define CPPPARSE_CVNAMES_HPP

#include "Int64.hpp"

#include <string>
#include <vector>

namespace cppparse {

// a type's spelling under construction
struct CvTypeText {
    std::string left, right;
    bool        grouped;      // an array or a function: a pointer or reference to it stands in parentheses
    bool        function;     // a function: its calling convention is written with the declarator, "(__cdecl*)"
    CvTypeText() : grouped(false), function(false) {}
};

class CvNames {
public:
    // ---- types ----
    // a fundamental type by its C++ name ("unsigned long long"): cl's name for it ("unsigned __int64")
    static CvTypeText basic(const std::string& cxxName, bool isConst, bool isVolatile);
    // a class, or an enumeration ("enum ns::E"), by its qualified name
    static CvTypeText named(const std::string& qualifiedName, bool isEnum, bool isConst, bool isVolatile);
    static CvTypeText pointer(const CvTypeText& to, bool isConst, bool isVolatile);
    static CvTypeText reference(const CvTypeText& to);
    static CvTypeText array(const CvTypeText& of, long count);
    // params: each parameter's text; none is "void", as cl writes it
    static CvTypeText function(const CvTypeText& result, const std::vector<std::string>& params, bool variadic, bool constMethod);
    // a pointer to a member of the class named cls
    static CvTypeText memberPointer(const CvTypeText& to, const std::string& cls, bool isConst, bool isVolatile);
    static std::string text(const CvTypeText& t);

    // ---- names ----
    static std::string templateId(const std::string& name, const std::vector<std::string>& args);
    static std::string scoped(const std::string& scope, const std::string& name);   // scope may be empty
    static std::string integer(int64 v);
    static std::string addressOf(const std::string& name) { return "&" + name; }
    // a pointer to member that is more than one value, as a template argument: "{C::f,0,4}"
    static std::string braced(const std::vector<std::string>& parts);
    static std::string unsigned32(long v) { return integer(static_cast<int64>(static_cast<unsigned int>(v))); }
    // cl's thunk for a pointer to the virtual function at 'slotOffset' of the vftable of class cls: C::`vcall'{8}'
    static std::string vcallThunk(const std::string& cls, long slotOffset);
    // the function that runs a variable's dynamic initializer
    static std::string dynamicInitializer(const std::string& variable) { return "`dynamic initializer for '" + variable + "''"; }
    static std::string anonymousNamespace() { return "`anonymous namespace'"; }
    // the scope of what is declared in block number 'block' of the function shown as 'function'
    static std::string localScope(const std::string& function, long block);
    static std::string conversionOperator(const std::string& type) { return "operator " + type; }

    // ---- the names of type records (-g) ----
    // The same spelling, with two scopes written another way than in a procedure's name: an unnamed namespace is
    // `anonymous-namespace', and a class local to a function is use::__l2::Local (the block's number again).
    static std::string anonymousNamespaceOfType() { return "`anonymous-namespace'"; }
    static std::string localScopeOfType(const std::string& function, long block);
    // a class or enumeration without a name: <unnamed-type-x> where a declarator x names it ("struct { } x;"),
    // else <unnamed-tag>
    static std::string unnamedType(const std::string& declarator);
    // a name as a record holds it: one of more than 4096 characters is cut to its first 4064 and the 32 hexadecimal
    // digits of the MD5 of the whole (measured on cl 19.52: a name of 4007 characters is whole, one of 8011 and one
    // of 16036 are cut so)
    static std::string cutName(const std::string& name);
    // a unique name (".?AV...@@") as a record holds it: one of more than 4096 characters is ".??@<MD5>@", the MD5 of
    // the name without its dot (cl's rule for every decorated name of that length; measured: 4040 whole, 8034 so)
    static std::string cutUniqueName(const std::string& uniqueName);

    // the name a debugger shows for a function of which only the IR name is known: that name without the parameter
    // list and the qualifiers after it ("geom::Circle::area() const" -> "geom::Circle::area", "S::operator()(int)" ->
    // "S::operator()")
    static std::string fromIrName(const std::string& irName);

private:
    // "T const ", "T volatile ", "T const volatile ": the qualifiers of what is not a pointer, a space behind them
    static std::string qualified(const std::string& type, bool isConst, bool isVolatile);
    // the declarator d put into t: behind a space where the left half does not end in one or in '('
    static CvTypeText declared(const CvTypeText& t, const std::string& d);
    // the MD5 of the text as 32 hexadecimal digits
    static std::string md5(const std::string& text);
};

} // namespace cppparse

#endif
