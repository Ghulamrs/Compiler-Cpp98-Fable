// IrGen.cpp - version 6 stage 4: IR generation from the analysed syntax tree. See IrGen.hpp.
#include "IrGen.hpp"
#include "Mangle.hpp"
#include "PathUtil.hpp"

#include <cstdlib>

namespace cppparse {

using sema::Symbol;
using sema::Scope;

namespace {

std::string numText(long v) { return intToString(static_cast<int>(v)); }

bool isReference(const sema::Type* t) { return t && t->kind == sema::Type::Reference; }

} // namespace

IrGen::IrGen(sema::Program& program, sema::TypeTable& semaTypes)
    : m_prog(program), m_semaTypes(semaTypes), m_types(m_emptyCb), m_outList(0), m_generated(0), m_out(0), m_fn(0), m_block(0), m_sretSlot(-1), m_tryCount(0) {}

IrGen::~IrGen()
{
    for (size_t i = 0; i < m_ownedClassInfos.size(); ++i) delete m_ownedClassInfos[i];
}

std::string IrGen::irName(const Symbol* fn)
{
    if (fn->isExternC || (fn->name == "main" && !fn->ownerClass && fn->scope && fn->scope->kind == Scope::Global)) return fn->name;
    std::string s = fn->qualifiedName() + "(";
    if (fn->type && fn->type->kind == sema::Type::Function) {
        for (size_t i = 0; i < fn->type->params.size(); ++i) s += (i ? ", " : "") + fn->type->params[i]->text();
        if (fn->type->variadic) s += fn->type->params.empty() ? "..." : ", ...";
    }
    s += ")";
    if (fn->type && fn->type->constMethod) s += " const";
    return s;
}

const ClassInfo* IrGen::classInfo(Symbol* cls)
{
    std::map<Symbol*, ClassInfo*>::iterator it = m_classInfos.find(cls);
    if (it != m_classInfos.end()) return it->second;
    ClassInfo* c = new ClassInfo;
    m_ownedClassInfos.push_back(c);
    m_classInfos[cls] = c;
    c->name = cls->name;
    c->qualifiedName = cls->qualifiedName();
    c->kind = cls->isUnion ? ClassInfo::Union : cls->isStruct ? ClassInfo::Struct : ClassInfo::Class;
    c->isDefinition = cls->isDefinition;
    for (size_t b = 0; b < cls->bases.size(); ++b) {
        BaseClass bc;
        bc.name = cls->bases[b].cls ? cls->bases[b].cls->qualifiedName() : "";
        bc.resolved = cls->bases[b].cls ? const_cast<ClassInfo*>(classInfo(cls->bases[b].cls)) : 0;
        c->bases.push_back(bc);
    }
    return c;
}

const Type* IrGen::oldType(const sema::Type* t)
{
    if (!t) return m_types.builtin(Type::Long);
    std::map<const sema::Type*, const Type*>::iterator it = m_typeCache.find(t);
    if (it != m_typeCache.end()) return it->second;
    const Type* r;
    switch (t->kind) {
    case sema::Type::Void: r = m_types.builtin(Type::Void, false, t->isConst); break;
    case sema::Type::Bool: r = m_types.builtin(Type::Bool, false, t->isConst); break;
    case sema::Type::Char: case sema::Type::SChar: r = m_types.builtin(Type::Char, false, t->isConst); break;
    case sema::Type::UChar: r = m_types.builtin(Type::Char, true, t->isConst); break;
    case sema::Type::WChar: case sema::Type::UShort: r = m_types.builtin(Type::Short, true, t->isConst); break;
    case sema::Type::Short: r = m_types.builtin(Type::Short, false, t->isConst); break;
    case sema::Type::Int: r = m_types.builtin(Type::Int, false, t->isConst); break;
    case sema::Type::UInt: r = m_types.builtin(Type::Int, true, t->isConst); break;
    case sema::Type::Long: r = m_types.builtin(Type::Long, false, t->isConst); break;
    case sema::Type::ULong: r = m_types.builtin(Type::Long, true, t->isConst); break;
    case sema::Type::LongLong: r = m_types.builtin(Type::LongLong, false, t->isConst); break;
    case sema::Type::ULongLong: r = m_types.builtin(Type::LongLong, true, t->isConst); break;
    case sema::Type::Float: r = m_types.builtin(Type::Float, false, t->isConst); break;
    case sema::Type::Double: case sema::Type::LongDouble: r = m_types.builtin(Type::Double, false, t->isConst); break;
    case sema::Type::NullPtr: r = m_types.nullType(); break;
    case sema::Type::Pointer: r = m_types.pointer(oldType(t->inner), t->isConst); break;
    case sema::Type::MemberPointer: r = m_types.pointer(m_types.builtin(Type::Void)); break;
    case sema::Type::Reference: r = m_types.reference(oldType(t->inner)); break;
    case sema::Type::Array: r = m_types.array(oldType(t->inner), t->arraySize > 0 ? static_cast<int>(t->arraySize) : 0); break;
    case sema::Type::Function: r = m_types.function(oldType(t->inner)); break;
    case sema::Type::Class: r = m_types.classType(t->decl ? classInfo(t->decl) : 0, t->isConst); break;
    case sema::Type::Enum: r = m_types.enumType(t->decl ? t->decl->qualifiedName() : std::string("enum"), t->isConst); break;
    default: r = m_types.unknown(t->text()); break;
    }
    m_typeCache[t] = r;
    return r;
}

bool IrGen::baseOffset(Symbol* derived, Symbol* base, int& offset, int depth) const
{
    if (!derived || !base || depth > 32) return false;
    if (derived == base) { offset = 0; return true; }
    for (size_t i = 0; i < derived->bases.size(); ++i) {
        int o = 0;
        if (baseOffset(derived->bases[i].cls, base, o, depth + 1)) { offset = derived->bases[i].offset + o; return true; }
    }
    return false;
}

std::string IrGen::throwDescriptorFor(const sema::Type* t) const
{
    t = m_semaTypes.unref(t);
    if (!t) return "unknown";
    if (t->kind != sema::Type::Class || !t->decl) return m_semaTypes.unqualified(t)->text();
    std::string d = t->decl->qualifiedName();
    std::vector<Symbol*> todo;
    todo.push_back(t->decl);
    for (size_t i = 0; i < todo.size() && i < 64; ++i)
        for (size_t b = 0; b < todo[i]->bases.size(); ++b)
            if (todo[i]->bases[b].cls) { d += ";" + todo[i]->bases[b].cls->qualifiedName(); todo.push_back(todo[i]->bases[b].cls); }
    return d;
}

const sema::Type* IrGen::exprType(cxx::Expr* e) const
{
    return e && e->semaType ? e->semaType : m_semaTypes.basic(sema::Type::Long);
}

// ---- emission helpers --------------------------------------------------------------------------

IrInst IrGen::make(IrInst::Op op, const sema::Type* type, int line)
{
    IrInst in;
    in.op = op;
    in.type = type ? oldType(type) : 0;
    in.line = line;
    return in;
}

bool IrGen::terminated() const
{
    const IrBlock& b = m_out->blocks[static_cast<size_t>(m_block)];
    return !b.insts.empty() && b.insts.back().isTerminator();
}

int IrGen::emit(IrInst inst)
{
    if (terminated()) m_block = m_out->newBlock("dead");
    bool producesValue = inst.op == IrInst::Const || inst.op == IrInst::FConst || inst.op == IrInst::StrConst || inst.op == IrInst::Copy ||
                         inst.op == IrInst::LoadLocal || inst.op == IrInst::AddrLocal || inst.op == IrInst::LoadGlobal || inst.op == IrInst::AddrGlobal ||
                         inst.op == IrInst::LoadMem || inst.op == IrInst::AddrField || inst.op == IrInst::AddrIndex || inst.op == IrInst::Bin ||
                         inst.op == IrInst::Un || inst.op == IrInst::Cmp || inst.op == IrInst::Conv || inst.op == IrInst::CatchParam ||
                         ((inst.op == IrInst::Call || inst.op == IrInst::VCall) && inst.type && !inst.type->isVoid());
    if (producesValue && inst.dst < 0) inst.dst = m_out->newTemp(inst.type);
    m_out->blocks[static_cast<size_t>(m_block)].insts.push_back(inst);
    return inst.dst;
}

void IrGen::br(int target, int line)
{
    if (terminated()) return;
    IrInst in = make(IrInst::Br, 0, line);
    in.args.push_back(IrOperand::block(target));
    emit(in);
}

void IrGen::condbr(int cond, int t, int f, int line)
{
    IrInst in = make(IrInst::CondBr, 0, line);
    in.args.push_back(IrOperand::temp(cond, m_types.boolType()));
    in.args.push_back(IrOperand::block(t));
    in.args.push_back(IrOperand::block(f));
    emit(in);
}

int IrGen::constant(long v, const sema::Type* t, int line)
{
    IrInst in = make(IrInst::Const, t, line);
    in.args.push_back(IrOperand::imm(v, in.type));
    return emit(in);
}

int IrGen::thisTemp(int line)
{
    IrInst in = make(IrInst::LoadLocal, 0, line);
    in.type = m_out->locals[0].type;
    in.args.push_back(IrOperand::local(0, m_out->locals[0].type));
    return emit(in);
}

int IrGen::newLocal(const std::string& name, const sema::Type* t, bool addressTaken)
{
    IrLocal l;
    l.name = name;
    l.type = oldType(t);
    l.size = isReference(t) ? 8 : sizeOf(t);
    l.align = isReference(t) ? 8 : alignOf(t);
    if (l.size <= 0) l.size = 8;
    if (l.align <= 0) l.align = 8;
    l.addressTaken = addressTaken;
    m_out->locals.push_back(l);
    return static_cast<int>(m_out->locals.size() - 1);
}

int IrGen::addressOf(const LValue& lv, int line)
{
    const sema::Type* u = lv.type;
    if (lv.kind == LValue::Local) {
        m_out->locals[static_cast<size_t>(lv.local)].addressTaken = true;
        IrInst in = make(IrInst::AddrLocal, m_semaTypes.pointer(u), line);
        in.args.push_back(IrOperand::local(lv.local, oldType(u)));
        return emit(in);
    }
    if (lv.kind == LValue::Global) {
        IrInst in = make(IrInst::AddrGlobal, m_semaTypes.pointer(u), line);
        in.args.push_back(IrOperand::global(lv.global, oldType(u)));
        return emit(in);
    }
    return lv.addr;
}

int IrGen::load(const LValue& lv, const sema::Type* t, int line)
{
    const sema::Type* u = m_semaTypes.unref(t);
    if (u && (u->kind == sema::Type::Class || u->kind == sema::Type::Array)) return addressOf(lv, line);
    if (lv.kind == LValue::Local) { IrInst in = make(IrInst::LoadLocal, u, line); in.args.push_back(IrOperand::local(lv.local, oldType(u))); return emit(in); }
    if (lv.kind == LValue::Global) { IrInst in = make(IrInst::LoadGlobal, u, line); in.args.push_back(IrOperand::global(lv.global, oldType(u))); return emit(in); }
    if (lv.kind == LValue::Mem) { IrInst in = make(IrInst::LoadMem, u, line); in.args.push_back(IrOperand::temp(lv.addr, oldType(m_semaTypes.pointer(u)))); return emit(in); }
    return constant(0, u, line);
}

void IrGen::store(const LValue& lv, int temp, const sema::Type* t, int line)
{
    const sema::Type* u = m_semaTypes.unref(t);
    if (u && (u->kind == sema::Type::Class || u->kind == sema::Type::Array)) {
        int dst = addressOf(lv, line);
        IrInst c = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
        c.sub = "__copy";
        c.args.push_back(IrOperand::temp(dst, oldType(m_semaTypes.pointer(u))));
        c.args.push_back(IrOperand::temp(temp, oldType(m_semaTypes.pointer(u))));
        c.args.push_back(IrOperand::imm(sizeOf(u), m_types.intType()));
        emit(c);
        return;
    }
    if (lv.kind == LValue::Local) { IrInst in = make(IrInst::StoreLocal, u, line); in.args.push_back(IrOperand::local(lv.local, oldType(u))); in.args.push_back(IrOperand::temp(temp, oldType(u))); emit(in); return; }
    if (lv.kind == LValue::Global) { IrInst in = make(IrInst::StoreGlobal, u, line); in.args.push_back(IrOperand::global(lv.global, oldType(u))); in.args.push_back(IrOperand::temp(temp, oldType(u))); emit(in); return; }
    if (lv.kind == LValue::Mem) { IrInst in = make(IrInst::StoreMem, u, line); in.args.push_back(IrOperand::temp(lv.addr, oldType(m_semaTypes.pointer(u)))); in.args.push_back(IrOperand::temp(temp, oldType(u))); emit(in); return; }
    m_notes.push_back("L" + numText(line) + ": store to a non-lvalue was dropped");
}

int IrGen::toBool(int temp, const sema::Type* t, int line)
{
    t = m_semaTypes.decay(t);
    if (!t || t->kind == sema::Type::Bool) return temp;
    IrInst in = make(IrInst::Cmp, m_semaTypes.basic(sema::Type::Bool), line);
    if (t->isFloating()) { in.sub = "fne"; in.args.push_back(IrOperand::temp(temp, oldType(t))); in.args.push_back(IrOperand::fimm(0.0, oldType(t))); }
    else { in.sub = "ne"; in.args.push_back(IrOperand::temp(temp, oldType(t))); in.args.push_back(IrOperand::imm(0, oldType(t))); }
    return emit(in);
}

int IrGen::convert(int temp, const sema::Type* from, const sema::Type* to, int line)
{
    from = m_semaTypes.decay(m_semaTypes.unqualified(m_semaTypes.unref(from)));
    to = m_semaTypes.unqualified(m_semaTypes.unref(to));
    if (!from || !to) return temp;
    from = m_semaTypes.unqualified(from);
    if (from == to) return temp;
    // pointer and reference conversions to a base class adjust the address
    if ((from->kind == sema::Type::Pointer && to->kind == sema::Type::Pointer) || (from->kind == sema::Type::Class && to->kind == sema::Type::Class)) {
        const sema::Type* fc = from->kind == sema::Type::Pointer ? m_semaTypes.unqualified(from->inner) : from;
        const sema::Type* tc = to->kind == sema::Type::Pointer ? m_semaTypes.unqualified(to->inner) : to;
        if (fc->kind == sema::Type::Class && tc->kind == sema::Type::Class && fc->decl != tc->decl) {
            int off = 0;
            if (baseOffset(fc->decl, tc->decl, off) && off > 0) {
                IrInst in = make(IrInst::AddrField, to->kind == sema::Type::Pointer ? to : m_semaTypes.pointer(to), line);
                in.sub = "base " + tc->decl->name;
                in.extra = off;
                in.args.push_back(IrOperand::temp(temp, oldType(from->kind == sema::Type::Pointer ? from : m_semaTypes.pointer(from))));
                return emit(in);
            }
        }
        return temp;
    }
    if (to->kind == sema::Type::Class || from->kind == sema::Type::Class || to->kind == sema::Type::Array || to->kind == sema::Type::Error || from->kind == sema::Type::Error) return temp;
    if (to->kind == sema::Type::Bool) return toBool(temp, from, line);
    std::string sub;
    if (from->isIntegral() && to->isIntegral()) {
        int fs = sizeOf(from), ts = sizeOf(to);
        if (fs == ts) return temp;
        sub = ts < fs ? "trunc" : (from->isUnsigned() ? "zext" : "sext");
    } else if (from->isIntegral() && to->isFloating()) sub = from->isUnsigned() && from->kind != sema::Type::Bool ? "uitofp" : "sitofp";
    else if (from->isFloating() && to->isIntegral()) sub = "fptosi";
    else if (from->isFloating() && to->isFloating()) sub = sizeOf(to) > sizeOf(from) ? "fpext" : "fptrunc";
    else if (from->isPointer() && to->isIntegral()) sub = "ptrtoint";
    else if (from->isIntegral() && to->isPointer()) sub = "inttoptr";
    else return temp;
    IrInst in = make(IrInst::Conv, to, line);
    in.sub = sub;
    in.args.push_back(IrOperand::temp(temp, oldType(from)));
    return emit(in);
}

int IrGen::binary(const std::string& op, int a, const sema::Type* ta, int b, const sema::Type* tb, const sema::Type* result, int line)
{
    ta = m_semaTypes.unqualified(m_semaTypes.decay(ta)); tb = m_semaTypes.unqualified(m_semaTypes.decay(tb)); result = m_semaTypes.unqualified(m_semaTypes.decay(result));
    bool cmp = op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=";
    const sema::Type* lng = m_semaTypes.basic(sema::Type::Long);
    if (!cmp && ta->kind == sema::Type::Pointer && tb->isIntegral() && (op == "+" || op == "-")) {
        int idx = convert(b, tb, lng, line);
        if (op == "-") { IrInst n = make(IrInst::Un, lng, line); n.sub = "neg"; n.args.push_back(IrOperand::temp(idx, oldType(lng))); idx = emit(n); }
        IrInst in = make(IrInst::AddrIndex, ta, line);
        in.extra = sizeOf(ta->inner) > 0 ? sizeOf(ta->inner) : 1;
        in.args.push_back(IrOperand::temp(a, oldType(ta)));
        in.args.push_back(IrOperand::temp(idx, oldType(lng)));
        return emit(in);
    }
    if (!cmp && op == "+" && ta->isIntegral() && tb->kind == sema::Type::Pointer) return binary(op, b, tb, a, ta, result, line);
    if (!cmp && op == "-" && ta->kind == sema::Type::Pointer && tb->kind == sema::Type::Pointer) {
        IrInst d = make(IrInst::Bin, lng, line); d.sub = "sub"; d.args.push_back(IrOperand::temp(a, oldType(lng))); d.args.push_back(IrOperand::temp(b, oldType(lng)));
        int diff = emit(d);
        int es = sizeOf(ta->inner) > 0 ? sizeOf(ta->inner) : 1;
        IrInst q = make(IrInst::Bin, lng, line); q.sub = "sdiv"; q.args.push_back(IrOperand::temp(diff, oldType(lng))); q.args.push_back(IrOperand::imm(es, oldType(lng)));
        return emit(q);
    }
    const sema::Type* common = result;
    if (cmp) {
        if (ta->isArithmetic() && tb->isArithmetic()) common = m_semaTypes.arithmeticResult(ta, tb);
        else if (ta->isPointer() || ta->kind == sema::Type::NullPtr) common = ta->kind == sema::Type::NullPtr ? tb : ta;
        else common = tb;
        if (common->kind == sema::Type::NullPtr) common = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
    } else if (!common->isArithmetic()) common = m_semaTypes.arithmeticResult(ta, tb);
    a = convert(a, ta, common, line);
    b = convert(b, tb, common, line);
    bool fl = common->isFloating();
    bool uns = common->isUnsigned() || common->isPointer();
    std::string sub;
    if (cmp) {
        if (op == "==") sub = fl ? "feq" : "eq";
        else if (op == "!=") sub = fl ? "fne" : "ne";
        else if (op == "<") sub = fl ? "flt" : (uns ? "ult" : "slt");
        else if (op == "<=") sub = fl ? "fle" : (uns ? "ule" : "sle");
        else if (op == ">") sub = fl ? "fgt" : (uns ? "ugt" : "sgt");
        else sub = fl ? "fge" : (uns ? "uge" : "sge");
        IrInst in = make(IrInst::Cmp, m_semaTypes.basic(sema::Type::Bool), line);
        in.sub = sub;
        in.args.push_back(IrOperand::temp(a, oldType(common)));
        in.args.push_back(IrOperand::temp(b, oldType(common)));
        return emit(in);
    }
    if (op == "+") sub = fl ? "fadd" : "add";
    else if (op == "-") sub = fl ? "fsub" : "sub";
    else if (op == "*") sub = fl ? "fmul" : "mul";
    else if (op == "/") sub = fl ? "fdiv" : (uns ? "udiv" : "sdiv");
    else if (op == "%") sub = uns ? "urem" : "srem";
    else if (op == "<<") sub = "shl";
    else if (op == ">>") sub = uns ? "lshr" : "ashr";
    else if (op == "&") sub = "and";
    else if (op == "|") sub = "or";
    else if (op == "^") sub = "xor";
    else sub = "add";
    IrInst in = make(IrInst::Bin, common, line);
    in.sub = sub;
    in.args.push_back(IrOperand::temp(a, oldType(common)));
    in.args.push_back(IrOperand::temp(b, oldType(common)));
    int r = emit(in);
    return convert(r, common, result, line);
}

void IrGen::leaveTryRegions(size_t depth, int line)
{
    for (size_t k = m_tryStack.size(); k > depth; --k) {
        if (m_tryStack[k - 1] < 0) { emit(make(IrInst::CatchEnd, 0, line)); continue; }
        IrInst te = make(IrInst::TryEnd, 0, line);
        te.extra = m_tryStack[k - 1];
        emit(te);
    }
}

// ---- lvalues and values ----------------------------------------------------------------------------

IrGen::LValue IrGen::lvalue(cxx::Expr* e)
{
    LValue lv;
    lv.type = m_semaTypes.unref(exprType(e));
    const int line = e->loc.line;
    switch (e->kind) {
    case cxx::Expr::Name: {
        Symbol* s = e->semaSym;
        if (!s || s->kind != Symbol::Variable) break;
        std::map<Symbol*, int>::iterator it = m_slots.find(s);
        if (it != m_slots.end()) {
            int idx = it->second;
            const sema::Type* declared = s->type;
            if (isReference(declared)) {
                IrInst in = make(IrInst::LoadLocal, m_semaTypes.pointer(declared->inner), line);
                in.args.push_back(IrOperand::local(idx, oldType(declared)));
                lv.kind = LValue::Mem; lv.addr = emit(in); lv.type = m_semaTypes.unref(declared);
                return lv;
            }
            lv.kind = LValue::Local; lv.local = idx; lv.type = declared;
            return lv;
        }
        if (s->isMember && !s->isStatic) {
            if (!m_out->hasThis) break;
            int self = thisTemp(line);
            int off = s->offset;
            if (m_fn && m_fn->ownerClass && s->ownerClass && m_fn->ownerClass != s->ownerClass) { int bo = 0; if (baseOffset(m_fn->ownerClass, s->ownerClass, bo)) off += bo; }
            IrInst in = make(IrInst::AddrField, m_semaTypes.pointer(lv.type), line);
            in.sub = s->name;
            in.extra = off;
            in.args.push_back(IrOperand::temp(self, m_out->locals[0].type));
            if (isReference(s->type)) { IrInst ld = make(IrInst::LoadMem, m_semaTypes.pointer(s->type->inner), line); ld.args.push_back(IrOperand::temp(emit(in), oldType(m_semaTypes.pointer(s->type)))); lv.kind = LValue::Mem; lv.addr = emit(ld); lv.type = s->type->inner; return lv; }
            lv.kind = LValue::Mem; lv.addr = emit(in);
            return lv;
        }
        lv.kind = LValue::Global; lv.global = s->qualifiedName();
        if (isReference(s->type)) { IrInst ld = make(IrInst::LoadGlobal, m_semaTypes.pointer(s->type->inner), line); ld.args.push_back(IrOperand::global(lv.global, oldType(s->type))); lv.kind = LValue::Mem; lv.addr = emit(ld); lv.type = s->type->inner; }
        return lv;
    }
    case cxx::Expr::Member: {
        Symbol* s = e->semaSym;
        if (!s || s->kind != Symbol::Variable) break;
        if (s->isStatic) { lv.kind = LValue::Global; lv.global = s->qualifiedName(); return lv; }
        cxx::Expr* obj = e->kids[0];
        const sema::Type* objT = m_semaTypes.unref(exprType(obj));
        int base;
        if (e->arrowMember) {
            base = value(obj);
            if (objT->kind == sema::Type::Pointer) objT = objT->inner;
            else if (obj->semaSym && obj->semaSym->kind == Symbol::Function && obj->semaSym->type) objT = m_semaTypes.unref(obj->semaSym->type->inner);
            if (objT->kind == sema::Type::Pointer) objT = objT->inner;
        } else {
            LValue ol = lvalue(obj);
            base = ol.kind == LValue::None ? value(obj) : load(ol, objT, line);
        }
        objT = m_semaTypes.unqualified(objT);
        int off = s->offset;
        if (objT->kind == sema::Type::Class && objT->decl && s->ownerClass && objT->decl != s->ownerClass) { int bo = 0; if (baseOffset(objT->decl, s->ownerClass, bo)) off += bo; }
        IrInst in = make(IrInst::AddrField, m_semaTypes.pointer(lv.type), line);
        in.sub = s->name;
        in.extra = off;
        in.args.push_back(IrOperand::temp(base, oldType(m_semaTypes.pointer(objT))));
        lv.kind = LValue::Mem; lv.addr = emit(in);
        if (isReference(s->type)) { IrInst ld = make(IrInst::LoadMem, m_semaTypes.pointer(s->type->inner), line); ld.args.push_back(IrOperand::temp(lv.addr, oldType(m_semaTypes.pointer(s->type)))); lv.addr = emit(ld); lv.type = s->type->inner; }
        return lv;
    }
    case cxx::Expr::Index: {
        if (e->semaSym) { lv.kind = LValue::Mem; lv.addr = value(e); return lv; }      // operator[] returning a reference
        cxx::Expr* base = e->kids[0];
        cxx::Expr* index = e->kids[1];
        const sema::Type* bt = m_semaTypes.unref(exprType(base));
        if (bt->kind != sema::Type::Pointer && bt->kind != sema::Type::Array) { std::swap(base, index); bt = m_semaTypes.unref(exprType(base)); }
        int basePtr;
        if (bt->kind == sema::Type::Array) { LValue bl = lvalue(base); basePtr = bl.kind == LValue::None ? value(base) : load(bl, bt, line); }
        else basePtr = value(base);
        const sema::Type* lng = m_semaTypes.basic(sema::Type::Long);
        int idx = convert(value(index), exprType(index), lng, line);
        IrInst in = make(IrInst::AddrIndex, m_semaTypes.pointer(lv.type), line);
        in.extra = sizeOf(lv.type) > 0 ? sizeOf(lv.type) : 1;
        in.args.push_back(IrOperand::temp(basePtr, oldType(m_semaTypes.pointer(lv.type))));
        in.args.push_back(IrOperand::temp(idx, oldType(lng)));
        lv.kind = LValue::Mem; lv.addr = emit(in);
        return lv;
    }
    case cxx::Expr::Unary:
        if (e->text == "*" && !e->semaSym) { lv.kind = LValue::Mem; lv.addr = value(e->kids[0]); return lv; }
        if ((e->text == "++" || e->text == "--") && !e->semaSym) { value(e); return lvalue(e->kids[0]); }
        break;
    case cxx::Expr::Paren: return lvalue(e->kids[0]);
    case cxx::Expr::Cast: case cxx::Expr::NamedCast:
        if (!e->kids.empty() && e->semaLvalue) { LValue inner = lvalue(e->kids[0]); if (inner.kind != LValue::None) { lv.kind = LValue::Mem; lv.addr = convert(addressOf(inner, line), m_semaTypes.pointer(inner.type), m_semaTypes.pointer(lv.type), line); } return lv; }
        break;
    case cxx::Expr::Call: case cxx::Expr::FunctionalCast: case cxx::Expr::Binary: case cxx::Expr::PostIncDec:
        if (e->semaLvalue || isClassType(exprType(e))) { lv.kind = LValue::Mem; lv.addr = value(e); return lv; }
        break;
    case cxx::Expr::Assign:
        if (e->semaSym) { lv.kind = LValue::Mem; lv.addr = value(e); return lv; }
        value(e);
        return lvalue(e->kids[0]);
    case cxx::Expr::Comma:
        value(e->kids[0]);
        return lvalue(e->kids[1]);
    case cxx::Expr::Cond:
        if (e->semaLvalue || isClassType(exprType(e))) { lv.kind = LValue::Mem; lv.addr = conditional(e, true); return lv; }
        break;
    case cxx::Expr::StringLit:
        lv.kind = LValue::Mem; lv.addr = value(e); return lv;
    default:
        break;
    }
    return lv;
}

// c ? a : b through a temporary: the chosen operand's address when byAddr (class objects, lvalue
// results), else its converted value
int IrGen::conditional(cxx::Expr* e, bool byAddr)
{
    const int line = e->loc.line;
    const sema::Type* ct = m_semaTypes.unqualified(m_semaTypes.unref(exprType(e)));
    int slot = newLocal("tmp.cond", byAddr ? m_semaTypes.pointer(ct) : ct, false);
    LValue lv; lv.kind = LValue::Local; lv.local = slot; lv.type = byAddr ? m_semaTypes.pointer(ct) : ct;
    int tb = m_out->newBlock("cond.true"), fb = m_out->newBlock("cond.false"), join = m_out->newBlock("cond.join");
    condition(e->kids[0], tb, fb);
    for (int k = 0; k < 2; ++k) {
        m_block = k == 0 ? tb : fb;
        cxx::Expr* arm = e->kids[static_cast<size_t>(k + 1)];
        int v;
        if (byAddr) { LValue al = lvalue(arm); v = al.kind == LValue::None ? value(arm) : addressOf(al, line); }
        else v = convert(value(arm), exprType(arm), ct, line);
        store(lv, v, lv.type, line);
        br(join, line);
    }
    m_block = join;
    return load(lv, lv.type, line);
}

void IrGen::condition(cxx::Expr* e, int trueBlock, int falseBlock)
{
    if (e->kind == cxx::Expr::Paren) { condition(e->kids[0], trueBlock, falseBlock); return; }
    if (e->kind == cxx::Expr::Binary && e->text == "&&" && !e->semaSym) {
        int mid = m_out->newBlock("and.rhs");
        condition(e->kids[0], mid, falseBlock);
        m_block = mid;
        condition(e->kids[1], trueBlock, falseBlock);
        return;
    }
    if (e->kind == cxx::Expr::Binary && e->text == "||" && !e->semaSym) {
        int mid = m_out->newBlock("or.rhs");
        condition(e->kids[0], trueBlock, mid);
        m_block = mid;
        condition(e->kids[1], trueBlock, falseBlock);
        return;
    }
    if (e->kind == cxx::Expr::Unary && e->text == "!" && !e->semaSym) { condition(e->kids[0], falseBlock, trueBlock); return; }
    int v = toBool(value(e), exprType(e), e->loc.line);
    condbr(v, trueBlock, falseBlock, e->loc.line);
}

int IrGen::argument(cxx::Expr* a, const sema::Type* pt, int line)
{
    if (pt && isReference(pt)) {
        const sema::Type* target = m_semaTypes.unref(pt);
        LValue al = lvalue(a);
        if (al.kind == LValue::None || (a->semaSym && a->semaSym->kind == Symbol::Function && a->kind == cxx::Expr::Call && !a->semaLvalue && !isClassType(exprType(a)))) {
            // bind a temporary
            int tv = value(a);
            if (isClassType(exprType(a)) && al.kind == LValue::None) return convert(tv, m_semaTypes.pointer(m_semaTypes.unref(exprType(a))), m_semaTypes.pointer(target), line);
            int slot = newLocal("tmp.arg", m_semaTypes.unqualified(target), true);
            LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = m_semaTypes.unqualified(target);
            if (!isClassType(target)) store(tl, convert(tv, exprType(a), target, line), target, line);
            else { IrInst c = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line); c.sub = "__copy"; c.args.push_back(IrOperand::temp(addressOf(tl, line), oldType(m_semaTypes.pointer(target)))); c.args.push_back(IrOperand::temp(tv, oldType(m_semaTypes.pointer(target)))); c.args.push_back(IrOperand::imm(sizeOf(target), m_types.intType())); emit(c); }
            return addressOf(tl, line);
        }
        int addr = addressOf(al, line);
        return convert(addr, m_semaTypes.pointer(al.type), m_semaTypes.pointer(target), line);
    }
    int v = value(a);
    if (pt && isClassType(pt)) {
        // by value: a bytewise copy of the object (the copy constructor is a version 6 gap)
        const sema::Type* ct = m_semaTypes.unqualified(m_semaTypes.unref(pt));
        int slot = newLocal("tmp.arg", ct, true);
        LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = ct;
        store(tl, convert(v, m_semaTypes.pointer(m_semaTypes.unref(exprType(a))), m_semaTypes.pointer(ct), line), ct, line);
        return addressOf(tl, line);
    }
    if (pt) v = convert(v, exprType(a), pt, line);
    return v;
}

