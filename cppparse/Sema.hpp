// Sema.hpp - version 6 stage 3: semantic analysis of the cxx:: syntax tree.
//
// Types are interned canonical objects; symbols are the named entities;
// scopes hold the symbols of a declarative region. Sema walks a translation
// unit, declares everything, resolves names, computes the type of every
// expression with the C++98 implicit conversion rules, chooses overloads,
// evaluates constant expressions, lays classes out, instantiates templates
// on the syntax tree, and reports S-coded diagnostics. The tree is
// annotated in place (Expr::semaType/semaSym, Declarator::semaSym,
// Decl::semaSym) for the IR generator.
//
// Strict C++98.
#ifndef CPPPARSE_SEMA_HPP
#define CPPPARSE_SEMA_HPP

#include "CxxAst.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace cppparse {
namespace sema {

struct Symbol;
struct Scope;


struct Type {
    enum Kind {
        Void, Bool, Char, SChar, UChar, WChar, Short, UShort, Int, UInt, Long, ULong, LongLong, ULongLong,
        Float, Double, LongDouble, NullPtr /* the constant 0 as a pointer */,
        Pointer, Reference, Array, Function, Class, Enum, MemberPointer, Dependent, Error
    };
    Kind                     kind;
    bool                     isConst, isVolatile;
    const Type*              inner;        // pointee / referent / element / return type / member pointer target
    long                     arraySize;    // Array: -1 when unknown
    std::vector<const Type*> params;       // Function
    bool                     variadic;     // Function
    bool                     constMethod;  // Function: member function with const this
    Symbol*                  decl;         // Class / Enum: the symbol; Dependent: the template parameter
    const Type*              memberClass;  // MemberPointer

    Type() : kind(Error), isConst(false), isVolatile(false), inner(0), arraySize(-1), variadic(false), constMethod(false), decl(0), memberClass(0) {}

