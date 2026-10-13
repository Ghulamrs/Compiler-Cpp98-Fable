// IrGen.cpp - version 6 stage 4: IR generation from the analysed syntax tree. See IrGen.hpp.
#include "IrGen.hpp"
#include "Mangle.hpp"
#include "PathUtil.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace cppparse {

using sema::Symbol;
using sema::Scope;

namespace {

std::string numText(long v) { return intToString(static_cast<int>(v)); }

bool isReference(const sema::Type* t) { return t && t->kind == sema::Type::Reference; }

// internal linkage: a static function or variable at namespace scope, or anything in an anonymous namespace
bool hasInternalLinkage(const sema::Symbol* s)
{
    if (!s) return false;
    if (s->isStatic && !s->ownerClass && !s->isLocal) return true;
    for (const sema::Scope* sc = s->scope; sc; sc = sc->parent)
        if (sc->kind == sema::Scope::Namespace && sc->owner && sc->owner->name.empty()) return true;
    return false;
}

// appends n zero bytes to static data, extending a zero run that ends it
void zeroData(std::vector<IrDataItem>& out, int n)
{
    if (n <= 0) return;
    if (!out.empty() && out.back().kind == IrDataItem::Zero) { out.back().size += n; return; }
    IrDataItem z;
    z.size = n;
    out.push_back(z);
}

} // namespace

IrGen::IrGen(sema::Program& program, sema::TypeTable& semaTypes)
    : m_prog(program), m_semaTypes(semaTypes), m_types(m_emptyCb), m_outList(0), m_generated(0), m_out(0), m_fn(0), m_block(0), m_sretSlot(-1), m_tryCount(0),
      m_initSymbol(0), m_exitThunksDone(0), m_x86(0), m_msvcNames(false), m_staticCount(0), m_unwindId(-1), m_bodyBlock(-1) {}

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
    if (t && t->isFloating()) {
        IrInst fin = make(IrInst::FConst, t, line);
        fin.args.push_back(IrOperand::fimm(static_cast<double>(v), fin.type));
        return emit(fin);
    }
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
    if (lv.bitWidth > 0 && lv.kind == LValue::Mem) return loadBits(lv, u, line);
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
    if (lv.bitWidth > 0 && lv.kind == LValue::Mem) { storeBits(lv, temp, u, line); return; }
    if (lv.kind == LValue::Local) { IrInst in = make(IrInst::StoreLocal, u, line); in.args.push_back(IrOperand::local(lv.local, oldType(u))); in.args.push_back(IrOperand::temp(temp, oldType(u))); emit(in); return; }
    if (lv.kind == LValue::Global) { IrInst in = make(IrInst::StoreGlobal, u, line); in.args.push_back(IrOperand::global(lv.global, oldType(u))); in.args.push_back(IrOperand::temp(temp, oldType(u))); emit(in); return; }
    if (lv.kind == LValue::Mem) { IrInst in = make(IrInst::StoreMem, u, line); in.args.push_back(IrOperand::temp(lv.addr, oldType(m_semaTypes.pointer(u)))); in.args.push_back(IrOperand::temp(temp, oldType(u))); emit(in); return; }
    m_notes.push_back("L" + numText(line) + ": store to a non-lvalue was dropped");
}

// ---- bit-fields: the unit (the declared type's size, at the field's offset) is read and written whole ----

void IrGen::bitField(LValue& lv, const Symbol* field) const
{
    if (!field || field->bitWidth <= 0 || lv.kind != LValue::Mem) return;
    lv.bitWidth = field->bitWidth;
    lv.bitOffset = field->bitOffset;
}

// a constant of the 32- or 64-bit unsigned type t; IR immediates are 32-bit and sign-extended, so a 64-bit
// value outside that range is put together from pieces
int IrGen::bitConstant(unsigned long long v, const sema::Type* t, int line)
{
    if (sizeOf(t) < 8) return constant(static_cast<long>(static_cast<int>(static_cast<unsigned int>(v))), t, line);
    const long long sv = static_cast<long long>(v);
    if (sv >= -2147483647LL - 1 && sv <= 2147483647LL) return constant(static_cast<long>(sv), t, line);
    const unsigned long lo = static_cast<unsigned long>(v & 0xFFFFFFFFULL);
    int hi = binary("<<", constant(static_cast<long>(static_cast<int>(static_cast<unsigned int>(v >> 32))), t, line), t, constant(32, t, line), t, t, line);
    int mid = binary("<<", constant(static_cast<long>(lo >> 16), t, line), t, constant(16, t, line), t, t, line);
    int r = binary("|", hi, t, mid, t, t, line);
    return binary("|", r, t, constant(static_cast<long>(lo & 0xFFFFUL), t, line), t, t, line);
}

namespace {

// the unsigned integer type of a bit-field's unit, and the 32- or 64-bit types its bits are worked in
sema::Type::Kind unitKind(int size) { return size == 1 ? sema::Type::UChar : size == 2 ? sema::Type::UShort : size == 8 ? sema::Type::ULongLong : sema::Type::UInt; }

} // namespace

int IrGen::loadBits(const LValue& lv, const sema::Type* t, int line)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    const sema::Type* unit = m_semaTypes.basic(unitKind(sizeOf(u)));
    const int bits = sizeOf(u) == 8 ? 64 : 32;
    const sema::Type* wu = m_semaTypes.basic(bits == 64 ? sema::Type::ULongLong : sema::Type::UInt);
    const sema::Type* ws = m_semaTypes.basic(bits == 64 ? sema::Type::LongLong : sema::Type::Int);
    IrInst in = make(IrInst::LoadMem, unit, line);
    in.args.push_back(IrOperand::temp(lv.addr, oldType(m_semaTypes.pointer(unit))));
    int x = convert(emit(in), unit, wu, line);
    if (!u->isUnsigned()) {
        // signed: the field's top bit to the top, then an arithmetic shift down
        x = convert(x, wu, ws, line);
        const int up = bits - lv.bitOffset - lv.bitWidth;
        if (up > 0) x = binary("<<", x, ws, constant(up, ws, line), ws, ws, line);
        if (lv.bitWidth < bits) x = binary(">>", x, ws, constant(bits - lv.bitWidth, ws, line), ws, ws, line);
        return convert(x, ws, u, line);
    }
    if (lv.bitOffset > 0) x = binary(">>", x, wu, constant(lv.bitOffset, wu, line), wu, wu, line);
    if (lv.bitWidth < bits) x = binary("&", x, wu, bitConstant((1ULL << lv.bitWidth) - 1, wu, line), wu, wu, line);
    return convert(x, wu, u, line);
}

void IrGen::storeBits(const LValue& lv, int temp, const sema::Type* t, int line)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    const sema::Type* unit = m_semaTypes.basic(unitKind(sizeOf(u)));
    const int bits = sizeOf(u) == 8 ? 64 : 32;
    const sema::Type* wu = m_semaTypes.basic(bits == 64 ? sema::Type::ULongLong : sema::Type::UInt);
    const unsigned long long all = bits == 64 ? ~0ULL : 0xFFFFFFFFULL;
    const unsigned long long field = (lv.bitWidth >= 64 ? all : ((1ULL << lv.bitWidth) - 1) << lv.bitOffset) & all;
    const sema::Type* pu = m_semaTypes.pointer(unit);
    IrInst ld = make(IrInst::LoadMem, unit, line);
    ld.args.push_back(IrOperand::temp(lv.addr, oldType(pu)));
    int old = binary("&", convert(emit(ld), unit, wu, line), wu, bitConstant(~field & all, wu, line), wu, wu, line);
    int v = convert(temp, u, wu, line);
    if (lv.bitOffset > 0) v = binary("<<", v, wu, constant(lv.bitOffset, wu, line), wu, wu, line);
    v = binary("&", v, wu, bitConstant(field, wu, line), wu, wu, line);
    int merged = convert(binary("|", old, wu, v, wu, wu, line), wu, unit, line);
    IrInst st = make(IrInst::StoreMem, unit, line);
    st.args.push_back(IrOperand::temp(lv.addr, oldType(pu)));
    st.args.push_back(IrOperand::temp(merged, oldType(unit)));
    emit(st);
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

int IrGen::offsetAddress(int temp, const sema::Type* fromPtr, const sema::Type* toPtr, long delta, int line)
{
    IrInst in = make(IrInst::AddrIndex, toPtr, line);
    in.extra = 1;
    in.args.push_back(IrOperand::temp(temp, oldType(fromPtr)));
    in.args.push_back(IrOperand::imm(delta, m_types.builtin(Type::LongLong)));
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
            long delta = 0;
            if (baseOffset(fc->decl, tc->decl, off) && off > 0) delta = off;                 // derived to base
            else if (baseOffset(tc->decl, fc->decl, off) && off > 0) delta = -off;           // base to derived (static_cast)
            if (delta != 0) {
                const sema::Type* fp = from->kind == sema::Type::Pointer ? from : m_semaTypes.pointer(from);
                const sema::Type* tp = to->kind == sema::Type::Pointer ? to : m_semaTypes.pointer(to);
                if (from->kind != sema::Type::Pointer) return offsetAddress(temp, fp, tp, delta, line);   // an object: never null
                // a null pointer stays null
                int slot = newLocal("tmp.cast", tp, false);
                LValue lv; lv.kind = LValue::Local; lv.local = slot; lv.type = tp;
                store(lv, constant(0, tp, line), tp, line);
                int nb = m_out->newBlock("cast.nonnull"), jb = m_out->newBlock("cast.end");
                condbr(toBool(temp, fp, line), nb, jb, line);
                m_block = nb;
                store(lv, offsetAddress(temp, fp, tp, delta, line), tp, line);
                br(jb, line);
                m_block = jb;
                return load(lv, tp, line);
            }
        }
        return temp;
    }
    if (to->kind == sema::Type::Class || from->kind == sema::Type::Class || to->kind == sema::Type::Array || to->kind == sema::Type::Error || from->kind == sema::Type::Error) return temp;
    if (to->kind == sema::Type::Bool) return toBool(temp, from, line);
    std::string sub;
    if (from->isIntegral() && to->isIntegral()) {
        int fs = sizeOf(from), ts = sizeOf(to);
        if (fs == ts && (fs >= 8 || from->isUnsigned() == to->isUnsigned())) return temp;
        // same size, other signedness: re-extended by the target's signedness (trunc to the same width)
        sub = ts <= fs ? "trunc" : (from->isUnsigned() ? "zext" : "sext");
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
    const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
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
        std::map<Symbol*, std::string>::iterator st = m_staticNames.find(s);
        if (st != m_staticNames.end()) {
            lv.kind = LValue::Global; lv.global = st->second; lv.type = m_semaTypes.unref(s->type);
            if (isReference(s->type)) { IrInst ld = make(IrInst::LoadGlobal, m_semaTypes.pointer(s->type->inner), line); ld.args.push_back(IrOperand::global(lv.global, oldType(s->type))); lv.kind = LValue::Mem; lv.addr = emit(ld); }
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
            bitField(lv, s);
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
        bitField(lv, s);
        return lv;
    }
    case cxx::Expr::Index: {
        if (e->semaSym && e->semaSym->kind == Symbol::Function) { lv.kind = LValue::Mem; lv.addr = operatorCall(e); return lv; }      // operator[] returning a reference
        cxx::Expr* base = e->kids[0];
        cxx::Expr* index = e->kids[1];
        const sema::Type* bt = m_semaTypes.unref(exprType(base));
        if (bt->kind != sema::Type::Pointer && bt->kind != sema::Type::Array) { std::swap(base, index); bt = m_semaTypes.unref(exprType(base)); }
        int basePtr;
        if (bt->kind == sema::Type::Array) { LValue bl = lvalue(base); basePtr = bl.kind == LValue::None ? value(base) : load(bl, bt, line); }
        else basePtr = value(base);
        const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
        int idx = convert(value(index), exprType(index), lng, line);
        IrInst in = make(IrInst::AddrIndex, m_semaTypes.pointer(lv.type), line);
        in.extra = sizeOf(lv.type) > 0 ? sizeOf(lv.type) : 1;
        in.args.push_back(IrOperand::temp(basePtr, oldType(m_semaTypes.pointer(lv.type))));
        in.args.push_back(IrOperand::temp(idx, oldType(lng)));
        lv.kind = LValue::Mem; lv.addr = emit(in);
        return lv;
    }
    case cxx::Expr::Unary:
        if (e->semaSym && e->semaSym->kind == Symbol::Function) { if (e->semaLvalue || isClassType(exprType(e))) { lv.kind = LValue::Mem; lv.addr = operatorCall(e); return lv; } break; }
        if (e->text == "*" && !e->semaSym) { lv.kind = LValue::Mem; lv.addr = value(e->kids[0]); return lv; }
        if ((e->text == "++" || e->text == "--") && !e->semaSym) { value(e); return lvalue(e->kids[0]); }
        break;
    case cxx::Expr::Paren: return lvalue(e->kids[0]);
    case cxx::Expr::Cast: case cxx::Expr::NamedCast:
        if (!e->kids.empty() && e->semaLvalue) { LValue inner = lvalue(e->kids[0]); if (inner.kind != LValue::None) { lv.kind = LValue::Mem; lv.addr = convert(addressOf(inner, line), m_semaTypes.pointer(inner.type), m_semaTypes.pointer(lv.type), line); } return lv; }
        break;
    case cxx::Expr::Call: case cxx::Expr::FunctionalCast: case cxx::Expr::Binary: case cxx::Expr::PostIncDec:
        if (e->semaLvalue || isClassType(exprType(e))) {
            // the raw call result: an address for a reference, the temporary's address for a class
            lv.kind = LValue::Mem;
            if (e->kind == cxx::Expr::Call) lv.addr = call(e);
            else if (e->kind != cxx::Expr::FunctionalCast && e->semaSym && e->semaSym->kind == Symbol::Function) lv.addr = operatorCall(e);
            else lv.addr = value(e);
            return lv;
        }
        break;
    case cxx::Expr::Assign:
        if (e->semaSym && e->semaSym->kind == Symbol::Function && !(e->semaSym->isImplicit && e->semaSym->name == "operator=")) { lv.kind = LValue::Mem; lv.addr = operatorCall(e); return lv; }
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
        size_t tempsBefore = m_temps.size();
        int v;
        if (byAddr && isClassType(ct) && m_semaTypes.unqualified(m_semaTypes.unref(exprType(arm))) != ct) {
            // an operand of another type (a string literal for a std::string result): converted into a temporary
            int conv = newLocal("tmp.cond", ct, true);
            LValue cl; cl.kind = LValue::Local; cl.local = conv; cl.type = ct;
            v = addressOf(cl, line);
            materialize(arm, ct, v, line);
            if (needsDtor(ct)) registerCleanup(conv, ct, false, 1, line);
        } else if (byAddr) {
            LValue al = lvalue(arm);
            v = al.kind == LValue::None ? value(arm) : addressOf(al, line);
            if (al.kind != LValue::None && arm->semaLvalue && !e->semaLvalue && isClassType(ct)) {
                // the other arm is a temporary, so the result is one too: a copy of this lvalue
                int copy = newLocal("tmp.cond", ct, true);
                LValue cl; cl.kind = LValue::Local; cl.local = copy; cl.type = ct;
                int dst = addressOf(cl, line);
                copyConstruct(ct, dst, v, line);
                if (needsDtor(ct)) registerCleanup(copy, ct, false, 1, line);
                v = dst;
            }
        } else v = convert(value(arm), exprType(arm), ct, line);
        store(lv, v, lv.type, line);
        // a temporary the result refers to survives to the end of the full expression, guarded by its
        // flag because only one arm runs; scalar arms destroy their temporaries here
        if (byAddr) for (size_t i = tempsBefore; i < m_temps.size(); ++i) m_temps[i].guarded = true;
        else endFullExpr(line, tempsBefore);
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
    size_t tempsBefore = m_temps.size();
    int v = toBool(value(e), exprType(e), e->loc.line);
    endFullExpr(e->loc.line, tempsBefore);   // temporaries of this operand die before the branch
    condbr(v, trueBlock, falseBlock, e->loc.line);
}

