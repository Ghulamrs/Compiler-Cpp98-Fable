// IrGen.cpp - version 6 stage 4: IR generation from the analysed syntax tree. See IrGen.hpp.
#include "IrGen.hpp"
#include "Mangle.hpp"
#include "PathUtil.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
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

// cl's name of the vector deleting destructor from the destructor's (or its thunk's): ??1D@@UEAA@XZ becomes
// ??_ED@@UEAAPEAXI@Z, void* (unsigned int); the adjustor thunk ??_ED@@W7EAAPEAXI@Z
std::string deletingName(const std::string& dtor)
{
    if (dtor.size() < 6 || dtor.compare(0, 3, "??1") != 0 || dtor.compare(dtor.size() - 3, 3, "@XZ") != 0) return dtor + "_E";
    return "??_E" + dtor.substr(3, dtor.size() - 6) + "PEAXI@Z";
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

static Symbol* namedLocal(cxx::Expr* e);

IrGen::IrGen(sema::Program& program, sema::TypeTable& semaTypes)
    : m_prog(program), m_semaTypes(semaTypes), m_types(m_emptyCb), m_outList(0), m_generated(0), m_out(0), m_fn(0), m_block(0), m_sretSlot(-1), m_retReg(0), m_nrvoEnabled(false), m_nrvo(0), m_tryCount(0),
      m_initSymbol(0), m_exitThunksDone(0), m_x86(0), m_msvcNames(false), m_deletingDone(0), m_ctorFlag(1), m_vbFlagSlot(-1),m_staticCount(0), m_guardCount(0), m_unwindId(-1), m_bodyBlock(-1) {}

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
        if (derived->bases[i].isVirtual) continue;   // its offset holds only in a complete object: virtualBasePath
        if (baseOffset(derived->bases[i].cls, base, o, depth + 1)) { offset = derived->bases[i].offset + o; return true; }
    }
    return false;
}

bool IrGen::virtualBasePath(Symbol* derived, Symbol* base, int& index, int& rest) const
{
    if (!derived || !base || derived->vbases.empty()) return false;
    const sema::VbTable* table = 0;
    for (size_t t = 0; t < derived->vbtables.size(); ++t) if (derived->vbtables[t].offset == derived->vbptrOffset) table = &derived->vbtables[t];
    if (!table) return false;
    for (size_t k = 0; k < table->order.size(); ++k) {
        int o = 0;
        if (baseOffset(table->order[k], base, o)) { index = static_cast<int>(k) + 1; rest = o; return true; }
    }
    return false;
}

void IrGen::unsupported(int line, const std::string& what)
{
    m_unsupported.push_back("line " + numText(static_cast<long>(line)) + ": " + what);
}

// a class some class in the program has as a virtual base
bool IrGen::isVirtualBaseAnywhere(Symbol* cls) const
{
    for (size_t i = 0; i < m_prog.symbols.size(); ++i) {
        const Symbol* s = m_prog.symbols[i];
        if (s->kind != Symbol::Class) continue;
        for (size_t v = 0; v < s->vbases.size(); ++v) if (s->vbases[v] == cls) return true;
    }
    return false;
}

// .* and ->* on a class with virtual bases: cl's member pointers into virtual bases carry a vbtable index (S132)
void IrGen::memberPointerCheck(cxx::Expr* e)
{
    if (e->kind != cxx::Expr::Binary || (e->text != ".*" && e->text != "->*") || e->kids.empty()) return;
    const sema::Type* ot = m_semaTypes.unref(exprType(e->kids[0]));
    if (ot && ot->kind == sema::Type::Pointer) ot = ot->inner;
    ot = ot ? m_semaTypes.unqualified(ot) : 0;
    if (ot && ot->kind == sema::Type::Class && hasVbases(ot->decl))
        unsupported(e->loc.line, "pointer to member of '" + ot->decl->qualifiedName() + "', a class with virtual bases, is not supported yet by --compile6");
}

// MSVC's conversion to a virtual base: the vbptr's table holds the base's distance from the vbptr
int IrGen::toBase(int addr, Symbol* derived, Symbol* base, int line)
{
    const sema::Type* dp = m_semaTypes.pointer(m_semaTypes.classType(derived));
    const sema::Type* bp = m_semaTypes.pointer(m_semaTypes.classType(base));
    int off = 0, index = 0, rest = 0;
    if (baseOffset(derived, base, off)) return off ? offsetAddress(addr, dp, bp, off, line) : addr;
    if (!virtualBasePath(derived, base, index, rest)) { unsupported(line, "internal error: no path from '" + derived->qualifiedName() + "' to its base '" + base->qualifiedName() + "'"); return addr; }
    const sema::Type* it = m_semaTypes.basic(sema::Type::Int);
    const sema::Type* ip = m_semaTypes.pointer(it);
    const sema::Type* cp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Char));
    int vbptr = offsetAddress(addr, dp, cp, derived->vbptrOffset, line);
    LValue pl; pl.kind = LValue::Mem; pl.addr = vbptr; pl.type = ip;
    int table = load(pl, ip, line);
    int entry = offsetAddress(table, ip, ip, 4L * index, line);
    LValue el; el.kind = LValue::Mem; el.addr = entry; el.type = it;
    int disp = convert(load(el, it, line), it, m_semaTypes.basic(sema::Type::LongLong), line);
    IrInst in = make(IrInst::AddrIndex, bp, line);
    in.extra = 1;
    in.args.push_back(IrOperand::temp(vbptr, oldType(cp)));
    in.args.push_back(IrOperand::temp(disp, oldType(m_semaTypes.basic(sema::Type::LongLong))));
    int r = emit(in);
    return rest ? offsetAddress(r, bp, bp, rest, line) : r;
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
            int index = 0, rest = 0;
            const bool viaVbase = !baseOffset(fc->decl, tc->decl, off) && virtualBasePath(fc->decl, tc->decl, index, rest);
            if (baseOffset(fc->decl, tc->decl, off) && off > 0) delta = off;                 // derived to base
            else if (baseOffset(tc->decl, fc->decl, off) && off > 0) delta = -off;           // base to derived (static_cast)
            int dummyIndex = 0, dummyRest = 0;
            if (!viaVbase && !baseOffset(tc->decl, fc->decl, off) && virtualBasePath(tc->decl, fc->decl, dummyIndex, dummyRest)) {
                // ill-formed (cl C2635): a virtual base cannot be converted down to the derived class
                unsupported(line, "cannot convert from '" + fc->decl->qualifiedName() + "' to '" + tc->decl->qualifiedName() + "': '" + fc->decl->qualifiedName() + "' is a virtual base");
                return temp;
            }
            if (viaVbase) {
                // through the vbtable; a null pointer stays null
                const sema::Type* fp = from->kind == sema::Type::Pointer ? from : m_semaTypes.pointer(from);
                const sema::Type* tp = to->kind == sema::Type::Pointer ? to : m_semaTypes.pointer(to);
                if (from->kind != sema::Type::Pointer) return toBase(temp, fc->decl, tc->decl, line);
                int slot = newLocal("tmp.cast", tp, false);
                LValue lv; lv.kind = LValue::Local; lv.local = slot; lv.type = tp;
                store(lv, constant(0, tp, line), tp, line);
                int nb = m_out->newBlock("cast.nonnull"), jb = m_out->newBlock("cast.end");
                condbr(toBool(temp, fp, line), nb, jb, line);
                m_block = nb;
                store(lv, toBase(temp, fc->decl, tc->decl, line), tp, line);
                br(jb, line);
                m_block = jb;
                return load(lv, tp, line);
            }
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
    memberPointerCheck(e);
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
            if (m_fn && m_fn->ownerClass && s->ownerClass && m_fn->ownerClass != s->ownerClass) { int bo = 0; if (baseOffset(m_fn->ownerClass, s->ownerClass, bo)) off += bo; else self = toBase(self, m_fn->ownerClass, s->ownerClass, line); }
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
        if (objT->kind == sema::Type::Class && objT->decl && s->ownerClass && objT->decl != s->ownerClass) { int bo = 0; if (baseOffset(objT->decl, s->ownerClass, bo)) off += bo; else base = toBase(base, objT->decl, s->ownerClass, line); }
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
        bool converted = isClassType(target) && (at->kind != sema::Type::Class || (at != tu && !(at->decl && tu->decl && isBaseOf(at->decl, tu->decl))));
        LValue al;
        // a class prvalue built by a constructor is evaluated once, below (V& bound to D())
        const bool built = isClassType(exprType(a)) && !a->semaLvalue && a->semaSym && a->semaSym->kind == Symbol::Function && a->semaSym->isConstructor;
        if (!converted && !built) al = lvalue(a);
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
        int addr = materialize(a, ct, addressOf(tl, line), line);
        if (const sema::Type* it = regParamType(ct)) { LValue m; m.kind = LValue::Mem; m.addr = addr; m.type = it; return load(m, it, line); }   // its bytes in a register
        return addr;
    }
    int v = value(a);
    if (pt) v = convert(v, exprType(a), pt, line);
    return v;
}

int IrGen::callFunction(Symbol* fn, int thisPtr, int sretAddr, const std::vector<cxx::Expr*>& args, size_t firstArg, bool virtualCall, int line)
{
    need(fn);
    const int vbFlag = m_ctorFlag;   // taken before the arguments, whose own constructions pass 1
    m_ctorFlag = 1;
    const sema::Type* ft = fn->type;
    const sema::Type* rt = ft && ft->kind == sema::Type::Function ? ft->inner : m_semaTypes.basic(sema::Type::Void);
    bool sret = isClassType(rt) && !isReference(rt);
    const sema::Type* retReg = sret ? regReturnType(fn, rt) : 0;
    IrInst in = make(IrInst::Call, retReg ? retReg : sret ? m_semaTypes.basic(sema::Type::Void) : rt, line);
    in.sub = irName(fn);
    const bool vcall = virtualCall && fn->isVirtual && fn->vtableSlot >= 0 && thisPtr >= 0;
    if (thisPtr >= 0) {
        // MSVC's this adjustor: a virtual function first held by a secondary vftable is called with the address of
        // that vfptr (a virtual call loads the vptr there); a destructor takes the complete object
        const int adjust = vcall ? fn->vfptrOffset : fn->thisAdjust;
        const sema::Type* cp = m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass));
        if (adjust != 0) thisPtr = offsetAddress(thisPtr, cp, cp, adjust, line);
        in.args.push_back(IrOperand::temp(thisPtr, oldType(cp)));
        // the destructor's slot holds the deleting destructor: flags 0 destroys without freeing
        if (vcall && fn->isDestructor) in.args.push_back(IrOperand::imm(0, oldType(m_semaTypes.basic(sema::Type::UInt))));
        // MSVC: a constructor of a class with virtual bases takes "most derived" (1: build them) as its last
        // parameter, after this when it is variadic
        if (fn->isConstructor && hasVbases(fn->ownerClass) && ft && ft->variadic) in.args.push_back(IrOperand::imm(vbFlag, oldType(m_semaTypes.basic(sema::Type::Int))));
    }
    int retSlot = -1;
    if (sret) {
        if (sretAddr < 0) { retSlot = newLocal("tmp.ret", m_semaTypes.unqualified(rt), true); LValue tl; tl.kind = LValue::Local; tl.local = retSlot; tl.type = m_semaTypes.unqualified(rt); sretAddr = addressOf(tl, line); }
        if (!retReg) in.args.push_back(IrOperand::temp(sretAddr, oldType(m_semaTypes.pointer(rt))));
    }
    for (size_t a = firstArg; a < args.size(); ++a) {
        size_t p = a - firstArg;
        const sema::Type* pt = ft && p < ft->params.size() ? ft->params[p] : 0;
        // a class past the named parameters (variadic) goes as a by-value parameter of its type would
        if (!pt && isClassType(exprType(args[a])) && !isReference(exprType(args[a]))) pt = m_semaTypes.unqualified(m_semaTypes.unref(exprType(args[a])));
        int v = argument(args[a], pt, line);
        in.args.push_back(IrOperand::temp(v, oldType(pt ? (isReference(pt) ? m_semaTypes.pointer(m_semaTypes.unref(pt)) : (isClassType(pt) ? (regParamType(pt) ? regParamType(pt) : m_semaTypes.pointer(pt)) : pt)) : m_semaTypes.decay(exprType(args[a])))));
    }
    // default arguments
    if (ft) for (size_t p = args.size() - firstArg; p < ft->params.size() && p < fn->params.size(); ++p) {
        cxx::Expr* def = fn->params[p]->init;
        if (!def) break;
        std::vector<cxx::Expr*> one; one.push_back(def);
        int v = argument(def, ft->params[p], line);
        in.args.push_back(IrOperand::temp(v, oldType(isReference(ft->params[p]) ? m_semaTypes.pointer(m_semaTypes.unref(ft->params[p])) : regParamType(ft->params[p]) ? regParamType(ft->params[p]) : isClassType(ft->params[p]) ? m_semaTypes.pointer(ft->params[p]) : ft->params[p])));
    }
    if (fn->isConstructor && hasVbases(fn->ownerClass) && thisPtr >= 0 && !(ft && ft->variadic)) in.args.push_back(IrOperand::imm(vbFlag, oldType(m_semaTypes.basic(sema::Type::Int))));
    if (vcall) { in.op = IrInst::VCall; in.extra = fn->vtableSlot; }
    if (isReference(rt)) in.type = oldType(m_semaTypes.pointer(rt->inner));
    int r = emit(in);
    if (retReg) { LValue m; m.kind = LValue::Mem; m.addr = sretAddr; m.type = retReg; store(m, r, retReg, line); }   // the result's bytes came in rax
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
    if (cls->hasCopyCtor || cls->isPolymorphic || hasVbases(cls)) return true;   // a copy gets its own class's vptr (vbptrs), not the source's
    for (size_t b = 0; b < cls->bases.size(); ++b) if (cls->bases[b].cls && needsCopyCtor(cls->bases[b].cls->type, depth + 1)) return true;
    for (size_t f = 0; f < cls->fields.size(); ++f) if (!isReference(cls->fields[f]->type) && needsCopyCtor(cls->fields[f]->type, depth + 1)) return true;
    return false;
}

