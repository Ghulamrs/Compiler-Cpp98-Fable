// Ir.hpp - version 3: three-address intermediate representation.
//
// One IrFunction per compiled function: a list of basic blocks holding
// instructions. Temporaries (t1, t2, ...) are assigned exactly once by the
// builder; locals and parameters are memory slots accessed with load/store
// (loadlocal / storelocal), so the optimizer can forward stores to loads and
// remove dead stores without an SSA construction. Class layouts give field
// offsets and vtable slots.
//
// Instructions:
//   const / fconst / str      t = immediate
//   copy                      t = t2
//   loadlocal / storelocal    t = local[i]   /  local[i] = t
//   addrlocal                 t = &local[i]
//   loadglobal / storeglobal / addrglobal      (by name)
//   loadmem / storemem        t = *addr  /  *addr = t
//   addrfield                 t = base + offset        (sub = field name)
//   addrindex                 t = base + index * size
//   bin  (sub: add sub mul sdiv udiv srem urem shl ashr lshr and or xor fadd fsub fmul fdiv)
//   un   (sub: neg fneg not)
//   cmp  (sub: eq ne slt sle sgt sge ult ule ugt uge feq fne flt fle fgt fge)
//   conv (sub: sext zext trunc sitofp uitofp fptosi fpext fptrunc ptrtoint inttoptr bitcast)
//   call (sub: callee name)   vcall (sub: method, args[0] = this, last arg = slot)
//   br / condbr / ret / throw / unreachable
//   trybegin (args: handler blocks, extra: region id) / tryend (extra: region id, also emitted
//   before return/break/continue that leave the region) / catchparam (sub: catch type, t = object
//   address) / catchend (end of a handler body)
//
// Strict C++98.
#ifndef CPPPARSE_IR_HPP
#define CPPPARSE_IR_HPP

#include "Ast.hpp"

#include <map>
#include <string>
#include <vector>

namespace cppparse {

struct IrOperand {
    enum Kind { None, Temp, Imm, FImm, Local, Global, Block, Str };
    Kind        kind;
    int         id;      // temp id, local index, block id
    long        ival;
    double      fval;
    std::string name;    // global name, string literal
    const Type* type;

    IrOperand() : kind(None), id(-1), ival(0), fval(0), type(0) {}
    static IrOperand temp(int id, const Type* t) { IrOperand o; o.kind = Temp; o.id = id; o.type = t; return o; }
    static IrOperand imm(long v, const Type* t) { IrOperand o; o.kind = Imm; o.ival = v; o.type = t; return o; }
    static IrOperand fimm(double v, const Type* t) { IrOperand o; o.kind = FImm; o.fval = v; o.type = t; return o; }
    static IrOperand local(int idx, const Type* t) { IrOperand o; o.kind = Local; o.id = idx; o.type = t; return o; }
    static IrOperand global(const std::string& n, const Type* t) { IrOperand o; o.kind = Global; o.name = n; o.type = t; return o; }
    static IrOperand block(int id) { IrOperand o; o.kind = Block; o.id = id; return o; }
    static IrOperand str(const std::string& s, const Type* t) { IrOperand o; o.kind = Str; o.name = s; o.type = t; return o; }
    bool isConst() const { return kind == Imm || kind == FImm; }
    std::string str() const;
};

struct IrInst {
    enum Op { Const, FConst, StrConst, Copy, LoadLocal, StoreLocal, AddrLocal, LoadGlobal, StoreGlobal, AddrGlobal,
              LoadMem, StoreMem, AddrField, AddrIndex, Bin, Un, Cmp, Conv, Call, VCall, Br, CondBr, Ret, Throw,
              TryBegin, TryEnd, CatchParam, CatchEnd, Unreachable, Nop, Phi };
    Op                     op;
    std::string            sub;    // sub-operation, callee name, field name
    int                    dst;    // result temp or -1
    const Type*            type;   // result type (or stored value type)
    std::vector<IrOperand> args;
    int                    line;
    long                   extra;  // offset / element size / vtable slot

    IrInst() : op(Nop), dst(-1), type(0), line(0), extra(0) {}
    bool pure() const;             // result-only, no side effects, no memory read
    bool readsMemory() const;
    bool isTerminator() const { return op == Br || op == CondBr || op == Ret || op == Throw || op == Unreachable; }
    std::string str() const;
};

struct IrBlock {
    int                 id;
    std::string         label;
    std::vector<IrInst> insts;
    std::vector<int>    succs;
    std::vector<int>    preds;
    IrBlock() : id(0) {}
};

struct IrLocal {
    std::string name;
    const Type* type;
    int         size;
    int         align;
    bool        addressTaken;
    bool        isParam;
    bool        isThis;
    IrLocal() : type(0), size(8), align(8), addressTaken(false), isParam(false), isThis(false) {}
};

struct IrFunction {
    std::string              name;       // "geom::Circle::area(...) const"
    const FunctionInfo*      fn;
    std::vector<IrBlock>     blocks;
    std::vector<IrLocal>     locals;     // parameters first ("this" at 0 for methods)
    int                      nextTemp;
    const Type*              retType;
    int                      paramCount;
    bool                     hasThis;
    bool                     hasTry;
    std::vector<std::string> notes;      // constructs that were approximated
    std::map<int, const Type*> tempTypes;

