// CvFromSema.hpp - the front end's symbols and types described for CodeView (docs/design-codeview.md 5.2, 5.4).
//
// The one place that sees both the semantic analysis (Sema.hpp) and the CodeView classes of cppparse/cv. It only
// reads what Sema has: no lookup, no instantiation, no completion of a class, so asking for a description can never
// change what is compiled.
// Names, for every object (static members, no state):
//     functionName    the name a procedure's symbol shows, as cl spells it (CvNames has the rules): the scopes, a
//                     class template's arguments from the instantiation's own (not from the text they were written
//                     in), a constructor and destructor under the class's name with its arguments, a conversion
//                     function under its type's name
//     typeName        a type inside such a name
//     variableName    a variable with its scopes
// The module's description, with -g (an object, which fills a CvModule):
//     typeOf          the number of a type's description, a new one or the one given before
//     functionType    of a function's type: a member function's with its class and the type of this
//     variable        lists a variable of namespace scope or a static member with the object's symbol of it
//     finish          at the end of the unit, when every layout is final: the typedef names of namespace scope, and
//                     for every class and enumeration described so far that is complete its size, bases, members
//                     and enumerators (which may describe further classes: until none is left)
// A class is described by name alone when it is first met and completed in finish, so a pointer to a class costs
// nothing where it is declared. Numbers follow the order of the calls, which IrGen makes in source order; the maps
// here that are keyed by address are only ever looked up, never walked.
// With -g2 (setMemberFunctions) a complete class lists the member functions its definition declares: each with its
// type, access and kind (static, virtual, introducing a vftable slot at an offset, pure), the functions of an
// overloaded name together. Their parameter and result types are described with them, so a class a member function
// names is in the object like one a data member names.
//
// Strict C++98.
#ifndef CPPPARSE_CVFROMSEMA_HPP
#define CPPPARSE_CVFROMSEMA_HPP

#include "CvModule.hpp"
#include "CvNames.hpp"
#include "Sema.hpp"

#include <map>
#include <string>
#include <vector>

namespace cppparse {

// The one fact of a type that the code generator has and Sema does not record: whether a class is a plain aggregate
// to the calling convention (no constructor, destructor, copy assignment, base, virtual function or non-public data
// member of its own, and none that its members bring: IrGen::podForReturn). A function's type says with it whether
// a class result comes back the C++ way.
class CvResultRule {
public:
    virtual ~CvResultRule() {}
    virtual bool plainAggregate(const sema::Type* classType) const = 0;
};

class CvFromSema {
public:
    static std::string functionName(const sema::Symbol* fn);
    static std::string typeName(const sema::Type* t);
    // a variable of namespace scope or a static member with its scopes: "LinkDyn<int>::name"
    static std::string variableName(const sema::Symbol* v);

    CvFromSema(const sema::TypeTable& types, CvModule& module);
    // who answers for the class results (the code generator; without one no free function's result is marked)
    void setResultRule(const CvResultRule* rule) { m_rule = rule; }
    void setMemberFunctions(bool on) { m_methods = on; }
    int  typeOf(const sema::Type* t);
    // the type of fn's own procedure. A member function that is entered with this moved (an override first reached
    // through a later base's table) has a second type, the one its class lists: that one carries the adjustment,
    // this one 0, because the function puts this right before its first statement (docs/design-codeview.md 5.2)
    int  functionType(const sema::Symbol* fn);
    // objectSymbol: the symbol the object defines for it; empty when it defines none (a constant that is folded)
    void variable(const sema::Symbol* v, const std::string& objectSymbol, bool internal);
    // a vftable of the class with that many slots (one S_GDATA32 each, as cl lists them)
    void vftable(const sema::Symbol* cls, size_t slots, const std::string& objectSymbol);
    void finish(const sema::Program& program);

private:
    // where a name stands: in a procedure's symbol, or in a type record (and in S_UDT, S_GDATA32)
    enum Form { InProcedure, InType };
    static CvTypeText  typeText(const sema::Type* t, Form form, int depth);
    // a class or enumeration with its scopes; the name alone, with the template arguments of an instantiation
    static std::string qualifiedName(const sema::Symbol* s, Form form, int depth);
    static std::string ownName(const sema::Symbol* s, Form form, int depth);
    // ofType: the scopes of a class or enumeration itself (one declared in a function is scoped by it)
    static std::string scopeName(const sema::Scope* s, bool ofType, Form form, int depth);
    // the arguments of an instantiation (a class, a function), each as cl spells it
    static std::vector<std::string> templateArguments(const sema::Symbol* inst, Form form, int depth);

    int  describe(const sema::Type* t, bool unqualified);
    int  describeFunction(const sema::Symbol* fn, bool listed);
    int  thisType(int ofClass, bool constMethod);
    bool returnsUdt(const sema::Type* result, bool nonStaticMember) const;
    int  classOf(const sema::Symbol* cls);
    void complete(int classNumber);
    void method(const sema::Symbol* cls, const sema::Symbol* m, std::vector<CvMember>& members, std::map<std::string, size_t>& at, CvClass& facts);
    static bool declaredText(const sema::Type* t, std::string& out, int depth);
    int  basic(unsigned long cvIndex);
    // the type needs no class described that is not described already
    bool known(const sema::Type* t, int depth) const;

    const sema::TypeTable&             m_semaTypes;
    CvModule&                          m_module;
    std::map<const sema::Type*, int>   m_types, m_unqualified;   // a type -> its number; the same without its own cv
    const CvResultRule*                m_rule;
    bool                               m_methods;                // the classes list their member functions
    std::map<const sema::Symbol*, int> m_functionTypes, m_listedTypes;   // a function -> its procedure's type; the type its class lists where that is another
    std::map<int, int>                 m_this, m_thisConst;      // a class's type number -> the type of this, in a const member function
    std::map<const sema::Symbol*, int> m_classes;                // a class or enumeration -> its number
    std::vector<const sema::Symbol*>   m_classSymbols;           // by number
    std::map<unsigned long, int>       m_basic;                  // CodeView's index of a fundamental type -> its number
    int                                m_completed;              // the classes finish has looked at
    int                                m_unnamed;                // the classes without a name met so far
    int                                m_vftableEntry;           // a vftable's slot: const pointer to void ()
    std::map<size_t, int>              m_vftableTypes;           // the vftables' types by their slot counts
};

} // namespace cppparse

#endif