int IrGen::callFunction(Symbol* fn, int thisPtr, int sretAddr, const std::vector<cxx::Expr*>& args, size_t firstArg, bool virtualCall, int line)
{
    need(fn);
    const sema::Type* ft = fn->type;
    const sema::Type* rt = ft && ft->kind == sema::Type::Function ? ft->inner : m_semaTypes.basic(sema::Type::Void);
    bool sret = isClassType(rt) && !isReference(rt);
    IrInst in = make(IrInst::Call, sret ? m_semaTypes.basic(sema::Type::Void) : rt, line);
    in.sub = irName(fn);
    if (thisPtr >= 0) in.args.push_back(IrOperand::temp(thisPtr, oldType(m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass)))));
    if (sret) {
        if (sretAddr < 0) { int slot = newLocal("tmp.ret", m_semaTypes.unqualified(rt), true); LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = m_semaTypes.unqualified(rt); sretAddr = addressOf(tl, line); }
        in.args.push_back(IrOperand::temp(sretAddr, oldType(m_semaTypes.pointer(rt))));
    }
    for (size_t a = firstArg; a < args.size(); ++a) {
        size_t p = a - firstArg;
        const sema::Type* pt = ft && p < ft->params.size() ? ft->params[p] : 0;
        int v = argument(args[a], pt, line);
        in.args.push_back(IrOperand::temp(v, oldType(pt ? (isReference(pt) ? m_semaTypes.pointer(m_semaTypes.unref(pt)) : (isClassType(pt) ? m_semaTypes.pointer(pt) : pt)) : m_semaTypes.decay(exprType(args[a])))));
    }
    // default arguments
    if (ft) for (size_t p = args.size() - firstArg; p < ft->params.size() && p < fn->params.size(); ++p) {
        cxx::Expr* def = fn->params[p]->init;
        if (!def) break;
        std::vector<cxx::Expr*> one; one.push_back(def);
        int v = argument(def, ft->params[p], line);
        in.args.push_back(IrOperand::temp(v, oldType(isReference(ft->params[p]) ? m_semaTypes.pointer(m_semaTypes.unref(ft->params[p])) : ft->params[p])));
    }
    if (virtualCall && fn->isVirtual && fn->vtableSlot >= 0 && thisPtr >= 0) { in.op = IrInst::VCall; in.extra = fn->vtableSlot; }
    if (isReference(rt)) in.type = oldType(m_semaTypes.pointer(rt->inner));
    int r = emit(in);
    return sret ? sretAddr : r;
}