int IrGen::argument(cxx::Expr* a, const sema::Type* pt, int line)
{
    if (pt && isReference(pt)) {
        const sema::Type* target = m_semaTypes.unref(pt);
        // a class reference bound to another type goes through a converted temporary, even for an lvalue ("pear")
        const sema::Type* at = m_semaTypes.unqualified(m_semaTypes.unref(exprType(a)));
        const sema::Type* tu = m_semaTypes.unqualified(target);
        int off = 0;
        bool converted = isClassType(target) && (at->kind != sema::Type::Class || (at != tu && !(at->decl && tu->decl && baseOffset(at->decl, tu->decl, off))));
        LValue al;
        if (!converted) al = lvalue(a);
        if (converted || al.kind == LValue::None || (a->semaSym && a->semaSym->kind == Symbol::Function && a->kind == cxx::Expr::Call && !a->semaLvalue && !isClassType(exprType(a)))) {
            // bind a temporary
            if (isClassType(exprType(a)) && al.kind == LValue::None) return convert(value(a), m_semaTypes.pointer(m_semaTypes.unref(exprType(a))), m_semaTypes.pointer(target), line);
            int slot = newLocal("tmp.arg", m_semaTypes.unqualified(target), true);
            LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = m_semaTypes.unqualified(target);
            if (!isClassType(target)) store(tl, convert(value(a), exprType(a), target, line), target, line);
            else {
                materialize(a, tl.type, addressOf(tl, line), line);
                if (needsDtor(tl.type)) registerCleanup(slot, tl.type, false, 1, line);
            }
            return addressOf(tl, line);
        }
        int addr = addressOf(al, line);
        return convert(addr, m_semaTypes.pointer(al.type), m_semaTypes.pointer(target), line);
    }
    if (pt && isClassType(pt)) {
        // by value: the callee owns the copy and destroys it (MSVC convention)
        const sema::Type* ct = m_semaTypes.unqualified(m_semaTypes.unref(pt));
        int slot = newLocal("tmp.arg", ct, true);
        LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = ct;
        return materialize(a, ct, addressOf(tl, line), line);
    }
    int v = value(a);
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
    int retSlot = -1;
    if (sret) {
        if (sretAddr < 0) { retSlot = newLocal("tmp.ret", m_semaTypes.unqualified(rt), true); LValue tl; tl.kind = LValue::Local; tl.local = retSlot; tl.type = m_semaTypes.unqualified(rt); sretAddr = addressOf(tl, line); }
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
    if (retSlot >= 0 && needsDtor(rt)) registerCleanup(retSlot, rt, false, 1, line);
    return sret ? sretAddr : r;
}

int IrGen::construct(Symbol* ctor, int objAddr, const std::vector<cxx::Expr*>& args, size_t firstArg, int line)
{
    if (!ctor) return objAddr;
    callFunction(ctor, objAddr, -1, args, firstArg, false, line);
    return objAddr;
}

// ---- special members -----------------------------------------------------------------------------------

Symbol* IrGen::findSpecial(Symbol* cls, char which) const
{
    if (!cls || !cls->members) return 0;
    const std::vector<Symbol*>* v = cls->members->find(which == '~' ? "~" + cls->ctorName() : which == 'a' ? std::string("operator=") : cls->ctorName());
    if (!v) return 0;
    for (size_t i = 0; i < v->size(); ++i) {
        Symbol* f = (*v)[i];
        if (f->kind != Symbol::Function) continue;
        if (which == '~') return f;
        if (which == 'd') { if (f->isConstructor && f->requiredParams == 0) return f; continue; }
        if (f->params.empty() || f->requiredParams > 1) continue;
        const sema::Type* p0 = f->params[0]->type;
        if (!isReference(p0) || m_semaTypes.unqualified(p0->inner) != cls->type) continue;
        if (which == 'c' && f->isConstructor) return f;
        if (which == 'a' && !f->isConstructor) return f;
    }
    return 0;
}

bool IrGen::needsDtor(const sema::Type* t, int depth) const
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (!u || depth > 32) return false;
    if (u->kind == sema::Type::Array) return needsDtor(u->inner, depth + 1);
    if (u->kind != sema::Type::Class || !u->decl) return false;
    Symbol* cls = u->decl;
    if (cls->hasDtor) return true;
    for (size_t b = 0; b < cls->bases.size(); ++b) if (cls->bases[b].cls && needsDtor(cls->bases[b].cls->type, depth + 1)) return true;
    for (size_t f = 0; f < cls->fields.size(); ++f) if (!isReference(cls->fields[f]->type) && needsDtor(cls->fields[f]->type, depth + 1)) return true;
    return false;
}

bool IrGen::needsCopyCtor(const sema::Type* t, int depth) const
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (!u || depth > 32) return false;
    if (u->kind == sema::Type::Array) return needsCopyCtor(u->inner, depth + 1);
    if (u->kind != sema::Type::Class || !u->decl) return false;
    Symbol* cls = u->decl;
    if (cls->hasCopyCtor || cls->isPolymorphic) return true;   // a copy gets its own class's vptr, not the source's
    for (size_t b = 0; b < cls->bases.size(); ++b) if (cls->bases[b].cls && needsCopyCtor(cls->bases[b].cls->type, depth + 1)) return true;
    for (size_t f = 0; f < cls->fields.size(); ++f) if (!isReference(cls->fields[f]->type) && needsCopyCtor(cls->fields[f]->type, depth + 1)) return true;
    return false;
}

bool IrGen::needsCopyAssign(const sema::Type* t, int depth) const
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (!u || depth > 32) return false;
    if (u->kind == sema::Type::Array) return needsCopyAssign(u->inner, depth + 1);
    if (u->kind != sema::Type::Class || !u->decl) return false;
    Symbol* cls = u->decl;
    if (cls->hasCopyAssign || cls->isPolymorphic) return true;   // an assignment keeps the target's vptr
    for (size_t b = 0; b < cls->bases.size(); ++b) if (cls->bases[b].cls && needsCopyAssign(cls->bases[b].cls->type, depth + 1)) return true;
    for (size_t f = 0; f < cls->fields.size(); ++f) if (!isReference(cls->fields[f]->type) && needsCopyAssign(cls->fields[f]->type, depth + 1)) return true;
    return false;
}

bool IrGen::needsDefaultCtor(const sema::Type* t, int depth) const
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (!u || depth > 32) return false;
    if (u->kind == sema::Type::Array) return needsDefaultCtor(u->inner, depth + 1);
    if (u->kind != sema::Type::Class || !u->decl) return false;
    Symbol* cls = u->decl;
    if (cls->hasUserCtor || cls->isPolymorphic) return true;
    for (size_t b = 0; b < cls->bases.size(); ++b) if (cls->bases[b].cls && needsDefaultCtor(cls->bases[b].cls->type, depth + 1)) return true;
    for (size_t f = 0; f < cls->fields.size(); ++f) if (!isReference(cls->fields[f]->type) && needsDefaultCtor(cls->fields[f]->type, depth + 1)) return true;
    return false;
}

void IrGen::callSpecial(Symbol* fn, int thisPtr, int argPtr, int line)
{
    need(fn);
    const sema::Type* pt = m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass));
    const sema::Type* rt = fn->type && fn->type->kind == sema::Type::Function ? fn->type->inner : m_semaTypes.basic(sema::Type::Void);
    IrInst in = make(IrInst::Call, isReference(rt) ? m_semaTypes.pointer(rt->inner) : rt, line);
    in.sub = irName(fn);
    in.args.push_back(IrOperand::temp(thisPtr, oldType(pt)));
    if (argPtr >= 0) in.args.push_back(IrOperand::temp(argPtr, oldType(pt)));
    emit(in);
}

void IrGen::copyBytes(int dst, int src, int size, int line)
{
    if (size <= 0) return;
    const sema::Type* vp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
    IrInst c = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
    c.sub = "__copy";
    c.args.push_back(IrOperand::temp(dst, oldType(vp)));
    c.args.push_back(IrOperand::temp(src, oldType(vp)));
    c.args.push_back(IrOperand::imm(size, m_types.intType()));
    emit(c);
}

int IrGen::subobject(int objAddr, const sema::Type* objType, const std::string& name, int offset, const sema::Type* partType, int line)
{
    if (offset == 0 && objType == partType) return objAddr;
    IrInst fa = make(IrInst::AddrField, m_semaTypes.pointer(partType), line);
    fa.sub = name;
    fa.extra = offset;
    fa.args.push_back(IrOperand::temp(objAddr, oldType(m_semaTypes.pointer(objType))));
    return emit(fa);
}

void IrGen::copyConstruct(const sema::Type* t, int dst, int src, int line)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (u->kind == sema::Type::Array && needsCopyCtor(u)) {
        const sema::Type* eu = m_semaTypes.unqualified(u->inner);
        const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
        for (long i = 0; i < u->arraySize; ++i) {
            int d, s;
            IrInst ad = make(IrInst::AddrIndex, m_semaTypes.pointer(eu), line); ad.extra = sizeOf(eu); ad.args.push_back(IrOperand::temp(dst, oldType(m_semaTypes.pointer(eu)))); ad.args.push_back(IrOperand::imm(i, oldType(lng))); d = emit(ad);
            IrInst as = make(IrInst::AddrIndex, m_semaTypes.pointer(eu), line); as.extra = sizeOf(eu); as.args.push_back(IrOperand::temp(src, oldType(m_semaTypes.pointer(eu)))); as.args.push_back(IrOperand::imm(i, oldType(lng))); s = emit(as);
            copyConstruct(eu, d, s, line);
        }
        return;
    }
    Symbol* ctor = u->kind == sema::Type::Class && needsCopyCtor(u) ? findSpecial(u->decl, 'c') : 0;
    if (ctor) callSpecial(ctor, dst, src, line);
    else copyBytes(dst, src, copySize(u), line);
}