    bool isIntegral()   const { return (kind >= Bool && kind <= ULongLong) || kind == Enum; }
    bool isFloating()   const { return kind == Float || kind == Double || kind == LongDouble; }
    bool isArithmetic() const { return isIntegral() || isFloating(); }
    bool isPointer()    const { return kind == Pointer || kind == NullPtr; }
    bool isScalar()     const { return isArithmetic() || isPointer() || kind == MemberPointer || kind == Dependent; }
    bool isClass()      const { return kind == Class; }
    bool isReference()  const { return kind == Reference; }
    bool isVoid()       const { return kind == Void; }
    bool isUnsigned()   const { return kind == Bool || kind == UChar || kind == UShort || kind == UInt || kind == ULong || kind == ULongLong || kind == Char; }
    bool isDependent()  const;
    std::string key()  const;   // interning key
    std::string text() const;   // "const int*"
};

class TypeTable {
public:
    TypeTable();
    ~TypeTable();
    const Type* basic(Type::Kind k) const { return m_basic[k]; }
    const Type* qualified(const Type* t, bool isConst, bool isVolatile);   // adds qualifiers (never removes)
    const Type* unqualified(const Type* t);
    const Type* pointer(const Type* to);
    const Type* reference(const Type* to);
    const Type* array(const Type* of, long size);
    const Type* function(const Type* ret, const std::vector<const Type*>& params, bool variadic, bool constMethod);
    const Type* memberPointer(const Type* cls, const Type* target);
    const Type* classType(Symbol* cls);
    const Type* enumType(Symbol* en);
    const Type* dependent(Symbol* param);
    const Type* nullPtr() const { return m_basic[Type::NullPtr]; }
    const Type* error() const { return m_basic[Type::Error]; }
    // decay: array -> pointer, function -> pointer, reference -> referent
    const Type* decay(const Type* t);
    const Type* unref(const Type* t) const { return t && t->kind == Type::Reference ? t->inner : t; }
    int  sizeOf(const Type* t) const;
    int  alignOf(const Type* t) const;
    int  rank(const Type* t) const;        // integer conversion rank
    const Type* promoted(const Type* t);   // integral / floating promotion
    const Type* arithmeticResult(const Type* a, const Type* b);   // usual arithmetic conversions
private:
    const Type* intern(const Type& t);
    const Type*                    m_basic[Type::Error + 1];
    std::map<std::string, Type*>   m_types;
};


struct BaseClass {
    Symbol*     cls;
    cxx::Access access;
    bool        isVirtual;
    int         offset;
    BaseClass() : cls(0), access(cxx::AccessPublic), isVirtual(false), offset(0) {}
};

struct Symbol {
    enum Kind { Namespace, Class, Enum, Typedef, Variable, Function, Enumerator, TemplateParam, ClassTemplate, FunctionTemplate, Label };
    Kind          kind;
    std::string   name;            // "Circle", "operator+", "~Shape", "Circle<int>" for instantiations
    Scope*        scope;           // the scope the symbol is declared in
    const Type*   type;            // Typedef: the aliased type; Variable: its type; Function: the function type; Class/Enum: the class/enum type
    cxx::Loc      loc;
    cxx::Access   access;
    // namespaces, classes: the member scope
    Scope*        members;
    // classes
    std::vector<BaseClass>   bases;
    bool          isDefinition, isStruct, isUnion, isPolymorphic, isAbstract, layoutDone;
    int           size, align;
    std::vector<Symbol*>     fields;      // non-static data members in order (offset in Symbol::offset)
    std::vector<Symbol*>     vtable;      // virtual functions in slot order (including inherited)
    int           vptrOffset;             // -1 when no vptr
    cxx::ClassDecl* classAst;
    // functions
    std::vector<Symbol*>     params;
    bool          isVirtual, isPureVirtual, isStatic, isExtern, isInline, isExplicit, isConstructor, isDestructor, isConversion, isOperator, isFriendDecl, hasBody, isExternC;
    cxx::Decl*    definition;             // the function definition (or 0)
    cxx::Decl*    declAst;                // the declaration this symbol came from
    Scope*        body;                   // function scope (parameters) when defined
    int           vtableSlot;
    Symbol*       overrides;
    Symbol*       ownerClass;             // member functions / static members / fields
    // variables, enumerators, fields
    int           offset;                 // field offset within the class
    int           index;                  // parameter index / enumerator order
    bool          isMember, isParam, isLocal, isMutable;
    cxx::Expr*    init;
    Symbol*       initCtor;               // variable of class type: the constructor chosen for its initialisation
    bool          hasConstValue;
    long          constValue;
    // templates
    cxx::Decl*    templateAst;            // the template declaration (Decl::Template)
    std::vector<Symbol*>     templateParams;
    std::map<std::string, Symbol*> instantiations;   // argument text -> instantiated class / function
    Symbol*       instantiatedFrom;
    std::vector<const Type*> templateArgs;           // for an instantiation: the type arguments (0 for non-type)
    std::vector<long>        templateIntArgs;
    bool          isDependentContext;     // declared inside a template: not analysed until instantiated
    std::vector<Symbol*>     partialSpecs;        // class template: its partial specialisations (ClassTemplate symbols)
    std::vector<cxx::TemplateArg> patternArgs;    // partial specialisation: the argument pattern
    bool          isImplicit;             // compiler-generated constructor / destructor / assignment
    int           requiredParams;         // function: parameters without default arguments
    bool          hasUserCtor, hasCopyCtor, hasDtor, hasCopyAssign, hasDefaultCtor;   // class: user-declared special members
    std::string   mangled;                // filled by code generation