int IrGen::construct(Symbol* ctor, int objAddr, const std::vector<cxx::Expr*>& args, size_t firstArg, int line)
{
    if (!ctor) return objAddr;
    callFunction(ctor, objAddr, -1, args, firstArg, false, line);
    return objAddr;
}

void IrGen::destroy(const sema::Type* t, int objAddr, int line, bool viaVtable)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (u->kind != sema::Type::Class || !u->decl || !u->decl->members) return;
    Symbol* dtor = 0;
    const std::vector<Symbol*>* v = u->decl->members->find("~" + u->decl->ctorName());
    if (v) for (size_t i = 0; i < v->size(); ++i) if ((*v)[i]->kind == Symbol::Function) dtor = (*v)[i];
    if (!dtor) return;
    if (dtor->isImplicit && !dtor->isVirtual) {
        // an implicit destructor only matters when a member or base has one
        bool work = false;
        for (size_t f = 0; f < u->decl->fields.size() && !work; ++f) if (isClassType(u->decl->fields[f]->type)) work = true;
        for (size_t b = 0; b < u->decl->bases.size() && !work; ++b) if (u->decl->bases[b].cls && (u->decl->bases[b].cls->hasDtor || u->decl->bases[b].cls->isPolymorphic)) work = true;
        if (!work) return;
    }
    std::vector<cxx::Expr*> none;
    callFunction(dtor, objAddr, -1, none, 0, viaVtable && dtor->isVirtual, line);
}