void IrGen::copyAssign(const sema::Type* t, int dst, int src, int line)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (u->kind == sema::Type::Array && needsCopyAssign(u)) {
        const sema::Type* eu = m_semaTypes.unqualified(u->inner);
        const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
        for (long i = 0; i < u->arraySize; ++i) {
            int d, s;
            IrInst ad = make(IrInst::AddrIndex, m_semaTypes.pointer(eu), line); ad.extra = sizeOf(eu); ad.args.push_back(IrOperand::temp(dst, oldType(m_semaTypes.pointer(eu)))); ad.args.push_back(IrOperand::imm(i, oldType(lng))); d = emit(ad);
            IrInst as = make(IrInst::AddrIndex, m_semaTypes.pointer(eu), line); as.extra = sizeOf(eu); as.args.push_back(IrOperand::temp(src, oldType(m_semaTypes.pointer(eu)))); as.args.push_back(IrOperand::imm(i, oldType(lng))); s = emit(as);
            copyAssign(eu, d, s, line);
        }
        return;
    }
    Symbol* op = u->kind == sema::Type::Class && needsCopyAssign(u) ? findSpecial(u->decl, 'a') : 0;
    if (op) callSpecial(op, dst, src, line);
    else copyBytes(dst, src, copySize(u), line);
}

void IrGen::constructDefault(const sema::Type* t, int addr, int line)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (u->kind == sema::Type::Array) {
        if (u->arraySize > 0 && needsDefaultCtor(u->inner)) forEachElement(u->inner, addr, constant(u->arraySize, m_semaTypes.basic(sema::Type::LongLong), line), true, line);
        return;
    }
    if (u->kind != sema::Type::Class || !u->decl || !needsDefaultCtor(u)) return;
    Symbol* ctor = findSpecial(u->decl, 'd');
    if (!ctor) { m_notes.push_back("L" + numText(line) + ": no default constructor for " + u->decl->qualifiedName()); return; }
    std::vector<cxx::Expr*> none;
    construct(ctor, addr, none, 0, line);
}

// constructs (default) or destroys count elements of an array through a loop; count is a long long temp
void IrGen::forEachElement(const sema::Type* elem, int base, int count, bool construct, int line)
{
    const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
    const sema::Type* eu = m_semaTypes.unqualified(elem);
    if (construct ? !needsDefaultCtor(eu) : !needsDtor(eu)) return;
    int idx = newLocal("tmp.idx", lng, false);
    LValue il; il.kind = LValue::Local; il.local = idx; il.type = lng;
    store(il, construct ? constant(0, lng, line) : count, lng, line);
    int cb = m_out->newBlock("elems.cond"), bb = m_out->newBlock("elems.body"), xb = m_out->newBlock("elems.end");
    br(cb, line);
    m_block = cb;
    int cur = load(il, lng, line);
    IrInst c = make(IrInst::Cmp, m_semaTypes.basic(sema::Type::Bool), line);
    c.sub = construct ? "slt" : "sgt";
    c.args.push_back(IrOperand::temp(cur, oldType(lng)));
    if (construct) c.args.push_back(IrOperand::temp(count, oldType(lng))); else c.args.push_back(IrOperand::imm(0, oldType(lng)));
    condbr(emit(c), bb, xb, line);
    m_block = bb;
    int i = load(il, lng, line);
    if (!construct) { i = binary("-", i, lng, constant(1, lng, line), lng, lng, line); store(il, i, lng, line); }
    IrInst ai = make(IrInst::AddrIndex, m_semaTypes.pointer(eu), line);
    ai.extra = sizeOf(eu) > 0 ? sizeOf(eu) : 1;
    ai.args.push_back(IrOperand::temp(base, oldType(m_semaTypes.pointer(eu))));
    ai.args.push_back(IrOperand::temp(i, oldType(lng)));
    int addr = emit(ai);
    if (construct) { constructDefault(eu, addr, line); store(il, binary("+", i, lng, constant(1, lng, line), lng, lng, line), lng, line); }
    else destroy(eu, addr, line, false);
    br(cb, line);
    m_block = xb;
}

// constructs the object that expression e denotes directly into dst (type t): constructor calls and
// calls returning the class build in place, anything else is copy-constructed from its address
int IrGen::materialize(cxx::Expr* e, const sema::Type* t, int dst, int line)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    cxx::Expr* x = e;
    while (x->kind == cxx::Expr::Paren && !x->kids.empty()) x = x->kids[0];
    Symbol* s = x->semaSym;
    const sema::Type* xt = m_semaTypes.unqualified(m_semaTypes.unref(exprType(x)));
    bool sameClass = xt == u;
    if (sameClass && s && s->kind == Symbol::Function && s->isConstructor && !x->semaLvalue) {
        if (x->kind == cxx::Expr::Call) return construct(s, dst, x->kids, 1, line);
        if (x->kind == cxx::Expr::FunctionalCast || x->kind == cxx::Expr::Cast || x->kind == cxx::Expr::NamedCast) return construct(s, dst, x->kids, 0, line);
    }
    if (sameClass && x->kind == cxx::Expr::Call && s && s->kind == Symbol::Function && !s->isConstructor && s->type && s->type->kind == sema::Type::Function && !isReference(s->type->inner) && !x->semaLvalue)
        return call(x, dst);
    int baseOff = 0;
    if (!sameClass && u->kind == sema::Type::Class && u->decl && xt->kind == sema::Type::Class && xt->decl && baseOffset(xt->decl, u->decl, baseOff)) {
        // an object of a derived class: its base part is copied (slicing)
        LValue xl = lvalue(e);
        int src = xl.kind == LValue::None ? value(e) : addressOf(xl, line);
        copyConstruct(u, dst, convert(src, m_semaTypes.pointer(xt), m_semaTypes.pointer(u), line), line);
        return dst;
    }
    if (!sameClass && u->kind == sema::Type::Class && u->decl) {
        // conversion through a constructor taking the operand's type
        const std::vector<Symbol*>* ctors = u->decl->members ? u->decl->members->find(u->decl->ctorName()) : 0;
        const sema::Type* at = m_semaTypes.unqualified(m_semaTypes.decay(exprType(x)));
        Symbol* chosen = 0;
        if (ctors) for (size_t i = 0; i < ctors->size() && !chosen; ++i) {
            Symbol* c = (*ctors)[i];
            if (c->kind != Symbol::Function || !c->isConstructor || c->params.empty() || c->requiredParams > 1) continue;
            const sema::Type* p = m_semaTypes.unqualified(m_semaTypes.unref(c->params[0]->type));
            if (p == at || (p->isArithmetic() && at->isArithmetic()) || (p->kind == sema::Type::Pointer && at->kind == sema::Type::Pointer)) chosen = c;
        }
        if (chosen) { std::vector<cxx::Expr*> one; one.push_back(e); return construct(chosen, dst, one, 0, line); }
        m_notes.push_back("L" + numText(line) + ": no conversion from " + at->text() + " to " + u->decl->qualifiedName());
        value(e);
        return dst;
    }
    int src = value(e);
    copyConstruct(u, dst, src, line);
    return dst;
}

void IrGen::destroy(const sema::Type* t, int objAddr, int line, bool viaVtable)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (u->kind == sema::Type::Array) {
        if (u->arraySize > 0 && needsDtor(u->inner)) forEachElement(u->inner, objAddr, constant(u->arraySize, m_semaTypes.basic(sema::Type::LongLong), line), false, line);
        return;
    }
    if (u->kind != sema::Type::Class || !u->decl) return;
    Symbol* dtor = findSpecial(u->decl, '~');
    if (!dtor) return;
    bool virtualCall = viaVtable && dtor->isVirtual;
    if (!needsDtor(u) && !virtualCall) return;
    std::vector<cxx::Expr*> none;
    callFunction(dtor, objAddr, -1, none, 0, virtualCall, line);
}

// ---- lifetimes ------------------------------------------------------------------------------------------

// mode 0: object of the innermost scope, 1: temporary of the current full expression, 2: destroyed only
// when an exception unwinds (members already built by a constructor whose body may still throw)
int IrGen::registerCleanup(int slot, const sema::Type* t, bool viaPointer, int mode, int line)
{
    Cleanup c;
    c.slot = slot;
    c.type = m_semaTypes.unqualified(m_semaTypes.unref(t));
    c.viaPointer = viaPointer;
    c.unwindOnly = mode == 2;
    const sema::Type* b = m_semaTypes.basic(sema::Type::Bool);
    c.flag = newLocal("flag." + m_out->locals[static_cast<size_t>(slot)].name, b, true);
    LValue fl; fl.kind = LValue::Local; fl.local = c.flag; fl.type = b;
    store(fl, constant(1, b, line), b, line);
    m_flagged.push_back(c);
    if (mode == 0) m_cleanups.push_back(c);
    else if (mode == 1) m_temps.push_back(c);
    return static_cast<int>(m_flagged.size() - 1);
}

void IrGen::runCleanup(const Cleanup& c, bool guarded, int line)
{
    const sema::Type* b = m_semaTypes.basic(sema::Type::Bool);
    LValue fl; fl.kind = LValue::Local; fl.local = c.flag; fl.type = b;
    int next = -1;
    if (guarded) {
        int doBlock = m_out->newBlock("dtor");
        next = m_out->newBlock("dtor.next");
        condbr(toBool(load(fl, b, line), b, line), doBlock, next, line);
        m_block = doBlock;
    }
    int addr;
    if (c.viaPointer) { LValue pl; pl.kind = LValue::Local; pl.local = c.slot; pl.type = m_semaTypes.pointer(c.type); addr = load(pl, pl.type, line); }
    else { LValue ol; ol.kind = LValue::Local; ol.local = c.slot; ol.type = c.type; addr = addressOf(ol, line); }
    destroy(c.type, addr, line, false);
    store(fl, constant(0, b, line), b, line);
    if (guarded) { br(next, line); m_block = next; }
}

void IrGen::openScope()
{
    m_scopes.push_back(m_cleanups.size());
}

void IrGen::closeScope(int line)
{
    size_t mark = m_scopes.back();
    m_scopes.pop_back();
    if (!terminated()) for (size_t i = m_cleanups.size(); i > mark; --i) runCleanup(m_cleanups[i - 1], m_cleanups[i - 1].guarded, line);
    m_cleanups.erase(m_cleanups.begin() + static_cast<std::ptrdiff_t>(mark), m_cleanups.end());
}

void IrGen::leaveScopes(size_t depth, int line)
{
    for (size_t i = m_cleanups.size(); i > depth; --i) runCleanup(m_cleanups[i - 1], m_cleanups[i - 1].guarded, line);
}

void IrGen::endFullExpr(int line, size_t from)
{
    if (!terminated()) for (size_t i = m_temps.size(); i > from; --i) runCleanup(m_temps[i - 1], m_temps[i - 1].guarded, line);
    m_temps.erase(m_temps.begin() + static_cast<std::ptrdiff_t>(from), m_temps.end());
}

// temporaries created since index 'from' live as long as the enclosing scope (bound to a reference); one
// built by a single arm of a conditional stays guarded by its flag
void IrGen::keepTemporaries(size_t from)
{
    for (size_t i = from; i < m_temps.size(); ++i) m_cleanups.push_back(m_temps[i]);
    m_temps.erase(m_temps.begin() + static_cast<std::ptrdiff_t>(from), m_temps.end());
}

// at a handler entry: the objects built inside the region (flagged from 'mark' on) that are still alive
void IrGen::handlerCleanups(size_t mark, int line)
{
    for (size_t i = m_flagged.size(); i > mark; --i) runCleanup(m_flagged[i - 1], true, line);
}

void IrGen::returnSequence(int line)
{
    leaveScopes(0, line);
    if (m_fn->isDestructor) destructorCleanup(m_fn);
    if (m_fn->isConstructor) {
        const sema::Type* b = m_semaTypes.basic(sema::Type::Bool);
        for (size_t i = 0; i < m_flagged.size(); ++i) if (m_flagged[i].unwindOnly) { LValue fl; fl.kind = LValue::Local; fl.local = m_flagged[i].flag; fl.type = b; store(fl, constant(0, b, line), b, line); }
    }
    leaveTryRegions(0, line);
}

