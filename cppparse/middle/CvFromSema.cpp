// CvFromSema.cpp - see CvFromSema.hpp.
#include "CvFromSema.hpp"
#include "Mangle.hpp"

namespace cppparse {

using sema::Scope;
using sema::Symbol;

namespace {

const int kMaxDepth = 64;   // of types and scopes inside one name: beyond it the name is cut with a "?"

const char* basicName(sema::Type::Kind k)
{
    switch (k) {
    case sema::Type::Void: return "void";
    case sema::Type::Bool: return "bool";
    case sema::Type::Char: return "char";
    case sema::Type::SChar: return "signed char";
    case sema::Type::UChar: return "unsigned char";
    case sema::Type::WChar: return "wchar_t";
    case sema::Type::Short: return "short";
    case sema::Type::UShort: return "unsigned short";
    case sema::Type::Int: return "int";
    case sema::Type::UInt: return "unsigned int";
    case sema::Type::Long: return "long";
    case sema::Type::ULong: return "unsigned long";
    case sema::Type::LongLong: return "long long";
    case sema::Type::ULongLong: return "unsigned long long";
    case sema::Type::Float: return "float";
    case sema::Type::Double: return "double";
    case sema::Type::LongDouble: return "long double";
    default: return "?";
    }
}

// CodeView's own index of a fundamental type (the design's 5.2; long double is cl's 8-byte one)
unsigned long basicIndex(sema::Type::Kind k)
{
    switch (k) {
    case sema::Type::Void: return 0x03;
    case sema::Type::Bool: return 0x30;
    case sema::Type::Char: return 0x70;
    case sema::Type::SChar: return 0x10;
    case sema::Type::UChar: return 0x20;
    case sema::Type::WChar: return 0x71;
    case sema::Type::Short: return 0x11;
    case sema::Type::UShort: return 0x21;
    case sema::Type::Int: return 0x74;
    case sema::Type::UInt: return 0x75;
    case sema::Type::Long: return 0x12;
    case sema::Type::ULong: return 0x22;
    case sema::Type::LongLong: return 0x13;
    case sema::Type::ULongLong: return 0x23;
    case sema::Type::Float: return 0x40;
    case sema::Type::Double: return 0x41;
    case sema::Type::LongDouble: return 0x41;
    default: return 0;
    }
}

int accessOf(cxx::Access a)
{
    return a == cxx::AccessPrivate ? 1 : a == cxx::AccessProtected ? 2 : 3;
}

std::string unitIdText()
{
    std::string hex;
    for (int i = 7; i >= 0; --i) hex += "0123456789abcdef"[(g_mangleUnitId >> (4 * i)) & 15];
    return hex;
}

// declared in a function (a local class, or a member of one)
bool isLocalTo(const Scope* s)
{
    for (; s; s = s->parent) {
        if (s->kind == Scope::Function || s->kind == Scope::Block) return true;
        if (s->kind != Scope::Class) return false;
    }
    return false;
}

bool inUnnamedNamespace(const Scope* s)
{
    for (; s; s = s->parent)
        if (s->kind == Scope::Namespace && s->owner && s->owner->name.empty()) return true;
    return false;
}

} // namespace

// ---- names ---------------------------------------------------------------------------------------------------------

std::vector<std::string> CvFromSema::templateArguments(const Symbol* inst, Form form, int depth)
{
    std::vector<std::string> args;
    for (size_t i = 0; i < inst->templateArgs.size(); ++i) {
        const Symbol* member = i < inst->templateSymArgs.size() ? inst->templateSymArgs[i] : 0;
        int64 v = i < inst->templateIntArgs.size() ? inst->templateIntArgs[i] : 0;
        if (inst->templateArgs[i]) args.push_back(CvNames::text(typeText(inst->templateArgs[i], form, depth + 1)));
        else if (member && (member->kind == Symbol::Function || member->ownerClass->memberPointerUnspecified || !member->ownerClass->vbases.empty())) {
            // a pointer to member, by the representation its class gives it (the cases of MangleSema's templateName,
            // which decorates the same values): a function is &C::f, a virtual one its vcall thunk; under multiple
            // inheritance {C::f,adjustment}, with virtual bases {C::f,adjustment,vbtable offset}, in the unspecified
            // model {C::f,adjustment,vbptr offset,vbtable offset}; a data member with virtual bases {offset,vbtable
            // offset}, unspecified {offset,vbptr offset,vbtable offset}. The numbers are unsigned 32-bit values
            bool multiple = false, virt = false;
            sema::TypeTable::memberPointerModel(member->ownerClass, multiple, virt, 0);
            const bool unspecified = !member->ownerClass->isDefinition || member->ownerClass->memberPointerUnspecified;
            int off = 0, adj = 0, vbptr = 0, vbindex = 0;
            sema::TypeTable::memberPointerValue(member, off, adj, vbptr, vbindex);
            std::vector<std::string> parts;
            if (member->kind == Symbol::Function) {
                const std::string fn = member->isVirtual ? CvNames::vcallThunk(qualifiedName(member->ownerClass, form, depth + 1), 8L * (member->vtableSlot < 0 ? 0 : member->vtableSlot)) : functionName(member);
                if (!unspecified && !virt && !multiple) { args.push_back(CvNames::addressOf(fn)); continue; }
                parts.push_back(fn);
                parts.push_back(CvNames::unsigned32(unspecified || virt ? adj : member->thisAdjust));
            }
            else parts.push_back(CvNames::unsigned32(off));
            if (unspecified) parts.push_back(CvNames::unsigned32(vbptr));
            if (unspecified || virt || member->kind != Symbol::Function) parts.push_back(CvNames::unsigned32(vbindex));
            args.push_back(CvNames::braced(parts));
        }
        else if (member) args.push_back(CvNames::integer(v));       // a pointer to a data member, no virtual bases: its offset
        else {
            // the value as a signed 64-bit number: an unsigned parameter of 4 bytes is held sign-extended
            // (sema::foldedConstant) and widens with zeros, as in the decorated name (MangleSema.cpp)
            const Symbol* from = inst->instantiatedFrom;
            const sema::Type* pt = from && i < from->templateParams.size() && from->templateParams[i] ? from->templateParams[i]->type : 0;
            if (pt && (pt->kind == sema::Type::UInt || pt->kind == sema::Type::ULong)) v = static_cast<int64>(static_cast<uint64>(v) & 0xFFFFFFFFu);
            args.push_back(CvNames::integer(v));
        }
    }
    return args;
}

std::string CvFromSema::ownName(const Symbol* s, Form form, int depth)
{
    if (s->kind == Symbol::Class && s->instantiatedFrom) return CvNames::templateId(s->instantiatedFrom->name, templateArguments(s, form, depth));
    if ((s->kind == Symbol::Class || s->kind == Symbol::Enum) && s->name.empty()) return CvNames::unnamedType(std::string());
    return s->name;
}

// the scopes around what is declared in s, outermost first. A function's or block's scope counts for a class
// declared there and for the members of such a class (local), as in the decorated name: the block's number is the
// one MangleSema gives it (2: the body; a nested block 2 plus its number in the function)
std::string CvFromSema::scopeName(const Scope* s, bool ofType, Form form, int depth)
{
    std::vector<std::string> parts;   // innermost first
    bool local = ofType;
    for (; s; s = s->parent) {
        if (s->kind == Scope::Namespace && s->owner) parts.push_back(!s->owner->name.empty() ? s->owner->name : form == InType ? CvNames::anonymousNamespaceOfType() : CvNames::anonymousNamespace());
        else if (s->kind == Scope::Class && s->owner) { parts.push_back(depth > kMaxDepth ? std::string("?") : ownName(s->owner, form, depth + 1)); local = true; }
        else if (s->kind == Scope::Function || s->kind == Scope::Block) {
            if (local) {
                const Scope* f = s;
                while (f && f->kind != Scope::Function) f = f->parent;
                const long block = 2 + (s->kind == Scope::Block ? s->blockIndex : 0);
                if (f && f->owner && depth <= kMaxDepth) parts.push_back(form == InType ? CvNames::localScopeOfType(functionName(f->owner), block) : CvNames::localScope(functionName(f->owner), block));
            }
            break;
        }
    }
    std::string out;
    for (size_t i = parts.size(); i > 0; --i) out = CvNames::scoped(out, parts[i - 1]);
    return out;
}

std::string CvFromSema::qualifiedName(const Symbol* s, Form form, int depth)
{
    if (depth > kMaxDepth) return "?";
    return CvNames::scoped(scopeName(s->scope, true, form, depth + 1), ownName(s, form, depth + 1));
}

CvTypeText CvFromSema::typeText(const sema::Type* t, Form form, int depth)
{
    if (!t || depth > kMaxDepth) return CvNames::basic("?", false, false);
    switch (t->kind) {
    case sema::Type::NullPtr: return CvNames::pointer(CvNames::basic("void", false, false), false, false);
    case sema::Type::Pointer: return CvNames::pointer(typeText(t->inner, form, depth + 1), t->isConst, t->isVolatile);
    case sema::Type::Reference: return CvNames::reference(typeText(t->inner, form, depth + 1));
    case sema::Type::Array: return CvNames::array(typeText(t->inner, form, depth + 1), t->arraySize >= 0 ? t->arraySize : 0);
    case sema::Type::Function: {
        std::vector<std::string> params;
        for (size_t i = 0; i < t->params.size(); ++i) params.push_back(CvNames::text(typeText(t->params[i], form, depth + 1)));
        return CvNames::function(typeText(t->inner, form, depth + 1), params, t->variadic, t->constMethod);
    }
    case sema::Type::Class: case sema::Type::Enum:
        return CvNames::named(t->decl ? qualifiedName(t->decl, form, depth + 1) : std::string("?"), t->kind == sema::Type::Enum, t->isConst, t->isVolatile);
    case sema::Type::MemberPointer:
        return CvNames::memberPointer(typeText(t->inner, form, depth + 1), t->memberClass && t->memberClass->decl ? qualifiedName(t->memberClass->decl, form, depth + 1) : std::string("?"), t->isConst, t->isVolatile);
    case sema::Type::Dependent: return CvNames::basic(t->decl ? t->decl->name : std::string("?"), t->isConst, t->isVolatile);
    default: return CvNames::basic(basicName(t->kind), t->isConst, t->isVolatile);
    }
}

std::string CvFromSema::typeName(const sema::Type* t)
{
    return CvNames::text(typeText(t, InProcedure, 0));
}

std::string CvFromSema::variableName(const Symbol* v)
{
    return v ? CvNames::scoped(scopeName(v->scope, false, InProcedure, 0), v->name) : std::string();
}

std::string CvFromSema::functionName(const Symbol* fn)
{
    if (!fn) return std::string();
    if (fn->isExternC || (fn->name == "main" && !fn->ownerClass && fn->scope && fn->scope->kind == Scope::Global)) return fn->name;
    const Symbol* owner = fn->ownerClass;
    // an instance of a function template; a constructor template's is a constructor (as MangleSema reads them)
    const Symbol* tmpl = fn->instantiatedFrom && !fn->templateArgs.empty() ? fn->instantiatedFrom : 0;
    const bool ctor = fn->isConstructor || (tmpl && owner && tmpl->name == owner->ctorName());
    std::string name;
    if (ctor && owner) name = ownName(owner, InProcedure, 0);                        // "Outer<int>::Outer<int>"
    else if (fn->isDestructor && owner) name = "~" + ownName(owner, InProcedure, 0);
    else if (fn->isConversion && fn->type && fn->type->kind == sema::Type::Function) name = CvNames::conversionOperator(typeName(fn->type->inner));
    else name = tmpl ? tmpl->name : fn->name;
    if (tmpl) name = CvNames::templateId(name, templateArguments(fn, InProcedure, 0));
    return CvNames::scoped(scopeName(fn->scope, false, InProcedure, 0), name);
}

// ---- the module's description (-g) ---------------------------------------------------------------------------------

CvFromSema::CvFromSema(const sema::TypeTable& types, CvModule& module) : m_semaTypes(types), m_module(module), m_completed(0), m_unnamed(0), m_vftableEntry(0)
{
}

int CvFromSema::basic(unsigned long cvIndex)
{
    std::map<unsigned long, int>::const_iterator it = m_basic.find(cvIndex);
    if (it != m_basic.end()) return it->second;
    CvType t; t.kind = CvType::Basic; t.basic = cvIndex;
    const int n = m_module.add(t);
    m_basic[cvIndex] = n;
    return n;
}

int CvFromSema::typeOf(const sema::Type* t)
{
    return describe(t, false);
}

// unqualified: t without its own const and volatile (what an LF_MODIFIER qualifies)
int CvFromSema::describe(const sema::Type* t, bool unqualified)
{
    if (!t) return 0;
    if (unqualified && !t->isConst && !t->isVolatile) unqualified = false;   // it has none: one description
    std::map<const sema::Type*, int>& memo = unqualified ? m_unqualified : m_types;
    std::map<const sema::Type*, int>::const_iterator it = memo.find(t);
    if (it != memo.end()) return it->second;
    const bool pointerLike = t->kind == sema::Type::Pointer || t->kind == sema::Type::MemberPointer;
    const bool isConst = t->isConst && !unqualified, isVolatile = t->isVolatile && !unqualified;
    int n = 0;
    CvType d;
    if ((isConst || isVolatile) && !pointerLike && t->kind != sema::Type::Reference && t->kind != sema::Type::Function && t->kind != sema::Type::NullPtr) {
        // a pointer's own qualifiers are in its record; any other type's are an LF_MODIFIER of the unqualified type
        d.kind = CvType::Modifier; d.isConst = isConst; d.isVolatile = isVolatile;
        d.to = describe(t, true);
        n = d.to ? m_module.add(d) : 0;
    }
    else switch (t->kind) {
    case sema::Type::NullPtr: d.kind = CvType::Pointer; d.to = basic(0x03); n = m_module.add(d); break;
    case sema::Type::Pointer: d.kind = CvType::Pointer; d.isConst = isConst; d.isVolatile = isVolatile; d.to = describe(t->inner, false); n = m_module.add(d); break;
    case sema::Type::Reference: d.kind = CvType::Reference; d.to = describe(t->inner, false); n = m_module.add(d); break;
    case sema::Type::Array: {
        d.kind = CvType::Array; d.to = describe(t->inner, false);
        const int bytes = t->arraySize >= 0 ? m_semaTypes.sizeOf(t) : 0;
        d.bytes = bytes > 0 ? static_cast<unsigned long>(bytes) : 0UL;
        n = m_module.add(d);
        break;
    }
    case sema::Type::Function:
        d.kind = CvType::Procedure; d.to = t->inner ? describe(t->inner, false) : basic(0x03); d.variadic = t->variadic;
        for (size_t i = 0; i < t->params.size(); ++i) d.params.push_back(describe(t->params[i], false));
        n = m_module.add(d);
        break;
    case sema::Type::Class: case sema::Type::Enum:
        if (!t->decl) return 0;
        d.kind = CvType::Class; d.classNumber = classOf(t->decl);
        n = m_module.add(d);
        break;
    case sema::Type::MemberPointer: {
        const Symbol* mc = t->memberClass ? t->memberClass->decl : 0;
        if (!mc || !t->inner) return 0;
        const bool toFunction = t->inner->kind == sema::Type::Function;
        bool multiple = false, virt = false;
        sema::TypeTable::memberPointerModel(mc, multiple, virt, 0);
        // (the size of one in the unspecified model is spelled out here: TypeTable::sizeOf notes on the class that it
        // was asked, and this file only reads)
        const bool unspecified = !mc->isDefinition || mc->memberPointerUnspecified;
        d.kind = CvType::MemberPointer; d.isConst = isConst; d.isVolatile = isVolatile; d.toFunction = toFunction;
        d.ofClass = describe(t->memberClass, true);
        d.bytes = unspecified ? (toFunction ? 24UL : 12UL) : static_cast<unsigned long>(m_semaTypes.sizeOf(t));
        d.model = (toFunction ? 4 : 0) + (unspecified ? 4 : virt ? 3 : multiple ? 2 : 1);
        if (toFunction) {
            // the member function's type: its class, and this as cl types it, a const pointer to the (const) class
            CvType self; self.kind = CvType::Pointer; self.isConst = true; self.to = d.ofClass;
            if (t->inner->constMethod) { CvType c; c.kind = CvType::Modifier; c.isConst = true; c.to = d.ofClass; self.to = m_module.add(c); }
            CvType f; f.kind = CvType::MemberFunction; f.ofClass = d.ofClass; f.thisType = m_module.add(self);
            f.to = t->inner->inner ? describe(t->inner->inner, false) : basic(0x03); f.variadic = t->inner->variadic;
            for (size_t i = 0; i < t->inner->params.size(); ++i) f.params.push_back(describe(t->inner->params[i], false));
            d.to = m_module.add(f);
        }
        else d.to = describe(t->inner, false);
        n = m_module.add(d);
        break;
    }
    case sema::Type::Dependent: case sema::Type::Error: return 0;
    default: { const unsigned long b = basicIndex(t->kind); n = b ? basic(b) : 0; break; }
    }
    memo[t] = n;
    return n;
}

int CvFromSema::functionType(const Symbol* fn)
{
    if (!fn || !fn->type || fn->type->kind != sema::Type::Function) return 0;
    const sema::Type* ft = fn->type;
    const Symbol* owner = fn->ownerClass;
    CvType f;
    f.kind = owner ? CvType::MemberFunction : CvType::Procedure;
    // a constructor and a destructor return nothing, as cl types them
    f.to = ft->inner && !fn->isConstructor && !fn->isDestructor ? typeOf(ft->inner) : basic(0x03);
    if (f.to == 0) f.to = basic(0x03);
    f.variadic = ft->variadic;
    for (size_t i = 0; i < ft->params.size(); ++i) f.params.push_back(typeOf(ft->params[i]));
    if (owner) {
        if (!owner->type) return 0;
        f.ofClass = describe(owner->type, true);
        if (!fn->isStatic) {
            CvType self; self.kind = CvType::Pointer; self.isConst = true; self.to = f.ofClass;
            if (ft->constMethod) { CvType c; c.kind = CvType::Modifier; c.isConst = true; c.to = f.ofClass; self.to = m_module.add(c); }
            f.thisType = m_module.add(self);
        }
    }
    return m_module.add(f);
}

int CvFromSema::classOf(const Symbol* cls)
{
    std::map<const Symbol*, int>::const_iterator it = m_classes.find(cls);
    if (it != m_classes.end()) return it->second;
    CvClass c;
    c.kind = cls->kind == Symbol::Enum ? CvClass::Enum : cls->isUnion ? CvClass::Union : cls->isStruct ? CvClass::Struct : CvClass::Class;
    c.nested = cls->scope && cls->scope->kind == Scope::Class;
    c.scoped = isLocalTo(cls->scope);
    if (cls->name.empty()) {
        // a class without a name: numbered in the unit, and told apart from every other unit's by this unit's id in
        // its unique name (cl names it after its first declarator, which Sema does not keep)
        ++m_unnamed;
        const std::string own = CvNames::unnamedType("$S" + CvNames::integer(m_unnamed));
        c.name = CvNames::scoped(scopeName(cls->scope, true, InType, 0), own);
        c.uniqueName = std::string(".?A") + (c.kind == CvClass::Enum ? "W4" : c.kind == CvClass::Union ? "T" : c.kind == CvClass::Struct ? "U" : "V") + own + "@@`" + unitIdText();
    }
    else {
        c.name = qualifiedName(cls, InType, 0);
        c.uniqueName = msvcRttiTypeName(cls->type);
        // cl tells a class local to a function apart by the unit as well: the unit's id behind a backquote
        if (c.scoped) c.uniqueName += "`" + unitIdText();
        // its S_UDT in an unnamed namespace, as cl writes it: "?A0x<unit id>::Hidden"
        if (inUnnamedNamespace(cls->scope)) {
            const std::string anon = CvNames::anonymousNamespaceOfType();
            c.udtName = c.name;
            for (size_t at = c.udtName.find(anon); at != std::string::npos; at = c.udtName.find(anon, at)) c.udtName.replace(at, anon.size(), "?A0x" + unitIdText());
        }
    }
    const int n = m_module.add(c);
    m_classes[cls] = n;
    m_classSymbols.push_back(cls);
    return n;
}

bool CvFromSema::known(const sema::Type* t, int depth) const
{
    if (!t || depth > kMaxDepth) return false;
    switch (t->kind) {
    case sema::Type::Pointer: case sema::Type::Reference: case sema::Type::Array: return known(t->inner, depth + 1);
    case sema::Type::Function:
        if (t->inner && !known(t->inner, depth + 1)) return false;
        for (size_t i = 0; i < t->params.size(); ++i) if (!known(t->params[i], depth + 1)) return false;
        return true;
    case sema::Type::Class: case sema::Type::Enum: return t->decl && m_classes.count(t->decl) != 0;
    case sema::Type::MemberPointer: return known(t->inner, depth + 1) && known(t->memberClass, depth + 1);
    case sema::Type::Dependent: case sema::Type::Error: return false;
    default: return true;
    }
}

void CvFromSema::variable(const Symbol* v, const std::string& objectSymbol, bool internal)
{
    if (!v || !v->type) return;
    CvVariable out;
    out.type = typeOf(v->type);
    if (out.type == 0) return;
    out.name = CvNames::scoped(scopeName(v->scope, false, InType, 0), v->name);
    out.symbol = objectSymbol;
    out.internal = internal;
    out.thread = v->isThread;
    // a const of integral or enumeration type with a constant value, at namespace scope: an S_CONSTANT where the
    // object has no storage for it
    out.hasValue = v->hasConstValue && !v->isMember && v->type->isConst && v->type->isIntegral();
    out.value = v->constValue;
    if (objectSymbol.empty() && !out.hasValue) return;
    m_module.add(out);
}

// a class's vftable as cl lists it: "Der::`vftable'", an array of const pointers to a function without parameters that
// returns void, one per slot (cl writes one such type of 16 bytes for every table; the slots are counted here)
void CvFromSema::vftable(const Symbol* cls, size_t slots, const std::string& objectSymbol)
{
    if (!cls || objectSymbol.empty() || slots == 0) return;
    if (m_vftableEntry == 0) {
        CvType fn; fn.kind = CvType::Procedure; fn.to = basic(0x03);
        CvType p; p.kind = CvType::Pointer; p.to = m_module.add(fn); p.isConst = true;
        m_vftableEntry = m_module.add(p);
    }
    std::map<size_t, int>::const_iterator known = m_vftableTypes.find(slots);
    int type = known == m_vftableTypes.end() ? 0 : known->second;
    if (type == 0) {
        CvType a; a.kind = CvType::Array; a.to = m_vftableEntry; a.bytes = static_cast<unsigned long>(8 * slots);
        type = m_module.add(a);
        m_vftableTypes[slots] = type;
    }
    CvVariable out;
    out.type = type;
    out.name = CvNames::scoped(qualifiedName(cls, InType, 0), "`vftable'");
    out.symbol = objectSymbol;
    m_module.add(out);
}
// the description of class or enumeration number 'classNumber', when the unit has its definition
void CvFromSema::complete(int classNumber)
{
    const Symbol* cls = m_classSymbols[static_cast<size_t>(classNumber - 1)];
    if (!cls->isDefinition || !cls->layoutDone || cls->isDependentContext) return;
    // (m_module.classAt(classNumber) is taken anew behind every call that may add a class: the vector grows)
    if (cls->kind == Symbol::Enum) {
        std::vector<CvEnumerator> enumerators;
        // the enumerators are symbols of the scope the enumeration is declared in
        if (cls->scope)
            for (size_t i = 0; i < cls->scope->order.size(); ++i) {
                const Symbol* e = cls->scope->order[i];
                if (e->kind != Symbol::Enumerator || !e->type || e->type->decl != cls) continue;
                CvEnumerator x; x.name = e->name; x.value = e->constValue;
                enumerators.push_back(x);
            }
        const int underlying = basic(0x74);
        CvClass& c = m_module.classAt(classNumber);
        c.enumerators = enumerators; c.underlying = underlying; c.size = 4; c.complete = true;
        return;
    }
    std::vector<CvBase> bases;
    std::vector<CvMember> members;
    bool hasNested = false;
    for (size_t b = 0; b < cls->bases.size(); ++b) {
        const sema::BaseClass& bc = cls->bases[b];
        if (!bc.cls || !bc.cls->type || bc.isVirtual) continue;
        CvBase x; x.type = describe(bc.cls->type, true); x.access = accessOf(bc.access); x.offset = bc.offset;
        bases.push_back(x);
    }
    // the virtual bases, each found through the class's vbptr and its entry of that vbtable: the ones the class
    // names itself, then the ones it has from its bases
    const sema::VbTable* table = 0;
    for (size_t t = 0; t < cls->vbtables.size(); ++t) if (cls->vbtables[t].offset == cls->vbptrOffset) { table = &cls->vbtables[t]; break; }
    for (int pass = 0; pass < 2 && table; ++pass) {
        for (size_t v = 0; v < cls->vbases.size(); ++v) {
            const Symbol* vb = cls->vbases[v];
            if (!vb || !vb->type) continue;
            cxx::Access access = cxx::AccessPublic;
            bool direct = false;
            for (size_t b = 0; b < cls->bases.size(); ++b) if (cls->bases[b].cls == vb && cls->bases[b].isVirtual) { direct = true; access = cls->bases[b].access; }
            if (direct != (pass == 0)) continue;
            long entry = 0;
            for (size_t k = 0; k < table->order.size(); ++k) if (table->order[k] == vb) entry = static_cast<long>(k) + 1;
            if (entry == 0) continue;
            CvBase x; x.type = describe(vb->type, true); x.access = accessOf(access); x.isVirtual = true; x.indirect = !direct;
            x.vbptrOffset = cls->vbptrOffset; x.vbIndex = entry;
            bases.push_back(x);
        }
    }
    if (cls->members) {
        for (size_t i = 0; i < cls->members->order.size(); ++i) {
            const Symbol* m = cls->members->order[i];
            CvMember x;
            x.name = m->name; x.access = accessOf(m->access);
            if (m->kind == Symbol::Variable && m->isMember && !m->name.empty()) {
                if (!m->type || m->isDependentContext) continue;
                if (m->isStatic) x.kind = CvMember::Static;
                else {
                    x.kind = CvMember::Data;
                    // a member of an anonymous union or struct is a member of the class, at the two offsets together
                    x.offset = m->anonHost && m->anonOf ? m->anonHost->offset + m->anonOf->offset : m->offset;
                    if (m->bitWidth >= 0) { x.bitWidth = m->bitWidth; x.bitPosition = m->bitOffset; }
                }
                x.type = describe(m->type, false);
                if (x.type != 0) members.push_back(x);
            }
            else if ((m->kind == Symbol::Class || m->kind == Symbol::Enum) && !m->name.empty() && m != cls && m->type && !m->isDependentContext) {
                if (m->kind == Symbol::Class && m->type == cls->type) continue;   // the class's own name in its scope
                x.kind = CvMember::Nested; x.name = ownName(m, InType, 0);
                x.type = describe(m->type, true);
                if (x.type != 0) { members.push_back(x); hasNested = true; }
            }
            else if (m->kind == Symbol::Typedef && !m->name.empty() && m->type && !m->isDependentContext && known(m->type, 0)) {
                // a member typedef, where it names nothing that would have to be described for it alone
                x.kind = CvMember::Nested;
                x.type = describe(m->type, false);
                if (x.type != 0) { members.push_back(x); hasNested = true; }
            }
        }
    }
    CvClass& c = m_module.classAt(classNumber);
    c.bases = bases; c.members = members; c.hasNested = hasNested;
    c.size = cls->size > 0 ? static_cast<unsigned long>(cls->size) : 0UL;
    c.hasConstructor = cls->hasUserCtor || cls->hasDtor;
    if (!cls->vftables.empty() && cls->vftables[0].offset == 0 && !cls->vftables[0].vbase) {
        c.vtableSlots = static_cast<int>(cls->vftables[0].fns.size());
        c.ownVfptr = cls->vftables[0].introducedBy == cls;
    }
    c.complete = true;
}

void CvFromSema::finish(const sema::Program& program)
{
    // the typedef names of namespace scope, in the order they were declared: those whose type is described already
    // or needs no class described (a typedef alone does not bring a class into the object)
    for (size_t i = 0; i < program.symbols.size(); ++i) {
        const Symbol* s = program.symbols[i];
        if (s->kind != Symbol::Typedef || s->name.empty() || !s->type || s->isDependentContext || !s->scope || !s->scope->isNamespaceLike()) continue;
        if (!known(s->type, 0)) continue;
        CvTypedef t;
        t.type = typeOf(s->type);
        if (t.type == 0) continue;
        t.name = CvNames::scoped(scopeName(s->scope, false, InType, 0), s->name);
        m_module.add(t);
    }
    // completing a class describes its members' types, which may be further classes: the list grows as it is walked
    while (m_completed < static_cast<int>(m_classSymbols.size())) complete(++m_completed);
}

} // namespace cppparse