int IrGen::call(cxx::Expr* e)
{
    const int line = e->loc.line;
    cxx::Expr* callee = e->kids[0];
    Symbol* fn = e->semaSym;
    const sema::Type* rt = exprType(e);
    if (!fn || fn->kind != Symbol::Function) {
        // function pointer or undeclared function
        IrInst in = make(IrInst::Call, rt, line);
        const sema::Type* ct = m_semaTypes.unref(exprType(callee));
        if (ct->kind == sema::Type::Pointer && ct->inner->kind == sema::Type::Function) {
            int fp = value(callee);
            in.args.push_back(IrOperand::temp(fp, oldType(ct)));
            in.sub = "(indirect)";
        } else {
            std::string name = callee->kind == cxx::Expr::Name ? callee->name.last() : callee->kind == cxx::Expr::Member ? callee->name.last() : "?";
            if (callee->kind == cxx::Expr::Member) { cxx::Expr* obj = callee->kids[0]; in.args.push_back(IrOperand::temp(callee->arrowMember ? value(obj) : addressOf(lvalue(obj), line), oldType(m_semaTypes.decay(exprType(obj))))); }
            in.sub = name + "?";
        }
        for (size_t a = 1; a < e->kids.size(); ++a) { int v = value(e->kids[a]); in.args.push_back(IrOperand::temp(v, oldType(m_semaTypes.decay(exprType(e->kids[a]))))); }
        if (!rt || rt->kind == sema::Type::Error || rt->kind == sema::Type::Dependent) in.type = m_types.builtin(Type::Long);
        return emit(in);
    }
    int thisPtr = -1;
    bool virtualCall = false;
    if (fn->isConstructor) {
        // T(args) as a call: construct a temporary
        int slot = newLocal("tmp." + fn->ownerClass->name, m_semaTypes.classType(fn->ownerClass), true);
        LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = m_semaTypes.classType(fn->ownerClass);
        return construct(fn, addressOf(tl, line), e->kids, 1, line);
    }
    if (callee->kind == cxx::Expr::Member && fn->ownerClass && !fn->isStatic) {
        cxx::Expr* obj = callee->kids[0];
        const sema::Type* objT = m_semaTypes.unref(exprType(obj));
        if (callee->arrowMember) {
            thisPtr = value(obj);
            if (objT->kind == sema::Type::Class && obj->semaSym && obj->semaSym->kind == Symbol::Function) objT = m_semaTypes.unref(obj->semaSym->type->inner);
            if (objT->kind == sema::Type::Pointer) objT = objT->inner;
        } else { LValue ol = lvalue(obj); thisPtr = ol.kind == LValue::None ? value(obj) : load(ol, objT, line); }
        objT = m_semaTypes.unqualified(objT);
        if (objT->kind == sema::Type::Class && objT->decl && objT->decl != fn->ownerClass) thisPtr = convert(thisPtr, m_semaTypes.pointer(objT), m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass)), line);
        virtualCall = callee->name.parts.size() == 1;
    } else if (callee->kind != cxx::Expr::Member && fn->ownerClass && !fn->isStatic && m_out->hasThis) {
        thisPtr = thisTemp(line);
        if (m_fn->ownerClass != fn->ownerClass) thisPtr = convert(thisPtr, m_semaTypes.pointer(m_semaTypes.classType(m_fn->ownerClass)), m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass)), line);
        virtualCall = callee->kind == cxx::Expr::Name && callee->name.parts.size() == 1;
    } else if (callee->kind != cxx::Expr::Name && callee->kind != cxx::Expr::Member && fn->ownerClass && !fn->isStatic) {
        // operator() on an object
        thisPtr = addressOf(lvalue(callee), line);
    }
    size_t firstArg = 1;
    if (fn->isOperator && fn->name == "operator()" && callee->kind == cxx::Expr::Name && thisPtr < 0) { LValue ol = lvalue(callee); thisPtr = ol.kind == LValue::None ? value(callee) : addressOf(ol, line); }
    return callFunction(fn, thisPtr, -1, e->kids, firstArg, virtualCall, line);
}

