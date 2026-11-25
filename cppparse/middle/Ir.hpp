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
//   addrtls                   t = the address of a thread-local in this thread (by name; cppparse/target TargetTls)
//   intrinsic (sub: name)     t = an intrinsic of the target emitted inline (cppparse/target TargetIntrinsics): the
//                             interlocked ones read and write *args[0] and are barriers as a call is
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
#include <set>
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
              TryBegin, TryEnd, CatchParam, CatchEnd, Unreachable, Nop, Phi, AddrTls, Intrinsic };
    Op                     op;
    std::string            sub;    // sub-operation, callee name, field name
    int                    dst;    // result temp or -1
    const Type*            type;   // result type (or stored value type)
    std::vector<IrOperand> args;
    int                    line;
    long                   extra;  // offset / element size / vtable slot
    bool                   nothrow;   // a call that cannot throw: a function with C linkage (cl /EHsc), a runtime entry
    bool                   variadic;  // a call to a variadic function, or through a pointer of unknown type: a floating-point
                                      // argument also goes to its integer register, where such a callee reads it
    int                    scope;     // -g: the innermost block scope that declares a variable and holds the statement this
                                      // was generated for (IrFunction::debugBlocks; 0: the function's own; -1: the entry
                                      // sequence, before the first statement). Set with 'line' and copied with it; no pass
                                      // reads it to decide anything (docs/design-codeview.md 5.4)

    IrInst() : op(Nop), dst(-1), type(0), line(0), extra(0), nothrow(false), variadic(false), scope(0) {}
    bool pure() const;             // result-only, no side effects, no memory read
    bool readsMemory() const;
    bool isTerminator() const { return op == Br || op == CondBr || op == Ret || op == Throw || op == Unreachable; }
    bool mayThrow() const { return op == Throw || op == VCall || (op == Call && !nothrow); }
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
    bool        nonNull;     // a parameter that always holds a valid address: this, a reference, an object passed by
                             // address, the hidden result pointer (loads through it cannot fault)
    bool        ehFlag;      // a constructed flag: 1 while its object lives, read by the function's unwind handler
    // -g (docs/design-codeview.md 5.4): a variable of the source as a debugger lists it. No pass reads these
    bool        source;      // a parameter or a local the source declares (not one of the compiler's own slots)
    std::string debugName;   // the name it is listed by ("__formal" for an unnamed parameter)
    int         debugType;   // its type, a number in the module's CvModule (0: none)
    int         debugParam;  // a parameter: its place among the declared ones (0: "this" or the first); -1: a local
    int         block;       // the block scope that declares it (IrFunction::debugBlocks; 0: the function's)
    bool        byAddress;   // a class parameter passed by address: listed as a reference to the class
    IrLocal() : type(0), size(8), align(8), addressTaken(false), isParam(false), isThis(false), nonNull(false), ehFlag(false),
                source(false), debugType(0), debugParam(-1), block(0), byAddress(false) {}
};

// -g: a static local of a function, for the S_LDATA32 in the function's symbols
struct IrDebugStatic {
    std::string name;        // as the source names it
    std::string global;      // the IR name of the module's global that it is
    int         type;        // a number in the module's CvModule
    int         block;       // the block scope that declares it (0: the function's)
    IrDebugStatic() : type(0), block(0) {}
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
    bool                     linkOnce;   // inline, template instantiation or implicit member: one copy survives linking (COMDAT)
    bool                     internal;   // internal linkage (a static function, a member of an anonymous namespace): not PUBLIC
    bool                     replaceable; // __declspec(replaceable): a default that another object's definition of the name
                                         // replaces when the program is linked, so calls of the name are never inlined
    bool                     variadic;   // ends in "...": the parameters live in their home slots, where va_start finds them
    std::vector<std::string> notes;      // constructs that were approximated
    std::map<int, const Type*> tempTypes;
    std::string              sourceFile; // the definition's source file (an absolute Windows path) and line, for the
    int                      sourceLine; // CodeView line table; empty / 0 when unknown (the version 3 pipeline)
    std::string              debugName;  // the name its CodeView symbol shows, in cl's spelling (CvFromSema); empty: the
                                         // IR name without its parameter list (CvNames::fromIrName)
    // -g (docs/design-codeview.md 5.4). No pass reads these to decide anything: debug information changes no code
    int                      debugType;  // the function's type, a number in the module's CvModule (0: none)
    std::vector<IrDebugStatic> debugStatics;   // its static locals, in declaration order
    std::vector<int>         debugBlocks;  // its block scopes that declare a variable: the parent of block k (1, 2, ...) at
                                           // [k - 1], 0 for the function's own; numbered in the order IrGen meets them
    bool                     debugInline;  // declared inline (S_FRAMEPROC's "marked inline", as cl sets it)
    bool                     debugOptimized; // the optimiser ran on it (-O1 and up): until stage 3 its variables have no place
    bool                     keepsFrame; // it calls _AddressOfReturnAddress (TargetIntrinsics): it keeps its frame at every level and
                                         // is never inlined, so [rbp+8] is its own return address

    IrFunction() : fn(0), nextTemp(1), retType(0), paramCount(0), hasThis(false), hasTry(false), linkOnce(false), internal(false), replaceable(false), variadic(false), sourceLine(0), debugType(0), debugInline(false), debugOptimized(false), keepsFrame(false) {}
    int  newTemp(const Type* t) { tempTypes[nextTemp] = t; return nextTemp++; }
    int  newBlock(const std::string& label);
    void computeCfg();
    // temporaries live on entry to and on exit from every block, with the successors taken from the branches;
    // extraSuccs (optional, one list per block) adds edges no instruction names: the back end's exception edges
    // from a block whose calls may throw to the handlers of the table regions around them
    void computeLiveness(std::vector<std::set<int> >& liveIn, std::vector<std::set<int> >& liveOut,
                         const std::vector<std::vector<int> >* extraSuccs = 0) const;
    int  instCount() const;
    const Type* typeOf(int temp) const;
};

std::string printIr(const IrFunction& f);

} // namespace cppparse

#endif