// MSVC x64: a class object of 1, 2, 4 or 8 bytes is passed as an integer of that size (in a register or its
// stack slot) unless copying it needs a constructor; then, like any other size, by address of a copy
const sema::Type* IrGen::regParamType(const sema::Type* t) const
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (!u || u->kind != sema::Type::Class || !u->decl || isReference(t) || needsCopyCtor(u)) return 0;
    if (hasVbases(u->decl)) return 0;
    switch (sizeOf(u)) {
    case 1: return m_semaTypes.basic(sema::Type::UChar);
    case 2: return m_semaTypes.basic(sema::Type::UShort);
    case 4: return m_semaTypes.basic(sema::Type::UInt);
    case 8: return m_semaTypes.basic(sema::Type::ULongLong);
    default: return 0;
    }
}

// a C++03 aggregate-like class (cl's rule): no user-declared constructor, destructor or copy assignment, no
// bases, no virtual functions, no private or protected data members, no reference members, members alike
bool IrGen::podForReturn(const sema::Type* t, int depth) const
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    while (u && u->kind == sema::Type::Array) u = m_semaTypes.unqualified(u->inner);
    if (!u || depth > 32) return false;
    if (u->kind != sema::Type::Class) return false;
    Symbol* cls = u->decl;
    if (!cls || cls->hasUserCtor || cls->hasCopyCtor || cls->hasDtor || cls->hasCopyAssign || cls->isPolymorphic || !cls->bases.empty()) return false;
    for (size_t f = 0; f < cls->fields.size(); ++f) {
        Symbol* m = cls->fields[f];
        if (m->access == cxx::AccessPrivate || m->access == cxx::AccessProtected) return false;
    }
    // members count only through the class's implicit members: a trivial destructor and a trivial,
    // not deleted copy assignment (a const or reference member deletes it)
    return !needsDtor(u) && !needsCopyAssign(u) && !copyAssignDeleted(u, 0);
}

bool IrGen::copyAssignDeleted(const sema::Type* t, int depth) const
{
    const sema::Type* u = m_semaTypes.unref(t);
    if (!u || depth > 32) return false;
    if (isReference(t)) return true;
    while (u->kind == sema::Type::Array) { if (u->isConst) return true; u = u->inner; }
    if (u->isConst) return true;
    u = m_semaTypes.unqualified(u);
    if (u->kind != sema::Type::Class || !u->decl || u->decl->hasCopyAssign) return false;
    for (size_t b = 0; b < u->decl->bases.size(); ++b) if (u->decl->bases[b].cls && copyAssignDeleted(u->decl->bases[b].cls->type, depth + 1)) return true;
    for (size_t f = 0; f < u->decl->fields.size(); ++f) if (copyAssignDeleted(u->decl->fields[f]->type, depth + 1)) return true;
    return false;
}

// the integer type a class result of fn comes back in (rax), or 0 for the hidden result pointer; member
// functions always use the hidden pointer (fn 0: a call through a pointer to function)
const sema::Type* IrGen::regReturnType(Symbol* fn, const sema::Type* rt) const
{
    if ((fn && fn->ownerClass && !fn->isStatic) || !rt || isReference(rt) || !isClassType(rt)) return 0;
    if (!podForReturn(rt, 0)) return 0;
    switch (sizeOf(m_semaTypes.unqualified(rt))) {
    case 1: return m_semaTypes.basic(sema::Type::UChar);
    case 2: return m_semaTypes.basic(sema::Type::UShort);
    case 4: return m_semaTypes.basic(sema::Type::UInt);
    case 8: return m_semaTypes.basic(sema::Type::ULongLong);
    default: return 0;
    }
}