// entry block: every flag cleared, then the catch-all region around the body when the function owns
// objects with destructors; its handler destroys what is still constructed and rethrows
void IrGen::finishFunction(int line)
{
    m_block = 0;
    const sema::Type* b = m_semaTypes.basic(sema::Type::Bool);
    for (size_t i = 0; i < m_flagged.size(); ++i) { LValue fl; fl.kind = LValue::Local; fl.local = m_flagged[i].flag; fl.type = b; store(fl, constant(0, b, line), b, line); }
    if (!m_flagged.empty()) {
        int handler = m_out->newBlock("unwind");
        IrInst tb = make(IrInst::TryBegin, 0, line);
        tb.extra = m_unwindId;
        tb.args.push_back(IrOperand::block(handler));
        emit(tb);
        br(m_bodyBlock, line);
        m_block = handler;
        IrInst cp = make(IrInst::CatchParam, m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void)), line);
        cp.sub = "...";
        emit(cp);
        handlerCleanups(0, line);
        emit(make(IrInst::Throw, m_semaTypes.basic(sema::Type::Void), line));
        m_out->hasTry = true;
    } else {
        br(m_bodyBlock, line);
        for (size_t bi = 0; bi < m_out->blocks.size(); ++bi)
            for (size_t i = 0; i < m_out->blocks[bi].insts.size(); ++i) {
                IrInst& in = m_out->blocks[bi].insts[i];
                if (in.op == IrInst::TryEnd && in.extra == m_unwindId) { in.op = IrInst::Nop; in.args.clear(); }
            }
    }
    m_out->computeCfg();
    m_outList->push_back(m_out);
    ++m_generated;
}

// a member declared in a base class is called with the address of that base subobject
int IrGen::adjustThis(Symbol* fn, int objAddr, const sema::Type* objType, int line)
{
    const sema::Type* ot = m_semaTypes.unqualified(m_semaTypes.unref(objType));
    if (ot->kind != sema::Type::Class || !ot->decl || !fn->ownerClass || ot->decl == fn->ownerClass) return objAddr;
    return convert(objAddr, m_semaTypes.pointer(ot), m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass)), line);
}

// the call an operator expression resolved to (e->semaSym): the raw result, an address for a reference result
int IrGen::operatorCall(cxx::Expr* e)
{
    const int line = e->loc.line;
    Symbol* fn = e->semaSym;
    std::vector<cxx::Expr*> args;
    int thisPtr = -1;
    switch (e->kind) {
    case cxx::Expr::Unary: case cxx::Expr::PostIncDec:
        if (fn->ownerClass) thisPtr = adjustThis(fn, addressOf(lvalue(e->kids[0]), line), exprType(e->kids[0]), line); else args.push_back(e->kids[0]);
        if (e->kind == cxx::Expr::PostIncDec && e->kids.size() > 1) args.push_back(e->kids[1]);   // the int of operator++(int)
        break;
    case cxx::Expr::Binary: case cxx::Expr::Assign: case cxx::Expr::Index:
        if (fn->ownerClass) { thisPtr = adjustThis(fn, addressOf(lvalue(e->kids[0]), line), exprType(e->kids[0]), line); args.push_back(e->kids[1]); }
        else { args.push_back(e->kids[0]); args.push_back(e->kids[1]); }
        break;
    default:
        break;
    }
    return callFunction(fn, thisPtr, -1, args, 0, true, line);
}

// a call whose result is a scalar reference yields its address: loaded where a value is wanted
int IrGen::rvalue(int r, cxx::Expr* e, int line)
{
    const sema::Type* rt = exprType(e);
    if (!isReference(rt)) return r;
    const sema::Type* u = m_semaTypes.unref(rt);
    if (u->kind == sema::Type::Class || u->kind == sema::Type::Array) return r;
    LValue m; m.kind = LValue::Mem; m.addr = r; m.type = u;
    return load(m, u, line);
}

int IrGen::call(cxx::Expr* e, int sretAddr)
{
    const int line = e->loc.line;
    cxx::Expr* callee = e->kids[0];
    Symbol* fn = e->semaSym;
    const sema::Type* rt = exprType(e);
    if (!fn && callee->kind == cxx::Expr::Member && !callee->name.last().empty() && callee->name.last()[0] == '~') {
        // pseudo-destructor call on a scalar: evaluate the object, do nothing
        if (!callee->kids.empty()) value(callee->kids[0]);
        return constant(0, m_semaTypes.basic(sema::Type::Int), line);
    }
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
        // T(args) as a call: construct into the requested object, else a temporary
        if (sretAddr >= 0) return construct(fn, sretAddr, e->kids, 1, line);
        const sema::Type* ct = m_semaTypes.classType(fn->ownerClass);
        int slot = newLocal("tmp." + fn->ownerClass->name, ct, true);
        LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = ct;
        int r = construct(fn, addressOf(tl, line), e->kids, 1, line);
        if (needsDtor(ct)) registerCleanup(slot, ct, false, 1, line);
        return r;
    }
    // "t(c, r)" with t an object: operator() on t, also inside a member function of t's own class, where
    // a plain name would otherwise call through the implicit this
    const bool objectCall = fn->isOperator && fn->name == "operator()" && callee->kind == cxx::Expr::Name && callee->name.last() != "operator()";
    if (callee->kind == cxx::Expr::Member && fn->ownerClass && !fn->isStatic) {
        cxx::Expr* obj = callee->kids[0];
        const sema::Type* objT = m_semaTypes.unref(exprType(obj));
        if (callee->arrowMember) {
            thisPtr = value(obj);
            if (objT->kind == sema::Type::Pointer) objT = objT->inner;
        } else { LValue ol = lvalue(obj); thisPtr = ol.kind == LValue::None ? value(obj) : load(ol, objT, line); }
        objT = m_semaTypes.unqualified(objT);
        if (objT->kind == sema::Type::Class && objT->decl && objT->decl != fn->ownerClass) thisPtr = convert(thisPtr, m_semaTypes.pointer(objT), m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass)), line);
        virtualCall = callee->name.parts.size() == 1;
    } else if (callee->kind != cxx::Expr::Member && !objectCall && fn->ownerClass && !fn->isStatic && m_out->hasThis) {
        thisPtr = thisTemp(line);
        if (m_fn->ownerClass != fn->ownerClass) thisPtr = convert(thisPtr, m_semaTypes.pointer(m_semaTypes.classType(m_fn->ownerClass)), m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass)), line);
        virtualCall = callee->kind == cxx::Expr::Name && callee->name.parts.size() == 1;
    } else if (callee->kind != cxx::Expr::Name && callee->kind != cxx::Expr::Member && fn->ownerClass && !fn->isStatic) {
        // operator() on an object
        thisPtr = addressOf(lvalue(callee), line);
    }
    size_t firstArg = 1;
    if (fn->isOperator && fn->name == "operator()" && callee->kind == cxx::Expr::Name && thisPtr < 0) { LValue ol = lvalue(callee); thisPtr = ol.kind == LValue::None ? value(callee) : addressOf(ol, line); }
    return callFunction(fn, thisPtr, sretAddr, e->kids, firstArg, virtualCall, line);
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
        IrInst in = make(IrInst::StrConst, 0, line); in.type = m_types.charPtr(); in.args.push_back(IrOperand::str(e->text, m_types.charPtr())); return emit(in);
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
        if (e->semaSym && e->semaSym->kind == Symbol::Function) return rvalue(operatorCall(e), e, line);
        LValue lv = lvalue(e);
        if (lv.kind == LValue::None) return constant(0, t, line);
        return load(lv, exprType(e), line);
    }
    case cxx::Expr::Call: return rvalue(call(e), e, line);
    case cxx::Expr::FunctionalCast: {
        const sema::Type* to = exprType(e);
        if (isClassType(to) && e->semaSym && e->semaSym->isConstructor) {
            Symbol* ctor = e->semaSym;
            int slot = newLocal("tmp." + ctor->ownerClass->name, m_semaTypes.unqualified(to), true);
            LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = m_semaTypes.unqualified(to);
            int r = construct(ctor, addressOf(tl, line), e->kids, 0, line);
            if (needsDtor(tl.type)) registerCleanup(slot, tl.type, false, 1, line);
            return r;
        }
        if (e->semaSym && e->semaSym->isConversion) { std::vector<cxx::Expr*> none; int obj = adjustThis(e->semaSym, addressOf(lvalue(e->kids[0]), line), exprType(e->kids[0]), line); return rvalue(callFunction(e->semaSym, obj, -1, none, 0, true, line), e, line); }
        if (e->kids.empty()) return constant(0, to, line);
        return convert(value(e->kids[0]), exprType(e->kids[0]), to, line);
    }
    case cxx::Expr::Cast: case cxx::Expr::NamedCast: {
        if (e->kids.empty()) return constant(0, t, line);
        if (e->semaSym && e->semaSym->kind == Symbol::Function && e->semaSym->isConversion) { std::vector<cxx::Expr*> none; int obj = adjustThis(e->semaSym, addressOf(lvalue(e->kids[0]), line), exprType(e->kids[0]), line); return rvalue(callFunction(e->semaSym, obj, -1, none, 0, true, line), e, line); }
        if (e->semaSym && e->semaSym->kind == Symbol::Function && e->semaSym->isConstructor) {
            int slot = newLocal("tmp." + e->semaSym->ownerClass->name, m_semaTypes.unqualified(t), true);
            LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = m_semaTypes.unqualified(t);
            int r = construct(e->semaSym, addressOf(tl, line), e->kids, 0, line);
            if (needsDtor(tl.type)) registerCleanup(slot, tl.type, false, 1, line);
            return r;
        }
        const bool reinterpret = e->kind == cxx::Expr::NamedCast && e->text == "reinterpret_cast";
        if (isReference(exprType(e))) { LValue inner = lvalue(e->kids[0]); if (inner.kind != LValue::None) return reinterpret ? addressOf(inner, line) : convert(addressOf(inner, line), m_semaTypes.pointer(inner.type), m_semaTypes.pointer(t), line); }
        if (reinterpret && t->isPointer() && m_semaTypes.decay(exprType(e->kids[0]))->isPointer()) return value(e->kids[0]);   // the same address
        return convert(value(e->kids[0]), exprType(e->kids[0]), t, line);
    }
    case cxx::Expr::Paren: return value(e->kids[0]);
    case cxx::Expr::Unary: {
        const std::string& op = e->text;
        if (e->semaSym && e->semaSym->kind == Symbol::Function) return rvalue(operatorCall(e), e, line);
        if (op == "&") {
            cxx::Expr* o = e->kids[0];
            // &f names the function; for an overloaded operator ("&v[0]") semaSym is the operator called
            if ((o->kind == cxx::Expr::Name || o->kind == cxx::Expr::Member) && o->semaSym && (o->semaSym->kind == Symbol::Function || o->semaSym->kind == Symbol::FunctionTemplate)) return value(o);
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
            return lv.bitWidth > 0 ? load(lv, ot, line) : nv;     // a bit-field holds the value cut to its width
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
        if (e->semaSym && e->semaSym->kind == Symbol::Function) return rvalue(operatorCall(e), e, line);
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
        if (e->semaSym && e->semaSym->kind == Symbol::Function) return rvalue(operatorCall(e), e, line);
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
                if (lv.kind == LValue::None) return src;
                int dst = addressOf(lv, line);
                copyAssign(lv.type, dst, convert(src, m_semaTypes.pointer(m_semaTypes.unref(exprType(e->kids[1]))), m_semaTypes.pointer(lv.type), line), line);
                return dst;
            }
            return rvalue(operatorCall(e), e, line);
        }
        LValue lv = lvalue(e->kids[0]);
        if (lv.kind == LValue::None) { value(e->kids[1]); m_notes.push_back("L" + numText(line) + ": assignment to a non-lvalue dropped"); return constant(0, t, line); }
        const sema::Type* lt = lv.type;
        int rv;
        if (op == "=") rv = convert(value(e->kids[1]), exprType(e->kids[1]), lt, line);
        else {
            int cur = load(lv, lt, line);
            int r = value(e->kids[1]);
            const std::string bop = op.substr(0, op.size() - 1);
            const sema::Type* lu = m_semaTypes.unqualified(lt);
            const sema::Type* ru = m_semaTypes.unqualified(m_semaTypes.decay(exprType(e->kids[1])));
            if (lu->isArithmetic() && ru->isArithmetic()) {
                // a op= b is a = a op b: computed in the operands' common type, then converted back
                const sema::Type* common = bop == "<<" || bop == ">>" ? m_semaTypes.promoted(lu) : m_semaTypes.arithmeticResult(lu, ru);
                rv = convert(binary(bop, cur, lu, r, ru, common, line), common, lu, line);
            } else rv = binary(bop, cur, lt, r, exprType(e->kids[1]), lt, line);
        }
        store(lv, rv, lt, line);
        return lv.bitWidth > 0 ? load(lv, lt, line) : rv;         // a bit-field holds the value cut to its width
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
            const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
            int count = countExpr ? convert(value(countExpr), exprType(countExpr), lng, line) : constant(1, lng, line);
            int bytes = binary("*", count, lng, constant(sizeOf(elem) > 0 ? sizeOf(elem) : 1, lng, line), lng, lng, line);
            bool header = needsDtor(elem);   // the element count is kept in front of the elements for delete[]
            if (header) bytes = binary("+", bytes, lng, constant(8, lng, line), lng, lng, line);   // an 8-byte cookie, as MSVC
            n.sub = "__new_array";
            n.args.push_back(IrOperand::temp(bytes, oldType(lng)));
            int p = emit(n);
            if (header) {
                LValue hl; hl.kind = LValue::Mem; hl.addr = p; hl.type = lng;
                store(hl, count, lng, line);
                IrInst adv = make(IrInst::AddrField, pt, line); adv.sub = "elements"; adv.extra = 8; adv.args.push_back(IrOperand::temp(p, oldType(pt)));
                p = emit(adv);
            }
            if (needsDefaultCtor(elem)) forEachElement(elem, p, count, true, line);
            return p;
        }
        int p;
        if (!e->placement.empty()) {
            // placement new: the object is built at the given address
            p = convert(value(e->placement[0]), m_semaTypes.decay(exprType(e->placement[0])), pt, line);
            for (size_t k = 1; k < e->placement.size(); ++k) value(e->placement[k]);
        } else {
            n.sub = "__new";
            n.args.push_back(IrOperand::imm(sizeOf(elem) > 0 ? sizeOf(elem) : 1, m_types.intType()));
            p = emit(n);
        }
        if (e->semaSym && e->semaSym->isConstructor) construct(e->semaSym, p, e->kids, 0, line);
        else if (!e->kids.empty()) { LValue lv; lv.kind = LValue::Mem; lv.addr = p; lv.type = elem; store(lv, convert(value(e->kids[0]), exprType(e->kids[0]), elem, line), elem, line); }
        else if (e->hasInit && !isClassType(elem)) { LValue lv; lv.kind = LValue::Mem; lv.addr = p; lv.type = elem; store(lv, constant(0, elem, line), elem, line); }   // new T(): value-initialised
        else if (isClassType(elem)) constructDefault(elem, p, line);
        return p;
    }
    case cxx::Expr::Delete: {
        int p = value(e->kids[0]);
        const sema::Type* pt = m_semaTypes.decay(exprType(e->kids[0]));
        bool dtors = pt->kind == sema::Type::Pointer && (needsDtor(pt->inner) || (!e->arrayDelete && isClassType(pt->inner) && pt->inner->decl && pt->inner->decl->isPolymorphic));
        int xb = -1;
        if (dtors) {
            // a null pointer is skipped
            int db = m_out->newBlock("delete.do");
            xb = m_out->newBlock("delete.end");
            condbr(toBool(p, pt, line), db, xb, line);
            m_block = db;
        }
        if (!e->arrayDelete && pt->kind == sema::Type::Pointer) destroy(pt->inner, p, line, true);
        else if (e->arrayDelete && pt->kind == sema::Type::Pointer && needsDtor(pt->inner)) {
            const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
            IrInst hd = make(IrInst::AddrIndex, pt, line); hd.extra = 1; hd.args.push_back(IrOperand::temp(p, oldType(pt))); hd.args.push_back(IrOperand::imm(-8, oldType(lng)));
            int header = emit(hd);
            LValue hl; hl.kind = LValue::Mem; hl.addr = header; hl.type = lng;
            forEachElement(pt->inner, p, load(hl, lng, line), false, line);
            p = header;
        }
        IrInst in = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
        in.sub = e->arrayDelete ? "__delete_array" : "__delete";
        in.args.push_back(IrOperand::temp(p, oldType(pt)));
        emit(in);
        if (xb >= 0) { br(xb, line); m_block = xb; }
        return constant(0, m_semaTypes.basic(sema::Type::Int), line);
    }
    case cxx::Expr::Throw: {
        IrInst in = make(IrInst::Throw, m_semaTypes.basic(sema::Type::Void), line);
        if (!e->kids.empty()) {
            const sema::Type* tt = m_semaTypes.unqualified(m_semaTypes.decay(exprType(e->kids[0])));
            if (isClassType(tt) && tt->decl) {
                // the exception object: copy-constructed into heap storage the runtime takes over and
                // destroys with the class's destructor when the handler ends
                IrInst n = make(IrInst::Call, m_semaTypes.pointer(tt), line);
                n.sub = "__new";
                n.args.push_back(IrOperand::imm(sizeOf(tt) > 0 ? sizeOf(tt) : 1, m_types.intType()));
                int obj = emit(n);
                materialize(e->kids[0], tt, obj, line);
                endFullExpr(line);
                in.args.push_back(IrOperand::temp(obj, oldType(tt)));
                Symbol* dtor = needsDtor(tt) ? findSpecial(tt->decl, '~') : 0;
                if (dtor) { need(dtor); IrInst fa = make(IrInst::AddrGlobal, 0, line); fa.type = m_types.function(oldType(m_semaTypes.basic(sema::Type::Void))); fa.args.push_back(IrOperand::global(irName(dtor), fa.type)); in.args.push_back(IrOperand::temp(emit(fa), fa.type)); }
                else in.args.push_back(IrOperand::imm(0, oldType(m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void)))));
            } else {
                int v = value(e->kids[0]);
                in.args.push_back(IrOperand::temp(v, oldType(tt)));
            }
            in.sub = throwDescriptorFor(tt);
            in.extra = isClassType(tt) ? sizeOf(tt) : 8;
        }
        emit(in);
        return constant(0, m_semaTypes.basic(sema::Type::Int), line);
    }
    case cxx::Expr::Typeid: case cxx::Expr::InitList: case cxx::Expr::Error: return constant(0, t, line);
    }
    return constant(0, t, line);
}