int IrGen::value(cxx::Expr* e)
{
    if (!e) return -1;
    const int line = e->loc.line;
    const sema::Type* t = m_semaTypes.unref(exprType(e));
    if (e->semaConst && e->kind != cxx::Expr::Call && e->kind != cxx::Expr::Assign && (t->isIntegral() || t->kind == sema::Type::NullPtr || t->isPointer()) && e->kind != cxx::Expr::StringLit)
        return constant(e->semaValue, t->kind == sema::Type::NullPtr ? m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void)) : t, line);

    switch (e->kind) {
    case cxx::Expr::IntLit: case cxx::Expr::CharLit: case cxx::Expr::BoolLit: return constant(e->semaValue, t, line);
    case cxx::Expr::FloatLit: { IrInst in = make(IrInst::FConst, t, line); in.args.push_back(IrOperand::fimm(strtod(e->text.c_str(), 0), oldType(t))); return emit(in); }
    case cxx::Expr::StringLit: {
        std::string text = e->text;
        for (size_t p = text.find("\" \""); p != std::string::npos; p = text.find("\" \"")) text.erase(p, 3);   // adjacent literals
        IrInst in = make(IrInst::StrConst, 0, line); in.type = m_types.charPtr(); in.args.push_back(IrOperand::str(text, m_types.charPtr())); return emit(in);
    }
    case cxx::Expr::This: return m_out->hasThis ? thisTemp(line) : constant(0, t, line);
    case cxx::Expr::Name: {
        Symbol* s = e->semaSym;
        if (s && (s->kind == Symbol::Function || s->kind == Symbol::FunctionTemplate)) { need(s); IrInst in = make(IrInst::AddrGlobal, 0, line); in.type = m_types.function(oldType(s->type ? s->type->inner : 0)); in.args.push_back(IrOperand::global(irName(s), in.type)); return emit(in); }
        if (s && s->kind == Symbol::Enumerator) return constant(s->constValue, t, line);
        LValue lv = lvalue(e);
        if (lv.kind == LValue::None) { m_notes.push_back("L" + numText(line) + ": unresolved name " + e->name.text() + " read as 0"); return constant(0, t, line); }
        return load(lv, exprType(e), line);
    }
    case cxx::Expr::Member: {
        Symbol* s = e->semaSym;
        if (s && s->kind == Symbol::Enumerator) return constant(s->constValue, t, line);
        LValue lv = lvalue(e);
        if (lv.kind == LValue::None) { if (!e->kids.empty()) value(e->kids[0]); m_notes.push_back("L" + numText(line) + ": unresolved member " + e->name.text() + " read as 0"); return constant(0, t, line); }
        return load(lv, exprType(e), line);
    }
    case cxx::Expr::Index: {
        if (e->semaSym && e->semaSym->kind == Symbol::Function) {
            Symbol* fn = e->semaSym;
            std::vector<cxx::Expr*> args; args.push_back(e->kids[1]);
            int thisPtr = fn->ownerClass ? addressOf(lvalue(e->kids[0]), line) : -1;
            if (!fn->ownerClass) args.insert(args.begin(), e->kids[0]);
            int r = callFunction(fn, thisPtr, -1, args, 0, false, line);
            if (isReference(fn->type->inner)) { LValue lv; lv.kind = LValue::Mem; lv.addr = r; lv.type = m_semaTypes.unref(fn->type->inner); return load(lv, lv.type, line); }
            return r;
        }
        LValue lv = lvalue(e);
        if (lv.kind == LValue::None) return constant(0, t, line);
        return load(lv, exprType(e), line);
    }
    case cxx::Expr::Call: return call(e);
    case cxx::Expr::FunctionalCast: {
        const sema::Type* to = exprType(e);
        if (isClassType(to) && e->semaSym && e->semaSym->isConstructor) {
            Symbol* ctor = e->semaSym;
            int slot = newLocal("tmp." + ctor->ownerClass->name, m_semaTypes.unqualified(to), true);
            LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = m_semaTypes.unqualified(to);
            return construct(ctor, addressOf(tl, line), e->kids, 0, line);
        }
        if (e->semaSym && e->semaSym->isConversion) { std::vector<cxx::Expr*> none; int obj = addressOf(lvalue(e->kids[0]), line); return callFunction(e->semaSym, obj, -1, none, 0, true, line); }
        if (e->kids.empty()) return constant(0, to, line);
        return convert(value(e->kids[0]), exprType(e->kids[0]), to, line);
    }
    case cxx::Expr::Cast: case cxx::Expr::NamedCast: {
        if (e->kids.empty()) return constant(0, t, line);
        if (e->semaSym && e->semaSym->kind == Symbol::Function && e->semaSym->isConversion) { std::vector<cxx::Expr*> none; int obj = addressOf(lvalue(e->kids[0]), line); return callFunction(e->semaSym, obj, -1, none, 0, true, line); }
        if (e->semaSym && e->semaSym->kind == Symbol::Function && e->semaSym->isConstructor) {
            int slot = newLocal("tmp." + e->semaSym->ownerClass->name, m_semaTypes.unqualified(t), true);
            LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = m_semaTypes.unqualified(t);
            return construct(e->semaSym, addressOf(tl, line), e->kids, 0, line);
        }
        if (isReference(exprType(e))) { LValue inner = lvalue(e->kids[0]); if (inner.kind != LValue::None) return convert(addressOf(inner, line), m_semaTypes.pointer(inner.type), m_semaTypes.pointer(t), line); }
        return convert(value(e->kids[0]), exprType(e->kids[0]), t, line);
    }
    case cxx::Expr::Paren: return value(e->kids[0]);
    case cxx::Expr::Unary: {
        const std::string& op = e->text;
        if (e->semaSym && e->semaSym->kind == Symbol::Function) {
            Symbol* fn = e->semaSym;
            std::vector<cxx::Expr*> args;
            int thisPtr = -1;
            if (fn->ownerClass) thisPtr = addressOf(lvalue(e->kids[0]), line); else args.push_back(e->kids[0]);
            return callFunction(fn, thisPtr, -1, args, 0, true, line);
        }
        if (op == "&") {
            cxx::Expr* o = e->kids[0];
            if (o->semaSym && (o->semaSym->kind == Symbol::Function || o->semaSym->kind == Symbol::FunctionTemplate)) return value(o);
            LValue lv = lvalue(o);
            if (lv.kind == LValue::None) return constant(0, t, line);
            return addressOf(lv, line);
        }
        if (op == "*") { LValue lv; lv.kind = LValue::Mem; lv.addr = value(e->kids[0]); lv.type = t; return load(lv, t, line); }
        if (op == "++" || op == "--") {
            LValue lv = lvalue(e->kids[0]);
            if (lv.kind == LValue::None) return constant(0, t, line);
            const sema::Type* ot = lv.type;
            int cur = load(lv, ot, line);
            int one = constant(1, ot->kind == sema::Type::Pointer ? m_semaTypes.basic(sema::Type::Int) : ot, line);
            int nv = binary(op == "++" ? "+" : "-", cur, ot, one, ot->kind == sema::Type::Pointer ? m_semaTypes.basic(sema::Type::Int) : ot, ot, line);
            store(lv, nv, ot, line);
            return nv;
        }
        int v = value(e->kids[0]);
        const sema::Type* ot = m_semaTypes.decay(exprType(e->kids[0]));
        if (op == "!") { int b = toBool(v, ot, line); IrInst in = make(IrInst::Cmp, m_semaTypes.basic(sema::Type::Bool), line); in.sub = "eq"; in.args.push_back(IrOperand::temp(b, m_types.boolType())); in.args.push_back(IrOperand::imm(0, m_types.boolType())); return emit(in); }
        if (op == "+") return convert(v, ot, t, line);
        v = convert(v, ot, t, line);
        IrInst in = make(IrInst::Un, t, line);
        in.sub = op == "-" ? (t->isFloating() ? "fneg" : "neg") : "not";
        in.args.push_back(IrOperand::temp(v, oldType(t)));
        return emit(in);
    }
    case cxx::Expr::PostIncDec: {
        if (e->semaSym && e->semaSym->kind == Symbol::Function) {
            Symbol* fn = e->semaSym;
            std::vector<cxx::Expr*> args;
            int thisPtr = fn->ownerClass ? addressOf(lvalue(e->kids[0]), line) : -1;
            if (!fn->ownerClass) args.push_back(e->kids[0]);
            return callFunction(fn, thisPtr, -1, args, 0, true, line);
        }
        LValue lv = lvalue(e->kids[0]);
        if (lv.kind == LValue::None) return constant(0, t, line);
        const sema::Type* ot = lv.type;
        int cur = load(lv, ot, line);
        int one = constant(1, ot->kind == sema::Type::Pointer ? m_semaTypes.basic(sema::Type::Int) : ot, line);
        int nv = binary(e->text == "++" ? "+" : "-", cur, ot, one, ot->kind == sema::Type::Pointer ? m_semaTypes.basic(sema::Type::Int) : ot, ot, line);
        store(lv, nv, ot, line);
        return cur;
    }
    case cxx::Expr::Binary: {
        const std::string& op = e->text;
        if (e->semaSym && e->semaSym->kind == Symbol::Function) {
            Symbol* fn = e->semaSym;
            std::vector<cxx::Expr*> args;
            int thisPtr = -1;
            if (fn->ownerClass) { thisPtr = addressOf(lvalue(e->kids[0]), line); args.push_back(e->kids[1]); }
            else { args.push_back(e->kids[0]); args.push_back(e->kids[1]); }
            return callFunction(fn, thisPtr, -1, args, 0, true, line);
        }
        if (op == "&&" || op == "||") {
            int slot = newLocal("tmp.logic", m_semaTypes.basic(sema::Type::Bool), false);
            LValue lv; lv.kind = LValue::Local; lv.local = slot; lv.type = m_semaTypes.basic(sema::Type::Bool);
            int tb = m_out->newBlock("logic.true"), fb = m_out->newBlock("logic.false"), join = m_out->newBlock("logic.join");
            condition(e, tb, fb);
            m_block = tb; store(lv, constant(1, lv.type, line), lv.type, line); br(join, line);
            m_block = fb; store(lv, constant(0, lv.type, line), lv.type, line); br(join, line);
            m_block = join;
            return load(lv, lv.type, line);
        }
        int a = value(e->kids[0]);
        int b = value(e->kids[1]);
        return binary(op, a, exprType(e->kids[0]), b, exprType(e->kids[1]), t, line);
    }
    case cxx::Expr::Assign: {
        const std::string& op = e->text;
        if (e->semaSym && e->semaSym->kind == Symbol::Function) {
            Symbol* fn = e->semaSym;
            if (fn->isImplicit && fn->name == "operator=") {
                LValue lv = lvalue(e->kids[0]);
                int src = value(e->kids[1]);
                if (lv.kind != LValue::None) store(lv, convert(src, m_semaTypes.pointer(m_semaTypes.unref(exprType(e->kids[1]))), m_semaTypes.pointer(lv.type), line), lv.type, line);
                return lv.kind == LValue::None ? src : addressOf(lv, line);
            }
            std::vector<cxx::Expr*> args;
            int thisPtr = -1;
            if (fn->ownerClass) { thisPtr = addressOf(lvalue(e->kids[0]), line); args.push_back(e->kids[1]); }
            else { args.push_back(e->kids[0]); args.push_back(e->kids[1]); }
            return callFunction(fn, thisPtr, -1, args, 0, true, line);
        }
        LValue lv = lvalue(e->kids[0]);
        if (lv.kind == LValue::None) { value(e->kids[1]); m_notes.push_back("L" + numText(line) + ": assignment to a non-lvalue dropped"); return constant(0, t, line); }
        const sema::Type* lt = lv.type;
        int rv;
        if (op == "=") rv = convert(value(e->kids[1]), exprType(e->kids[1]), lt, line);
        else {
            int cur = load(lv, lt, line);
            int r = value(e->kids[1]);
            rv = binary(op.substr(0, op.size() - 1), cur, lt, r, exprType(e->kids[1]), lt, line);
        }
        store(lv, rv, lt, line);
        return rv;
    }
    case cxx::Expr::Cond: {
        const sema::Type* ct = m_semaTypes.unqualified(t);
        int r = conditional(e, isClassType(ct) || e->semaLvalue);
        if (e->semaLvalue && !isClassType(ct)) { LValue m; m.kind = LValue::Mem; m.addr = r; m.type = ct; return load(m, ct, line); }
        return r;
    }
    case cxx::Expr::Comma: value(e->kids[0]); return value(e->kids[1]);
    case cxx::Expr::SizeofExpr: case cxx::Expr::SizeofType: return constant(e->semaConst ? e->semaValue : 8, t, line);
    case cxx::Expr::New: {
        const sema::Type* pt = exprType(e);
        const sema::Type* elem = pt->kind == sema::Type::Pointer ? pt->inner : pt;
        bool isArray = e->type && e->type->decl && !e->type->decl->parts.empty() && e->type->decl->parts.back().kind == cxx::DeclaratorPart::Array;
        IrInst n = make(IrInst::Call, pt, line);
        if (isArray) {
            cxx::Expr* countExpr = e->type->decl->parts.back().arraySize;
            n.sub = "__new_array";
            n.args.push_back(IrOperand::imm(sizeOf(elem), m_types.intType()));
            int count = countExpr ? convert(value(countExpr), exprType(countExpr), m_semaTypes.basic(sema::Type::Long), line) : constant(1, m_semaTypes.basic(sema::Type::Long), line);
            n.args.push_back(IrOperand::temp(count, m_types.builtin(Type::Long)));
            return emit(n);
        }
        n.sub = "__new";
        n.args.push_back(IrOperand::imm(sizeOf(elem) > 0 ? sizeOf(elem) : 1, m_types.intType()));
        int p = emit(n);
        if (e->semaSym && e->semaSym->isConstructor) construct(e->semaSym, p, e->kids, 0, line);
        else if (!e->kids.empty()) { LValue lv; lv.kind = LValue::Mem; lv.addr = p; lv.type = elem; store(lv, convert(value(e->kids[0]), exprType(e->kids[0]), elem, line), elem, line); }
        return p;
    }
    case cxx::Expr::Delete: {
        int p = value(e->kids[0]);
        const sema::Type* pt = m_semaTypes.decay(exprType(e->kids[0]));
        if (!e->arrayDelete && pt->kind == sema::Type::Pointer) destroy(pt->inner, p, line, true);
        IrInst in = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
        in.sub = e->arrayDelete ? "__delete_array" : "__delete";
        in.args.push_back(IrOperand::temp(p, oldType(pt)));
        emit(in);
        return constant(0, m_semaTypes.basic(sema::Type::Int), line);
    }
    case cxx::Expr::Throw: {
        IrInst in = make(IrInst::Throw, m_semaTypes.basic(sema::Type::Void), line);
        if (!e->kids.empty()) {
            int v = value(e->kids[0]);
            const sema::Type* tt = m_semaTypes.unqualified(m_semaTypes.decay(exprType(e->kids[0])));
            in.args.push_back(IrOperand::temp(v, oldType(tt)));
            in.sub = throwDescriptorFor(tt);
            in.extra = isClassType(tt) ? sizeOf(tt) : 8;
        }
        emit(in);
        return constant(0, m_semaTypes.basic(sema::Type::Int), line);
    }
    case cxx::Expr::Typeid: case cxx::Expr::Error: return constant(0, t, line);
    }
    return constant(0, t, line);
}