    Symbol();
    std::string qualifiedName() const;   // "geom::Circle::area"
    // constructors of "Buffer<int, 4>" are named "Buffer", like those of the template
    std::string ctorName() const { return instantiatedFrom ? instantiatedFrom->name : name; }
    bool isType() const { return kind == Class || kind == Enum || kind == Typedef || kind == TemplateParam; }
};

struct Scope {
    enum Kind { Global, Namespace, Class, Function, Block, TemplateParams, Instantiation };
    Kind                                       kind;
    Scope*                                     parent;
    Symbol*                                    owner;       // namespace / class / function symbol
    std::map<std::string, std::vector<Symbol*> > names;     // overload sets
    std::vector<Symbol*>                       order;       // declaration order
    std::vector<Scope*>                        usingDirectives;
    Scope() : kind(Block), parent(0), owner(0) {}
    void add(Symbol* s) { names[s->name].push_back(s); order.push_back(s); }
    const std::vector<Symbol*>* find(const std::string& name) const { std::map<std::string, std::vector<Symbol*> >::const_iterator it = names.find(name); return it == names.end() ? 0 : &it->second; }
    bool isNamespaceLike() const { return kind == Global || kind == Namespace; }
};

struct Diag {
    enum Severity { Error, Warning, Note };
    Severity    severity;
    std::string code;        // "S100"
    cxx::Loc    loc;
    std::string message;
    Diag() : severity(Error) {}
};

struct SemaStats {
    int symbols, classes, functionsDefined, variables, expressionsTyped, namesResolved, namesUnresolved, callsResolved, callsAmbiguous, classInstantiations, functionInstantiations;
    SemaStats() : symbols(0), classes(0), functionsDefined(0), variables(0), expressionsTyped(0), namesResolved(0), namesUnresolved(0), callsResolved(0), callsAmbiguous(0), classInstantiations(0), functionInstantiations(0) {}
};

// the result of analysing one translation unit
struct Program {
    Scope*               global;
    std::vector<Symbol*> symbols;        // every symbol, in creation order (owned)
    std::vector<Scope*>  scopes;         // every scope (owned)
    Program();
    ~Program();
    Symbol* newSymbol(Symbol::Kind k, const std::string& name, Scope* in);
    Scope*  newScope(Scope::Kind k, Scope* parent, Symbol* owner);
};

// an implicit conversion sequence, ranked as in C++98 13.3.3.1
struct Conversion {
    enum Rank { Exact = 0, Promotion = 1, Standard = 2, UserDefined = 3, Ellipsis = 4, None = 5 };
    Rank        rank;
    Symbol*     userFunction;    // converting constructor or conversion operator
    const Type* target;
    bool        derivedToBase;   // a pointer/reference conversion to a base class (worse than identity among Standard)
    bool        qualification;   // only cv-qualification added
    Conversion() : rank(None), userFunction(0), target(0), derivedToBase(false), qualification(false) {}
};

class Sema {
public:
    Sema(TypeTable& types, Program& program);
    ~Sema();
    void analyze(cxx::TranslationUnit& tu, const std::vector<std::string>& files);
    const std::vector<Diag>& diagnostics() const { return m_diags; }
    const SemaStats& stats() const { return m_stats; }
    int errorCount() const;
    std::string dumpSymbols() const;     // text dump of the global scope for tests

    // used by later stages
    Symbol* lookupQualified(const cxx::QualifiedName& n, Scope* from, bool typesOnly);
    const Type* typeOf(const cxx::DeclSpec& spec, Scope* in);
    const Type* applyDeclarator(const Type* base, const cxx::Declarator& d, Scope* in, std::vector<Symbol*>* params);

private:
    // declarations
    void   declareAll(const std::vector<cxx::Decl*>& decls, Scope* in, cxx::Access access);
    void   declare(cxx::Decl* d, Scope* in, cxx::Access access);
    void   declareSimple(cxx::Decl* d, Scope* in, cxx::Access access);
    Symbol* declareFunction(cxx::Decl* d, cxx::Declarator* dcl, const cxx::DeclSpec& spec, Scope* in, cxx::Access access, const Type* ft, const std::vector<Symbol*>& params);
    Symbol* declareClass(cxx::ClassDecl* c, Scope* in, cxx::Access access, Symbol* existing);
    Symbol* declareEnum(cxx::EnumDecl* e, Scope* in, cxx::Access access);
    void   declareTemplate(cxx::Decl* d, Scope* in, cxx::Access access);
    void   completeClass(Symbol* cls);          // layout, vtable, implicit members
    void   layoutClass(Symbol* cls);
    Symbol* findClassSymbolForQualifier(const cxx::QualifiedName& n, Scope* from);
    Scope* scopeForQualifier(const cxx::QualifiedName& n, Scope* from);