// ---- statements -----------------------------------------------------------------------------------------

// true when the elements initialise every scalar of t, so that nothing is left to zero
bool IrGen::coversAll(const sema::Type* t, const std::vector<cxx::Expr*>& elems) const
{
    const sema::Type* u = m_semaTypes.unref(t);
    size_t want = 0;
    if (u->kind == sema::Type::Array) want = u->arraySize > 0 ? static_cast<size_t>(u->arraySize) : 0;
    else if (u->kind == sema::Type::Class && u->decl) want = u->decl->isUnion ? 1 : u->decl->fields.size();
    if (elems.size() < want) return false;
    for (size_t i = 0; i < elems.size(); ++i) {
        if (elems[i]->kind == cxx::Expr::Error) return false;
        if (elems[i]->kind != cxx::Expr::InitList) continue;
        const sema::Type* et = u->kind == sema::Type::Array ? u->inner : (i < u->decl->fields.size() ? u->decl->fields[i]->type : 0);
        if (!et || !coversAll(et, elems[i]->kids)) return false;
    }
    return true;
}

// brace initialisation of an aggregate whose braces sema made explicit: an InitList element is a nested
// aggregate; members without an initializer are value-initialised (static storage is zero already)
void IrGen::initAggregate(const sema::Type* t, int addr, const std::vector<cxx::Expr*>& elems, bool zeroRest, int line)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (zeroRest && !coversAll(u, elems)) {
        const sema::Type* vp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
        IrInst z = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
        z.sub = "__zero";
        z.args.push_back(IrOperand::temp(addr, oldType(vp)));
        z.args.push_back(IrOperand::imm(0, m_types.intType()));
        z.args.push_back(IrOperand::imm(sizeOf(u), m_types.builtin(Type::LongLong)));
        emit(z);
    }
    size_t parts = 0;
    if (u->kind == sema::Type::Array) parts = u->arraySize > 0 ? static_cast<size_t>(u->arraySize) : 0;
    else if (u->kind == sema::Type::Class && u->decl) parts = u->decl->isUnion ? (u->decl->fields.empty() ? 0 : 1) : u->decl->fields.size();
    for (size_t i = 0; i < parts; ++i) {
        const sema::Type* et;
        int eaddr;
        if (u->kind == sema::Type::Array) {
            et = u->inner;
            IrInst ai = make(IrInst::AddrIndex, m_semaTypes.pointer(et), line); ai.extra = sizeOf(et); ai.args.push_back(IrOperand::temp(addr, oldType(m_semaTypes.pointer(et)))); ai.args.push_back(IrOperand::imm(static_cast<long>(i), m_types.builtin(Type::LongLong)));
            eaddr = emit(ai);
        } else {
            Symbol* fld = u->decl->fields[i];
            et = fld->type;
            eaddr = subobject(addr, u, fld->name, fld->offset, et, line);
        }
        if (i >= elems.size()) { constructDefault(et, eaddr, line); continue; }
        cxx::Expr* x = elems[i];
        if (x->kind == cxx::Expr::Error) continue;
        if (x->kind == cxx::Expr::InitList) { initAggregate(et, eaddr, x->kids, false, line); continue; }
        if (isClassType(et)) { materialize(x, et, eaddr, line); continue; }
        LValue el; el.kind = LValue::Mem; el.addr = eaddr; el.type = et;
        if (u->kind == sema::Type::Class) bitField(el, u->decl->fields[i]);
        store(el, convert(value(x), exprType(x), et, line), et, line);
    }
}

void IrGen::initObject(const LValue& lv, const sema::Type* t, Symbol* var, cxx::InitDeclarator& id, int line)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (isReference(t)) {
        if (id.init.args.empty()) return;
        cxx::Expr* initE = id.init.args[0];
        size_t tempsBefore = m_temps.size();
        LValue src = lvalue(initE);
        int addr = src.kind == LValue::None ? value(initE) : addressOf(src, line);
        addr = convert(addr, m_semaTypes.pointer(src.kind == LValue::None ? m_semaTypes.unref(exprType(initE)) : src.type), m_semaTypes.pointer(u), line);
        if (lv.kind == LValue::Local) { IrInst st = make(IrInst::StoreLocal, t, line); st.args.push_back(IrOperand::local(lv.local, oldType(t))); st.args.push_back(IrOperand::temp(addr, oldType(m_semaTypes.pointer(u)))); emit(st); }
        else { IrInst st = make(IrInst::StoreGlobal, t, line); st.args.push_back(IrOperand::global(lv.global, oldType(t))); st.args.push_back(IrOperand::temp(addr, oldType(m_semaTypes.pointer(u)))); emit(st); }
        keepTemporaries(tempsBefore);   // a temporary bound to the reference lives as long as it
        return;
    }
    if (u->kind == sema::Type::Class && u->decl) {
        int addr = addressOf(lv, line);
        cxx::Expr* src = id.init.args.size() == 1 ? id.init.args[0] : 0;
        bool sameClass = src && isClassType(exprType(src)) && m_semaTypes.unqualified(m_semaTypes.unref(exprType(src))) == u;
        if (id.init.kind == cxx::Initializer::Assign && src) {
            if (sameClass) { materialize(src, u, addr, line); return; }
            if (src->semaSym && src->semaSym->kind == Symbol::Function && src->semaSym->isConstructor) { construct(src->semaSym, addr, id.init.args, 0, line); return; }
            if (var->initCtor) { construct(var->initCtor, addr, id.init.args, 0, line); return; }
            materialize(src, u, addr, line);
            return;
        }
        if (id.init.kind == cxx::Initializer::Parens && sameClass && (!var->initCtor || var->initCtor == findSpecial(u->decl, 'c'))) { materialize(src, u, addr, line); return; }
        if (var->initCtor) { construct(var->initCtor, addr, id.init.args, 0, line); return; }
        if (id.init.kind == cxx::Initializer::None) { constructDefault(u, addr, line); return; }
        if (id.init.kind == cxx::Initializer::Brace) initAggregate(u, addr, id.init.args, lv.kind == LValue::Local, line);
        return;
    }
    if (u->kind == sema::Type::Array) {
        int addr = addressOf(lv, line);
        if (id.init.kind != cxx::Initializer::Brace && id.init.args.size() == 1 && id.init.args[0]->kind == cxx::Expr::StringLit && sizeOf(u->inner) == 1) {
            // char s[N] = "...": the literal's bytes with its terminator; the rest of a longer local array is zero
            // (static storage is zero already)
            const sema::Type* lt = m_semaTypes.unref(exprType(id.init.args[0]));
            const long litBytes = lt && lt->kind == sema::Type::Array ? lt->arraySize : 0;
            const long bytes = u->arraySize < litBytes ? u->arraySize : litBytes;
            const sema::Type* vp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
            if (lv.kind == LValue::Local && u->arraySize > bytes) {
                IrInst z = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
                z.sub = "__zero";
                z.args.push_back(IrOperand::temp(addr, oldType(vp)));
                z.args.push_back(IrOperand::imm(0, m_types.intType()));
                z.args.push_back(IrOperand::imm(u->arraySize, m_types.builtin(Type::LongLong)));
                emit(z);
            }
            const int src = value(id.init.args[0]);
            IrInst c = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
            c.sub = "__copy";
            c.args.push_back(IrOperand::temp(addr, oldType(vp)));
            c.args.push_back(IrOperand::temp(src, oldType(vp)));
            c.args.push_back(IrOperand::imm(bytes, m_types.builtin(Type::LongLong)));
            emit(c);
            return;
        }
        if (id.init.kind != cxx::Initializer::Brace) { constructDefault(u, addr, line); return; }
        initAggregate(u, addr, id.init.args, lv.kind == LValue::Local, line);
        return;
    }
    if (id.init.args.empty()) return;
    cxx::Expr* src = id.init.args[0];
    int v = convert(value(src), exprType(src), u, line);   // a conversion operator was wrapped around src by sema
    store(lv, v, u, line);
}