// ---- statements -----------------------------------------------------------------------------------------

void IrGen::initVariable(int slot, const sema::Type* t, Symbol* var, cxx::InitDeclarator& id, int line)
{
    LValue lv; lv.kind = LValue::Local; lv.local = slot; lv.type = m_semaTypes.unref(t);
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (isReference(t)) {
        if (id.init.args.empty()) return;
        cxx::Expr* initE = id.init.args[0];
        LValue src = lvalue(initE);
        int addr = src.kind == LValue::None ? value(initE) : addressOf(src, line);
        addr = convert(addr, m_semaTypes.pointer(src.kind == LValue::None ? m_semaTypes.unref(exprType(initE)) : src.type), m_semaTypes.pointer(u), line);
        IrInst st = make(IrInst::StoreLocal, t, line); st.args.push_back(IrOperand::local(slot, oldType(t))); st.args.push_back(IrOperand::temp(addr, oldType(m_semaTypes.pointer(u)))); emit(st);
        return;
    }
    if (u->kind == sema::Type::Class && u->decl) {
        int addr = addressOf(lv, line);
        if (id.init.kind == cxx::Initializer::Assign && id.init.args.size() == 1) {
            cxx::Expr* src = id.init.args[0];
            if (src->semaSym && src->semaSym->kind == Symbol::Function && src->semaSym->isConstructor) { construct(src->semaSym, addr, id.init.args, 0, line); return; }
            int sv = value(src);
            if (isClassType(exprType(src))) { LValue dst = lv; store(dst, convert(sv, m_semaTypes.pointer(m_semaTypes.unref(exprType(src))), m_semaTypes.pointer(u), line), u, line); return; }
            if (var->initCtor) { construct(var->initCtor, addr, id.init.args, 0, line); return; }
            return;
        }
        if (var->initCtor) { construct(var->initCtor, addr, id.init.args, 0, line); return; }
        if (id.init.kind == cxx::Initializer::None) {
            // default construction
            const std::vector<Symbol*>* ctors = u->decl->members ? u->decl->members->find(u->decl->ctorName()) : 0;
            if (ctors) for (size_t i = 0; i < ctors->size(); ++i) if ((*ctors)[i]->kind == Symbol::Function && (*ctors)[i]->isConstructor && (*ctors)[i]->requiredParams == 0) { std::vector<cxx::Expr*> none; construct((*ctors)[i], addr, none, 0, line); break; }
            return;
        }
        if (id.init.kind == cxx::Initializer::Brace) {
            for (size_t f = 0; f < id.init.args.size() && f < u->decl->fields.size(); ++f) {
                Symbol* fld = u->decl->fields[f];
                if (id.init.args[f]->kind == cxx::Expr::Error) continue;
                IrInst fa = make(IrInst::AddrField, m_semaTypes.pointer(fld->type), line); fa.sub = fld->name; fa.extra = fld->offset; fa.args.push_back(IrOperand::temp(addr, oldType(m_semaTypes.pointer(u))));
                LValue fl; fl.kind = LValue::Mem; fl.addr = emit(fa); fl.type = fld->type;
                store(fl, convert(value(id.init.args[f]), exprType(id.init.args[f]), fld->type, line), fld->type, line);
            }
        }
        return;
    }
    if (u->kind == sema::Type::Array) {
        if (id.init.kind != cxx::Initializer::Brace) return;
        int addr = addressOf(lv, line);
        const sema::Type* elem = u->inner;
        for (size_t i = 0; i < id.init.args.size(); ++i) {
            if (id.init.args[i]->kind == cxx::Expr::Error) continue;
            IrInst ai = make(IrInst::AddrIndex, m_semaTypes.pointer(elem), line); ai.extra = sizeOf(elem); ai.args.push_back(IrOperand::temp(addr, oldType(m_semaTypes.pointer(elem)))); ai.args.push_back(IrOperand::imm(static_cast<long>(i), m_types.builtin(Type::Long)));
            LValue el; el.kind = LValue::Mem; el.addr = emit(ai); el.type = elem;
            store(el, convert(value(id.init.args[i]), exprType(id.init.args[i]), elem, line), elem, line);
        }
        return;
    }
    if (id.init.args.empty()) return;
    cxx::Expr* src = id.init.args[0];
    int v;
    if (src->semaSym && src->semaSym->kind == Symbol::Function && src->semaSym->isConversion) { std::vector<cxx::Expr*> none; v = callFunction(src->semaSym, addressOf(lvalue(src), line), -1, none, 0, true, line); v = convert(v, src->semaSym->type->inner, u, line); }
    else v = convert(value(src), exprType(src), u, line);
    store(lv, v, u, line);
}

void IrGen::localDecl(cxx::Decl* d)
{
    if (d->kind != cxx::Decl::Simple) return;
    for (size_t i = 0; i < d->declarators.size(); ++i) {
        cxx::InitDeclarator& id = d->declarators[i];
        Symbol* var = id.decl ? id.decl->semaSym : 0;
        if (!var || var->kind != Symbol::Variable) continue;
        if (var->isStatic) { m_notes.push_back("L" + numText(d->loc.line) + ": static local '" + var->name + "' treated as an ordinary local"); }
        int slot = newLocal(var->name, var->type, false);
        m_slots[var] = slot;
        initVariable(slot, var->type, var, id, id.decl->loc.line);
    }
}

