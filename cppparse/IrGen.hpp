// IrGen.hpp - version 6 stage 4: IR generation from the semantically analysed syntax tree.
//
// Walks every function with a body (including template instantiations and the
// implicit special members that are used) and produces IrFunctions for the
// shared back end. The IR still carries the old front end's Type objects, so
// sema types are converted through an adapter; class types get a synthesised
// ClassInfo that only names the class and its bases. Conventions are those of
// the old builder: locals are memory slots, class values are addresses, member
// access is AddrField with the sema offset, virtual calls carry the vtable slot,
// exceptions use the try-region ids. Functions returning a class by value take a
// hidden result pointer as their first parameter after 'this'.
//
// Strict C++98.
#ifndef CPPPARSE_IRGEN_HPP
#define CPPPARSE_IRGEN_HPP

#include "CodeBase.hpp"
#include "Ir.hpp"
#include "Sema.hpp"
#include "Types.hpp"
#include "X86.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace cppparse {

class IrGen {
public:
    IrGen(sema::Program& program, sema::TypeTable& semaTypes);
    ~IrGen();

    // generates every function of the program; the IrFunctions are owned by the caller
    void generate(std::vector<IrFunction*>& out);
    // vtables, globals and (when msvc) decorated names for the x86 module
    void registerMetadata(X86Module& x86, bool msvc);
    const std::vector<std::string>& notes() const { return m_notes; }
    static std::string irName(const sema::Symbol* fn);
    int functionsGenerated() const { return m_generated; }

private:
    struct LValue {
        enum Kind { None, Local, Global, Mem } kind;
        int         local;
        std::string global;
        int         addr;
        const sema::Type* type;      // the object's sema type (unreferenced)
        LValue() : kind(None), local(-1), addr(-1), type(0) {}
    };
    struct LoopTargets { int breakBlock, continueBlock; size_t tryDepth; };

    // types
    const Type* oldType(const sema::Type* t);
    const ClassInfo* classInfo(sema::Symbol* cls);
    int  sizeOf(const sema::Type* t) const { return m_semaTypes.sizeOf(t); }
    int  alignOf(const sema::Type* t) const { return m_semaTypes.alignOf(t); }
    bool isClassType(const sema::Type* t) const { t = m_semaTypes.unref(t); return t && t->kind == sema::Type::Class; }
    bool baseOffset(sema::Symbol* derived, sema::Symbol* base, int& offset, int depth = 0) const;
    std::string throwDescriptorFor(const sema::Type* t) const;

    // functions
    void genFunction(sema::Symbol* fn);
    void genImplicit(sema::Symbol* fn);
    void prologue(sema::Symbol* fn);
    void constructorInit(sema::Symbol* fn, cxx::Decl* def);
    void destructorCleanup(sema::Symbol* fn);
    void storeVptr(sema::Symbol* cls, int objAddr, int line);
    void need(sema::Symbol* fn);
    int  newLocal(const std::string& name, const sema::Type* t, bool addressTaken);

    // IR emission
    int    emit(IrInst inst);
    IrInst make(IrInst::Op op, const sema::Type* type, int line);
    bool   terminated() const;
    void   br(int target, int line);
    void   condbr(int cond, int t, int f, int line);
    int    constant(long v, const sema::Type* t, int line);
    int    thisTemp(int line);
    int    addressOf(const LValue& lv, int line);
    int    load(const LValue& lv, const sema::Type* t, int line);
    void   store(const LValue& lv, int temp, const sema::Type* t, int line);
    int    convert(int temp, const sema::Type* from, const sema::Type* to, int line);
    int    toBool(int temp, const sema::Type* t, int line);
    int    binary(const std::string& op, int a, const sema::Type* ta, int b, const sema::Type* tb, const sema::Type* result, int line);
    void   leaveTryRegions(size_t depth, int line);

    // expressions and statements
    int    value(cxx::Expr* e);
    LValue lvalue(cxx::Expr* e);
    void   condition(cxx::Expr* e, int trueBlock, int falseBlock);
    int    conditional(cxx::Expr* e, bool byAddr);
    int    call(cxx::Expr* e);
    int    callFunction(sema::Symbol* fn, int thisPtr, int sretAddr, const std::vector<cxx::Expr*>& args, size_t firstArg, bool virtualCall, int line);
    int    construct(sema::Symbol* ctor, int objAddr, const std::vector<cxx::Expr*>& args, size_t firstArg, int line);
    int    argument(cxx::Expr* a, const sema::Type* paramType, int line);
    void   destroy(const sema::Type* t, int objAddr, int line, bool viaVtable);
    void   stmt(cxx::Stmt* s);
    void   localDecl(cxx::Decl* d);
    void   initVariable(int slot, const sema::Type* t, sema::Symbol* var, cxx::InitDeclarator& id, int line);
    const sema::Type* exprType(cxx::Expr* e) const;

    sema::Program&         m_prog;
    sema::TypeTable&       m_semaTypes;
    CodeBase               m_emptyCb;
    TypeTable              m_types;
    std::map<sema::Symbol*, ClassInfo*> m_classInfos;
    std::vector<ClassInfo*> m_ownedClassInfos;
    std::map<const sema::Type*, const Type*> m_typeCache;

    std::vector<IrFunction*>* m_outList;
    std::set<sema::Symbol*>   m_done;
    std::vector<sema::Symbol*> m_queue;
    std::vector<std::string>   m_notes;
    int                        m_generated;

    IrFunction*                m_out;
    sema::Symbol*              m_fn;
    int                        m_block;
    int                        m_sretSlot;
    std::map<sema::Symbol*, int> m_slots;
    std::vector<LoopTargets>   m_loops;
    std::vector<int>           m_tryStack;
    int                        m_tryCount;
};

} // namespace cppparse

#endif