void IrGen::localDecl(cxx::Decl* d)
{
    if (d->kind != cxx::Decl::Simple) return;
    for (size_t i = 0; i < d->declarators.size(); ++i) {
        cxx::InitDeclarator& id = d->declarators[i];
        Symbol* var = id.decl ? id.decl->semaSym : 0;
        if (!var || var->kind != Symbol::Variable) continue;
        if (var->isStatic) {
            // a static local is a global of this module; an initializer that is not a constant runs on the first pass
            const int line = id.decl->loc.line;
            // named after the function and numbered within it, so every unit that compiles an inline function
            // emits the same symbol (the global is then a COMDAT like the function)
            std::string gname = m_out->name + "::" + var->name + "#" + numText(m_staticCount++);
            m_staticNames[var] = gname;
            if (m_msvcNames && m_x86) {
                // cl numbers the blocks of a function; the body is 2. A nested block gets 2 plus its position
                // among the function's blocks: unique within the function, though not always cl's number
                long block = 2;
                if (var->scope && !(var->scope->parent && var->scope->parent->kind == Scope::Function)) {
                    long k = 0;
                    for (size_t si = 0; si < m_prog.scopes.size(); ++si) {
                        const Scope* sc = m_prog.scopes[si];
                        if (sc->kind != Scope::Block || sc->owner != var->scope->owner) continue;
                        if (sc == var->scope) { block = 2 + k; break; }
                        ++k;
                    }
                }
                m_x86->registerGlobalName(gname, msvcMangleStaticLocal(*var, *m_fn, block));
            }
            const sema::Type* vt = m_semaTypes.unqualified(m_semaTypes.unref(var->type));
            IrGlobal g;
            g.name = gname;
            g.size = isReference(var->type) ? 8 : (sizeOf(vt) > 0 ? sizeOf(vt) : 8);   // a reference is a pointer slot
            g.isFloat = !isReference(var->type) && vt->isFloating();
            g.linkOnce = m_out->linkOnce;
            // internal unless the function is inline: then every unit's copy is one COMDAT the linker picks, as cl's
            g.internal = !g.linkOnce;
            g.align = isReference(var->type) ? 8 : alignOf(vt);
            if (var->declAlign > g.align) g.align = var->declAlign;
            bool scalarConstant = id.init.kind != cxx::Initializer::None && id.init.args.size() == 1 && id.init.args[0]->semaConst && vt->isScalar() && !isReference(var->type) && !vt->isFloating();
            if (scalarConstant) g.init = numText(id.init.args[0]->semaValue);
            // a table of constants and addresses is static data, built before anything runs (no guard)
            else if (id.init.kind != cxx::Initializer::None && !isReference(var->type) && !needsDtor(vt) && !var->initCtor) {
                std::vector<IrDataItem> items;
                if (staticData(var->type, id.init, items)) { g.data = items; g.readOnly = readOnlyObject(var->type); scalarConstant = true; }
            }
            if (m_x86) m_x86->addGlobal(g);
            const bool destroyed = !isReference(var->type) && needsDtor(vt);   // registered with atexit once built
            bool needsCode = !scalarConstant && (id.init.kind != cxx::Initializer::None || needsDefaultCtor(vt) || destroyed);
            if (needsCode) {
                const sema::Type* b = m_semaTypes.basic(sema::Type::Bool);
                IrGlobal guard;
                guard.name = gname + "$guard";
                guard.size = 1;
                guard.isFloat = false;
                guard.linkOnce = m_out->linkOnce;
                guard.internal = !guard.linkOnce;
                if (m_x86) m_x86->addGlobal(guard);
                LValue gl; gl.kind = LValue::Global; gl.global = guard.name; gl.type = b;
                // thread-safe as cl's: a clear guard leads to the runtime's lock, which says whether this
                // thread runs the initializer (another one may have finished it meanwhile); the runtime sets
                // the guard after the object is built and releases the lock
                int done = m_out->newBlock("static.done"), check = m_out->newBlock("static.lock"), doInit = m_out->newBlock("static.init");
                condbr(toBool(load(gl, b, line), b, line), done, check, line);
                m_block = check;
                IrInst enter = make(IrInst::Call, m_semaTypes.basic(sema::Type::Int), line);
                enter.sub = "cppparse_static_enter";
                enter.args.push_back(IrOperand::temp(addressOf(gl, line), oldType(m_semaTypes.pointer(b))));
                condbr(emit(enter), doInit, done, line);
                m_block = doInit;
                LValue lv; lv.kind = LValue::Global; lv.global = gname; lv.type = m_semaTypes.unref(var->type);
                initObject(lv, var->type, var, id, line);
                endFullExpr(line);
                if (destroyed) registerAtExit(gname, lv.type, line);
                IrInst leave = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
                leave.sub = "cppparse_static_leave";
                leave.args.push_back(IrOperand::temp(addressOf(gl, line), oldType(m_semaTypes.pointer(b))));
                emit(leave);
                br(done, line);
                m_block = done;
            }
            continue;
        }
        int slot = newLocal(var->name, var->type, false);
        m_slots[var] = slot;
        LValue lv; lv.kind = LValue::Local; lv.local = slot; lv.type = m_semaTypes.unref(var->type);
        initObject(lv, var->type, var, id, id.decl->loc.line);
        if (!isReference(var->type) && needsDtor(var->type)) registerCleanup(slot, var->type, false, 0, id.decl->loc.line);
        endFullExpr(id.decl->loc.line);
    }
}

int IrGen::labelBlock(const std::string& name)
{
    std::map<std::string, int>::iterator it = m_labels.find(name);
    if (it != m_labels.end()) return it->second;
    int b = m_out->newBlock("label." + name);
    m_labels[name] = b;
    return b;
}

void IrGen::scopedStmt(cxx::Stmt* s, int line)
{
    openScope();
    stmt(s);
    closeScope(line);
}