void IrGen::stmt(cxx::Stmt* s)
{
    if (!s) return;
    const int line = s->loc.line;
    switch (s->kind) {
    case cxx::Stmt::Compound: for (size_t i = 0; i < s->kids.size(); ++i) stmt(s->kids[i]); return;
    case cxx::Stmt::Empty: case cxx::Stmt::Asm: case cxx::Stmt::Error: return;
    case cxx::Stmt::ExprStmt: value(s->expr); return;
    case cxx::Stmt::DeclStmt: localDecl(s->decl); return;
    case cxx::Stmt::If: {
        int tb = m_out->newBlock("if.then"), eb = s->kids.size() > 1 ? m_out->newBlock("if.else") : -1, join = m_out->newBlock("if.end");
        if (s->decl) { localDecl(s->decl); Symbol* v = s->decl->declarators[0].decl->semaSym; LValue lv; lv.kind = LValue::Local; lv.local = m_slots[v]; lv.type = v->type; condbr(toBool(load(lv, v->type, line), v->type, line), tb, eb >= 0 ? eb : join, line); }
        else condition(s->expr, tb, eb >= 0 ? eb : join);
        m_block = tb; stmt(s->kids[0]); br(join, line);
        if (eb >= 0) { m_block = eb; stmt(s->kids[1]); br(join, line); }
        m_block = join;
        return;
    }
    case cxx::Stmt::While: {
        int cb = m_out->newBlock("while.cond"), bb = m_out->newBlock("while.body"), xb = m_out->newBlock("while.end");
        br(cb, line);
        m_block = cb; condition(s->expr, bb, xb);
        LoopTargets lt; lt.breakBlock = xb; lt.continueBlock = cb; lt.tryDepth = m_tryStack.size(); m_loops.push_back(lt);
        m_block = bb; stmt(s->kids[0]); br(cb, line);
        m_loops.pop_back();
        m_block = xb;
        return;
    }
    case cxx::Stmt::DoWhile: {
        int bb = m_out->newBlock("do.body"), cb = m_out->newBlock("do.cond"), xb = m_out->newBlock("do.end");
        br(bb, line);
        LoopTargets lt; lt.breakBlock = xb; lt.continueBlock = cb; lt.tryDepth = m_tryStack.size(); m_loops.push_back(lt);
        m_block = bb; stmt(s->kids[0]); br(cb, line);
        m_loops.pop_back();
        m_block = cb; condition(s->expr, bb, xb);
        m_block = xb;
        return;
    }
    case cxx::Stmt::For: {
        if (s->init) stmt(s->init);
        int cb = m_out->newBlock("for.cond"), bb = m_out->newBlock("for.body"), sb = m_out->newBlock("for.step"), xb = m_out->newBlock("for.end");
        br(cb, line);
        m_block = cb;
        if (s->decl) { localDecl(s->decl); Symbol* v = s->decl->declarators[0].decl->semaSym; LValue lv; lv.kind = LValue::Local; lv.local = m_slots[v]; lv.type = v->type; condbr(toBool(load(lv, v->type, line), v->type, line), bb, xb, line); }
        else if (s->expr) condition(s->expr, bb, xb); else br(bb, line);
        LoopTargets lt; lt.breakBlock = xb; lt.continueBlock = sb; lt.tryDepth = m_tryStack.size(); m_loops.push_back(lt);
        m_block = bb; stmt(s->kids[0]); br(sb, line);
        m_loops.pop_back();
        m_block = sb; if (s->expr2) value(s->expr2); br(cb, line);
        m_block = xb;
        return;
    }
    case cxx::Stmt::Switch: {
        int v = value(s->expr);
        const sema::Type* vt = m_semaTypes.unqualified(m_semaTypes.decay(exprType(s->expr)));
        if (!vt->isIntegral()) vt = m_semaTypes.basic(sema::Type::Int);
        cxx::Stmt* body = s->kids[0];
        std::vector<cxx::Stmt*> items;
        if (body->kind == cxx::Stmt::Compound) items = body->kids; else items.push_back(body);
        // nested labels: "case 1: case 2: stmt" are chains of Case statements
        std::vector<cxx::Stmt*> flat;
        for (size_t i = 0; i < items.size(); ++i) {
            cxx::Stmt* it = items[i];
            while (it && (it->kind == cxx::Stmt::Case || it->kind == cxx::Stmt::Default)) { flat.push_back(it); it = it->kids.empty() ? 0 : it->kids[0]; }
            if (it) flat.push_back(it);
        }
        int xb = m_out->newBlock("switch.end");
        std::map<const cxx::Stmt*, int> labelBlock;
        int defaultBlock = -1;
        for (size_t i = 0; i < flat.size(); ++i) {
            if (flat[i]->kind == cxx::Stmt::Case) labelBlock[flat[i]] = m_out->newBlock("case");
            else if (flat[i]->kind == cxx::Stmt::Default) { defaultBlock = m_out->newBlock("default"); labelBlock[flat[i]] = defaultBlock; }
        }
        for (size_t i = 0; i < flat.size(); ++i) {
            if (flat[i]->kind != cxx::Stmt::Case) continue;
            int cv = constant(flat[i]->expr->semaValue, vt, flat[i]->loc.line);
            IrInst c = make(IrInst::Cmp, m_semaTypes.basic(sema::Type::Bool), flat[i]->loc.line); c.sub = "eq";
            c.args.push_back(IrOperand::temp(convert(v, exprType(s->expr), vt, line), oldType(vt))); c.args.push_back(IrOperand::temp(cv, oldType(vt)));
            int cond = emit(c);
            int next = m_out->newBlock("switch.test");
            condbr(cond, labelBlock[flat[i]], next, flat[i]->loc.line);
            m_block = next;
        }
        br(defaultBlock >= 0 ? defaultBlock : xb, line);
        LoopTargets lt; lt.breakBlock = xb; lt.continueBlock = -1; lt.tryDepth = m_tryStack.size(); m_loops.push_back(lt);
        bool inLabel = false;
        for (size_t i = 0; i < flat.size(); ++i) {
            if (flat[i]->kind == cxx::Stmt::Case || flat[i]->kind == cxx::Stmt::Default) {
                int b = labelBlock[flat[i]];
                if (inLabel) br(b, flat[i]->loc.line);
                m_block = b; inLabel = true;
                continue;
            }
            if (!inLabel) { m_block = m_out->newBlock("switch.dead"); inLabel = true; }
            stmt(flat[i]);
        }
        br(xb, line);
        m_loops.pop_back();
        m_block = xb;
        return;
    }
    case cxx::Stmt::Case: case cxx::Stmt::Default: for (size_t i = 0; i < s->kids.size(); ++i) stmt(s->kids[i]); return;
    case cxx::Stmt::Break: for (size_t i = m_loops.size(); i > 0; --i) { leaveTryRegions(m_loops[i - 1].tryDepth, line); br(m_loops[i - 1].breakBlock, line); return; } return;
    case cxx::Stmt::Continue: for (size_t i = m_loops.size(); i > 0; --i) if (m_loops[i - 1].continueBlock >= 0) { leaveTryRegions(m_loops[i - 1].tryDepth, line); br(m_loops[i - 1].continueBlock, line); return; } return;
    case cxx::Stmt::Return: {
        IrInst in = make(IrInst::Ret, 0, line);
        const sema::Type* rt = m_fn->type ? m_fn->type->inner : m_semaTypes.basic(sema::Type::Void);
        if (s->expr) {
            if (isReference(rt)) {
                LValue lv = lvalue(s->expr);
                int addr = lv.kind == LValue::None ? value(s->expr) : addressOf(lv, line);
                addr = convert(addr, m_semaTypes.pointer(lv.kind == LValue::None ? m_semaTypes.unref(exprType(s->expr)) : lv.type), m_semaTypes.pointer(m_semaTypes.unref(rt)), line);
                in.args.push_back(IrOperand::temp(addr, oldType(m_semaTypes.pointer(rt->inner))));
            } else if (isClassType(rt)) {
                // copy into the hidden result object
                int src = value(s->expr);
                IrInst ld = make(IrInst::LoadLocal, m_semaTypes.pointer(rt), line); ld.args.push_back(IrOperand::local(m_sretSlot, oldType(m_semaTypes.pointer(rt))));
                int dst = emit(ld);
                LValue dl; dl.kind = LValue::Mem; dl.addr = dst; dl.type = m_semaTypes.unqualified(rt);
                store(dl, convert(src, m_semaTypes.pointer(m_semaTypes.unref(exprType(s->expr))), m_semaTypes.pointer(dl.type), line), dl.type, line);
            } else if (!rt->isVoid()) {
                int v = convert(value(s->expr), exprType(s->expr), rt, line);
                in.args.push_back(IrOperand::temp(v, oldType(m_semaTypes.unqualified(rt))));
            } else value(s->expr);
        }
        if (m_fn->isDestructor) destructorCleanup(m_fn);
        leaveTryRegions(0, line);
        emit(in);
        return;
    }
    case cxx::Stmt::Goto: case cxx::Stmt::Label: m_notes.push_back("L" + numText(line) + ": goto/label not supported"); for (size_t i = 0; i < s->kids.size(); ++i) stmt(s->kids[i]); return;
    case cxx::Stmt::Try: {
        m_out->hasTry = true;
        int id = m_tryCount++;
        std::vector<int> handlers;
        for (size_t i = 1; i < s->kids.size(); ++i) handlers.push_back(m_out->newBlock("catch"));
        int join = m_out->newBlock("try.end");
        int body = m_out->newBlock("try.body");
        IrInst tb = make(IrInst::TryBegin, 0, line);
        tb.extra = id;
        for (size_t i = 0; i < handlers.size(); ++i) tb.args.push_back(IrOperand::block(handlers[i]));
        emit(tb);
        br(body, line);
        m_block = body;
        m_tryStack.push_back(id);
        stmt(s->kids[0]);
        m_tryStack.pop_back();
        if (!terminated()) { IrInst te = make(IrInst::TryEnd, 0, line); te.extra = id; emit(te); }
        br(join, line);
        for (size_t i = 1; i < s->kids.size(); ++i) {
            cxx::Stmt* h = s->kids[i];
            m_block = handlers[i - 1];
            IrInst cp = make(IrInst::CatchParam, m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void)), h->loc.line);
            Symbol* var = 0;
            if (h->decl && !h->decl->declarators.empty() && h->decl->declarators[0].decl) var = h->decl->declarators[0].decl->semaSym;
            const sema::Type* ct = var ? m_semaTypes.unqualified(m_semaTypes.unref(var->type)) : 0;
            if (h->catchAll) cp.sub = "...";
            else if (ct && ct->kind == sema::Type::Class && ct->decl) cp.sub = ct->decl->qualifiedName();
            else if (ct) cp.sub = ct->text();
            else if (h->decl) cp.sub = h->decl->spec.text();
            int ev = emit(cp);
            if (var) {
                if (ct->kind != sema::Type::Class && !isReference(var->type)) {
                    int slot = newLocal(var->name, var->type, false);
                    m_slots[var] = slot;
                    IrInst ld = make(IrInst::LoadMem, ct, h->loc.line); ld.args.push_back(IrOperand::temp(ev, oldType(m_semaTypes.pointer(ct))));
                    int v = emit(ld);
                    LValue lv; lv.kind = LValue::Local; lv.local = slot; lv.type = ct;
                    store(lv, v, ct, h->loc.line);
                } else {
                    // the handler variable refers to the runtime's copy
                    const sema::Type* refT = m_semaTypes.reference(ct);
                    int slot = newLocal(var->name, refT, false);
                    m_slots[var] = slot;
                    var->type = refT;
                    IrInst st = make(IrInst::StoreLocal, refT, h->loc.line); st.args.push_back(IrOperand::local(slot, oldType(refT))); st.args.push_back(IrOperand::temp(ev, oldType(m_semaTypes.pointer(ct)))); emit(st);
                }
            }
            m_tryStack.push_back(-1);
            stmt(h->kids[0]);
            m_tryStack.pop_back();
            if (!terminated()) emit(make(IrInst::CatchEnd, 0, h->loc.line));
            br(join, h->loc.line);
        }
        m_block = join;
        return;
    }
    case cxx::Stmt::Handler: return;
    }
}

// ---- functions -----------------------------------------------------------------------------------------------

void IrGen::need(Symbol* fn)
{
    if (!fn || fn->kind != Symbol::Function || m_done.count(fn)) return;
    if (fn->isImplicit || (fn->hasBody && fn->definition)) m_queue.push_back(fn);
}

void IrGen::storeVptr(Symbol* cls, int objAddr, int line)
{
    if (!cls->isPolymorphic) return;
    const sema::Type* vp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
    IrInst vt = make(IrInst::AddrGlobal, vp, line);
    vt.args.push_back(IrOperand::global("vtable@" + cls->qualifiedName(), oldType(m_semaTypes.basic(sema::Type::Void))));
    int vtp = emit(vt);
    IrInst st = make(IrInst::StoreMem, vp, line);
    st.sub = "vptr";
    st.args.push_back(IrOperand::temp(objAddr, oldType(m_semaTypes.pointer(m_semaTypes.classType(cls)))));
    st.args.push_back(IrOperand::temp(vtp, oldType(vp)));
    emit(st);
}

void IrGen::prologue(Symbol* fn)
{
    m_out->name = irName(fn);
    m_out->fn = 0;
    const sema::Type* rt = fn->type && fn->type->kind == sema::Type::Function ? fn->type->inner : m_semaTypes.basic(sema::Type::Void);
    bool sret = isClassType(rt) && !isReference(rt);
    m_out->retType = oldType(sret ? m_semaTypes.basic(sema::Type::Void) : (isReference(rt) ? m_semaTypes.pointer(rt->inner) : rt));
    m_out->hasThis = fn->ownerClass && !fn->isStatic;
    if (m_out->hasThis) {
        const sema::Type* ct = m_semaTypes.classType(fn->ownerClass);
        if (fn->type && fn->type->constMethod) ct = m_semaTypes.qualified(ct, true, false);
        int slot = newLocal("this", m_semaTypes.pointer(ct), false);
        m_out->locals[static_cast<size_t>(slot)].isParam = true;
        m_out->locals[static_cast<size_t>(slot)].isThis = true;
    }
    m_sretSlot = -1;
    if (sret) { m_sretSlot = newLocal("sret", m_semaTypes.pointer(rt), false); m_out->locals[static_cast<size_t>(m_sretSlot)].isParam = true; }
    for (size_t p = 0; p < fn->params.size(); ++p) {
        Symbol* ps = fn->params[p];
        const sema::Type* pt = ps->type;
        if (isClassType(pt) && !isReference(pt)) pt = m_semaTypes.pointer(pt);   // objects arrive by address
        int slot = newLocal(ps->name.empty() ? "arg" + numText(static_cast<long>(p)) : ps->name, pt, false);
        m_out->locals[static_cast<size_t>(slot)].isParam = true;
        m_slots[ps] = slot;
        if (isClassType(ps->type) && !isReference(ps->type)) ps->type = m_semaTypes.reference(ps->type);   // reads go through the pointer
    }
    m_out->paramCount = static_cast<int>(m_out->locals.size());
    m_out->newBlock("entry");
    m_block = 0;
}