    // bodies
    struct PendingBody { Symbol* fn; cxx::Decl* def; Scope* scope; };
    void   analyzeBodies();
    void   analyzeFunctionBody(Symbol* fn, cxx::Decl* def, Scope* enclosing);
    void   stmt(cxx::Stmt* s, Scope* in, Symbol* fn);
    void   localDecl(cxx::Decl* d, Scope* in, bool isHandler = false);

    // lookup
    Symbol*  lookupUnqualified(const std::string& name, Scope* from, bool typesOnly, std::vector<Symbol*>* all);
    bool     lookupInScope(Scope* s, const std::string& name, bool typesOnly, std::vector<Symbol*>& out, std::set<Scope*>& seen);
    bool     lookupMember(Symbol* cls, const std::string& name, std::vector<Symbol*>& out, int depth = 0);
    Symbol*  resolveName(const cxx::QualifiedName& n, Scope* from, bool typesOnly, std::vector<Symbol*>* overloads);

    // types
    const Type* namedType(const cxx::QualifiedName& n, Scope* in, const std::string& elaborated, const cxx::Loc& loc);
    const Type* instantiateClassTemplate(Symbol* tmpl, const std::vector<cxx::TemplateArg>& args, Scope* from, const cxx::Loc& loc);
    bool   templateArgText(const std::vector<cxx::TemplateArg>& args, Scope* from, std::vector<const Type*>& types, std::vector<long>& ints, std::string& text);
    bool   derivesFrom(Symbol* derived, Symbol* base, int* offset, int depth = 0);

    // expressions
    const Type* expr(cxx::Expr* e, Scope* in);
    const Type* exprName(cxx::Expr* e, Scope* in);
    const Type* exprMember(cxx::Expr* e, Scope* in);
    const Type* exprCall(cxx::Expr* e, Scope* in);
    const Type* exprBinary(cxx::Expr* e, Scope* in);
    const Type* exprUnary(cxx::Expr* e, Scope* in);
    const Type* exprAssign(cxx::Expr* e, Scope* in);
    const Type* exprNew(cxx::Expr* e, Scope* in);
    const Type* exprCast(cxx::Expr* e, Scope* in, const Type* to, bool checked);
    const Type* valueType(cxx::Expr* e);          // lvalue-to-rvalue: the type after decay, without top-level cv
    bool   constantValue(cxx::Expr* e, long& out);
    Symbol* classOperator(const std::string& op, cxx::Expr* e, const std::vector<cxx::Expr*>& operands, Scope* in, const Type*& result);
    Symbol* resolveOverload(const std::vector<Symbol*>& cands, const std::vector<cxx::Expr*>& args, size_t firstArg, cxx::Expr* thisExpr, Scope* in, const cxx::Loc& loc, const std::string& what, bool report);
    Conversion convert(cxx::Expr* from, const Type* to, Scope* in);
    Conversion convertType(const Type* from, bool lvalue, bool nullConstant, const Type* to, Scope* in, bool allowUser);
    Symbol* instantiateFunctionTemplate(Symbol* tmpl, const std::vector<cxx::Expr*>& args, size_t firstArg, Scope* in, const cxx::Loc& loc);
    bool   deduce(const cxx::Param* p, const Type* argType, Scope* tscope, std::map<std::string, const Type*>& bindings, std::map<std::string, long>& ints);
    const Type* resultOfCall(Symbol* fn);

    // diagnostics
    void error(const cxx::Loc& loc, const char* code, const std::string& msg);
    void warning(const cxx::Loc& loc, const char* code, const std::string& msg);
    std::string where(const cxx::Loc& loc) const;

    TypeTable&                 m_types;
    Program&                   m_prog;
    std::vector<Diag>          m_diags;
    SemaStats                  m_stats;
    std::vector<std::string>   m_files;
    std::vector<PendingBody>   m_pending;
    Symbol*                    m_currentFunction;
    Symbol*                    m_currentClass;
    int                        m_loopDepth, m_switchDepth, m_depth;
    std::set<std::string>      m_reportedUnknown;
    std::vector<cxx::Decl*>       m_ownedDecls;        // rewritten copies of out-of-line template member definitions
    std::vector<cxx::Declarator*> m_ownedDeclarators;
};

} // namespace sema
} // namespace cppparse

#endif