    IrFunction() : fn(0), nextTemp(1), retType(0), paramCount(0), hasThis(false), hasTry(false) {}
    int  newTemp(const Type* t) { tempTypes[nextTemp] = t; return nextTemp++; }
    int  newBlock(const std::string& label);
    void computeCfg();
    int  instCount() const;
    const Type* typeOf(int temp) const;
};

// class layouts: sizes, field offsets, vtables
class Layouts {
public:
    struct Field { int offset; const Type* type; bool isStatic; Field() : offset(0), type(0), isStatic(false) {} };
    Layouts(TypeTable& types, const CodeBase& cb) : m_types(types), m_cb(cb) {}
    int  sizeOf(const Type* t);
    int  alignOf(const Type* t);
    bool field(const ClassInfo* cls, const std::string& name, Field& out);
    int  baseOffset(const ClassInfo* cls, const ClassInfo* base);
    bool hasVtable(const ClassInfo* cls);
    int  vtableSlot(const ClassInfo* cls, const FunctionInfo* method);   // -1 if not virtual
    int  classSize(const ClassInfo* cls);
    std::string describe(const ClassInfo* cls);   // "class X: size N { +0 vptr, +8 int a, ... }"

private:
    struct Layout {
        int size, align; bool vptr;
        std::map<std::string, Field> fields;
        std::map<const ClassInfo*, int> bases;
        std::vector<std::string> vtable;   // "name(params)const" keys
        std::vector<std::string> order;    // display
        Layout() : size(0), align(1), vptr(false) {}
    };
    const Layout& layout(const ClassInfo* cls);
    std::map<const ClassInfo*, Layout> m_cache;
    TypeTable&      m_types;
    const CodeBase& m_cb;
};

class IrBuilder {
public:
    IrBuilder(const CodeBase& cb, TypeTable& types, Layouts& layouts);
    bool build(const FunctionAst& ast, IrFunction& out);

private:
    struct LValue {
        enum Kind { None, Local, Global, Mem } kind;
        int         local;
        std::string global;
        int         addr;      // temp holding the address
        const Type* type;      // type of the object
        LValue() : kind(None), local(-1), addr(-1), type(0) {}
    };
    struct LoopTargets { int breakBlock, continueBlock; size_t tryDepth; };   // tryDepth: try regions open at loop entry

    int    emit(IrInst inst);                       // returns dst
    void   leaveTryRegions(size_t depth, int line); // tryend for every region deeper than depth
    IrInst make(IrInst::Op op, const Type* type, int line);
    int    cur() const { return m_block; }
    void   setBlock(int b) { m_block = b; }
    bool   terminated() const;
    void   br(int target, int line);
    void   condbr(int cond, int t, int f, int line);

    void   stmt(Stmt* s);
    int    value(Expr* e);                          // rvalue temp
    LValue lvalue(Expr* e);                         // address of an lvalue
    int    load(const LValue& lv, const Type* t, int line);
    void   store(const LValue& lv, int temp, const Type* t, int line);
    int    convert(int temp, const Type* from, const Type* to, int line);
    int    toBool(int temp, const Type* t, int line);
    void   condition(Expr* e, int trueBlock, int falseBlock);
    int    call(Expr* e);
    int    binary(const std::string& op, int a, const Type* ta, int b, const Type* tb, const Type* result, int line);
    int    constant(long v, const Type* t, int line);
    int    thisTemp(int line);
    int    localIndexFor(const Expr* e) const;      // AST local -> IR local

    const CodeBase&     m_cb;
    TypeTable&          m_types;
    Layouts&            m_layouts;
    const FunctionAst*  m_ast;
    IrFunction*         m_out;
    int                 m_block;
    std::vector<LoopTargets> m_loops;
    std::vector<int>    m_switchBreaks;
    std::vector<int>    m_localMap;                  // ast local index -> ir local index
    int                 m_catchDepth;
    std::vector<int>    m_tryStack;                  // cleanup stack: try region id (>= 0) or -1 for a catch handler body
    int                 m_tryCount;                  // try regions in this function (ids 0..n-1, IrInst::extra)
};

std::string printIr(const IrFunction& f);

} // namespace cppparse

#endif