bool IrGen::needsCopyAssign(const sema::Type* t, int depth) const
{
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(t));
    if (!u || depth > 32) return false;
    if (u->kind == sema::Type::Array) return needsCopyAssign(u->inner, depth + 1);
    if (u->kind != sema::Type::Class || !u->decl) return false;
    Symbol* cls = u->decl;
    if (cls->hasCopyAssign || cls->isPolymorphic || hasVbases(cls)) return true;   // an assignment keeps the target's vptr (vbptrs)
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
    if (cls->hasUserCtor || cls->isPolymorphic || hasVbases(cls)) return true;
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
    if (fn->thisAdjust != 0) thisPtr = offsetAddress(thisPtr, pt, pt, fn->thisAdjust, line);
    in.args.push_back(IrOperand::temp(thisPtr, oldType(pt)));
    if (argPtr >= 0) in.args.push_back(IrOperand::temp(argPtr, oldType(pt)));
    if (fn->isConstructor && hasVbases(fn->ownerClass)) in.args.push_back(IrOperand::imm(m_ctorFlag, oldType(m_semaTypes.basic(sema::Type::Int))));   // "most derived", last
    m_ctorFlag = 1;
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
    if (!sameClass && u->kind == sema::Type::Class && u->decl && xt->kind == sema::Type::Class && xt->decl && isBaseOf(xt->decl, u->decl)) {
        // an object of a derived class: its base part is copied (slicing)
        // a prvalue is evaluated once (value: a temporary destroyed at the end of the full-expression)
        int src;
        if (x->semaLvalue) { LValue xl = lvalue(e); src = xl.kind == LValue::None ? value(e) : addressOf(xl, line); }
        else src = value(e);
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

void IrGen::destroy(const sema::Type* t, int objAddr, int line, bool viaVtable, bool complete)
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
    // a complete object with virtual bases: cl's vbase destructor (??_D) destroys them after ~D
    if (complete && !virtualCall && hasVbases(u->decl)) { callSpecial(vbaseDestructor(u->decl), objAddr, -1, line); return; }
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
    if (!c.abortGuard.empty()) {
        // a static local's initializer threw: the guard goes back to "not built", so a later call retries
        LValue gl; gl.kind = LValue::Global; gl.global = c.abortGuard; gl.type = c.type;
        IrInst ab = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
        ab.sub = "_Init_thread_abort";
        ab.args.push_back(IrOperand::temp(addressOf(gl, line), oldType(m_semaTypes.pointer(c.type))));
        emit(ab);
        store(fl, constant(0, b, line), b, line);
        if (guarded) { br(next, line); m_block = next; }
        return;
    }
    int addr;
    if (c.viaPointer) { LValue pl; pl.kind = LValue::Local; pl.local = c.slot; pl.type = m_semaTypes.pointer(c.type); addr = load(pl, pl.type, line); }
    else { LValue ol; ol.kind = LValue::Local; ol.local = c.slot; ol.type = c.type; addr = addressOf(ol, line); }
    destroy(c.type, addr, line, false, !c.baseSubobject);
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
        // (*fp)(x), (fp)(x): the pointer itself is called
        while ((callee->kind == cxx::Expr::Paren || (callee->kind == cxx::Expr::Unary && callee->text == "*")) && callee->kids.size() == 1 && !callee->semaSym) {
            const sema::Type* kt = m_semaTypes.unref(exprType(callee->kids[0]));
            if (!kt || !((kt->kind == sema::Type::Pointer && kt->inner->kind == sema::Type::Function) || kt->kind == sema::Type::Function || callee->kind == cxx::Expr::Paren)) break;
            callee = callee->kids[0];
        }
        const sema::Type* ct = m_semaTypes.unref(exprType(callee));
        // "(*fp)(x)": the dereferenced pointer is the address called
        while (ct->kind == sema::Type::Function && callee->kind == cxx::Expr::Unary && callee->text == "*" && callee->kids.size() == 1) {
            callee = callee->kids[0];
            ct = m_semaTypes.unref(exprType(callee));
        }
        if (ct->kind == sema::Type::Function && callee->semaSym && callee->semaSym->kind == Symbol::Function) ct = m_semaTypes.pointer(ct);   // "(*f)(x)", f a function
        if (ct->kind == sema::Type::Pointer && ct->inner->kind == sema::Type::Function) {
            int fp = value(callee);
            in.args.push_back(IrOperand::temp(fp, oldType(ct)));
            in.sub = "(indirect)";
        } else {
            std::string name = callee->kind == cxx::Expr::Name ? callee->name.last() : callee->kind == cxx::Expr::Member ? callee->name.last() : "?";
            if (callee->kind == cxx::Expr::Member) { cxx::Expr* obj = callee->kids[0]; in.args.push_back(IrOperand::temp(callee->arrowMember ? value(obj) : addressOf(lvalue(obj), line), oldType(m_semaTypes.decay(exprType(obj))))); }
            in.sub = name + "?";
        }
        const sema::Type* pft = ct->kind == sema::Type::Pointer && ct->inner->kind == sema::Type::Function ? ct->inner : 0;
        for (size_t a = 1; a < e->kids.size(); ++a) {
            const sema::Type* pt = pft && a - 1 < pft->params.size() ? pft->params[a - 1] : 0;
            if (pt && isClassType(pt) && !isReference(pt)) { int v = argument(e->kids[a], pt, line); in.args.push_back(IrOperand::temp(v, oldType(regParamType(pt) ? regParamType(pt) : m_semaTypes.pointer(pt)))); continue; }
            int v = value(e->kids[a]); in.args.push_back(IrOperand::temp(v, oldType(m_semaTypes.decay(exprType(e->kids[a])))));
        }
        if (!rt || rt->kind == sema::Type::Error || rt->kind == sema::Type::Dependent) in.type = m_types.builtin(Type::Long);
        if (pft && isClassType(rt) && !isReference(rt)) {
            // a class result: the hidden result pointer after the callee, or the bytes in rax
            const sema::Type* ru = m_semaTypes.unqualified(m_semaTypes.unref(rt));
            int slot = sretAddr >= 0 ? -1 : newLocal("tmp.ret", ru, true);
            int addr = sretAddr;
            if (slot >= 0) { LValue tl; tl.kind = LValue::Local; tl.local = slot; tl.type = ru; addr = addressOf(tl, line); }
            const sema::Type* retReg = regReturnType(0, ru);
            if (retReg) in.type = oldType(retReg);
            else { in.type = oldType(m_semaTypes.basic(sema::Type::Void)); in.args.insert(in.args.begin() + 1, IrOperand::temp(addr, oldType(m_semaTypes.pointer(ru)))); }
            int r = emit(in);
            if (retReg) { LValue m; m.kind = LValue::Mem; m.addr = addr; m.type = retReg; store(m, r, retReg, line); }
            if (slot >= 0 && needsDtor(ru)) registerCleanup(slot, ru, false, 1, line);
            return addr;
        }
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
    if (callee->kind == cxx::Expr::Member && fn->ownerClass && !fn->isStatic && fn->isOperator && fn->name == "operator()" &&
        callee->semaSym && callee->semaSym->kind == Symbol::Variable) {
        // "h.f(1)", "p->f(1)" with f a data member of class type: operator() on the member itself
        thisPtr = addressOf(lvalue(callee), line);
        const sema::Type* memT = m_semaTypes.unqualified(m_semaTypes.unref(exprType(callee)));
        if (memT->kind == sema::Type::Class && memT->decl && memT->decl != fn->ownerClass) thisPtr = convert(thisPtr, m_semaTypes.pointer(memT), m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass)), line);
        virtualCall = true;
    } else if (callee->kind == cxx::Expr::Member && fn->ownerClass && !fn->isStatic) {
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
    memberPointerCheck(e);
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
        // the class's own operator new / new[] (its own or inherited, as lookup finds it), unless ::new
        Symbol* classNew = !e->globalScope && e->placement.empty() && isClassType(elem) && elem->decl
            ? classOperatorDelete(elem->decl, 0, isArray ? "operator new[]" : "operator new", 1) : 0;
        if (classNew) need(classNew);
        if (isArray) {
            cxx::Expr* countExpr = e->type->decl->parts.back().arraySize;
            const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
            int count = countExpr ? convert(value(countExpr), exprType(countExpr), lng, line) : constant(1, lng, line);
            int bytes = binary("*", count, lng, constant(sizeOf(elem) > 0 ? sizeOf(elem) : 1, lng, line), lng, lng, line);
            bool header = needsDtor(elem);   // the element count is kept in front of the elements for delete[]
            if (header) bytes = binary("+", bytes, lng, constant(8, lng, line), lng, lng, line);   // an 8-byte cookie, as MSVC
            n.sub = classNew ? irName(classNew) : std::string("__new_array");
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
            n.sub = classNew ? irName(classNew) : std::string("__new");
            n.args.push_back(IrOperand::imm(sizeOf(elem) > 0 ? sizeOf(elem) : 1, classNew ? oldType(m_semaTypes.sizeType()) : m_types.intType()));
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
        Symbol* vdtor = !e->arrayDelete && pt->kind == sema::Type::Pointer && isClassType(pt->inner) && pt->inner->decl ? findSpecial(pt->inner->decl, '~') : 0;
        if (vdtor && vdtor->isVirtual && vdtor->vtableSlot >= 0) {
            // a virtual destructor: the slot's deleting destructor destroys the complete object and frees it (flags
            // 1), as cl calls it, so a pointer to a secondary base frees the right address
            need(vdtor);
            const sema::Type* cp = m_semaTypes.pointer(m_semaTypes.classType(vdtor->ownerClass));
            IrInst in = make(IrInst::VCall, m_semaTypes.basic(sema::Type::Void), line);
            in.sub = irName(vdtor);
            in.extra = vdtor->vtableSlot;
            in.args.push_back(IrOperand::temp(vdtor->vfptrOffset ? offsetAddress(p, cp, cp, vdtor->vfptrOffset, line) : p, oldType(cp)));
            in.args.push_back(IrOperand::imm(1, oldType(m_semaTypes.basic(sema::Type::UInt))));
            emit(in);
            br(xb, line);
            m_block = xb;
            return constant(0, m_semaTypes.basic(sema::Type::Int), line);
        }
        Symbol* adtor = dtors && e->arrayDelete && isClassType(pt->inner) && pt->inner->decl ? findSpecial(pt->inner->decl, '~') : 0;
        if (adtor && adtor->isVirtual && adtor->vtableSlot >= 0) {
            // delete[] of a class with a virtual destructor, as cl: an empty array is freed from its cookie, any
            // other goes to the slot's vector deleting destructor with flags 3 (destroy the elements, free)
            need(adtor);
            const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
            const sema::Type* cp = m_semaTypes.pointer(m_semaTypes.classType(adtor->ownerClass));
            IrInst hd = make(IrInst::AddrIndex, pt, line); hd.extra = 1; hd.args.push_back(IrOperand::temp(p, oldType(pt))); hd.args.push_back(IrOperand::imm(-8, oldType(lng)));
            const int cookie = emit(hd);
            LValue hl; hl.kind = LValue::Mem; hl.addr = cookie; hl.type = lng;
            const int count = load(hl, lng, line);
            int vb = m_out->newBlock("delete.vector"), zb = m_out->newBlock("delete.empty");
            condbr(toBool(count, lng, line), vb, zb, line);
            m_block = zb;
            freeArray(pt->inner->decl, cookie, count, line);
            br(xb, line);
            m_block = vb;
            IrInst in = make(IrInst::VCall, m_semaTypes.basic(sema::Type::Void), line);
            in.sub = irName(adtor);
            in.extra = adtor->vtableSlot;
            in.args.push_back(IrOperand::temp(adtor->vfptrOffset ? offsetAddress(p, cp, cp, adtor->vfptrOffset, line) : p, oldType(cp)));
            in.args.push_back(IrOperand::imm(3, oldType(m_semaTypes.basic(sema::Type::UInt))));
            emit(in);
            br(xb, line);
            m_block = xb;
            return constant(0, m_semaTypes.basic(sema::Type::Int), line);
        }
        if (!e->arrayDelete && pt->kind == sema::Type::Pointer) destroy(pt->inner, p, line, true);
        else if (e->arrayDelete && pt->kind == sema::Type::Pointer && needsDtor(pt->inner)) {
            const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
            IrInst hd = make(IrInst::AddrIndex, pt, line); hd.extra = 1; hd.args.push_back(IrOperand::temp(p, oldType(pt))); hd.args.push_back(IrOperand::imm(-8, oldType(lng)));
            int header = emit(hd);
            LValue hl; hl.kind = LValue::Mem; hl.addr = header; hl.type = lng;
            const int count = load(hl, lng, line);
            forEachElement(pt->inner, p, count, false, line);
            p = header;
            if (isClassType(pt->inner) && pt->inner->decl && !e->globalScope) {
                // from the cookie through the class's operator delete[] (count * size + 8), as cl
                freeArray(pt->inner->decl, header, count, line);
                if (xb >= 0) { br(xb, line); m_block = xb; }
                return constant(0, m_semaTypes.basic(sema::Type::Int), line);
            }
        }
        if (e->arrayDelete || pt->kind != sema::Type::Pointer || !isClassType(pt->inner) || e->globalScope || !freeObject(pt->inner->decl, p, line)) {
            IrInst in = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
            in.sub = e->arrayDelete ? "__delete_array" : "__delete";
            in.args.push_back(IrOperand::temp(p, oldType(pt)));
            emit(in);
        }
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
                Symbol* dtor = needsDtor(tt) ? (hasVbases(tt->decl) ? vbaseDestructor(tt->decl) : findSpecial(tt->decl, '~')) : 0;   // a complete object: ??_D with virtual bases
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
            if (src->semaSym && src->semaSym->kind == Symbol::Function && src->semaSym->isConstructor && src->semaSym->ownerClass == u->decl) { construct(src->semaSym, addr, id.init.args, 0, line); return; }   // not a derived class's constructor (V v = D())
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
            long block = 2;
            if (m_msvcNames && m_x86) {
                // cl numbers the blocks of a function; the body is 2 (numberStaticBlocks). Without that number
                // (no body walked): 2 plus the block's position among the function's blocks, unique only
                std::map<Symbol*, long>::const_iterator nb = m_staticBlocks.find(var);
                if (nb != m_staticBlocks.end()) block = nb->second;
                else if (var->scope && !(var->scope->parent && var->scope->parent->kind == Scope::Function)) {
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
                // cl's thread-safe statics, so that an inline function's object (a COMDAT shared with cl's units)
                // is built once whichever side runs first: an int guard "$TSS<n>", 0 before, -1 while some
                // thread builds the object, then the CRT's epoch (negative). A guard of 0 or -1 leads to
                // _Init_thread_header, which leaves it -1 when this thread is to run the initializer (it then
                // calls _Init_thread_footer) and waits for another thread that is running it. cl's fast path
                // compares with the thread's epoch instead (a TLS read); this test is equivalent for its values
                const sema::Type* it = m_semaTypes.basic(sema::Type::Int);
                const sema::Type* ut = m_semaTypes.basic(sema::Type::UInt);
                const sema::Type* b = m_semaTypes.basic(sema::Type::Bool);
                IrGlobal guard;
                guard.name = gname + "$guard";
                guard.size = 4;
                guard.align = 4;
                guard.isFloat = false;
                guard.linkOnce = m_out->linkOnce;
                guard.internal = !guard.linkOnce;
                if (m_x86) {
                    if (m_msvcNames) m_x86->registerGlobalName(guard.name, msvcMangleStaticGuard(*m_fn, block, m_guardCount));
                    m_x86->addGlobal(guard);
                }
                ++m_guardCount;
                LValue gl; gl.kind = LValue::Global; gl.global = guard.name; gl.type = it;
                int done = m_out->newBlock("static.done"), check = m_out->newBlock("static.lock"), doInit = m_out->newBlock("static.init");
                // one statement per emitted value: the order of a call's arguments differs between compilers,
                // and the self-built compiler must produce the same code as the MSVC-built one
                int g0 = load(gl, it, line);
                int one = constant(1, ut, line);
                int biased = binary("+", g0, ut, one, ut, ut, line);
                int one2 = constant(1, ut, line);
                condbr(binary("<=", biased, ut, one2, ut, b, line), check, done, line);
                m_block = check;
                IrInst enter = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
                enter.sub = "_Init_thread_header";
                enter.args.push_back(IrOperand::temp(addressOf(gl, line), oldType(m_semaTypes.pointer(it))));
                emit(enter);
                int g1 = load(gl, it, line);
                int minus1 = constant(-1, it, line);
                condbr(binary("==", g1, it, minus1, it, b, line), doInit, done, line);
                m_block = doInit;
                // an initializer that throws hands the guard to _Init_thread_abort (as cl's unwind funclet
                // does): an unwind-only cleanup whose flag is set while the initializer runs
                Cleanup ac;
                ac.type = it;
                ac.unwindOnly = true;
                ac.abortGuard = guard.name;
                ac.flag = newLocal("flag." + guard.name, b, true);
                LValue afl; afl.kind = LValue::Local; afl.local = ac.flag; afl.type = b;
                int on = constant(1, b, line);
                store(afl, on, b, line);
                m_flagged.push_back(ac);
                LValue lv; lv.kind = LValue::Global; lv.global = gname; lv.type = m_semaTypes.unref(var->type);
                initObject(lv, var->type, var, id, line);
                endFullExpr(line);
                int off = constant(0, b, line);
                store(afl, off, b, line);
                if (destroyed) registerAtExit(gname, lv.type, line);
                IrInst leave = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
                leave.sub = "_Init_thread_footer";
                leave.args.push_back(IrOperand::temp(addressOf(gl, line), oldType(m_semaTypes.pointer(it))));
                emit(leave);
                br(done, line);
                m_block = done;
            }
            continue;
        }
        if (var == m_nrvo) {
            // NRVO: built in the caller's result object; the returns copy nothing, and only an exception that
            // leaves the function destroys it
            const int line = id.decl->loc.line;
            const sema::Type* vt = var->type;
            LValue sl; sl.kind = LValue::Local; sl.local = m_sretSlot; sl.type = m_semaTypes.pointer(vt);
            LValue rl; rl.kind = LValue::Mem; rl.addr = load(sl, sl.type, line); rl.type = vt;
            // the name is in scope in its own initializer (A a = &a): reads go through the hidden pointer
            m_slots[var] = m_sretSlot;
            var->type = m_semaTypes.reference(vt);
            initObject(rl, vt, var, id, line);
            if (needsDtor(vt)) registerCleanup(m_sretSlot, vt, true, 2, line);
            endFullExpr(line);
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

// a declaration as the condition of if, while, for or switch: the variable is built (in the scope the caller
// opened) and its value returned, a class object's through its conversion function; t receives the value's type
int IrGen::conditionDecl(cxx::Decl* d, const sema::Type*& t, int line)
{
    localDecl(d);
    Symbol* v = d->declarators.empty() || !d->declarators[0].decl ? 0 : d->declarators[0].decl->semaSym;
    t = m_semaTypes.basic(sema::Type::Int);
    if (!v || m_slots.find(v) == m_slots.end()) return constant(0, t, line);
    LValue lv; lv.kind = LValue::Local; lv.local = m_slots[v]; lv.type = m_semaTypes.unref(v->type);
    const sema::Type* u = m_semaTypes.unqualified(m_semaTypes.unref(v->type));
    if (u && u->kind == sema::Type::Class && u->decl && u->decl->members) {
        Symbol* conv = 0;
        const std::vector<Symbol*>& ms = u->decl->members->order;
        for (size_t i = 0; i < ms.size() && !conv; ++i)
            if (ms[i]->kind == Symbol::Function && ms[i]->isConversion && ms[i]->type && ms[i]->type->inner && ms[i]->type->inner->isScalar()) conv = ms[i];
        if (!conv) { m_notes.push_back("L" + numText(line) + ": no conversion of the condition's " + u->decl->qualifiedName()); return constant(0, t, line); }
        std::vector<cxx::Expr*> none;
        int obj = isReference(v->type) ? load(lv, v->type, line) : addressOf(lv, line);
        obj = adjustThis(conv, obj, u, line);
        t = conv->type->inner;
        return callFunction(conv, obj, -1, none, 0, true, line);
    }
    t = lv.type;
    return load(lv, v->type, line);
}

// a loop whose condition declares a variable: the variable is built from the condition on every pass and
// destroyed at the end of each pass, when the condition is false, and on break and continue (as cl does). The
// body and a break reach the latch, which destroys it and repeats (to next) while the condition held
void IrGen::declLoop(cxx::Stmt* s, int cb, int next, int xb, int line)
{
    const sema::Type* b = m_semaTypes.basic(sema::Type::Bool);
    const int bb = m_out->newBlock("loop.body"), latch = m_out->newBlock("loop.latch"), brk = m_out->newBlock("loop.break");
    const int ok = newLocal("cond.ok", b, false);
    LValue okl; okl.kind = LValue::Local; okl.local = ok; okl.type = b;
    m_block = cb;
    openScope();
    const sema::Type* t = 0;
    int c = toBool(conditionDecl(s->decl, t, line), t, line);
    endFullExpr(line);
    store(okl, c, b, line);
    condbr(c, bb, latch, line);
    LoopTargets lt; lt.breakBlock = brk; lt.continueBlock = latch; lt.tryDepth = m_tryStack.size(); lt.cleanupDepth = m_cleanups.size(); m_loops.push_back(lt);
    m_block = bb; scopedStmt(s->kids.empty() ? 0 : s->kids[0], line); br(latch, line);
    m_loops.pop_back();
    m_block = brk; store(okl, constant(0, b, line), b, line); br(latch, line);
    m_block = latch;
    closeScope(line);
    int again = load(okl, b, line);
    condbr(again, next, xb, line);
    m_block = xb;
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
        if (s->decl) { const sema::Type* t = 0; int c = toBool(conditionDecl(s->decl, t, line), t, line); endFullExpr(line); condbr(c, tb, eb >= 0 ? eb : join, line); }
        else condition(s->expr, tb, eb >= 0 ? eb : join);
        m_block = tb; scopedStmt(s->kids[0], line); br(join, line);
        if (eb >= 0) { m_block = eb; scopedStmt(s->kids[1], line); br(join, line); }
        m_block = join;
        closeScope(line);
        return;
    }
    case cxx::Stmt::While: {
        if (s->decl) {
            int dc = m_out->newBlock("while.cond"), dx = m_out->newBlock("while.end");
            br(dc, line);
            declLoop(s, dc, dc, dx, line);
            return;
        }
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
        if (s->decl) {
            int cb = m_out->newBlock("for.cond"), sb = m_out->newBlock("for.step"), xb = m_out->newBlock("for.end");
            br(cb, line);
            declLoop(s, cb, sb, xb, line);
            m_block = sb; if (s->expr2) { value(s->expr2); endFullExpr(line); } br(cb, line);
            m_block = xb;
            closeScope(line);
            return;
        }
        int cb = m_out->newBlock("for.cond"), bb = m_out->newBlock("for.body"), sb = m_out->newBlock("for.step"), xb = m_out->newBlock("for.end");
        br(cb, line);
        m_block = cb;
        if (s->expr) condition(s->expr, bb, xb); else br(bb, line);
        LoopTargets lt; lt.breakBlock = xb; lt.continueBlock = sb; lt.tryDepth = m_tryStack.size(); lt.cleanupDepth = m_cleanups.size(); m_loops.push_back(lt);
        m_block = bb; scopedStmt(s->kids[0], line); br(sb, line);
        m_loops.pop_back();
        m_block = sb; if (s->expr2) { value(s->expr2); endFullExpr(line); } br(cb, line);
        m_block = xb;
        closeScope(line);
        return;
    }
    case cxx::Stmt::Switch: {
        int v;
        const sema::Type* st;   // the controlling value's type
        if (s->decl) {          // "switch (int y = ...)": y lives through the body
            openScope();
            v = conditionDecl(s->decl, st, line);
            endFullExpr(line);
        } else {
            v = value(s->expr);
            st = exprType(s->expr);
            endFullExpr(line);
            openScope();
        }
        const sema::Type* vt = m_semaTypes.unqualified(m_semaTypes.decay(st));
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
            c.args.push_back(IrOperand::temp(convert(v, st, vt, line), oldType(vt))); c.args.push_back(IrOperand::temp(cv, oldType(vt)));
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
            } else if (isClassType(rt) && m_nrvo && namedLocal(s->expr) == m_nrvo) {
                // NRVO: the object already is the result
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
            {
                // a handler for a class used as a virtual base would need the thrown object's vbptr to find its
                // subobject: refused (S132)
                const sema::Type* hc = ct && ct->kind == sema::Type::Pointer ? m_semaTypes.unqualified(ct->inner) : ct;
                if (hc && hc->kind == sema::Type::Class && hc->decl && isVirtualBaseAnywhere(hc->decl))
                    unsupported(h->loc.line, "catching '" + hc->decl->qualifiedName() + "', a virtual base class, is not supported yet by --compile6");
            }
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

// the IR name of a class's vftable t (0: the one at offset 0, "vtable@D"; then "vtable@D{for PB@16}")
std::string IrGen::vtableName(const Symbol* cls, size_t t)
{
    std::string n = "vtable@" + cls->qualifiedName();
    if (t == 0 || t >= cls->vftables.size()) return n;
    const sema::VfTable& v = cls->vftables[t];
    return n + "{for " + (v.introducedBy ? v.introducedBy->qualifiedName() : std::string("?")) + "@" + numText(v.offset) + "}";
}

// every vfptr of the object: the one at offset 0, then those of the secondary vftables at their offsets
void IrGen::storeVptr(Symbol* cls, int objAddr, int line)
{
    if (!cls->isPolymorphic) return;
    const sema::Type* vp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
    const sema::Type* cp = m_semaTypes.pointer(m_semaTypes.classType(cls));
    const size_t n = cls->vftables.empty() ? 1 : cls->vftables.size();
    for (size_t t = 0; t < n; ++t) {
        IrInst vt = make(IrInst::AddrGlobal, vp, line);
        vt.args.push_back(IrOperand::global(vtableName(cls, t), oldType(m_semaTypes.basic(sema::Type::Void))));
        int vtp = emit(vt);
        int at = objAddr;
        if (t > 0) { IrInst fa = make(IrInst::AddrField, m_semaTypes.pointer(vp), line); fa.sub = "vptr"; fa.extra = cls->vftables[t].offset; fa.args.push_back(IrOperand::temp(objAddr, oldType(cp))); at = emit(fa); }
        IrInst st = make(IrInst::StoreMem, vp, line);
        st.sub = "vptr";
        st.args.push_back(IrOperand::temp(at, oldType(t > 0 ? m_semaTypes.pointer(vp) : cp)));
        st.args.push_back(IrOperand::temp(vtp, oldType(vp)));
        emit(st);
    }
}

// the IR name of a class's vbtable t ("vbtable@D", "vbtable@D{for VL@16}")
std::string IrGen::vbtableName(const Symbol* cls, size_t t)
{
    std::string n = "vbtable@" + cls->qualifiedName();
    if (t >= cls->vbtables.size() || cls->vbtables[t].path.empty()) return n;
    const sema::VbTable& v = cls->vbtables[t];
    return n + "{for " + v.path.back()->qualifiedName() + "@" + numText(v.offset) + "}";
}

// every vbptr of a complete object (MSVC: stored by the constructor that has the "most derived" flag)
void IrGen::storeVbptrs(Symbol* cls, int objAddr, int line)
{
    const sema::Type* vp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
    const sema::Type* cp = m_semaTypes.pointer(m_semaTypes.classType(cls));
    for (size_t t = 0; t < cls->vbtables.size(); ++t) {
        IrInst vt = make(IrInst::AddrGlobal, vp, line);
        vt.args.push_back(IrOperand::global(vbtableName(cls, t), oldType(m_semaTypes.basic(sema::Type::Int))));
        int vtp = emit(vt);
        IrInst fa = make(IrInst::AddrField, m_semaTypes.pointer(vp), line); fa.sub = "vbptr"; fa.extra = cls->vbtables[t].offset; fa.args.push_back(IrOperand::temp(objAddr, oldType(cp)));
        int at = emit(fa);
        IrInst st = make(IrInst::StoreMem, vp, line);
        st.sub = "vbptr";
        st.args.push_back(IrOperand::temp(at, oldType(m_semaTypes.pointer(vp))));
        st.args.push_back(IrOperand::temp(vtp, oldType(vp)));
        emit(st);
    }
}

// cl's vbase destructor (??_DD@@QEAAXXZ): ~D, then the virtual bases in the reverse of their order
Symbol* IrGen::vbaseDestructor(Symbol* cls)
{
    std::map<Symbol*, Symbol*>::iterator it = m_vbaseDtors.find(cls);
    if (it != m_vbaseDtors.end()) return it->second;
    Symbol* fn = m_prog.newSymbol(Symbol::Function, "`vbase destructor'", cls->members);
    fn->ownerClass = cls;
    fn->isImplicit = true;                 // one copy survives linking, as cl's COMDAT
    fn->hasBody = true;
    fn->loc = cls->loc;
    fn->type = m_semaTypes.function(m_semaTypes.basic(sema::Type::Void), std::vector<const sema::Type*>(), false, false);
    m_vbaseDtors[cls] = fn;
    m_vbaseDtorOf[fn] = cls;
    if (m_msvcNames && m_x86) {
        Symbol* dtor = findSpecial(cls, '~');
        const std::string d = dtor ? msvcMangleSymbol(*dtor) : std::string();   // ??1D@@QEAA@XZ
        const size_t at = d.find("@@");
        if (d.compare(0, 3, "??1") == 0 && at != std::string::npos) m_x86->registerSymbolName(irName(fn), "??_D" + d.substr(3, at - 3) + "@@QEAAXXZ");
    }
    return fn;
}

void IrGen::genVbaseDestructor(Symbol* fn)
{
    Symbol* cls = m_vbaseDtorOf[fn];
    const int line = fn->loc.line;
    const int self = thisTemp(line);
    if (Symbol* dtor = findSpecial(cls, '~')) callSpecial(dtor, self, -1, line);
    const sema::Type* cp = m_semaTypes.pointer(m_semaTypes.classType(cls));
    for (size_t v = cls->vbases.size(); v > 0; --v) {
        Symbol* vb = cls->vbases[v - 1];
        if (!needsDtor(vb->type)) continue;
        destroy(vb->type, offsetAddress(self, cp, m_semaTypes.pointer(vb->type), cls->vbaseOffsets[v - 1], line), line, false, false);
    }
}

void IrGen::prologue(Symbol* fn)
{
    m_nrvo = 0;
    m_out->name = irName(fn);
    m_out->fn = 0;
    m_out->variadic = fn->type && fn->type->kind == sema::Type::Function && fn->type->variadic;
    m_out->linkOnce = fn->isInline || fn->isImplicit || fn->instantiatedFrom != 0 || (fn->ownerClass && fn->ownerClass->instantiatedFrom != 0);
    m_out->internal = hasInternalLinkage(fn) || (fn->ownerClass && hasInternalLinkage(fn->ownerClass));
    if (m_out->internal) m_out->linkOnce = false;   // every unit keeps its own copy
    const sema::Type* rt = fn->type && fn->type->kind == sema::Type::Function ? fn->type->inner : m_semaTypes.basic(sema::Type::Void);
    bool sret = isClassType(rt) && !isReference(rt);
    m_retReg = sret ? regReturnType(fn, rt) : 0;
    m_out->retType = oldType(m_retReg ? m_retReg : sret ? m_semaTypes.basic(sema::Type::Void) : (isReference(rt) ? m_semaTypes.pointer(rt->inner) : rt));
    m_out->hasThis = fn->ownerClass && !fn->isStatic;
    if (m_out->hasThis) {
        const sema::Type* ct = m_semaTypes.classType(fn->ownerClass);
        if (fn->type && fn->type->constMethod) ct = m_semaTypes.qualified(ct, true, false);
        int slot = newLocal("this", m_semaTypes.pointer(ct), false);
        m_out->locals[static_cast<size_t>(slot)].isParam = true;
        m_out->locals[static_cast<size_t>(slot)].isThis = true;
        m_out->locals[static_cast<size_t>(slot)].nonNull = true;
    }
    m_vbFlagSlot = -1;
    // MSVC's "most derived" flag of a constructor of a class with virtual bases (1: it builds them): the last
    // parameter, the one after this when the constructor is variadic
    const bool vbFlag = m_out->hasThis && fn->isConstructor && hasVbases(fn->ownerClass);
    if (vbFlag && m_out->variadic) {
        m_vbFlagSlot = newLocal("vb.flag", m_semaTypes.basic(sema::Type::Int), false);
        m_out->locals[static_cast<size_t>(m_vbFlagSlot)].isParam = true;
    }
    m_sretSlot = -1;
    if (sret && !m_retReg) {
        m_sretSlot = newLocal("sret", m_semaTypes.pointer(rt), false);
        m_out->locals[static_cast<size_t>(m_sretSlot)].isParam = true;
        m_out->locals[static_cast<size_t>(m_sretSlot)].nonNull = true;
    }
    std::vector<std::pair<int, const sema::Type*> > byValue;   // class objects the callee owns
    std::vector<Symbol*> byValueParam;                          // their parameters
    std::vector<std::pair<int, Symbol*> > inRegister;           // small class objects that arrive as an integer
    for (size_t p = 0; p < fn->params.size(); ++p) {
        Symbol* ps = fn->params[p];
        const sema::Type* pt = ps->type;
        if (const sema::Type* it = regParamType(pt)) {
            int slot = newLocal(ps->name.empty() ? "arg" + numText(static_cast<long>(p)) : ps->name + ".bits", it, false);
            m_out->locals[static_cast<size_t>(slot)].isParam = true;
            inRegister.push_back(std::make_pair(slot, ps));
            continue;
        }
        if (isClassType(pt) && !isReference(pt)) pt = m_semaTypes.pointer(pt);   // objects arrive by address
        int slot = newLocal(ps->name.empty() ? "arg" + numText(static_cast<long>(p)) : ps->name, pt, false);
        m_out->locals[static_cast<size_t>(slot)].isParam = true;
        m_out->locals[static_cast<size_t>(slot)].nonNull = isReference(ps->type) || isClassType(ps->type);
        m_slots[ps] = slot;
        if (isClassType(ps->type) && !isReference(ps->type)) {
            byValue.push_back(std::make_pair(slot, ps->type));
            byValueParam.push_back(ps);
            ps->type = m_semaTypes.reference(ps->type);   // reads go through the pointer
        }
    }
    if (vbFlag && !m_out->variadic) {
        m_vbFlagSlot = newLocal("vb.flag", m_semaTypes.basic(sema::Type::Int), false);
        m_out->locals[static_cast<size_t>(m_vbFlagSlot)].isParam = true;
    }
    m_out->paramCount = static_cast<int>(m_out->locals.size());
    m_out->newBlock("entry");       // filled by finishFunction: flags, the unwind region, branch to the body
    m_bodyBlock = m_out->newBlock("body");
    m_block = m_bodyBlock;
    m_unwindId = m_tryCount++;
    m_tryStack.push_back(m_unwindId);
    m_cleanups.clear(); m_scopes.clear(); m_temps.clear(); m_flagged.clear(); m_labels.clear(); m_staticCount = 0; m_guardCount = 0;
    if (m_out->hasThis && fn->thisAdjust != 0) {
        // called with the address of the vfptr whose table first holds it (MSVC's this adjustor): back to the object
        const sema::Type* tp = m_semaTypes.pointer(m_semaTypes.classType(fn->ownerClass));
        LValue tl; tl.kind = LValue::Local; tl.local = 0; tl.type = tp;
        store(tl, offsetAddress(thisTemp(fn->loc.line), tp, tp, -fn->thisAdjust, fn->loc.line), tp, fn->loc.line);
    }
    for (size_t i = 0; i < inRegister.size(); ++i) {
        // the object lives in a local of the callee; the parameter name reaches it through a pointer, as for
        // objects passed by address
        Symbol* ps = inRegister[i].second;
        const sema::Type* ct = m_semaTypes.unqualified(m_semaTypes.unref(ps->type));
        const sema::Type* it = regParamType(ct);
        int obj = newLocal(ps->name.empty() ? "argobj" : ps->name + ".obj", ct, true);
        LValue ol; ol.kind = LValue::Local; ol.local = obj; ol.type = ct;
        int addr = addressOf(ol, fn->loc.line);
        LValue bl; bl.kind = LValue::Local; bl.local = inRegister[i].first; bl.type = it;
        LValue ml; ml.kind = LValue::Mem; ml.addr = addr; ml.type = it;
        store(ml, load(bl, it, fn->loc.line), it, fn->loc.line);
        int ptr = newLocal(ps->name.empty() ? "argptr" : ps->name, m_semaTypes.pointer(ct), false);
        m_out->locals[static_cast<size_t>(ptr)].nonNull = true;
        LValue pl; pl.kind = LValue::Local; pl.local = ptr; pl.type = m_semaTypes.pointer(ct);
        store(pl, addr, pl.type, fn->loc.line);
        m_slots[ps] = ptr;
        byValue.push_back(std::make_pair(ptr, ps->type));
        byValueParam.push_back(ps);
        ps->type = m_semaTypes.reference(ps->type);
    }
    if (m_retReg) {
        // the result is built in a local and its bytes are returned in rax (abiResult)
        int obj = newLocal("result", m_semaTypes.unqualified(rt), true);
        LValue ol; ol.kind = LValue::Local; ol.local = obj; ol.type = m_semaTypes.unqualified(rt);
        int addr = addressOf(ol, fn->loc.line);
        m_sretSlot = newLocal("sret", m_semaTypes.pointer(rt), false);
        m_out->locals[static_cast<size_t>(m_sretSlot)].nonNull = true;
        LValue pl; pl.kind = LValue::Local; pl.local = m_sretSlot; pl.type = m_semaTypes.pointer(rt);
        store(pl, addr, pl.type, fn->loc.line);
    }
    // the callee destroys its by-value parameters left to right, as cl does: registered from the last one
    for (size_t p = fn->params.size(); p-- > 0;)
        for (size_t i = 0; i < byValue.size(); ++i)
            if (byValueParam[i] == fn->params[p] && needsDtor(byValue[i].second)) registerCleanup(byValue[i].first, byValue[i].second, true, 0, fn->loc.line);
}

void IrGen::constructorInit(Symbol* fn, cxx::Decl* def)
{
    Symbol* cls = fn->ownerClass;
    if (!cls) return;
    const int line = fn->loc.line;
    int self = thisTemp(line);
    std::set<Symbol*> initialised;
    // MSVC: with the "most derived" flag set, the vbptrs are stored and the virtual bases built first, in cl's
    // order (at their places in this complete object); then the non-virtual bases, which get the flag 0
    std::vector<std::pair<Symbol*, int> > parts;
    std::vector<bool> virtualPart;
    int joinBlock = -1;
    if (hasVbases(cls) && m_vbFlagSlot >= 0) {
        const sema::Type* it = m_semaTypes.basic(sema::Type::Int);
        LValue fl; fl.kind = LValue::Local; fl.local = m_vbFlagSlot; fl.type = it;
        int vb = m_out->newBlock("vbases");
        joinBlock = m_out->newBlock("vbases.done");
        condbr(toBool(load(fl, it, line), it, line), vb, joinBlock, line);
        m_block = vb;
        storeVbptrs(cls, self, line);
        for (size_t v = 0; v < cls->vbases.size(); ++v) { parts.push_back(std::make_pair(cls->vbases[v], cls->vbaseOffsets[v])); virtualPart.push_back(true); }
    }
    const size_t firstNonVirtual = parts.size();
    for (size_t b = 0; b < cls->bases.size(); ++b)
        if (cls->bases[b].cls && !cls->bases[b].isVirtual) { parts.push_back(std::make_pair(cls->bases[b].cls, cls->bases[b].offset)); virtualPart.push_back(false); }
    // bases named in the initializer list, in declaration order of the bases
    for (size_t p = 0; p < parts.size(); ++p) {
        if (p == firstNonVirtual && joinBlock >= 0) { br(joinBlock, line); m_block = joinBlock; }
        Symbol* base = parts[p].first;
        const int boff = parts[p].second;
        cxx::MemInit* mi = 0;
        if (def) for (size_t i = 0; i < def->memInits.size(); ++i) {
            const std::string& n = def->memInits[i].name.last();
            if (n == base->name || n == base->ctorName() || def->memInits[i].name.text() == base->qualifiedName()) mi = &def->memInits[i];
        }
        int baddr = self;
        if (boff > 0) { IrInst adj = make(IrInst::AddrField, m_semaTypes.pointer(m_semaTypes.classType(base)), line); adj.sub = "base " + base->name; adj.extra = boff; adj.args.push_back(IrOperand::temp(self, m_out->locals[0].type)); baddr = emit(adj); }
        const std::vector<Symbol*>* ctors = base->members ? base->members->find(base->ctorName()) : 0;
        Symbol* chosen = 0;
        size_t nargs = mi ? mi->args.size() : 0;
        // the constructor overload resolution chose (sema); by the number of arguments only when there is none
        if (mi && mi->semaSym && mi->semaSym->isConstructor && mi->semaSym->ownerClass == base) chosen = mi->semaSym;
        if (ctors) for (size_t i = 0; i < ctors->size() && !chosen; ++i) { Symbol* c = (*ctors)[i]; if (c->kind == Symbol::Function && c->isConstructor && nargs >= static_cast<size_t>(c->requiredParams) && nargs <= c->params.size()) chosen = c; }
        m_ctorFlag = 0;   // a base, virtual or not, never builds its own virtual bases here
        if (chosen) { std::vector<cxx::Expr*> none; construct(chosen, baddr, mi ? mi->args : none, 0, mi ? mi->loc.line : line); }
        else constructDefault(base->type, baddr, line);
        m_ctorFlag = 1;
        if (mi) endFullExpr(mi->loc.line);
        if (needsDtor(base->type)) {
            // destroyed when the body throws
            int part = newLocal("part." + base->name, m_semaTypes.pointer(base->type), false);
            LValue pl; pl.kind = LValue::Local; pl.local = part; pl.type = m_semaTypes.pointer(base->type);
            store(pl, baddr, pl.type, line);
            m_flagged[static_cast<size_t>(registerCleanup(part, base->type, true, 2, line))].baseSubobject = true;
        }
    }
    if (parts.size() == firstNonVirtual && joinBlock >= 0) { br(joinBlock, line); m_block = joinBlock; }
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
                Symbol* chosen = (mi->semaSym && mi->semaSym->isConstructor && mi->semaSym->ownerClass == fu->decl) ? mi->semaSym : 0;
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
        if (!base || cls->bases[b - 1].isVirtual) continue;   // virtual bases: the vbase destructor (??_D)
        int baddr = self;
        if (cls->bases[b - 1].offset > 0) { IrInst adj = make(IrInst::AddrField, m_semaTypes.pointer(m_semaTypes.classType(base)), line); adj.sub = "base " + base->name; adj.extra = cls->bases[b - 1].offset; adj.args.push_back(IrOperand::temp(self, m_out->locals[0].type)); baddr = emit(adj); }
        destroy(m_semaTypes.classType(base), baddr, line, false, false);
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
    if (m_vbaseDtorOf.count(fn)) genVbaseDestructor(fn);
    else if (fn->isConstructor && fn->params.empty()) constructorInit(fn, 0);
    else if (!fn->isDestructor) {   // an implicit destructor's body is empty: returnSequence destroys members and bases
        // copy constructor / copy assignment: memberwise, bases first
        bool assign = !fn->isConstructor;
        int self = thisTemp(line);
        const int other = 1;   // after this (the "most derived" flag is the last parameter)
        IrInst ld = make(IrInst::LoadLocal, 0, line); ld.type = m_out->locals[static_cast<size_t>(other)].type; ld.args.push_back(IrOperand::local(other, m_out->locals[static_cast<size_t>(other)].type));
        int src = emit(ld);
        if (hasVbases(cls) && !assign && m_vbFlagSlot >= 0) {
            // the most derived object's copy: vbptrs and virtual bases first (the source's found through its vbptr)
            const sema::Type* it = m_semaTypes.basic(sema::Type::Int);
            LValue fl; fl.kind = LValue::Local; fl.local = m_vbFlagSlot; fl.type = it;
            int vb = m_out->newBlock("vbases"), jb = m_out->newBlock("vbases.done");
            condbr(toBool(load(fl, it, line), it, line), vb, jb, line);
            m_block = vb;
            storeVbptrs(cls, self, line);
            for (size_t v = 0; v < cls->vbases.size(); ++v) {
                Symbol* base = cls->vbases[v];
                int d = subobject(self, ct, "vbase " + base->name, cls->vbaseOffsets[v], base->type, line);
                const int s = toBase(src, cls, base, line);
                m_ctorFlag = 0;
                copyConstruct(base->type, d, s, line);
                m_ctorFlag = 1;
            }
            br(jb, line);
            m_block = jb;
        }
        for (size_t b = 0; b < cls->bases.size(); ++b) {
            Symbol* base = cls->bases[b].cls;
            if (!base) continue;
            if (cls->bases[b].isVirtual) {
                // an assignment assigns its direct virtual bases too, as cl's implicit one does
                if (assign) copyAssign(base->type, toBase(self, cls, base, line), toBase(src, cls, base, line), line);
                continue;
            }
            int d = subobject(self, ct, "base " + base->name, cls->bases[b].offset, base->type, line);
            int s = subobject(src, ct, "base " + base->name, cls->bases[b].offset, base->type, line);
            m_ctorFlag = 0;
            if (assign) copyAssign(base->type, d, s, line); else copyConstruct(base->type, d, s, line);
            m_ctorFlag = 1;
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
        int p = emit(ld);
        if (m_retReg) { LValue m; m.kind = LValue::Mem; m.addr = p; m.type = m_retReg; r.args.push_back(IrOperand::temp(load(m, m_retReg, line), oldType(m_retReg))); }   // the bytes in rax
        else r.args.push_back(IrOperand::temp(p, l.type));
    }
}

// cl's numbers for the blocks of a function, which name its static locals and their guards ("?1" for the body,
// 2): counted in source order from the body, a compound statement takes the next number; an if takes one, and
// each of its branches one more (the branch's scope) before its own statements; for, while, do and switch take
// one before their body; a try block and a handler are their compound statements. Found from cl's objects
// (static locals in blocks, branches, loops, switch, try/catch, labels, conditions with declarations)
void IrGen::numberStaticBlocks(cxx::Stmt* s, long& next, long current)
{
    if (!s) return;
    switch (s->kind) {
    case cxx::Stmt::Compound: {
        const long mine = next++;
        (void)current;
        for (size_t i = 0; i < s->kids.size(); ++i) numberStaticBlocks(s->kids[i], next, mine);
        return;
    }
    case cxx::Stmt::DeclStmt:
        if (s->decl)
            for (size_t i = 0; i < s->decl->declarators.size(); ++i) {
                Symbol* v = s->decl->declarators[i].decl ? s->decl->declarators[i].decl->semaSym : 0;
                if (v && v->kind == Symbol::Variable && v->isStatic) m_staticBlocks[v] = current;
            }
        return;
    case cxx::Stmt::If:
        ++next;
        for (size_t i = 0; i < s->kids.size() && i < 2; ++i) { const long branch = next++; numberStaticBlocks(s->kids[i], next, branch); }
        return;
    case cxx::Stmt::For: case cxx::Stmt::While: case cxx::Stmt::DoWhile: case cxx::Stmt::Switch: {
        const long mine = next++;
        numberStaticBlocks(s->init, next, mine);
        for (size_t i = 0; i < s->kids.size(); ++i) numberStaticBlocks(s->kids[i], next, mine);
        return;
    }
    default:
        numberStaticBlocks(s->init, next, current);
        for (size_t i = 0; i < s->kids.size(); ++i) numberStaticBlocks(s->kids[i], next, current);
        return;
    }
}

// the variable a name expression (in parentheses or not) refers to, or 0
static Symbol* namedLocal(cxx::Expr* e)
{
    while (e && e->kind == cxx::Expr::Paren && !e->kids.empty()) e = e->kids[0];
    return e && e->kind == cxx::Expr::Name && e->semaSym && e->semaSym->kind == Symbol::Variable ? e->semaSym : 0;
}

static void collectReturns(cxx::Stmt* s, std::vector<cxx::Stmt*>& rets, bool& hasGoto)
{
    if (!s) return;
    if (s->kind == cxx::Stmt::Return) rets.push_back(s);
    if (s->kind == cxx::Stmt::Goto || s->kind == cxx::Stmt::Asm) hasGoto = true;
    collectReturns(s->init, rets, hasGoto);
    for (size_t i = 0; i < s->kids.size(); ++i) collectReturns(s->kids[i], rets, hasGoto);
}

// cl /O2's named return value optimisation, for a result returned through the hidden pointer: a local of the
// result's class declared directly in the body and named by every return statement is built in the result
// object. Not with goto (a jump back over the declaration would build it twice)
Symbol* IrGen::namedResult(Symbol* fn, cxx::Stmt* body)
{
    if (!body || body->kind != cxx::Stmt::Compound || !fn->type || !fn->type->inner) return 0;
    const sema::Type* rt = m_semaTypes.unqualified(fn->type->inner);
    std::vector<cxx::Stmt*> rets;
    bool hasGoto = false;
    collectReturns(body, rets, hasGoto);
    if (rets.empty() || hasGoto) return 0;
    Symbol* var = 0;
    for (size_t i = 0; i < rets.size(); ++i) {
        Symbol* v = namedLocal(rets[i]->expr);
        if (!v || (var && v != var)) return 0;
        var = v;
    }
    if (!var->isLocal || var->isStatic || !var->type || isReference(var->type) || m_semaTypes.unqualified(var->type) != rt) return 0;
    for (size_t i = 0; i < body->kids.size(); ++i) {
        cxx::Stmt* k = body->kids[i];
        if (k->kind != cxx::Stmt::DeclStmt || !k->decl || k->decl->kind != cxx::Decl::Simple) continue;
        for (size_t d = 0; d < k->decl->declarators.size(); ++d)
            if (k->decl->declarators[d].decl && k->decl->declarators[d].decl->semaSym == var) return var;
    }
    return 0;
}

void IrGen::genFunction(Symbol* fn)
{
    cxx::Decl* def = fn->definition;
    if (!def || !def->body) return;
    m_out = new IrFunction;
    m_fn = fn;
    m_slots.clear(); m_loops.clear(); m_tryStack.clear(); m_tryCount = 0;
    prologue(fn);
    m_nrvo = (m_nrvoEnabled && m_sretSlot >= 0 && !m_retReg) ? namedResult(fn, def->body) : 0;
    if (fn->isConstructor) constructorInit(fn, def);
    // as cl: a destructor stores its own class's vptrs first, so a virtual call made while it (or a base's
    // destructor after it) runs reaches this class's functions, not those of the derived object being destroyed
    if (fn->isDestructor && fn->ownerClass) storeVptr(fn->ownerClass, thisTemp(def->loc.line), def->loc.line);
    m_staticBlocks.clear();
    // a function-try-block: the body (2) holds the try block (3, the next number) and its handlers
    if (m_msvcNames) { long next = def->body->kind == cxx::Stmt::Try ? 3 : 2; numberStaticBlocks(def->body, next, 2); }
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
    genDeletingDestructors();
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

// the class-specific operator delete (or delete[]) of cls (its own or a base's, as lookup finds it), or 0
Symbol* IrGen::classOperatorDelete(Symbol* cls, int depth, const char* name, size_t maxParams) const
{
    if (!cls || !cls->members || depth > 32) return 0;
    const std::vector<Symbol*>* v = cls->members->find(name);
    // the one-parameter form when the class declares both (C++98, as cl), else the sized one
    if (v) for (size_t n = 1; n <= maxParams; ++n)
        for (size_t i = 0; i < v->size(); ++i) if ((*v)[i]->kind == Symbol::Function && (*v)[i]->params.size() == n) return (*v)[i];
    for (size_t b = 0; b < cls->bases.size(); ++b) { Symbol* d = classOperatorDelete(cls->bases[b].cls, depth + 1, name, maxParams); if (d) return d; }
    return 0;
}

// frees an array of class cls from its cookie (count elements follow it): through the class's operator delete[]
// when lookup finds one, given count * size + 8 in the two-parameter form, as cl's vector deleting destructor
// does; else through the global one
void IrGen::freeArray(Symbol* cls, int cookie, int count, int line)
{
    const sema::Type* vp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
    const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
    Symbol* op = classOperatorDelete(cls, 0, "operator delete[]");
    int bytes = -1;
    if (op && op->params.size() == 2) {
        const int size = constant(sizeOf(cls->type), lng, line);
        const int elems = binary("*", count, lng, size, lng, lng, line);
        const int eight = constant(8, lng, line);
        bytes = binary("+", elems, lng, eight, lng, lng, line);
    }
    if (op) need(op);
    IrInst in = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
    in.sub = op ? irName(op) : std::string("__delete_array");
    in.args.push_back(IrOperand::temp(cookie, oldType(vp)));
    if (bytes >= 0) in.args.push_back(IrOperand::temp(bytes, oldType(m_semaTypes.sizeType())));
    emit(in);
}

// frees an object of class cls at p through its class-specific operator delete, with the complete object's size
// for the two-parameter form, as cl does; false when the class has none (the global one frees it)
bool IrGen::freeObject(Symbol* cls, int p, int line)
{
    Symbol* op = classOperatorDelete(cls, 0);
    if (!op) return false;
    need(op);
    const sema::Type* vp = m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void));
    IrInst in = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
    in.sub = irName(op);
    in.args.push_back(IrOperand::temp(p, oldType(vp)));
    if (op->params.size() == 2) in.args.push_back(IrOperand::imm(sizeOf(cls->type), oldType(m_semaTypes.sizeType())));
    emit(in);
    return true;
}

std::string IrGen::deletingDestructor(Symbol* dtor, X86Module& x86, bool msvc)
{
    std::map<Symbol*, Symbol*>::iterator it = m_deleting.find(dtor);
    if (it != m_deleting.end()) return irName(it->second);
    Symbol* cls = dtor->ownerClass;
    Symbol* fn = m_prog.newSymbol(Symbol::Function, "`vector deleting destructor'", cls->members);
    fn->ownerClass = cls;
    fn->isImplicit = true;                 // one copy survives linking, as cl's COMDAT
    fn->hasBody = true;
    fn->loc = dtor->loc;
    const sema::Type* u = m_semaTypes.basic(sema::Type::UInt);
    fn->type = m_semaTypes.function(m_semaTypes.pointer(m_semaTypes.basic(sema::Type::Void)), std::vector<const sema::Type*>(1, u), false, false);
    Symbol* flags = m_prog.newSymbol(Symbol::Variable, "flags", 0);
    flags->type = u; flags->isParam = true;
    fn->params.push_back(flags);
    m_deleting[dtor] = fn;
    m_deletingOrder.push_back(dtor);
    if (msvc) x86.registerSymbolName(irName(fn), deletingName(msvcMangleSymbol(*dtor)));
    return irName(fn);
}

// cl's vector deleting destructor:
//   flags & 2 (delete[]): n = cookie before this; destroy this[n-1] .. this[0]; if (flags & 1) operator delete[]
//                         (cookie, n * size + 8); return cookie
//   else:                 this->~D(); if (flags & 1) operator delete(this); return this
void IrGen::genDeletingDestructors()
{
    for (; m_deletingDone < m_deletingOrder.size(); ++m_deletingDone) {
        Symbol* dtor = m_deletingOrder[m_deletingDone];
        Symbol* fn = m_deleting[dtor];
        const int line = fn->loc.line;
        m_out = new IrFunction;
        m_fn = fn;
        m_slots.clear(); m_loops.clear(); m_tryStack.clear(); m_tryCount = 0;
        prologue(fn);
        const sema::Type* u = m_semaTypes.basic(sema::Type::UInt);
        const sema::Type* lng = m_semaTypes.basic(sema::Type::LongLong);
        const sema::Type* ct = m_semaTypes.classType(dtor->ownerClass);
        const sema::Type* cp = m_semaTypes.pointer(ct);
        LValue rl; rl.kind = LValue::Local; rl.local = newLocal("tmp.result", cp, false); rl.type = cp;
        LValue fl; fl.kind = LValue::Local; fl.local = m_slots[fn->params[0]]; fl.type = u;
        const int flags0 = load(fl, u, line);      // one statement each: the order of the IR must not depend on
        const int two = constant(2, u, line);      // the host compiler's order of evaluating arguments
        const int arrayBit = binary("&", flags0, u, two, u, u, line);
        int ab = m_out->newBlock("array"), sb = m_out->newBlock("scalar"), xb = m_out->newBlock("done");
        condbr(toBool(arrayBit, u, line), ab, sb, line);

        m_block = ab;
        const int first = thisTemp(line);
        IrInst hd = make(IrInst::AddrIndex, cp, line); hd.extra = 1;
        hd.args.push_back(IrOperand::temp(first, oldType(cp))); hd.args.push_back(IrOperand::imm(-8, oldType(lng)));
        const int cookie = emit(hd);
        LValue hl; hl.kind = LValue::Mem; hl.addr = cookie; hl.type = lng;
        const int count = load(hl, lng, line);
        forEachElement(ct, first, count, false, line);
        store(rl, cookie, cp, line);
        const int flags1 = load(fl, u, line);
        const int one1 = constant(1, u, line);
        const int freeArr = binary("&", flags1, u, one1, u, u, line);
        int fab = m_out->newBlock("delete.array");
        condbr(toBool(freeArr, u, line), fab, xb, line);
        m_block = fab;
        freeArray(dtor->ownerClass, cookie, count, line);
        br(xb, line);

        m_block = sb;
        callSpecial(dtor, thisTemp(line), -1, line);
        const int self0 = thisTemp(line);
        store(rl, self0, cp, line);
        const int flags2 = load(fl, u, line);
        const int one2 = constant(1, u, line);
        const int freeOne = binary("&", flags2, u, one2, u, u, line);
        int db = m_out->newBlock("delete");
        condbr(toBool(freeOne, u, line), db, xb, line);
        m_block = db;
        const int self = thisTemp(line);
        if (!freeObject(dtor->ownerClass, self, line)) {
            IrInst del = make(IrInst::Call, m_semaTypes.basic(sema::Type::Void), line);
            del.sub = "__delete";
            del.args.push_back(IrOperand::temp(self, m_out->locals[0].type));
            emit(del);
        }
        br(xb, line);

        m_block = xb;
        returnSequence(line);
        const int result = load(rl, cp, line);
        IrInst r = make(IrInst::Ret, 0, line);
        r.args.push_back(IrOperand::temp(result, m_out->locals[0].type));
        emit(r);
        finishFunction(line);
    }
}

void IrGen::registerMetadata(X86Module& x86, bool msvc)
{
    m_x86 = &x86;
    m_msvcNames = msvc;
    for (size_t i = 0; i < m_prog.symbols.size(); ++i) {
        Symbol* s = m_prog.symbols[i];
        if (s->kind == Symbol::Class && s->isDefinition && s->isPolymorphic && !s->isDependentContext && s->layoutDone) {
            // one table per vfptr; a slot whose overrider expects this elsewhere holds an adjustor thunk, named
            // as cl names it (?f@D@@WBA@EAAHXZ: D::f`adjustor{16}')
            const size_t n = s->vftables.empty() ? 1 : s->vftables.size();
            // the class each table is named for: the one that introduced its vfptr, or, when two tables share
            // that, the base on the way down to it that tells them apart (G : Q1, Q2, both from P: Q1 and Q2)
            std::vector<std::vector<Symbol*> > paths(n);
            for (size_t t = 0; t < n && n > 1; ++t) {
                const Symbol* cur = s;
                int off = s->vftables[t].offset;
                for (int guard = 0; guard < 32 && cur != s->vftables[t].introducedBy; ++guard) {
                    const sema::BaseClass* hit = 0;
                    for (size_t b = 0; b < cur->bases.size() && !hit; ++b) {
                        const sema::BaseClass& bc = cur->bases[b];
                        if (!bc.cls || !bc.cls->isPolymorphic) continue;
                        for (size_t k = 0; k < bc.cls->vftables.size(); ++k) if (bc.cls->vftables[k].offset == off - bc.offset) hit = &bc;
                    }
                    if (!hit) break;
                    paths[t].push_back(hit->cls);
                    off -= hit->offset;
                    cur = hit->cls;
                }
                if (paths[t].empty() || paths[t].back() != s->vftables[t].introducedBy) paths[t].push_back(s->vftables[t].introducedBy);
            }
            std::vector<size_t> pick(n, 0);
            for (size_t t = 0; t < n; ++t) pick[t] = paths[t].empty() ? 0 : paths[t].size() - 1;
            for (int round = 0; round < 32; ++round) {
                bool moved = false;
                for (size_t t = 0; t < n; ++t)
                    for (size_t u = t + 1; u < n; ++u)
                        if (!paths[t].empty() && !paths[u].empty() && paths[t][pick[t]] == paths[u][pick[u]]) {
                            if (pick[t] > 0) { --pick[t]; moved = true; }
                            if (pick[u] > 0) { --pick[u]; moved = true; }
                        }
                if (!moved) break;
            }
            for (size_t t = 0; t < n; ++t) {
                IrVtable v;
                const std::string name = vtableName(s, t);
                v.className = name.substr(7);
                const std::vector<Symbol*>& fns = s->vftables.empty() ? s->vtable : s->vftables[t].fns;
                for (size_t k = 0; k < fns.size(); ++k) {
                    Symbol* m = fns[k];
                    const int adjust = s->vftables.empty() ? 0 : s->vftables[t].thunks[k];
                    if (m->isPureVirtual) { v.entries.push_back(""); continue; }
                    need(m);
                    // the destructor's slot: the vector deleting destructor (??_ED@@UEAAPEAXI@Z), as with cl
                    const std::string target = m->isDestructor ? deletingDestructor(m, x86, msvc) : irName(m);
                    if (adjust == 0) { v.entries.push_back(target); continue; }
                    const std::string thunk = target + " adjustor{" + numText(adjust) + "}";
                    v.entries.push_back(thunk);
                    x86.addThunk(thunk, target, adjust);
                    if (msvc) x86.registerSymbolName(thunk, m->isDestructor ? deletingName(msvcMangleThunkSymbol(*m, adjust)) : msvcMangleThunkSymbol(*m, adjust));
                }
                x86.addVtable(v);
                if (msvc) x86.registerGlobalName(name, msvcMangleVtableSymbol(*s, n > 1 && !paths[t].empty() ? paths[t][pick[t]] : 0));
            }
        }
        if (s->kind == Symbol::Class && s->isDefinition && !s->isDependentContext && s->layoutDone && !s->vbtables.empty()) {
            // MSVC's vbtables (??_8D@@7B@, ??_8VX@@7BVL@@@): read-only int entries, one copy kept (COMDAT)
            for (size_t t = 0; t < s->vbtables.size(); ++t) {
                IrGlobal g;
                g.name = vbtableName(s, t);
                g.size = static_cast<int>(4 * s->vbtables[t].entries.size());
                g.readOnly = true; g.linkOnce = true; g.align = 4;
                for (size_t e = 0; e < s->vbtables[t].entries.size(); ++e) { IrDataItem d; d.kind = IrDataItem::Int; d.size = 4; d.value = s->vbtables[t].entries[e]; g.data.push_back(d); }
                x86.addGlobal(g);
                if (msvc) {
                    // the vftable's name with ??_8 and 7B: ??_7VX@@6BVL@@@ -> ??_8VX@@7BVL@@@
                    const sema::VbTable& v = s->vbtables[t];
                    std::string n = msvcMangleVtableSymbol(*s, v.path.empty() ? 0 : v.path.back());
                    const std::string plain = msvcMangleVtableSymbol(*s, 0);
                    if (n.compare(0, 4, "??_7") == 0 && plain.size() >= 7) n = "??_8" + n.substr(4, plain.size() - 7) + "7B" + n.substr(plain.size() - 1);
                    x86.registerGlobalName(g.name, n);
                }
            }
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
            // an integer constant for an integer or a pointer as it is; a floating, bool or converted value through
            // constantData, which converts it exactly as cl does ("double d = 0xFFFFFFFFu;", "bool b = 0.5;")
            const sema::Type* st = m_semaTypes.unqualified(m_semaTypes.unref(s->type));
            const sema::Type* it0 = s->init ? m_semaTypes.unqualified(m_semaTypes.unref(exprType(s->init))) : 0;
            if (s->init && s->init->semaConst && scalar && st && !st->isFloating() && st->kind != sema::Type::Bool && it0 && (it0->isIntegral() || it0->isPointer())) g.init = numText(s->init->semaValue);
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
    // "const char* p = "x";", "int* p = &g;": a pointer initialised by an address; "double d = 1.0/4;",
    // "int u = int();", "float f(1.5f*2);": an arithmetic constant, folded
    return init.args.size() == 1 && (u->kind == sema::Type::Pointer || u->isArithmetic()) && constantData(t, init.args[0], out, 0);
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
        // folded as cl folds it: "1.0/4", "1.5f*2", "double()", "(float)(1.0/3) + 1", an integer constant
        double v = 0;
        if (!foldArith(x, v, 0) || !convertFolded(u, v)) return false;
        // the bits as MASM's encoded real ("03FD0000000000000r"): no decimal text to round again
        unsigned char bytes[8];
        const float fv = static_cast<float>(v);
        if (size == 4) std::memcpy(bytes, &fv, 4); else if (size == 8) std::memcpy(bytes, &v, 8); else return false;
        static const char hexDigits[] = "0123456789ABCDEF";
        it.kind = IrDataItem::Float;
        it.text = "0";
        for (int i = size - 1; i >= 0; --i) { it.text += hexDigits[bytes[i] >> 4]; it.text += hexDigits[bytes[i] & 15]; }
        it.text += "r";
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
    if (!u->isIntegral()) return false;
    long v = x->semaValue;
    const sema::Type* xt = x->semaType ? m_semaTypes.unqualified(m_semaTypes.unref(x->semaType)) : 0;
    if (!x->semaConst || (xt && !xt->isIntegral())) {
        // "int()", "(int)2.75", "bool b = 0.5;": an arithmetic constant converted
        double d = 0;
        if (!foldArith(x, d, 0) || !convertFolded(u, d)) return false;
        v = d >= 2147483648.0 ? static_cast<long>(d - 4294967296.0) : static_cast<long>(d);   // the 32 bits
    }
    if (u->kind == sema::Type::Bool) v = v != 0;
    else if (size == 1) v &= 0xFF;
    else if (size == 2) v &= 0xFFFF;
    it.kind = IrDataItem::Int;
    it.value = v;
    out.push_back(it);
    return true;
}

// an arithmetic constant expression of the initializer of a static object, folded into v as a value of x's own
// type, exactly as cl folds it: literals, integer constants, unary + -, binary + - * / and conversions (casts,
// "T()"). The usual arithmetic conversions apply first (float op int is float), and every float-typed result is
// rounded to float (one double operation on two floats rounded to float is the float operation; overflow gives
// infinity, as cl). A decimal float literal whose double is exactly halfway between two floats is not folded:
// rounding it again could differ from cl's direct rounding. Anything else not provably cl's value: false
bool IrGen::foldArith(cxx::Expr* x, double& v, int depth)
{
    if (!x || depth > 64) return false;
    const sema::Type* t = x->semaType ? m_semaTypes.unqualified(m_semaTypes.unref(x->semaType)) : 0;
    if (!t || !t->isArithmetic()) return false;
    if (t->isIntegral() && x->semaConst) {
        if (t->kind == sema::Type::LongLong || t->kind == sema::Type::ULongLong) {
            // semaValue is a long: only a sizeof, or a literal whose digits give that same value
            if (x->kind != cxx::Expr::SizeofType && x->kind != cxx::Expr::SizeofExpr) {
                if (x->kind != cxx::Expr::IntLit) return false;
                std::string d = x->text;
                while (!d.empty() && std::strchr("uUlL", d[d.size() - 1])) d.erase(d.size() - 1);
                double lit = 0;
                int base = 10;
                size_t i = 0;
                if (d.size() > 2 && d[0] == '0' && (d[1] == 'x' || d[1] == 'X')) { base = 16; i = 2; }
                else if (d.size() > 1 && d[0] == '0') { base = 8; i = 1; }
                for (; i < d.size(); ++i) {
                    const char ch = static_cast<char>(std::tolower(static_cast<unsigned char>(d[i])));
                    int dv = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : 99;
                    if (dv >= base) return false;
                    lit = lit * base + dv;
                }
                if (lit != static_cast<double>(x->semaValue)) return false;
            }
            if (x->semaValue < 0) return false;
        }
        v = static_cast<double>(x->semaValue);
        const bool isUnsigned = t->kind == sema::Type::UChar || t->kind == sema::Type::UShort || t->kind == sema::Type::UInt || t->kind == sema::Type::ULong;
        if (isUnsigned && v < 0) v += 4294967296.0;
        return true;
    }
    switch (x->kind) {
    case cxx::Expr::FloatLit: {
        std::string text = x->text;
        while (!text.empty() && std::strchr("fFlL", text[text.size() - 1])) text.erase(text.size() - 1);
        if (text.empty() || text.find_first_not_of("0123456789.eE+-") != std::string::npos) return false;
        v = std::strtod(text.c_str(), 0);
        if (t->kind == sema::Type::Float && !floatRoundsOnce(v)) return false;
        break;
    }
    case cxx::Expr::Paren:
        if (x->kids.size() != 1 || !foldArith(x->kids[0], v, depth + 1)) return false;
        break;
    case cxx::Expr::Unary:
        if (x->kids.size() != 1 || (x->text != "-" && x->text != "+") || !foldArith(x->kids[0], v, depth + 1)) return false;
        if (x->text == "-") v = -v;
        break;
    case cxx::Expr::Binary: {
        double a = 0, b = 0;
        if (x->kids.size() != 2 || !foldArith(x->kids[0], a, depth + 1) || !foldArith(x->kids[1], b, depth + 1)) return false;
        if (!t->isFloating()) return false;   // integer arithmetic is sema's (semaConst)
        // the usual arithmetic conversions, from the operands' own types: double if either is, else float
        const sema::Type* ta = m_semaTypes.unqualified(m_semaTypes.unref(exprType(x->kids[0])));
        const sema::Type* tb = m_semaTypes.unqualified(m_semaTypes.unref(exprType(x->kids[1])));
        const bool wide = ta->kind == sema::Type::Double || ta->kind == sema::Type::LongDouble || tb->kind == sema::Type::Double || tb->kind == sema::Type::LongDouble;
        const sema::Type* ct = m_semaTypes.basic(wide ? sema::Type::Double : sema::Type::Float);
        if (!convertFolded(ct, a) || !convertFolded(ct, b)) return false;
        if (x->text == "+") v = a + b;
        else if (x->text == "-") v = a - b;
        else if (x->text == "*") v = a * b;
        else if (x->text == "/") { if (b == 0) return false; v = a / b; }
        else return false;
        // the operation's own type, whatever sema made of it (float op int is float: rounded here)
        if (!convertFolded(ct, v)) return false;
        break;
    }
    case cxx::Expr::Cast:
    case cxx::Expr::NamedCast:
    case cxx::Expr::FunctionalCast:
        if (x->kind == cxx::Expr::NamedCast && x->text != "static_cast") return false;
        if (x->kids.empty() && x->kind == cxx::Expr::FunctionalCast) { v = 0; break; }   // "int()": value-initialised
        if (x->kids.size() != 1 || !foldArith(x->kids[0], v, depth + 1)) return false;
        break;
    default:
        return false;
    }
    return convertFolded(t, v);
}

// false when the double d lies exactly halfway between two floats: the decimal it came from may not
bool IrGen::floatRoundsOnce(double d)
{
    const float f = static_cast<float>(d);
    if (static_cast<double>(f) == d || f != f) return true;
    unsigned int bits = 0;
    std::memcpy(&bits, &f, 4);
    // the float on d's other side: one step away from zero when f is nearer zero than d, else toward it
    const bool fBelowMagnitude = (d < 0 ? -static_cast<double>(f) < -d : static_cast<double>(f) < d);
    unsigned int nb = fBelowMagnitude ? bits + 1 : bits - 1;
    float g;
    std::memcpy(&g, &nb, 4);
    if (g != g) return false;
    return (static_cast<double>(f) + static_cast<double>(g)) / 2 != d;
}

// v converted to the arithmetic type t as cl converts a constant; false when the result is not a plain value
// (NaN, an infinity or a value out of range converted to an integer, an enum)
bool IrGen::convertFolded(const sema::Type* t, double& v) const
{
    if (v != v) return false;
    const bool finite = v <= 1.7976931348623157e308 && v >= -1.7976931348623157e308;
    if (t->kind == sema::Type::Float) { v = static_cast<double>(static_cast<float>(v)); return true; }   // to infinity beyond
    if (t->kind == sema::Type::Double || t->kind == sema::Type::LongDouble) return true;
    if (!finite) return false;
    if (t->kind == sema::Type::Bool) { v = v != 0 ? 1 : 0; return true; }
    if (!t->isIntegral() || t->kind == sema::Type::Enum) return false;
    const double n = v < 0 ? -std::floor(-v) : std::floor(v);   // toward zero
    double lo = -2147483648.0, hi = 2147483647.0;
    switch (t->kind) {
    case sema::Type::Char: case sema::Type::SChar: lo = -128; hi = 127; break;
    case sema::Type::UChar: lo = 0; hi = 255; break;
    case sema::Type::Short: lo = -32768; hi = 32767; break;
    case sema::Type::UShort: case sema::Type::WChar: lo = 0; hi = 65535; break;
    case sema::Type::UInt: case sema::Type::ULong: lo = 0; hi = 4294967295.0; break;
    case sema::Type::ULongLong: lo = 0; break;   // emitted from a 32-bit value: below 2^31
    default: break;
    }
    if (n < lo || n > hi) return false;
    v = n;
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