void IrGen::constructorInit(Symbol* fn, cxx::Decl* def)
{
    Symbol* cls = fn->ownerClass;
    if (!cls) return;
    const int line = fn->loc.line;
    int self = thisTemp(line);
    std::set<Symbol*> initialised;
    // bases named in the initializer list, in declaration order of the bases
    for (size_t b = 0; b < cls->bases.size(); ++b) {
        Symbol* base = cls->bases[b].cls;
        if (!base) continue;
        cxx::MemInit* mi = 0;
        if (def) for (size_t i = 0; i < def->memInits.size(); ++i) {
            const std::string& n = def->memInits[i].name.last();
            if (n == base->name || n == base->ctorName() || def->memInits[i].name.text() == base->qualifiedName()) mi = &def->memInits[i];
        }
        int baddr = self;
        if (cls->bases[b].offset > 0) { IrInst adj = make(IrInst::AddrField, m_semaTypes.pointer(m_semaTypes.classType(base)), line); adj.sub = "base " + base->name; adj.extra = cls->bases[b].offset; adj.args.push_back(IrOperand::temp(self, m_out->locals[0].type)); baddr = emit(adj); }
        const std::vector<Symbol*>* ctors = base->members ? base->members->find(base->ctorName()) : 0;
        Symbol* chosen = 0;
        size_t nargs = mi ? mi->args.size() : 0;
        if (ctors) for (size_t i = 0; i < ctors->size() && !chosen; ++i) { Symbol* c = (*ctors)[i]; if (c->kind == Symbol::Function && c->isConstructor && nargs >= static_cast<size_t>(c->requiredParams) && nargs <= c->params.size()) chosen = c; }
        if (chosen) { std::vector<cxx::Expr*> none; construct(chosen, baddr, mi ? mi->args : none, 0, mi ? mi->loc.line : line); }
    }
    storeVptr(cls, self, line);
    // members in declaration order
    for (size_t f = 0; f < cls->fields.size(); ++f) {
        Symbol* fld = cls->fields[f];
        cxx::MemInit* mi = 0;
        if (def) for (size_t i = 0; i < def->memInits.size(); ++i) if (def->memInits[i].name.parts.size() == 1 && def->memInits[i].name.last() == fld->name) mi = &def->memInits[i];
        IrInst fa = make(IrInst::AddrField, m_semaTypes.pointer(m_semaTypes.unref(fld->type)), line); fa.sub = fld->name; fa.extra = fld->offset; fa.args.push_back(IrOperand::temp(self, m_out->locals[0].type));
        const sema::Type* ft = fld->type;
        if (isReference(ft)) {
            if (!mi || mi->args.empty()) continue;
            int faddr = emit(fa);
            LValue src = lvalue(mi->args[0]);
            int addr = src.kind == LValue::None ? value(mi->args[0]) : addressOf(src, mi->loc.line);
            IrInst st = make(IrInst::StoreMem, m_semaTypes.pointer(ft->inner), mi->loc.line); st.args.push_back(IrOperand::temp(faddr, oldType(m_semaTypes.pointer(ft)))); st.args.push_back(IrOperand::temp(addr, oldType(m_semaTypes.pointer(ft->inner)))); emit(st);
            continue;
        }
        const sema::Type* fu = m_semaTypes.unqualified(ft);
        if (fu->kind == sema::Type::Class && fu->decl) {
            int faddr = emit(fa);
            const std::vector<Symbol*>* ctors = fu->decl->members ? fu->decl->members->find(fu->decl->ctorName()) : 0;
            size_t nargs = mi ? mi->args.size() : 0;
            if (mi && nargs == 1 && isClassType(exprType(mi->args[0])) && m_semaTypes.unqualified(m_semaTypes.unref(exprType(mi->args[0]))) == fu) {
                // copy from an object of the same class
                int sv = value(mi->args[0]);
                LValue dl; dl.kind = LValue::Mem; dl.addr = faddr; dl.type = fu;
                store(dl, sv, fu, mi->loc.line);
                continue;
            }
            Symbol* chosen = 0;
            if (ctors) for (size_t i = 0; i < ctors->size() && !chosen; ++i) { Symbol* c = (*ctors)[i]; if (c->kind == Symbol::Function && c->isConstructor && nargs >= static_cast<size_t>(c->requiredParams) && nargs <= c->params.size()) chosen = c; }
            if (chosen) { std::vector<cxx::Expr*> none; construct(chosen, faddr, mi ? mi->args : none, 0, mi ? mi->loc.line : line); }
            continue;
        }
        if (!mi || mi->args.empty()) continue;
        int faddr = emit(fa);
        LValue fl; fl.kind = LValue::Mem; fl.addr = faddr; fl.type = fu;
        store(fl, convert(value(mi->args[0]), exprType(mi->args[0]), fu, mi->loc.line), fu, mi->loc.line);
    }
}

void IrGen::destructorCleanup(Symbol* fn)
{
    Symbol* cls = fn->ownerClass;
    if (!cls) return;
    const int line = fn->loc.line;
    int self = thisTemp(line);
    for (size_t f = cls->fields.size(); f > 0; --f) {
        Symbol* fld = cls->fields[f - 1];
        if (!isClassType(fld->type) || isReference(fld->type)) continue;
        IrInst fa = make(IrInst::AddrField, m_semaTypes.pointer(fld->type), line); fa.sub = fld->name; fa.extra = fld->offset; fa.args.push_back(IrOperand::temp(self, m_out->locals[0].type));
        destroy(fld->type, emit(fa), line, false);
    }
    for (size_t b = cls->bases.size(); b > 0; --b) {
        Symbol* base = cls->bases[b - 1].cls;
        if (!base) continue;
        int baddr = self;
        if (cls->bases[b - 1].offset > 0) { IrInst adj = make(IrInst::AddrField, m_semaTypes.pointer(m_semaTypes.classType(base)), line); adj.sub = "base " + base->name; adj.extra = cls->bases[b - 1].offset; adj.args.push_back(IrOperand::temp(self, m_out->locals[0].type)); baddr = emit(adj); }
        destroy(m_semaTypes.classType(base), baddr, line, false);
    }
}

void IrGen::genImplicit(Symbol* fn)
{
    m_out = new IrFunction;
    m_fn = fn;
    m_slots.clear(); m_loops.clear(); m_tryStack.clear(); m_tryCount = 0;
    prologue(fn);
    const int line = fn->loc.line;
    Symbol* cls = fn->ownerClass;
    if (fn->isConstructor && fn->params.empty()) constructorInit(fn, 0);
    else if (fn->isConstructor) {
        // copy constructor: bytewise copy, then the vptr
        int self = thisTemp(line);
        IrInst ld = make(IrInst::LoadLocal, 0, line); ld.type = m_out->locals[1].type; ld.args.push_back(IrOperand::local(1, m_out->locals[1].type));
        int src = emit(ld);
        IrInst c = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line); c.sub = "__copy";
        c.args.push_back(IrOperand::temp(self, m_out->locals[0].type)); c.args.push_back(IrOperand::temp(src, m_out->locals[1].type)); c.args.push_back(IrOperand::imm(cls->size, m_types.intType()));
        emit(c);
        storeVptr(cls, self, line);
    } else if (fn->isDestructor) {
        destructorCleanup(fn);
    } else {
        // copy assignment: bytewise copy, result this
        int self = thisTemp(line);
        IrInst ld = make(IrInst::LoadLocal, 0, line); ld.type = m_out->locals[1].type; ld.args.push_back(IrOperand::local(1, m_out->locals[1].type));
        int src = emit(ld);
        IrInst c = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line); c.sub = "__copy";
        c.args.push_back(IrOperand::temp(self, m_out->locals[0].type)); c.args.push_back(IrOperand::temp(src, m_out->locals[1].type)); c.args.push_back(IrOperand::imm(cls->size, m_types.intType()));
        emit(c);
        storeVptr(cls, self, line);
        IrInst r = make(IrInst::Ret, 0, line);
        r.args.push_back(IrOperand::temp(thisTemp(line), m_out->locals[0].type));
        emit(r);
    }
    if (!terminated()) emit(make(IrInst::Ret, 0, line));
    m_out->computeCfg();
    m_outList->push_back(m_out);
    ++m_generated;
}

void IrGen::genFunction(Symbol* fn)
{
    cxx::Decl* def = fn->definition;
    if (!def || !def->body) return;
    m_out = new IrFunction;
    m_fn = fn;
    m_slots.clear(); m_loops.clear(); m_tryStack.clear(); m_tryCount = 0;
    prologue(fn);
    if (fn->isConstructor) constructorInit(fn, def);
    stmt(def->body);
    if (!terminated()) {
        if (fn->isDestructor) destructorCleanup(fn);
        IrInst r = make(IrInst::Ret, 0, def->loc.line);
        const sema::Type* rt = fn->type ? fn->type->inner : 0;
        if (rt && !rt->isVoid() && !isClassType(rt) && !isReference(rt)) {
            if (fn->name == "main") r.args.push_back(IrOperand::temp(constant(0, rt, def->loc.line), oldType(rt)));
            else r.args.push_back(IrOperand::temp(constant(0, m_semaTypes.unqualified(rt), def->loc.line), oldType(m_semaTypes.unqualified(rt))));
        }
        emit(r);
    }
    m_out->computeCfg();
    m_outList->push_back(m_out);
    ++m_generated;
}

void IrGen::generate(std::vector<IrFunction*>& out)
{
    m_outList = &out;
    for (size_t i = 0; i < m_prog.symbols.size(); ++i) {
        Symbol* s = m_prog.symbols[i];
        if (s->kind == Symbol::Function && s->hasBody && s->definition && !s->isDependentContext && !(s->ownerClass && s->ownerClass->isDependentContext)) m_queue.push_back(s);
    }
    for (size_t i = 0; i < m_queue.size(); ++i) {
        Symbol* fn = m_queue[i];
        if (m_done.count(fn)) continue;
        m_done.insert(fn);
        if (fn->isImplicit) genImplicit(fn);
        else genFunction(fn);
    }
}

void IrGen::registerMetadata(X86Module& x86, bool msvc)
{
    for (size_t i = 0; i < m_prog.symbols.size(); ++i) {
        Symbol* s = m_prog.symbols[i];
        if (s->kind == Symbol::Class && s->isDefinition && s->isPolymorphic && !s->isDependentContext && s->layoutDone) {
            IrVtable v;
            v.className = s->qualifiedName();
            for (size_t k = 0; k < s->vtable.size(); ++k) {
                Symbol* m = s->vtable[k];
                v.entries.push_back(m->isPureVirtual ? std::string("") : irName(m));
                if (!m->isPureVirtual) need(m);
            }
            x86.addVtable(v);
            if (msvc) x86.registerGlobalName("vtable@" + v.className, msvcMangleVtableSymbol(*s));
        }
        if (s->kind == Symbol::Variable && !s->isLocal && !s->isParam && !s->isExtern && (!s->isMember || s->isStatic) && s->scope && !s->isDependentContext) {
            if (s->isMember && !s->isDefinition) continue;
            IrGlobal g;
            g.name = s->qualifiedName();
            g.size = sizeOf(s->type) > 0 ? sizeOf(s->type) : 8;
            g.isFloat = s->type && s->type->isFloating();
            if (s->init && s->init->semaConst) g.init = numText(s->init->semaValue);
            else if (s->init && s->init->kind == cxx::Expr::FloatLit) g.init = s->init->text;
            x86.addGlobal(g);
            if (msvc) x86.registerGlobalName(g.name, msvcMangleVariableSymbol(*s));
        }
        if (msvc && s->kind == Symbol::Function && !s->isExternC) x86.registerSymbolName(irName(s), msvcMangleSymbol(*s));
    }
}

} // namespace cppparse
