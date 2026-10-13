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
// Object lifetimes: every constructed local, by-value parameter (destroyed by
// the callee, as MSVC does) and class temporary is recorded as a Cleanup with a
// "constructed" flag byte. Block scopes destroy their objects in reverse order
// on normal exit, break/continue/return unwind the scopes they leave, and
// temporaries die at the end of the full expression. A function that owns such
// objects gets a catch-all try region around its body whose handler destroys the
// objects whose flag is still set and rethrows, so destructors also run while an
// exception unwinds through the function.
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
    // members of class template instantiations whose bodies had errors (Symbol::instantiationFailed) that the
    // generated code needs all the same, e.g. through an implicit destructor call: the unit cannot be compiled
    const std::vector<sema::Symbol*>& failedMembersUsed() const { return m_failedUsed; }

private:
    struct LValue {
        enum Kind { None, Local, Global, Mem } kind;
        int         local;
        std::string global;
        int         addr;
        const sema::Type* type;      // the object's sema type (unreferenced)
        int         bitWidth;        // a bit-field (Mem only): its width (0: not a bit-field) and first bit in the unit at addr
        int         bitOffset;
        LValue() : kind(None), local(-1), addr(-1), type(0), bitWidth(0), bitOffset(0) {}
    };
    struct LoopTargets { int breakBlock, continueBlock; size_t tryDepth, cleanupDepth; };
    struct Cleanup {
        int               slot;        // the object's local, or the local holding its address when viaPointer
        const sema::Type* type;
        bool              viaPointer;
        int               flag;        // local byte: 1 while the object is constructed
        bool              guarded;     // the normal-path destruction must test the flag (temporary of a conditional arm)
        bool              unwindOnly;  // a member built by a constructor: destroyed only when the body throws
        Cleanup() : slot(-1), type(0), viaPointer(false), flag(-1), guarded(false), unwindOnly(false) {}
    };
    struct ExitThunk {                 // destroys a global or static local at exit; registered with atexit once it is built
        sema::Symbol*     fn;
        std::string       global;      // the object's IR name
        const sema::Type* type;
    };

    // types
    const Type* oldType(const sema::Type* t);
    const ClassInfo* classInfo(sema::Symbol* cls);
    int  sizeOf(const sema::Type* t) const { return m_semaTypes.sizeOf(t); }
    // the bytes a trivial copy moves: none for an empty class, whose byte may belong to a member of the
    // derived object when it is a base subobject (MSVC places empty bases over other members)
    int  copySize(const sema::Type* t) const { return t && t->kind == sema::Type::Class && t->decl && t->decl->layoutDone && t->decl->nvSize == 0 ? 0 : sizeOf(t); }
    int  alignOf(const sema::Type* t) const { return m_semaTypes.alignOf(t); }
    bool isClassType(const sema::Type* t) const { t = m_semaTypes.unref(t); return t && t->kind == sema::Type::Class; }
    bool baseOffset(sema::Symbol* derived, sema::Symbol* base, int& offset, int depth = 0) const;
    std::string throwDescriptorFor(const sema::Type* t) const;

    // special members
    sema::Symbol* findSpecial(sema::Symbol* cls, char which) const;   // 'd' default ctor, 'c' copy ctor, 'a' copy assignment, '~' destructor
    bool needsDtor(const sema::Type* t, int depth = 0) const;
    bool needsCopyCtor(const sema::Type* t, int depth = 0) const;
    bool needsCopyAssign(const sema::Type* t, int depth = 0) const;
    bool needsDefaultCtor(const sema::Type* t, int depth = 0) const;
    // MSVC x64: a class of 1, 2, 4 or 8 bytes without a non-trivial copy constructor travels in an integer
    // register; a POD-like one of those sizes is returned in rax by a non-member or static function
    const sema::Type* regParamType(const sema::Type* t) const;
    const sema::Type* regReturnType(sema::Symbol* fn, const sema::Type* rt) const;
    bool podForReturn(const sema::Type* t, int depth) const;
    void callSpecial(sema::Symbol* fn, int thisPtr, int argPtr, int line);
    void copyBytes(int dst, int src, int size, int line);
    void bitField(LValue& lv, const sema::Symbol* field) const;   // marks lv as the bit-field field (when it is one)
    int  loadBits(const LValue& lv, const sema::Type* t, int line);
    void storeBits(const LValue& lv, int temp, const sema::Type* t, int line);
    int  bitConstant(unsigned long long v, const sema::Type* t, int line);
    void copyConstruct(const sema::Type* t, int dst, int src, int line);
    void copyAssign(const sema::Type* t, int dst, int src, int line);
    void constructDefault(const sema::Type* t, int addr, int line);
    void forEachElement(const sema::Type* elem, int base, int count, bool construct, int line);
    int  materialize(cxx::Expr* e, const sema::Type* t, int dst, int line);
    void destroy(const sema::Type* t, int objAddr, int line, bool viaVtable);

    // lifetimes
    int  registerCleanup(int slot, const sema::Type* t, bool viaPointer, int mode, int line);
    void runCleanup(const Cleanup& c, bool guarded, int line);
    void openScope();
    void closeScope(int line);
    void leaveScopes(size_t depth, int line);
    void endFullExpr(int line, size_t from = 0);
    void keepTemporaries(size_t from);
    void handlerCleanups(size_t mark, int line);
    void returnSequence(int line);
    void finishFunction(int line);
    void scopedStmt(cxx::Stmt* s, int line);

    // functions
    void genFunction(sema::Symbol* fn);
    void genImplicit(sema::Symbol* fn);
    void genInitializer();
    bool staticInit(sema::Symbol* var, IrGlobal& g);
    bool staticData(const sema::Type* t, const cxx::Initializer& init, std::vector<IrDataItem>& out);
    bool constantData(const sema::Type* t, cxx::Expr* x, std::vector<IrDataItem>& out, int depth);
    bool aggregateData(const sema::Type* t, const std::vector<cxx::Expr*>* elems, std::vector<IrDataItem>& out, int depth);
    bool readOnlyObject(const sema::Type* t) const;
    void registerAtExit(const std::string& global, const sema::Type* t, int line);
    void genExitThunks();
    void drainQueue();
    void prologue(sema::Symbol* fn);
    void constructorInit(sema::Symbol* fn, cxx::Decl* def);
    void destructorCleanup(sema::Symbol* fn);
    void storeVptr(sema::Symbol* cls, int objAddr, int line);
    void need(sema::Symbol* fn);
    int  newLocal(const std::string& name, const sema::Type* t, bool addressTaken);
    int  subobject(int objAddr, const sema::Type* objType, const std::string& name, int offset, const sema::Type* partType, int line);

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
    int    offsetAddress(int temp, const sema::Type* fromPtr, const sema::Type* toPtr, long delta, int line);
    int    toBool(int temp, const sema::Type* t, int line);
    int    binary(const std::string& op, int a, const sema::Type* ta, int b, const sema::Type* tb, const sema::Type* result, int line);
    void   leaveTryRegions(size_t depth, int line);

    // expressions and statements
    int    value(cxx::Expr* e);
    LValue lvalue(cxx::Expr* e);
    void   condition(cxx::Expr* e, int trueBlock, int falseBlock);
    int    conditional(cxx::Expr* e, bool byAddr);
    int    call(cxx::Expr* e, int sretAddr = -1);
    int    operatorCall(cxx::Expr* e);
    int    adjustThis(sema::Symbol* fn, int objAddr, const sema::Type* objType, int line);
    int    rvalue(int r, cxx::Expr* e, int line);
    int    callFunction(sema::Symbol* fn, int thisPtr, int sretAddr, const std::vector<cxx::Expr*>& args, size_t firstArg, bool virtualCall, int line);
    int    construct(sema::Symbol* ctor, int objAddr, const std::vector<cxx::Expr*>& args, size_t firstArg, int line);
    int    argument(cxx::Expr* a, const sema::Type* paramType, int line);
    void   stmt(cxx::Stmt* s);
    int    labelBlock(const std::string& name);
    void   localDecl(cxx::Decl* d);
    void   initObject(const LValue& lv, const sema::Type* t, sema::Symbol* var, cxx::InitDeclarator& id, int line);
    void   abiResult(IrInst& r, int line);
    void   initAggregate(const sema::Type* t, int addr, const std::vector<cxx::Expr*>& elems, bool zeroRest, int line);
    bool   coversAll(const sema::Type* t, const std::vector<cxx::Expr*>& elems) const;
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
    std::vector<sema::Symbol*> m_failedUsed;
    std::vector<std::string>   m_notes;
    int                        m_generated;
    std::vector<sema::Symbol*> m_dynamicGlobals;   // globals built by the module's initialiser, in declaration order
    sema::Symbol*              m_initSymbol;
    std::vector<ExitThunk>     m_exitThunks;       // destructor thunks registered with atexit; generated after the functions
    size_t                     m_exitThunksDone;
    X86Module*                 m_x86;              // receives the globals that static locals become
    bool                       m_msvcNames;        // --msvc-mangling: static locals get cl's decorated names
    std::map<sema::Symbol*, std::string> m_staticNames;   // static local -> its global's IR name
    int                        m_staticCount;
    int                        m_guardCount;    // guarded static locals so far in the function (cl's $TSS<n>)
    std::map<std::string, int> m_labels;           // goto labels of the current function -> block

    IrFunction*                m_out;
    sema::Symbol*              m_fn;
    int                        m_block;
    int                        m_sretSlot;
    const sema::Type*          m_retReg;        // the integer type a small class result returns in, else 0
    std::map<sema::Symbol*, int> m_slots;
    std::vector<LoopTargets>   m_loops;
    std::vector<int>           m_tryStack;      // region ids, -1 inside a handler body
    int                        m_tryCount;
    std::vector<Cleanup>       m_cleanups;      // constructed objects of the open scopes, in construction order
    std::vector<size_t>        m_scopes;        // m_cleanups.size() at each open block scope
    std::vector<Cleanup>       m_temps;         // temporaries of the current full expression
    std::vector<Cleanup>       m_flagged;       // every cleanup of the function, for the unwind handlers
    int                        m_unwindId;      // the function's catch-all region
    int                        m_bodyBlock;
};

} // namespace cppparse

#endif