void IrGen::stmt(cxx::Stmt* s)
{
    if (!s) return;
    const int line = s->loc.line;
    switch (s->kind) {
    case cxx::Stmt::Compound: openScope(); for (size_t i = 0; i < s->kids.size(); ++i) stmt(s->kids[i]); closeScope(line); return;
    case cxx::Stmt::Empty: case cxx::Stmt::Asm: case cxx::Stmt::Error: return;
    case cxx::Stmt::ExprStmt: value(s->expr); endFullExpr(line); return;
    case cxx::Stmt::DeclStmt: localDecl(s->decl); return;
    case cxx::Stmt::If: {
        int tb = m_out->newBlock("if.then"), eb = s->kids.size() > 1 ? m_out->newBlock("if.else") : -1, join = m_out->newBlock("if.end");
        openScope();   // a condition variable lives through both branches
        if (s->decl) { localDecl(s->decl); Symbol* v = s->decl->declarators[0].decl->semaSym; LValue lv; lv.kind = LValue::Local; lv.local = m_slots[v]; lv.type = v->type; condbr(toBool(load(lv, v->type, line), v->type, line), tb, eb >= 0 ? eb : join, line); }
        else condition(s->expr, tb, eb >= 0 ? eb : join);
        m_block = tb; scopedStmt(s->kids[0], line); br(join, line);
        if (eb >= 0) { m_block = eb; scopedStmt(s->kids[1], line); br(join, line); }
        m_block = join;
        closeScope(line);
        return;
    }
    case cxx::Stmt::While: {
        int cb = m_out->newBlock("while.cond"), bb = m_out->newBlock("while.body"), xb = m_out->newBlock("while.end");
        br(cb, line);
        m_block = cb; condition(s->expr, bb, xb);
        LoopTargets lt; lt.breakBlock = xb; lt.continueBlock = cb; lt.tryDepth = m_tryStack.size(); lt.cleanupDepth = m_cleanups.size(); m_loops.push_back(lt);
        m_block = bb; scopedStmt(s->kids[0], line); br(cb, line);
        m_loops.pop_back();
        m_block = xb;
        return;
    }
    case cxx::Stmt::DoWhile: {
        int bb = m_out->newBlock("do.body"), cb = m_out->newBlock("do.cond"), xb = m_out->newBlock("do.end");
        br(bb, line);
        LoopTargets lt; lt.breakBlock = xb; lt.continueBlock = cb; lt.tryDepth = m_tryStack.size(); lt.cleanupDepth = m_cleanups.size(); m_loops.push_back(lt);
        m_block = bb; scopedStmt(s->kids[0], line); br(cb, line);
        m_loops.pop_back();
        m_block = cb; condition(s->expr, bb, xb);
        m_block = xb;
        return;
    }
    case cxx::Stmt::For: {
        openScope();   // the init-statement's variables live until the loop ends
        if (s->init) stmt(s->init);
        int cb = m_out->newBlock("for.cond"), bb = m_out->newBlock("for.body"), sb = m_out->newBlock("for.step"), xb = m_out->newBlock("for.end");
        br(cb, line);
        m_block = cb;
        if (s->decl) { localDecl(s->decl); Symbol* v = s->decl->declarators[0].decl->semaSym; LValue lv; lv.kind = LValue::Local; lv.local = m_slots[v]; lv.type = v->type; condbr(toBool(load(lv, v->type, line), v->type, line), bb, xb, line); }
        else if (s->expr) condition(s->expr, bb, xb); else br(bb, line);
        LoopTargets lt; lt.breakBlock = xb; lt.continueBlock = sb; lt.tryDepth = m_tryStack.size(); lt.cleanupDepth = m_cleanups.size(); m_loops.push_back(lt);
        m_block = bb; scopedStmt(s->kids[0], line); br(sb, line);
        m_loops.pop_back();
        m_block = sb; if (s->expr2) { value(s->expr2); endFullExpr(line); } br(cb, line);
        m_block = xb;
        closeScope(line);
        return;
    }
    case cxx::Stmt::Switch: {
        int v = value(s->expr);
        endFullExpr(line);
        openScope();
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
        LoopTargets lt; lt.breakBlock = xb; lt.continueBlock = -1; lt.tryDepth = m_tryStack.size(); lt.cleanupDepth = m_cleanups.size(); m_loops.push_back(lt);
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
        closeScope(line);
        return;
    }
    case cxx::Stmt::Case: case cxx::Stmt::Default: for (size_t i = 0; i < s->kids.size(); ++i) stmt(s->kids[i]); return;
    case cxx::Stmt::Break: if (!m_loops.empty()) { const LoopTargets& lt = m_loops.back(); leaveScopes(lt.cleanupDepth, line); leaveTryRegions(lt.tryDepth, line); br(lt.breakBlock, line); } return;
    case cxx::Stmt::Continue: for (size_t i = m_loops.size(); i > 0; --i) if (m_loops[i - 1].continueBlock >= 0) { leaveScopes(m_loops[i - 1].cleanupDepth, line); leaveTryRegions(m_loops[i - 1].tryDepth, line); br(m_loops[i - 1].continueBlock, line); return; } return;
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
                // build the result in the hidden result object
                IrInst ld = make(IrInst::LoadLocal, m_semaTypes.pointer(rt), line); ld.args.push_back(IrOperand::local(m_sretSlot, oldType(m_semaTypes.pointer(rt))));
                materialize(s->expr, rt, emit(ld), line);
            } else if (!rt->isVoid()) {
                int v = convert(value(s->expr), exprType(s->expr), rt, line);
                in.args.push_back(IrOperand::temp(v, oldType(m_semaTypes.unqualified(rt))));
            } else value(s->expr);
        }
        endFullExpr(line);
        returnSequence(line);
        abiResult(in, line);
        emit(in);
        return;
    }
    case cxx::Stmt::Goto: br(labelBlock(s->label), line); return;   // objects of the scopes left are not destroyed
    case cxx::Stmt::Label: {
        int b = labelBlock(s->label);
        br(b, line);
        m_block = b;
        for (size_t i = 0; i < s->kids.size(); ++i) stmt(s->kids[i]);
        return;
    }
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
        size_t mark = m_flagged.size();   // objects built inside the region are destroyed at a handler entry
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
            handlerCleanups(mark, h->loc.line);
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
            // an exception that leaves the handler ends the one it handles: the body is a region of its own, whose
            // landing pad has the runtime release the handled exception (unless it is the one leaving, a "throw;")
            // and passes the new one on
            const sema::Type* vp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
            const int handledSlot = newLocal("catch.handled", vp, false);
            { LValue hs; hs.kind = LValue::Local; hs.local = handledSlot; hs.type = vp; store(hs, ev, vp, h->loc.line); }
            const int hid = m_tryCount++;
            const int unwind = m_out->newBlock("catch.unwind");
            const int hbody = m_out->newBlock("catch.body");
            IrInst htb = make(IrInst::TryBegin, 0, h->loc.line);
            htb.extra = hid;
            htb.args.push_back(IrOperand::block(unwind));
            emit(htb);
            br(hbody, h->loc.line);
            m_block = hbody;
            m_tryStack.push_back(-1);
            m_tryStack.push_back(hid);
            stmt(h->kids[0]);
            m_tryStack.pop_back();
            m_tryStack.pop_back();
            if (!terminated()) {
                IrInst hte = make(IrInst::TryEnd, 0, h->loc.line);
                hte.extra = hid;
                emit(hte);
                emit(make(IrInst::CatchEnd, 0, h->loc.line));
            }
            br(join, h->loc.line);
            m_block = unwind;
            IrInst ucp = make(IrInst::CatchParam, vp, h->loc.line);
            ucp.sub = "...";
            emit(ucp);
            IrInst ld = make(IrInst::LoadLocal, vp, h->loc.line);
            ld.args.push_back(IrOperand::local(handledSlot, oldType(vp)));
            const int handled = emit(ld);
            IrInst rel = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), h->loc.line);
            rel.sub = "cppparse_catch_unwind";
            rel.args.push_back(IrOperand::temp(handled, oldType(vp)));
            emit(rel);
            emit(make(IrInst::Throw, m_semaTypes.basic(sema::Type::Void), h->loc.line));
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
    if (fn->instantiationFailed) {          // never generated: its body did not analyse
        if (std::find(m_failedUsed.begin(), m_failedUsed.end(), fn) == m_failedUsed.end()) m_failedUsed.push_back(fn);
        return;
    }
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
    m_out->variadic = fn->type && fn->type->kind == sema::Type::Function && fn->type->variadic;
    m_out->linkOnce = fn->isInline || fn->isImplicit || fn->instantiatedFrom != 0 || (fn->ownerClass && fn->ownerClass->instantiatedFrom != 0);
    m_out->internal = hasInternalLinkage(fn) || (fn->ownerClass && hasInternalLinkage(fn->ownerClass));
    if (m_out->internal) m_out->linkOnce = false;   // every unit keeps its own copy
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
        m_out->locals[static_cast<size_t>(slot)].nonNull = true;
    }
    m_sretSlot = -1;
    if (sret) {
        m_sretSlot = newLocal("sret", m_semaTypes.pointer(rt), false);
        m_out->locals[static_cast<size_t>(m_sretSlot)].isParam = true;
        m_out->locals[static_cast<size_t>(m_sretSlot)].nonNull = true;
    }
    std::vector<std::pair<int, const sema::Type*> > byValue;   // class objects the callee owns
    for (size_t p = 0; p < fn->params.size(); ++p) {
        Symbol* ps = fn->params[p];
        const sema::Type* pt = ps->type;
        if (isClassType(pt) && !isReference(pt)) pt = m_semaTypes.pointer(pt);   // objects arrive by address
        int slot = newLocal(ps->name.empty() ? "arg" + numText(static_cast<long>(p)) : ps->name, pt, false);
        m_out->locals[static_cast<size_t>(slot)].isParam = true;
        m_out->locals[static_cast<size_t>(slot)].nonNull = isReference(ps->type) || isClassType(ps->type);
        m_slots[ps] = slot;
        if (isClassType(ps->type) && !isReference(ps->type)) {
            byValue.push_back(std::make_pair(slot, ps->type));
            ps->type = m_semaTypes.reference(ps->type);   // reads go through the pointer
        }
    }
    m_out->paramCount = static_cast<int>(m_out->locals.size());
    m_out->newBlock("entry");       // filled by finishFunction: flags, the unwind region, branch to the body
    m_bodyBlock = m_out->newBlock("body");
    m_block = m_bodyBlock;
    m_unwindId = m_tryCount++;
    m_tryStack.push_back(m_unwindId);
    m_cleanups.clear(); m_scopes.clear(); m_temps.clear(); m_flagged.clear(); m_labels.clear(); m_staticCount = 0;
    for (size_t i = 0; i < byValue.size(); ++i)
        if (needsDtor(byValue[i].second)) registerCleanup(byValue[i].first, byValue[i].second, true, 0, fn->loc.line);
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
        else constructDefault(base->type, baddr, line);
        if (mi) endFullExpr(mi->loc.line);
        if (needsDtor(base->type)) {
            // destroyed when the body throws
            int part = newLocal("part." + base->name, m_semaTypes.pointer(base->type), false);
            LValue pl; pl.kind = LValue::Local; pl.local = part; pl.type = m_semaTypes.pointer(base->type);
            store(pl, baddr, pl.type, line);
            registerCleanup(part, base->type, true, 2, line);
        }
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
        if ((fu->kind == sema::Type::Class && fu->decl) || (fu->kind == sema::Type::Array && isClassType(fu->inner))) {
            int faddr = emit(fa);
            size_t nargs = mi ? mi->args.size() : 0;
            if (fu->kind == sema::Type::Array || !mi) constructDefault(fu, faddr, line);
            else if (nargs == 1 && isClassType(exprType(mi->args[0])) && m_semaTypes.unqualified(m_semaTypes.unref(exprType(mi->args[0]))) == fu) materialize(mi->args[0], fu, faddr, mi->loc.line);
            else {
                const std::vector<Symbol*>* ctors = fu->decl->members ? fu->decl->members->find(fu->decl->ctorName()) : 0;
                Symbol* chosen = 0;
                if (ctors) for (size_t i = 0; i < ctors->size() && !chosen; ++i) { Symbol* c = (*ctors)[i]; if (c->kind == Symbol::Function && c->isConstructor && nargs >= static_cast<size_t>(c->requiredParams) && nargs <= c->params.size()) chosen = c; }
                if (chosen) construct(chosen, faddr, mi->args, 0, mi->loc.line);
                else constructDefault(fu, faddr, line);
            }
            if (mi) endFullExpr(mi->loc.line);
            if (needsDtor(fu)) {
                // destroyed when the body throws
                int part = newLocal("part." + fld->name, m_semaTypes.pointer(fu), false);
                LValue pl; pl.kind = LValue::Local; pl.local = part; pl.type = m_semaTypes.pointer(fu);
                store(pl, faddr, pl.type, line);
                registerCleanup(part, fu, true, 2, line);
            }
            continue;
        }
        if (!mi) continue;
        int faddr = emit(fa);
        LValue fl; fl.kind = LValue::Mem; fl.addr = faddr; fl.type = fu;
        bitField(fl, fld);
        if (mi->args.empty()) { store(fl, constant(0, fu, mi->loc.line), fu, mi->loc.line); continue; }   // member(): value-initialised
        store(fl, convert(value(mi->args[0]), exprType(mi->args[0]), fu, mi->loc.line), fu, mi->loc.line);
        endFullExpr(mi->loc.line);
    }
    // a member of an anonymous union or struct named in the initializer list ("X() : tag(1), iv(0)")
    if (def && cls->members) for (size_t i = 0; i < cls->members->order.size(); ++i) {
        Symbol* m = cls->members->order[i];
        if (m->kind != Symbol::Variable || !m->anonHost || isReference(m->type) || isClassType(m->type)) continue;
        cxx::MemInit* mi = 0;
        for (size_t k = 0; k < def->memInits.size(); ++k) if (def->memInits[k].name.parts.size() == 1 && def->memInits[k].name.last() == m->name) mi = &def->memInits[k];
        if (!mi) continue;
        const sema::Type* fu = m_semaTypes.unqualified(m->type);
        IrInst fa = make(IrInst::AddrField, m_semaTypes.pointer(fu), mi->loc.line); fa.sub = m->name; fa.extra = m->offset; fa.args.push_back(IrOperand::temp(self, m_out->locals[0].type));
        LValue fl; fl.kind = LValue::Mem; fl.addr = emit(fa); fl.type = fu;
        bitField(fl, m);
        store(fl, mi->args.empty() ? constant(0, fu, mi->loc.line) : convert(value(mi->args[0]), exprType(mi->args[0]), fu, mi->loc.line), fu, mi->loc.line);
        endFullExpr(mi->loc.line);
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
        if (isReference(fld->type) || !needsDtor(fld->type)) continue;
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
    const sema::Type* ct = m_semaTypes.classType(cls);
    if (fn->isConstructor && fn->params.empty()) constructorInit(fn, 0);
    else if (!fn->isDestructor) {   // an implicit destructor's body is empty: returnSequence destroys members and bases
        // copy constructor / copy assignment: memberwise, bases first
        bool assign = !fn->isConstructor;
        int self = thisTemp(line);
        IrInst ld = make(IrInst::LoadLocal, 0, line); ld.type = m_out->locals[1].type; ld.args.push_back(IrOperand::local(1, m_out->locals[1].type));
        int src = emit(ld);
        for (size_t b = 0; b < cls->bases.size(); ++b) {
            Symbol* base = cls->bases[b].cls;
            if (!base) continue;
            int d = subobject(self, ct, "base " + base->name, cls->bases[b].offset, base->type, line);
            int s = subobject(src, ct, "base " + base->name, cls->bases[b].offset, base->type, line);
            if (assign) copyAssign(base->type, d, s, line); else copyConstruct(base->type, d, s, line);
        }
        if (!assign) storeVptr(cls, self, line);
        for (size_t f = 0; f < cls->fields.size(); ++f) {
            Symbol* fld = cls->fields[f];
            const sema::Type* ft = m_semaTypes.unref(fld->type);
            int d = subobject(self, ct, fld->name, fld->offset, ft, line);
            int s = subobject(src, ct, fld->name, fld->offset, ft, line);
            if (isReference(fld->type)) copyBytes(d, s, 8, line);
            else if (assign) copyAssign(ft, d, s, line);
            else copyConstruct(ft, d, s, line);
        }
        if (assign) {
            IrInst r = make(IrInst::Ret, 0, line);
            r.args.push_back(IrOperand::temp(thisTemp(line), m_out->locals[0].type));
            returnSequence(line);
            emit(r);
        }
    }
    if (!terminated()) { returnSequence(line); IrInst r = make(IrInst::Ret, 0, line); abiResult(r, line); emit(r); }
    finishFunction(line);
}

// MSVC's x64 convention: a constructor returns this and a function with a hidden result pointer returns
// that pointer, both in rax; MSVC-compiled callers use them
void IrGen::abiResult(IrInst& r, int line)
{
    if (!r.args.empty() || !m_fn) return;
    if (m_fn->isConstructor && m_out->hasThis) r.args.push_back(IrOperand::temp(thisTemp(line), m_out->locals[0].type));
    else if (m_sretSlot >= 0) {
        const IrLocal& l = m_out->locals[static_cast<size_t>(m_sretSlot)];
        IrInst ld = make(IrInst::LoadLocal, 0, line); ld.type = l.type; ld.args.push_back(IrOperand::local(m_sretSlot, l.type));
        r.args.push_back(IrOperand::temp(emit(ld), l.type));
    }
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
        IrInst r = make(IrInst::Ret, 0, def->loc.line);
        const sema::Type* rt = fn->type ? fn->type->inner : 0;
        if (rt && !rt->isVoid() && !isClassType(rt) && !isReference(rt)) {
            if (fn->name == "main") r.args.push_back(IrOperand::temp(constant(0, rt, def->loc.line), oldType(rt)));
            else r.args.push_back(IrOperand::temp(constant(0, m_semaTypes.unqualified(rt), def->loc.line), oldType(m_semaTypes.unqualified(rt))));
        }
        returnSequence(def->loc.line);
        abiResult(r, def->loc.line);
        emit(r);
    }
    finishFunction(def->loc.line);
}

void IrGen::generate(std::vector<IrFunction*>& out)
{
    m_outList = &out;
    for (size_t i = 0; i < m_prog.symbols.size(); ++i) {
        Symbol* s = m_prog.symbols[i];
        if (s->kind == Symbol::Function && s->hasBody && s->definition && !s->isDependentContext && !(s->ownerClass && s->ownerClass->isDependentContext)
            && !s->isLazyInstantiation) m_queue.push_back(s);   // template instantiations: only through need()
    }
    drainQueue();
    genInitializer();
    // the destructor thunks call destructors that may not be generated yet, and those may build static locals
    // that register further thunks
    while (m_exitThunksDone < m_exitThunks.size()) {
        genExitThunks();
        drainQueue();
    }
}

void IrGen::drainQueue()
{
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
    m_x86 = &x86;
    m_msvcNames = msvc;
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
        if (s->kind == Symbol::Variable && !s->isLocal && !s->isParam && (!s->isMember || s->isStatic) && s->scope && !s->isDependentContext) {
            // a static const member with a constant initializer is folded where it is used and not emitted
            if (s->isMember && s->isStatic && s->hasConstValue && s->init && s->init->semaConst) continue;
            std::string name = s->qualifiedName();
            if (msvc) x86.registerGlobalName(name, msvcMangleVariableSymbol(*s));
            if (!s->isDefinition) continue;   // "extern T x;" only, or a static member without its out-of-line definition
            IrGlobal g;
            g.name = name;
            g.size = sizeOf(s->type) > 0 ? sizeOf(s->type) : 8;
            g.isFloat = s->type && s->type->isFloating();
            g.internal = hasInternalLinkage(s);
            g.align = isReference(s->type) ? 8 : alignOf(s->type);
            if (s->declAlign > g.align) g.align = s->declAlign;
            bool dynamic = false;
            const bool scalar = !isClassType(s->type) && m_semaTypes.unref(s->type)->kind != sema::Type::Array;
            if (s->init && s->init->semaConst && scalar) g.init = numText(s->init->semaValue);
            else if (s->init && s->init->kind == cxx::Expr::FloatLit && scalar) g.init = s->init->text;
            else if (staticInit(s, g)) {}   // constants and addresses: static data, as cl emits a constant table
            else if (isClassType(s->type) && !isReference(s->type)) dynamic = s->initCtor != 0 || s->init != 0 || needsDefaultCtor(s->type);
            else dynamic = s->init != 0;
            if (dynamic) m_dynamicGlobals.push_back(s);
            x86.addGlobal(g);
        }
        if (msvc && s->kind == Symbol::Function && !s->isExternC) x86.registerSymbolName(irName(s), msvcMangleSymbol(*s));
    }
    if (!m_dynamicGlobals.empty()) {
        // the module's initialiser, named after its first global so that modules do not clash
        std::string base = m_dynamicGlobals[0]->qualifiedName();
        for (size_t i = 0; i < base.size(); ++i) if (!isalnum(static_cast<unsigned char>(base[i]))) base[i] = '_';
        m_initSymbol = m_prog.newSymbol(Symbol::Function, "__cppparse_init_" + base, m_prog.global);
        m_initSymbol->type = m_semaTypes.function(m_semaTypes.basic(sema::Type::Void), std::vector<const sema::Type*>(), false, false);
        m_initSymbol->isExternC = true;
        m_initSymbol->hasBody = true;
        x86.addInitializer(irName(m_initSymbol));
    }
}

// the module initialiser: constructs the globals that need code, in declaration order
void IrGen::genInitializer()
{
    if (!m_initSymbol) return;
    m_out = new IrFunction;
    m_fn = m_initSymbol;
    m_slots.clear(); m_loops.clear(); m_tryStack.clear(); m_tryCount = 0;
    prologue(m_initSymbol);
    for (size_t i = 0; i < m_dynamicGlobals.size(); ++i) {
        Symbol* g = m_dynamicGlobals[i];
        const int line = g->loc.line;
        LValue lv; lv.kind = LValue::Global; lv.global = g->qualifiedName(); lv.type = m_semaTypes.unref(g->type);
        cxx::InitDeclarator* id = 0;
        if (g->declAst) for (size_t k = 0; k < g->declAst->declarators.size(); ++k) if (g->declAst->declarators[k].decl && g->declAst->declarators[k].decl->semaSym == g) id = &g->declAst->declarators[k];
        if (id) initObject(lv, g->type, g, *id, line);
        else constructDefault(g->type, addressOf(lv, line), line);
        endFullExpr(line);
        if (!isReference(g->type) && needsDtor(g->type)) registerAtExit(lv.global, lv.type, line);
    }
    returnSequence(m_initSymbol->loc.line);
    emit(make(IrInst::Ret, 0, m_initSymbol->loc.line));
    finishFunction(m_initSymbol->loc.line);
}

// a global initialised by constants, string literals and addresses of globals and functions is emitted as static
// data (in .rdata when it is const), as cl does: it is valid before any initializer runs, whatever the link order
bool IrGen::staticInit(Symbol* var, IrGlobal& g)
{
    if (isReference(var->type) || needsDtor(var->type) || var->initCtor) return false;
    cxx::InitDeclarator* id = 0;
    if (var->declAst) for (size_t k = 0; k < var->declAst->declarators.size(); ++k) if (var->declAst->declarators[k].decl && var->declAst->declarators[k].decl->semaSym == var) id = &var->declAst->declarators[k];
    if (!id || id->init.kind == cxx::Initializer::None) return false;
    std::vector<IrDataItem> d;
    if (!staticData(var->type, id->init, d)) return false;
    g.data = d;
    g.readOnly = readOnlyObject(var->type);
    return true;
}

bool IrGen::staticData(const sema::Type* t, const cxx::Initializer& init, std::vector<IrDataItem>& out)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (!u) return false;
    if (init.kind == cxx::Initializer::Brace) {
        if (u->kind == sema::Type::Array || u->kind == sema::Type::Class) return aggregateData(u, &init.args, out, 0);
        return init.args.size() == 1 && constantData(t, init.args[0], out, 0);
    }
    // "const char* p = "x";", "int* p = &g;": a pointer initialised by an address
    return init.args.size() == 1 && u->kind == sema::Type::Pointer && constantData(t, init.args[0], out, 0);
}

// the bytes of an object of type t initialised by x (0: zero-initialised); false when the initializer needs code
bool IrGen::constantData(const sema::Type* t, cxx::Expr* x, std::vector<IrDataItem>& out, int depth)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (!u || depth > 16 || isReference(t)) return false;
    while (x && x->kind == cxx::Expr::Paren && x->kids.size() == 1) x = x->kids[0];
    if (x && x->kind == cxx::Expr::Error) return false;
    const int size = sizeOf(u);
    if (size <= 0) return false;
    if (u->kind == sema::Type::Array || u->kind == sema::Type::Class) {
        if (x && x->kind != cxx::Expr::InitList) return false;
        return aggregateData(u, x ? &x->kids : 0, out, depth + 1);
    }
    if (x && x->kind == cxx::Expr::InitList) {   // a scalar in braces
        if (x->kids.size() > 1) return false;
        return constantData(u, x->kids.empty() ? 0 : x->kids[0], out, depth + 1);
    }
    if (!x) { zeroData(out, size); return true; }
    IrDataItem it;
    it.size = size;
    if (u->isFloating()) {
        cxx::Expr* f = x;
        bool neg = false;
        if (f->kind == cxx::Expr::Unary && f->text == "-" && f->kids.size() == 1) { neg = true; f = f->kids[0]; }
        std::string text;
        if (f->kind == cxx::Expr::FloatLit) {
            text = f->text;
            while (!text.empty() && std::strchr("fFlL", text[text.size() - 1])) text.erase(text.size() - 1);
        } else if (f->semaConst && exprType(f) && m_semaTypes.unref(exprType(f))->isIntegral()) text = numText(f->semaValue) + ".0";
        else return false;
        if (text.empty() || text.find_first_not_of("0123456789.eE+-") != std::string::npos) return false;
        it.kind = IrDataItem::Float;
        it.text = (neg ? "-" : "") + text;
        out.push_back(it);
        return true;
    }
    if (u->kind == sema::Type::Pointer) {
        if (x->semaConst) {
            if (x->semaValue != 0) return false;
            zeroData(out, size);                     // the null pointer
            return true;
        }
        if (x->kind == cxx::Expr::StringLit) {
            if (x->text.empty() || x->text[0] != '"') return false;
            it.kind = IrDataItem::String;
            it.text = x->text;
            out.push_back(it);
            return true;
        }
        cxx::Expr* target = x;
        const bool addressOf = x->kind == cxx::Expr::Unary && x->text == "&" && x->kids.size() == 1;
        if (addressOf) target = x->kids[0];
        while (target->kind == cxx::Expr::Paren && target->kids.size() == 1) target = target->kids[0];
        Symbol* s = target->kind == cxx::Expr::Name ? target->semaSym : 0;
        if (!s) return false;
        if (s->kind == Symbol::Function) {
            if (s->ownerClass && !s->isStatic) return false;
            need(s);
            it.text = irName(s);
        } else if (s->kind == Symbol::Variable) {
            // the address of a global, or an array naming its first element; not the value of a pointer variable
            if (isReference(s->type) || s->isParam || (s->isMember && !s->isStatic)) return false;
            if (!addressOf && m_semaTypes.unref(s->type)->kind != sema::Type::Array) return false;
            if (s->isMember && s->isStatic && s->hasConstValue && s->init && s->init->semaConst) return false;   // folded, never emitted
            if (s->isLocal) {
                std::map<Symbol*, std::string>::const_iterator sn = m_staticNames.find(s);
                if (!s->isStatic || sn == m_staticNames.end()) return false;
                it.text = sn->second;
            } else it.text = s->qualifiedName();
        } else return false;
        it.kind = IrDataItem::Address;
        out.push_back(it);
        return true;
    }
    if (!x->semaConst || !u->isIntegral()) return false;
    long v = x->semaValue;
    if (u->kind == sema::Type::Bool) v = v != 0;
    else if (size == 1) v &= 0xFF;
    else if (size == 2) v &= 0xFFFF;
    it.kind = IrDataItem::Int;
    it.value = v;
    out.push_back(it);
    return true;
}

// an array, or a plain aggregate class (no constructor, destructor, base, virtual function, union or bit-field;
// the layout ignores bit-fields, so such a class keeps being built by code): the elements in order, the padding
// and the members without an initializer zero
bool IrGen::aggregateData(const sema::Type* t, const std::vector<cxx::Expr*>* elems, std::vector<IrDataItem>& out, int depth)
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    const size_t given = elems ? elems->size() : 0;
    if (u->kind == sema::Type::Array) {
        if (u->arraySize <= 0 || given > static_cast<size_t>(u->arraySize)) return false;
        for (long i = 0; i < u->arraySize; ++i)
            if (!constantData(u->inner, static_cast<size_t>(i) < given ? (*elems)[static_cast<size_t>(i)] : 0, out, depth + 1)) return false;
        return true;
    }
    Symbol* c = u->kind == sema::Type::Class ? u->decl : 0;
    if (!c || !c->layoutDone || c->isUnion || c->isPolymorphic || !c->bases.empty() || needsDefaultCtor(u) || needsDtor(u)) return false;
    if (given > c->fields.size()) return false;
    int at = 0;
    for (size_t f = 0; f < c->fields.size(); ++f) {
        Symbol* fld = c->fields[f];
        if (isReference(fld->type) || fld->offset < at) return false;
        if (fld->declAst)
            for (size_t k = 0; k < fld->declAst->declarators.size(); ++k)
                if (fld->declAst->declarators[k].decl && fld->declAst->declarators[k].decl->semaSym == fld && fld->declAst->declarators[k].decl->bitfield) return false;
        zeroData(out, fld->offset - at);
        if (!constantData(fld->type, f < given ? (*elems)[f] : 0, out, depth + 1)) return false;
        at = fld->offset + sizeOf(fld->type);
    }
    if (at > sizeOf(u)) return false;
    zeroData(out, sizeOf(u) - at);
    return true;
}

// const all the way down (the elements of an array) and no mutable member: the object can live in .rdata
bool IrGen::readOnlyObject(const sema::Type* t) const
{
    const sema::Type* e = m_semaTypes.unref(t);
    while (e && e->kind == sema::Type::Array) { if (e->isConst) return true; e = e->inner; }
    if (!e || !e->isConst) return false;
    if (e->kind == sema::Type::Class && e->decl)
        for (size_t f = 0; f < e->decl->fields.size(); ++f) if (e->decl->fields[f]->isMutable) return false;
    return true;
}

// as cl does: once an object with a destructor is built, an internal thunk that destroys it is registered with
// atexit, so the objects are destroyed after main in the reverse order of their construction
void IrGen::registerAtExit(const std::string& global, const sema::Type* t, int line)
{
    ExitThunk x;
    // a C++ name ("cppparse_atexit_0()"), so that its address and its definition get the same symbol; internal:
    // a static local of an inline function registers the thunk of the unit that built it
    x.fn = m_prog.newSymbol(Symbol::Function, "cppparse_atexit_" + numText(static_cast<long>(m_exitThunks.size())), m_prog.global);
    x.fn->type = m_semaTypes.function(m_semaTypes.basic(sema::Type::Void), std::vector<const sema::Type*>(), false, false);
    x.fn->isStatic = true;
    x.fn->hasBody = true;
    x.fn->loc.line = line;
    x.global = global;
    x.type = t;
    m_exitThunks.push_back(x);
    IrInst fa = make(IrInst::AddrGlobal, 0, line);
    fa.type = m_types.function(oldType(m_semaTypes.basic(sema::Type::Void)));
    fa.args.push_back(IrOperand::global(irName(x.fn), fa.type));
    IrInst in = make(IrInst::Call, m_semaTypes.basic(sema::Type::Int), line);
    in.sub = "atexit";
    in.args.push_back(IrOperand::temp(emit(fa), fa.type));
    emit(in);
}

void IrGen::genExitThunks()
{
    for (; m_exitThunksDone < m_exitThunks.size(); ++m_exitThunksDone) {
        const ExitThunk x = m_exitThunks[m_exitThunksDone];
        const int line = x.fn->loc.line;
        m_out = new IrFunction;
        m_fn = x.fn;
        m_slots.clear(); m_loops.clear(); m_tryStack.clear(); m_tryCount = 0;
        prologue(x.fn);
        LValue lv; lv.kind = LValue::Global; lv.global = x.global; lv.type = x.type;
        destroy(x.type, addressOf(lv, line), line, false);
        returnSequence(line);
        emit(make(IrInst::Ret, 0, line));
        finishFunction(line);
    }
}

} // namespace cppparse
