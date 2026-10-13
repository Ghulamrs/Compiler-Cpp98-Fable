// Sema.cpp - version 6 stage 3: types, symbols, declarations, lookup, class layout, templates.
// Expressions and statements are in SemaExpr.cpp.
#include "Sema.hpp"
#include "PathUtil.hpp"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace cppparse {
namespace sema {


bool Type::isDependent() const
{
    if (kind == Dependent) return true;
    if (inner && inner->isDependent()) return true;
    for (size_t i = 0; i < params.size(); ++i) if (params[i]->isDependent()) return true;
    return false;
}

std::string Type::key() const
{
    std::ostringstream s;
    s << (isConst ? "c" : "") << (isVolatile ? "v" : "") << static_cast<int>(kind);
    switch (kind) {
    case Pointer: case Reference: s << "(" << inner->key() << ")"; break;
    case Array: s << "[" << arraySize << "](" << inner->key() << ")"; break;
    case Function:
        s << "(" << inner->key() << ")(";
        for (size_t i = 0; i < params.size(); ++i) s << (i ? "," : "") << params[i]->key();
        s << (variadic ? "..." : "") << ")" << (constMethod ? "c" : "");
        break;
    case Class: case Enum: case Dependent: s << "@" << static_cast<const void*>(decl); break;
    case MemberPointer: s << "(" << memberClass->key() << ")::*(" << inner->key() << ")"; break;
    default: break;
    }
    return s.str();
}

std::string Type::text() const
{
    std::string cv = std::string(isConst ? "const " : "") + (isVolatile ? "volatile " : "");
    switch (kind) {
    case Void: return cv + "void";
    case Bool: return cv + "bool";
    case Char: return cv + "char";
    case SChar: return cv + "signed char";
    case UChar: return cv + "unsigned char";
    case WChar: return cv + "wchar_t";
    case Short: return cv + "short";
    case UShort: return cv + "unsigned short";
    case Int: return cv + "int";
    case UInt: return cv + "unsigned int";
    case Long: return cv + "long";
    case ULong: return cv + "unsigned long";
    case LongLong: return cv + "long long";
    case ULongLong: return cv + "unsigned long long";
    case Float: return cv + "float";
    case Double: return cv + "double";
    case LongDouble: return cv + "long double";
    case NullPtr: return "(null)";
    case Pointer: return inner->text() + "*" + (isConst ? " const" : "");
    case Reference: return inner->text() + "&";
    case Array: { std::ostringstream s; s << inner->text() << "[" << (arraySize >= 0 ? arraySize : 0) << "]"; return s.str(); }
    case Function: {
        std::string s = inner->text() + "(";
        for (size_t i = 0; i < params.size(); ++i) s += (i ? ", " : "") + params[i]->text();
        if (variadic) s += params.empty() ? "..." : ", ...";
        return s + ")" + (constMethod ? " const" : "");
    }
    case Class: case Enum: return cv + (decl ? decl->qualifiedName() : std::string("?"));
    case MemberPointer: return inner->text() + " " + memberClass->text() + "::*";
    case Dependent: return cv + (decl ? decl->name : std::string("T"));
    case Error: return "<error>";
    }
    return "?";
}

TypeTable::TypeTable()
{
    for (int k = 0; k <= Type::Error; ++k) {
        Type t;
        t.kind = static_cast<Type::Kind>(k);
        m_basic[k] = (k >= Type::Pointer && k <= Type::Dependent) ? 0 : intern(t);
    }
    setTarget(defaultTarget());
}

void TypeTable::setTarget(const Target& target)
{
    m_pointerSize = target.pointerSize;
    m_longSize = target.longSize;
    m_wcharSize = target.wcharSize;
    m_longDoubleSize = target.longDoubleSize;
    m_maxAlign = target.maxScalarAlign;
    m_sizeIsLong = target.sizeTypeIsLong;
}

TypeTable::~TypeTable()
{
    for (std::map<std::string, Type*>::iterator it = m_types.begin(); it != m_types.end(); ++it) delete it->second;
}

const Type* TypeTable::intern(const Type& t)
{
    std::string k = t.key();
    std::map<std::string, Type*>::iterator it = m_types.find(k);
    if (it != m_types.end()) return it->second;
    Type* n = new Type(t);
    m_types[k] = n;
    return n;
}

const Type* TypeTable::qualified(const Type* t, bool c, bool v)
{
    if (!t || (!c && !v) || (t->isConst || !c) && (t->isVolatile || !v)) return t;
    if (t->kind == Type::Reference || t->kind == Type::Function) return t;    // references and functions carry no cv
    if (t->kind == Type::Array) return array(qualified(t->inner, c, v), t->arraySize);
    Type n = *t;
    n.isConst = n.isConst || c;
    n.isVolatile = n.isVolatile || v;
    return intern(n);
}

const Type* TypeTable::unqualified(const Type* t)
{
    if (!t || (!t->isConst && !t->isVolatile)) return t;
    Type n = *t;
    n.isConst = false; n.isVolatile = false;
    return intern(n);
}

const Type* TypeTable::pointer(const Type* to) { Type t; t.kind = Type::Pointer; t.inner = to; return intern(t); }
const Type* TypeTable::reference(const Type* to) { if (to && to->kind == Type::Reference) return to; Type t; t.kind = Type::Reference; t.inner = to; return intern(t); }
const Type* TypeTable::array(const Type* of, long size) { Type t; t.kind = Type::Array; t.inner = of; t.arraySize = size; return intern(t); }
const Type* TypeTable::memberPointer(const Type* cls, const Type* target) { Type t; t.kind = Type::MemberPointer; t.memberClass = cls; t.inner = target; return intern(t); }
const Type* TypeTable::classType(Symbol* cls) { Type t; t.kind = Type::Class; t.decl = cls; return intern(t); }
const Type* TypeTable::enumType(Symbol* en) { Type t; t.kind = Type::Enum; t.decl = en; return intern(t); }
const Type* TypeTable::dependent(Symbol* param) { Type t; t.kind = Type::Dependent; t.decl = param; return intern(t); }

const Type* TypeTable::function(const Type* ret, const std::vector<const Type*>& params, bool variadic, bool constMethod)
{
    Type t; t.kind = Type::Function; t.inner = ret; t.params = params; t.variadic = variadic; t.constMethod = constMethod;
    return intern(t);
}

const Type* TypeTable::decay(const Type* t)
{
    if (!t) return t;
    if (t->kind == Type::Reference) t = t->inner;
    if (t->kind == Type::Array) return pointer(t->inner);
    if (t->kind == Type::Function) return pointer(t);
    return t;
}

int TypeTable::sizeOf(const Type* t) const
{
    if (!t) return 8;
    switch (t->kind) {
    case Type::Void: return 1;
    case Type::Bool: case Type::Char: case Type::SChar: case Type::UChar: return 1;
    case Type::Short: case Type::UShort: return 2;
    case Type::WChar: return m_wcharSize;
    case Type::Int: case Type::UInt: case Type::Float: case Type::Enum: return 4;
    case Type::Long: case Type::ULong: return m_longSize;
    case Type::LongLong: case Type::ULongLong: case Type::Double: return 8;
    case Type::LongDouble: return m_longDoubleSize;
    case Type::Pointer: case Type::Reference: case Type::NullPtr: case Type::MemberPointer: return m_pointerSize;
    case Type::Array: return t->arraySize > 0 ? static_cast<int>(t->arraySize) * sizeOf(t->inner) : 0;
    case Type::Class: return t->decl && t->decl->layoutDone ? t->decl->size : 8;
    default: return 8;
    }
}

int TypeTable::alignOf(const Type* t) const
{
    if (!t) return 8;
    if (t->kind == Type::Array) return alignOf(t->inner);
    if (t->kind == Type::Class) return t->decl && t->decl->layoutDone ? t->decl->align : 8;
    int s = sizeOf(t);
    return s > m_maxAlign ? m_maxAlign : (s < 1 ? 1 : s);
}

int TypeTable::rank(const Type* t) const
{
    switch (t->kind) {
    case Type::Bool: return 1;
    case Type::Char: case Type::SChar: case Type::UChar: return 2;
    case Type::WChar: case Type::Short: case Type::UShort: return 3;
    case Type::Int: case Type::UInt: case Type::Enum: return 4;
    case Type::Long: case Type::ULong: return 5;
    case Type::LongLong: case Type::ULongLong: return 6;
    default: return 0;
    }
}

const Type* TypeTable::promoted(const Type* t)
{
    t = unqualified(unref(t));
    if (t->kind == Type::Float) return basic(Type::Double);
    if (t->isIntegral() && rank(t) < 4) return basic(Type::Int);
    if (t->kind == Type::Enum) return basic(Type::Int);
    return t;
}

const Type* TypeTable::arithmeticResult(const Type* a, const Type* b)
{
    a = promoted(a); b = promoted(b);
    if (a->kind == Type::LongDouble || b->kind == Type::LongDouble) return basic(Type::LongDouble);
    if (a->kind == Type::Double || b->kind == Type::Double) return basic(Type::Double);
    if (a->kind == Type::Float || b->kind == Type::Float) return basic(Type::Float);
    if (rank(a) == rank(b)) return a->isUnsigned() ? a : b;
    const Type* hi = rank(a) > rank(b) ? a : b;
    const Type* lo = hi == a ? b : a;
    // long and unsigned int have the same size here: the signed type cannot hold every value of the other
    if (!hi->isUnsigned() && lo->isUnsigned() && sizeOf(hi) == sizeOf(lo))
        return basic(hi->kind == Type::Long ? Type::ULong : hi->kind == Type::LongLong ? Type::ULongLong : Type::UInt);
    return hi;
}


Symbol::Symbol()
    : kind(Variable), scope(0), type(0), access(cxx::AccessPublic), members(0), isDefinition(false), isStruct(false), isUnion(false),
      isPolymorphic(false), isAbstract(false), layoutDone(false), size(0), align(1), vptrOffset(-1), classAst(0),
      isVirtual(false), isPureVirtual(false), isStatic(false), isExtern(false), isInline(false), isExplicit(false), isConstructor(false),
      isDestructor(false), isConversion(false), isOperator(false), isFriendDecl(false), hasBody(false), isExternC(false),
      definition(0), declAst(0), body(0), vtableSlot(-1), overrides(0), ownerClass(0), offset(0), index(0), isMember(false), isParam(false),
      isLocal(false), isMutable(false), init(0), initCtor(0), hasConstValue(false), constValue(0), declAlign(0), templateAst(0), instantiatedFrom(0),
      isDependentContext(false), isImplicitInstantiation(false), isLazyInstantiation(false), instantiationFailed(false), isImplicit(false), requiredParams(0), hasUserCtor(false), hasCopyCtor(false), hasDtor(false),
      hasCopyAssign(false), hasDefaultCtor(false) {}

std::string Symbol::qualifiedName() const
{
    std::string s = name;
    for (Scope* sc = scope; sc; sc = sc->parent) {
        if ((sc->kind == Scope::Namespace || sc->kind == Scope::Class) && sc->owner && !sc->owner->name.empty()) s = sc->owner->name + "::" + s;
        if (sc->kind == Scope::Function || sc->kind == Scope::Block) break;
    }
    return s;
}

Program::Program()
{
    global = newScope(Scope::Global, 0, 0);
}

Program::~Program()
{
    for (size_t i = 0; i < symbols.size(); ++i) delete symbols[i];
    for (size_t i = 0; i < scopes.size(); ++i) delete scopes[i];
}

Symbol* Program::newSymbol(Symbol::Kind k, const std::string& name, Scope* in)
{
    Symbol* s = new Symbol;
    s->kind = k;
    s->name = name;
    s->scope = in;
    symbols.push_back(s);
    if (in) in->add(s);
    return s;
}

Scope* Program::newScope(Scope::Kind k, Scope* parent, Symbol* owner)
{
    Scope* s = new Scope;
    s->kind = k;
    s->parent = parent;
    s->owner = owner;
    scopes.push_back(s);
    return s;
}


Sema::Sema(TypeTable& types, Program& program)
    : m_types(types), m_prog(program), m_currentFunction(0), m_currentClass(0), m_loopDepth(0), m_switchDepth(0), m_depth(0), m_externalArena(0) {}

Sema::~Sema()
{
    for (size_t i = 0; i < m_ownedDecls.size(); ++i) delete m_ownedDecls[i];
    for (size_t i = 0; i < m_ownedDeclarators.size(); ++i) delete m_ownedDeclarators[i];
}

std::string Sema::where(const cxx::Loc& loc) const
{
    std::ostringstream s;
    s << (loc.file >= 0 && loc.file < static_cast<int>(m_files.size()) ? m_files[static_cast<size_t>(loc.file)] : std::string("?")) << "(" << loc.line << ")";
    return s.str();
}

void Sema::error(const cxx::Loc& loc, const char* code, const std::string& msg)
{
    Diag d; d.severity = Diag::Error; d.code = code; d.loc = loc; d.message = msg;
    m_diags.push_back(d);
}

void Sema::warning(const cxx::Loc& loc, const char* code, const std::string& msg)
{
    Diag d; d.severity = Diag::Warning; d.code = code; d.loc = loc; d.message = msg;
    m_diags.push_back(d);
}

int Sema::errorCount() const
{
    int n = 0;
    for (size_t i = 0; i < m_diags.size(); ++i) if (m_diags[i].severity == Diag::Error) ++n;
    return n;
}


bool Sema::lookupMember(Symbol* cls, const std::string& name, std::vector<Symbol*>& out, int depth)
{
    if (!cls || !cls->members || depth > 32) return false;
    const std::vector<Symbol*>* v = cls->members->find(name);
    if (v) { out.insert(out.end(), v->begin(), v->end()); return true; }
    bool found = false;
    for (size_t b = 0; b < cls->bases.size(); ++b) if (lookupMember(cls->bases[b].cls, name, out, depth + 1)) found = true;
    return found;
}

bool Sema::lookupInScope(Scope* s, const std::string& name, bool typesOnly, std::vector<Symbol*>& out, std::set<Scope*>& seen)
{
    if (!s || seen.count(s)) return false;
    seen.insert(s);
    bool found = false;
    if (s->kind == Scope::Class && s->owner) {
        std::vector<Symbol*> tmp;
        if (lookupMember(s->owner, name, tmp)) {
            for (size_t i = 0; i < tmp.size(); ++i) if (!typesOnly || tmp[i]->isType() || tmp[i]->kind == Symbol::ClassTemplate) out.push_back(tmp[i]);
            found = !out.empty();
        }
    } else {
        const std::vector<Symbol*>* v = s->find(name);
        if (v) for (size_t i = 0; i < v->size(); ++i) if (!typesOnly || (*v)[i]->isType() || (*v)[i]->kind == Symbol::ClassTemplate) { out.push_back((*v)[i]); found = true; }
    }
    // namespaces nominated by using-directives in this scope
    for (size_t u = 0; u < s->usingDirectives.size(); ++u) if (lookupInScope(s->usingDirectives[u], name, typesOnly, out, seen)) found = true;
    return found;
}

Symbol* Sema::lookupUnqualified(const std::string& name, Scope* from, bool typesOnly, std::vector<Symbol*>* all)
{
    for (Scope* s = from; s; s = s->parent) {
        std::vector<Symbol*> out;
        std::set<Scope*> seen;
        if (lookupInScope(s, name, typesOnly, out, seen) && !out.empty()) {
            if (all) *all = out;
            return out[0];
        }
        // the enclosing class of an out-of-line member function: its scope is the class's member scope (parent chain)
    }
    return 0;
}

// inside an instantiation the template's name is the injected class name, a typedef of the instantiation; written
// with template arguments ("Matrix<T, C, R>" inside Matrix<T, R, C>) it names the template, so those arguments
// select the specialisation (14.6.1/1). Returns the class template then, 0 otherwise
Symbol* Sema::injectedTemplate(Symbol* s, const cxx::NameComponent& c) const
{
    if (!s || !c.hasTemplateArgs || s->kind != Symbol::Typedef || !s->type || s->type->kind != Type::Class || !s->type->decl) return 0;
    Symbol* tmpl = s->type->decl->instantiatedFrom;
    return tmpl && tmpl->kind == Symbol::ClassTemplate && tmpl->name == s->name ? tmpl : 0;
}

Scope* Sema::scopeForQualifier(const cxx::QualifiedName& n, Scope* from)
{
    // the scope denoted by all components but the last
    if (n.parts.size() <= 1) return n.global ? m_prog.global : 0;
    Scope* cur = n.global ? m_prog.global : 0;
    for (size_t i = 0; i + 1 < n.parts.size(); ++i) {
        const cxx::NameComponent& c = n.parts[i];
        Symbol* s = 0;
        if (cur) {
            std::vector<Symbol*> out;
            if (cur->kind == Scope::Class) lookupMember(cur->owner, c.ident, out);
            else { const std::vector<Symbol*>* v = cur->find(c.ident); if (v) out = *v; std::set<Scope*> seen; if (out.empty()) lookupInScope(cur, c.ident, true, out, seen); }
            for (size_t k = 0; k < out.size() && !s; ++k) if (out[k]->kind == Symbol::Namespace || out[k]->kind == Symbol::Class || out[k]->kind == Symbol::ClassTemplate || out[k]->kind == Symbol::Typedef) s = out[k];
        } else {
            // scope by scope: inside class X the name X first means its constructors, which do not stop the search
            for (Scope* sc = from; sc && !s; sc = sc->parent) {
                std::vector<Symbol*> out;
                std::set<Scope*> seen;
                lookupInScope(sc, c.ident, false, out, seen);
                for (size_t k = 0; k < out.size() && !s; ++k) if (out[k]->kind == Symbol::Namespace || out[k]->kind == Symbol::Class || out[k]->kind == Symbol::ClassTemplate || out[k]->kind == Symbol::Typedef) s = out[k];
            }
        }
        if (!s) return 0;
        if (Symbol* tmpl = injectedTemplate(s, c)) s = tmpl;    // "M<T, C, R>::" inside M<T, R, C>: another instantiation
        if (s->kind == Symbol::Typedef) { const Type* t = s->type; if (t && t->kind == Type::Class && t->decl) s = t->decl; else return 0; }
        if (s->kind == Symbol::ClassTemplate && !c.hasTemplateArgs) {
            // the template's name inside one of its instantiations is the injected class name
            for (Scope* sc = from; sc && s->kind == Symbol::ClassTemplate; sc = sc->parent) {
                if (sc->kind != Scope::Instantiation) continue;
                const std::vector<Symbol*>* v = sc->find(s->name);
                if (v) for (size_t k = 0; k < v->size(); ++k) if ((*v)[k]->kind == Symbol::Typedef && (*v)[k]->type && (*v)[k]->type->kind == Type::Class && (*v)[k]->type->decl) { s = (*v)[k]->type->decl; break; }
            }
        }
        if (s->kind == Symbol::ClassTemplate) {
            if (!c.hasTemplateArgs) return 0;
            const Type* inst = instantiateClassTemplate(s, c.templateArgs, from, cxx::Loc());
            if (!inst || inst->kind != Type::Class) return 0;
            s = inst->decl;
        }
        cur = s->members;
        if (!cur) return 0;
    }
    return cur;
}

Symbol* Sema::resolveName(const cxx::QualifiedName& n, Scope* from, bool typesOnly, std::vector<Symbol*>* overloads)
{
    if (n.parts.empty()) return 0;
    const std::string& last = n.parts.back().ident;
    if (n.parts.size() == 1 && !n.global) {
        std::vector<Symbol*> all;
        Symbol* s = lookupUnqualified(last, from, typesOnly, &all);
        if (overloads) *overloads = all;
        return s;
    }
    Scope* sc = scopeForQualifier(n, from);
    if (!sc) return 0;
    std::vector<Symbol*> out;
    if (sc->kind == Scope::Class) lookupMember(sc->owner, last, out);
    else { std::set<Scope*> seen; lookupInScope(sc, last, typesOnly, out, seen); }
    if (typesOnly) { std::vector<Symbol*> t; for (size_t i = 0; i < out.size(); ++i) if (out[i]->isType() || out[i]->kind == Symbol::ClassTemplate) t.push_back(out[i]); out = t; }
    if (overloads) *overloads = out;
    return out.empty() ? 0 : out[0];
}

Symbol* Sema::lookupQualified(const cxx::QualifiedName& n, Scope* from, bool typesOnly) { return resolveName(n, from, typesOnly, 0); }


bool Sema::templateArgText(const std::vector<cxx::TemplateArg>& args, Scope* from, std::vector<const Type*>& types, std::vector<long>& ints, std::string& text)
{
    text = "<";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) text += ", ";
        const cxx::TemplateArg& a = args[i];
        if (a.type) {
            const Type* t = typeOf(a.type->spec, from);
            t = applyDeclarator(t, *a.type->decl, from, 0);
            if (!t || t->kind == Type::Error) return false;
            types.push_back(t); ints.push_back(0);
            text += t->text();
        } else if (a.expr) {
            const Type* et = expr(a.expr, from);
            if (et && et->kind == Type::Dependent) { types.push_back(et); ints.push_back(0); text += "?"; continue; }
            // a type named by an expression (an identifier that is a type)
            if (a.expr->kind == cxx::Expr::Name) {
                Symbol* ts = resolveName(a.expr->name, from, true, 0);
                if (ts && ts->isType()) { const Type* t = ts->kind == Symbol::Typedef ? ts->type : ts->kind == Symbol::Class ? m_types.classType(ts) : ts->kind == Symbol::Enum ? m_types.enumType(ts) : m_types.dependent(ts); types.push_back(t); ints.push_back(0); text += t->text(); continue; }
            }
            long v;
            if (!constantValue(a.expr, v)) return false;
            types.push_back(0); ints.push_back(v);
            std::ostringstream s; s << v; text += s.str();
        }
    }
    text += ">";
    return true;
}

const Type* Sema::namedType(const cxx::QualifiedName& n, Scope* in, const std::string& elaborated, const cxx::Loc& loc)
{
    std::vector<Symbol*> cands;
    Symbol* s = resolveName(n, in, true, &cands);
    const cxx::NameComponent& last = n.parts.back();
    if (!s && elaborated == "class" || !s && elaborated == "struct" || !s && elaborated == "union") {
        // "class X" of an undeclared X declares it in the enclosing namespace scope
        if (n.parts.size() == 1 && !n.global) {
            Scope* target = in; while (target && !target->isNamespaceLike() && target->kind != Scope::Class) target = target->parent;
            s = m_prog.newSymbol(Symbol::Class, last.ident, target ? target : m_prog.global);
            s->isStruct = elaborated != "class"; s->isUnion = elaborated == "union"; s->loc = loc;
            s->type = m_types.classType(s);
            return s->type;
        }
    }
    if (!s) {
        if (!m_reportedUnknown.count(n.text())) { m_reportedUnknown.insert(n.text()); error(loc, "S100", "unknown type '" + n.text() + "'"); }
        return m_types.error();
    }
    if (s->kind == Symbol::ClassTemplate) {
        if (!last.hasTemplateArgs) {
            // the injected class name inside the template itself, or a dependent use
            if (s->isDependentContext || in->kind == Scope::TemplateParams) return m_types.dependent(s);
            error(loc, "S111", "template '" + n.text() + "' needs arguments");
            return m_types.error();
        }
        return instantiateClassTemplate(s, last.templateArgs, in, loc);
    }
    if (Symbol* tmpl = injectedTemplate(s, last)) return instantiateClassTemplate(tmpl, last.templateArgs, in, loc);
    if (s->kind == Symbol::Typedef) return s->type ? s->type : m_types.error();
    if (s->kind == Symbol::Class) return m_types.classType(s);
    if (s->kind == Symbol::Enum) return m_types.enumType(s);
    if (s->kind == Symbol::TemplateParam) return s->type ? s->type : m_types.dependent(s);
    error(loc, "S100", "'" + n.text() + "' is not a type");
    return m_types.error();
}

const Type* Sema::typeOf(const cxx::DeclSpec& spec, Scope* in)
{
    const Type* t = 0;
    switch (spec.kind) {
    case cxx::DeclSpec::None: t = m_types.error(); break;
    case cxx::DeclSpec::Builtin: {
        Type::Kind k = Type::Int;
        if (spec.builtinVoid) k = Type::Void;
        else if (spec.builtinBool) k = Type::Bool;
        else if (spec.builtinWchar) k = Type::WChar;
        else if (spec.builtinFloat) k = Type::Float;
        else if (spec.builtinDouble) k = spec.builtinLong ? Type::LongDouble : Type::Double;
        else if (spec.builtinChar) k = spec.builtinUnsigned ? Type::UChar : spec.builtinSigned ? Type::SChar : Type::Char;
        else if (spec.builtinShort) k = spec.builtinUnsigned ? Type::UShort : Type::Short;
        else if (spec.builtinLong >= 2) k = spec.builtinUnsigned ? Type::ULongLong : Type::LongLong;
        else if (spec.builtinLong == 1) k = spec.builtinUnsigned ? Type::ULong : Type::Long;
        else k = spec.builtinUnsigned ? Type::UInt : Type::Int;
        t = m_types.basic(k);
        break;
    }
    case cxx::DeclSpec::Named: t = namedType(spec.name, in, spec.elaborated, spec.loc); break;
    case cxx::DeclSpec::Class: {
        // the class was declared when the enclosing declaration was processed: find it again by name
        if (spec.classDef && !spec.classDef->name.empty()) {
            Symbol* s = resolveName(spec.classDef->name, in, true, 0);
            if (s && s->kind == Symbol::Class) { t = m_types.classType(s); break; }
        }
        if (spec.classDef) { Symbol* s = declareClass(spec.classDef, in, cxx::AccessPublic, 0); t = s ? m_types.classType(s) : m_types.error(); }
        else t = m_types.error();
        break;
    }
    case cxx::DeclSpec::Enum: {
        if (spec.enumDef && !spec.enumDef->name.empty()) {
            cxx::QualifiedName q; cxx::NameComponent c; c.ident = spec.enumDef->name; q.parts.push_back(c);
            Symbol* s = resolveName(q, in, true, 0);
            if (s && s->kind == Symbol::Enum) { t = m_types.enumType(s); break; }
        }
        if (spec.enumDef) { Symbol* s = declareEnum(spec.enumDef, in, cxx::AccessPublic); t = s ? m_types.enumType(s) : m_types.error(); }
        else t = m_types.error();
        break;
    }
    }
    return m_types.qualified(t, spec.isConst, spec.isVolatile);
}

const Type* Sema::applyDeclarator(const Type* base, const cxx::Declarator& d, Scope* in, std::vector<Symbol*>* params)
{
    const Type* t = base;
    for (size_t i = 0; i < d.parts.size(); ++i) {
        const cxx::DeclaratorPart& p = d.parts[i];
        switch (p.kind) {
        case cxx::DeclaratorPart::Pointer: t = m_types.qualified(m_types.pointer(t), p.isConst, p.isVolatile); break;
        case cxx::DeclaratorPart::Reference: t = m_types.reference(t); break;
        case cxx::DeclaratorPart::Array: {
            long n = -1;
            if (p.arraySize) expr(p.arraySize, in);
            if (p.arraySize && !constantValue(p.arraySize, n)) { if (!(p.arraySize->semaType && p.arraySize->semaType->kind == Type::Dependent)) error(d.loc, "S114", "array bound is not a constant"); n = -1; }
            t = m_types.array(t, n);
            break;
        }
        case cxx::DeclaratorPart::MemberPointer: {
            const Type* cls = namedType(p.memberClass, in, "", d.loc);
            t = m_types.memberPointer(cls, t);
            break;
        }
        case cxx::DeclaratorPart::Function: {
            std::vector<const Type*> pts;
            for (size_t k = 0; k < p.params.size(); ++k) {
                const cxx::Param* pa = p.params[k];
                const Type* pt = typeOf(pa->spec, in);
                if (pa->decl) pt = applyDeclarator(pt, *pa->decl, in, 0);
                // parameters of array and function type decay
                if (pt->kind == Type::Array || pt->kind == Type::Function) pt = m_types.decay(pt);
                pts.push_back(pt);
                if (params) {
                    Symbol* ps = m_prog.newSymbol(Symbol::Variable, pa->decl ? pa->decl->name.text() : std::string(""), 0);
                    ps->type = pt; ps->isParam = true; ps->isLocal = true; ps->index = static_cast<int>(k); ps->loc = pa->loc; ps->init = pa->defaultValue;
                    params->push_back(ps);
                }
            }
            t = m_types.function(t, pts, p.variadic, p.isConst);
            break;
        }
        case cxx::DeclaratorPart::Paren: break;
        }
    }
    return t;
}


void Sema::analyze(cxx::TranslationUnit& tu, const std::vector<std::string>& files)
{
    m_files = files;
    declareAll(tu.decls, m_prog.global, cxx::AccessPublic);
    analyzeBodies();
    reportUsedFailedMembers();
}

void Sema::declareAll(const std::vector<cxx::Decl*>& decls, Scope* in, cxx::Access access)
{
    for (size_t i = 0; i < decls.size(); ++i) {
        cxx::Decl* d = decls[i];
        cxx::Access a = in->kind == Scope::Class ? d->access : access;
        declare(d, in, a);
    }
}

void Sema::declare(cxx::Decl* d, Scope* in, cxx::Access access)
{
    if (!d) return;
    switch (d->kind) {
    case cxx::Decl::Namespace: {
        Symbol* ns = 0;
        const std::vector<Symbol*>* v = in->find(d->name.empty() ? std::string("(anonymous)") : d->name);
        if (v) for (size_t i = 0; i < v->size() && !ns; ++i) if ((*v)[i]->kind == Symbol::Namespace) ns = (*v)[i];
        if (!ns) {
            ns = m_prog.newSymbol(Symbol::Namespace, d->name.empty() ? std::string("(anonymous)") : d->name, in);
            ns->loc = d->loc;
            ns->members = m_prog.newScope(Scope::Namespace, in, ns);
            if (d->name.empty()) { ns->name = ""; in->usingDirectives.push_back(ns->members); }
        }
        d->semaSym = ns;
        declareAll(d->decls, ns->members, cxx::AccessPublic);
        break;
    }
    case cxx::Decl::NamespaceAlias: {
        Symbol* target = resolveName(d->target, in, false, 0);
        if (!target || target->kind != Symbol::Namespace) { error(d->loc, "S115", "'" + d->target.text() + "' is not a namespace"); break; }
        Symbol* alias = m_prog.newSymbol(Symbol::Namespace, d->name, in);
        alias->members = target->members; alias->loc = d->loc;
        break;
    }
    case cxx::Decl::UsingDirective: {
        Symbol* target = resolveName(d->target, in, false, 0);
        if (!target || target->kind != Symbol::Namespace) { if (!m_reportedUnknown.count(d->target.text())) { m_reportedUnknown.insert(d->target.text()); error(d->loc, "S115", "unknown namespace '" + d->target.text() + "'"); } break; }
        in->usingDirectives.push_back(target->members);
        break;
    }
    case cxx::Decl::UsingDecl: {
        std::vector<Symbol*> all;
        resolveName(d->target, in, false, &all);
        if (all.empty()) { if (!m_reportedUnknown.count(d->target.text())) { m_reportedUnknown.insert(d->target.text()); error(d->loc, "S101", "unknown name '" + d->target.text() + "' in using declaration"); } break; }
        for (size_t i = 0; i < all.size(); ++i) in->add(all[i]);
        break;
    }
    case cxx::Decl::Class: d->semaSym = declareClass(d->classDef, in, access, 0); break;
    case cxx::Decl::Enum: d->semaSym = declareEnum(d->enumDef, in, access); break;
    case cxx::Decl::Simple: declareSimple(d, in, access); break;
    case cxx::Decl::Function: {
        const Type* base = typeOf(d->spec, in);
        std::vector<Symbol*> params;
        // the declarator's scope for parameter types: the class of an out-of-line member
        Scope* dscope = in;
        if (d->declarator && d->declarator->name.parts.size() > 1) { Scope* q = scopeForQualifier(d->declarator->name, in); if (q) dscope = q; }
        const Type* ft = d->declarator ? applyDeclarator(base, *d->declarator, dscope, &params) : m_types.error();
        Symbol* fn = declareFunction(d, d->declarator, d->spec, in, access, ft, params);
        if (!fn) break;
        d->semaSym = fn;
        if (fn->hasBody && fn->definition != d) { error(d->loc, "S110", "redefinition of '" + fn->qualifiedName() + "'"); break; }
        fn->hasBody = true; fn->definition = d; fn->isInline = fn->isInline || d->spec.isInline;
        if (!fn->isDependentContext) { PendingBody pb; pb.fn = fn; pb.def = d; pb.scope = fn->ownerClass ? fn->ownerClass->members : (dscope != in ? dscope : in); m_pending.push_back(pb); }
        break;
    }
    case cxx::Decl::Template: declareTemplate(d, in, access); break;
    case cxx::Decl::ExplicitInstantiation: {
        if (!d->decls.empty() && d->decls[0]->spec.kind == cxx::DeclSpec::Named) typeOf(d->decls[0]->spec, in);   // forces the instantiation
        break;
    }
    case cxx::Decl::LinkageSpec: {
        for (size_t i = 0; i < d->decls.size(); ++i) {
            declare(d->decls[i], in, access);
            // extern "C": plain names
            cxx::Decl* m = d->decls[i];
            if (d->name == "C") {
                if (m->kind == cxx::Decl::Function && m->semaSym) m->semaSym->isExternC = true;
                if (m->kind == cxx::Decl::Simple) for (size_t k = 0; k < m->declarators.size(); ++k) if (m->declarators[k].decl && m->declarators[k].decl->semaSym) m->declarators[k].decl->semaSym->isExternC = true;
            }
        }
        break;
    }
    case cxx::Decl::AccessLabel: case cxx::Decl::Friend: case cxx::Decl::Empty: case cxx::Decl::Error: break;
    }
}

Symbol* Sema::declareEnum(cxx::EnumDecl* e, Scope* in, cxx::Access access)
{
    Symbol* en = 0;
    if (!e->name.empty()) {
        const std::vector<Symbol*>* v = in->find(e->name);
        if (v) for (size_t i = 0; i < v->size() && !en; ++i) if ((*v)[i]->kind == Symbol::Enum) en = (*v)[i];
    }
    if (!en) { en = m_prog.newSymbol(Symbol::Enum, e->name, in); en->loc = e->loc; en->access = access; en->type = m_types.enumType(en); en->size = 4; en->align = 4; en->layoutDone = true; }
    if (!e->isDefinition) return en;
    en->isDefinition = true;
    long next = 0;
    for (size_t i = 0; i < e->enumerators.size(); ++i) {
        const cxx::Enumerator& x = e->enumerators[i];
        Symbol* s = m_prog.newSymbol(Symbol::Enumerator, x.name, in);     // enumerators live in the enclosing scope
        s->loc = x.loc; s->access = access; s->type = en->type; s->ownerClass = in->kind == Scope::Class ? in->owner : 0;
        if (x.value) {
            expr(x.value, in);
            long v;
            // in a template's pattern the value may depend on the parameters (sizeof of a dependent type): checked per instantiation
            const bool dependent = in->kind == Scope::TemplateParams || (in->kind == Scope::Class && in->owner && in->owner->isDependentContext);
            if (constantValue(x.value, v)) next = v;
            else if (!dependent) error(x.loc, "S114", "enumerator value is not a constant");
        }
        s->hasConstValue = true; s->constValue = next; s->index = static_cast<int>(i);
        ++next;
    }
    return en;
}

Symbol* Sema::declareClass(cxx::ClassDecl* c, Scope* in, cxx::Access access, Symbol* existing)
{
    Scope* target = in;
    std::string simple = c->name.parts.empty() ? std::string("") : c->name.parts.back().ident;
    if (c->name.parts.size() > 1) { Scope* q = scopeForQualifier(c->name, in); if (q) target = q; }
    Symbol* cls = existing;
    if (!cls && !simple.empty()) {
        std::vector<Symbol*> out;
        if (target->kind == Scope::Class) lookupMember(target->owner, simple, out);
        else { const std::vector<Symbol*>* v = target->find(simple); if (v) out = *v; }
        for (size_t i = 0; i < out.size() && !cls; ++i) if (out[i]->kind == Symbol::Class && out[i]->scope == target) cls = out[i];
    }
    if (!cls) {
        cls = m_prog.newSymbol(Symbol::Class, simple.empty() ? std::string("(anonymous)") : simple, target);
        cls->loc = c->loc; cls->access = access;
        cls->type = m_types.classType(cls);
        if (simple.empty()) cls->name = "";
    }
    cls->isStruct = c->key != "class"; cls->isUnion = c->key == "union";
    cls->classAst = c;
    // a class pattern of a template, or a class nested in one: its member bodies wait for an instantiation
    if (!existing) cls->isDependentContext = target->kind == Scope::TemplateParams || (target->kind == Scope::Class && target->owner && target->owner->isDependentContext);
    if (!c->isDefinition) return cls;
    if (cls->isDefinition) { error(c->loc, "S110", "redefinition of class '" + cls->qualifiedName() + "'"); return cls; }
    cls->isDefinition = true;
    cls->members = m_prog.newScope(Scope::Class, target, cls);
    ++m_stats.classes;
    // bases
    for (size_t b = 0; b < c->bases.size(); ++b) {
        const Type* bt = namedType(c->bases[b].name, in, "", c->bases[b].loc);
        BaseClass bc;
        bc.access = c->bases[b].access == cxx::AccessDefault ? (cls->isStruct ? cxx::AccessPublic : cxx::AccessPrivate) : c->bases[b].access;
        bc.isVirtual = c->bases[b].isVirtual;
        if (bt->kind == Type::Class && bt->decl) {
            if (!bt->decl->isDefinition) error(c->bases[b].loc, "S113", "base class '" + bt->decl->qualifiedName() + "' is incomplete");
            bc.cls = bt->decl;
            cls->bases.push_back(bc);
        } else if (bt->kind == Type::Dependent) {
            // a dependent base: members are looked up at instantiation time
        } else if (bt->kind != Type::Error) error(c->bases[b].loc, "S113", "base '" + c->bases[b].name.text() + "' is not a class");
    }
    // members: the class is complete for member function bodies, which are analysed later
    Symbol* savedClass = m_currentClass;
    m_currentClass = cls;
    declareAll(c->members, cls->members, cls->isStruct ? cxx::AccessPublic : cxx::AccessPrivate);
    m_currentClass = savedClass;
    completeClass(cls);
    return cls;
}

namespace {

bool sameSignature(const Type* a, const Type* b)
{
    if (!a || !b || a->kind != Type::Function || b->kind != Type::Function) return false;
    if (a->params.size() != b->params.size() || a->variadic != b->variadic || a->constMethod != b->constMethod) return false;
    for (size_t i = 0; i < a->params.size(); ++i) {
        const Type* x = a->params[i]; const Type* y = b->params[i];
        // top-level cv of parameters does not take part in the signature
        if (x->kind != Type::Reference && x->kind != Type::Pointer) { if (x->isConst || x->isVolatile) x = 0; }
        if (y->kind != Type::Reference && y->kind != Type::Pointer) { if (y->isConst || y->isVolatile) y = 0; }
        if (x && y && x != y) return false;
        if ((!x || !y) && a->params[i]->kind != b->params[i]->kind) return false;
    }
    return true;
}

// the written form of a function template's declaration, for matching a redeclaration or the definition with an
// earlier declaration (14.5.5.1: the same template parameters, result and parameter types). Parameter names are
// left out, template parameter names must agree, and template arguments that are expressions keep their text
// ("Matrix<T, R - 1, C - 1>"), so two overloads that differ only there stay apart
std::string nameShape(const cxx::QualifiedName& n);

std::string specShape(const cxx::DeclSpec& s)
{
    if (s.kind != cxx::DeclSpec::Named) return s.text();
    return std::string(s.isConst ? "const " : "") + (s.isVolatile ? "volatile " : "") + nameShape(s.name);
}

std::string declaratorShape(const cxx::Declarator* d)
{
    if (!d) return "";
    cxx::Declarator unnamed = *d;
    unnamed.name = cxx::QualifiedName();
    return unnamed.text();
}

std::string nameShape(const cxx::QualifiedName& n)
{
    std::string s = n.global ? "::" : "";
    for (size_t i = 0; i < n.parts.size(); ++i) {
        const cxx::NameComponent& c = n.parts[i];
        s += (i ? "::" : "") + c.ident;
        if (!c.hasTemplateArgs) continue;
        s += "<";
        for (size_t a = 0; a < c.templateArgs.size(); ++a) {
            const cxx::TemplateArg& ta = c.templateArgs[a];
            s += a ? ", " : "";
            if (ta.type) s += specShape(ta.type->spec) + " " + declaratorShape(ta.type->decl);
            else s += cxx::expressionText(ta.expr);
        }
        s += ">";
    }
    return s;
}

std::string templateSignature(const cxx::Decl* tmpl, const cxx::Decl* inner, const cxx::Declarator* dcl)
{
    std::string s = "<";
    for (size_t i = 0; i < tmpl->templateParams.size(); ++i) {
        const cxx::TemplateParam* p = tmpl->templateParams[i];
        s += (i ? ", " : "") + std::string(p->kind == cxx::TemplateParam::Type ? "typename " : p->kind == cxx::TemplateParam::Template ? "template " : "");
        if (p->kind == cxx::TemplateParam::NonType && p->nonType) s += specShape(p->nonType->spec) + " " + declaratorShape(p->nonType->decl) + " ";
        s += p->name;
    }
    // the result and the function's own declarator parts (const, variadic), then the parameters one by one
    cxx::Declarator outer = *dcl;
    if (!outer.parts.empty()) outer.parts.back().params.clear();
    s += "> " + specShape(inner->spec) + " " + declaratorShape(&outer);
    const cxx::DeclaratorPart* fp = dcl->functionPart();
    for (size_t k = 0; fp && k < fp->params.size(); ++k) s += " | " + specShape(fp->params[k]->spec) + " " + declaratorShape(fp->params[k]->decl);
    return s;
}

} // namespace

Symbol* Sema::declareFunction(cxx::Decl* d, cxx::Declarator* dcl, const cxx::DeclSpec& spec, Scope* in, cxx::Access access, const Type* ft, const std::vector<Symbol*>& params)
{
    if (!dcl || ft->kind != Type::Function) return 0;
    const cxx::QualifiedName& n = dcl->name;
    std::string name = n.parts.back().ident;
    if (n.parts.back().convType) name = "operator " + n.parts.back().convType->text();
    Scope* target = in;
    Symbol* owner = in->kind == Scope::Class ? in->owner : 0;
    if (n.parts.size() > 1) {
        Scope* q = scopeForQualifier(n, in);
        if (!q) { if (!m_reportedUnknown.count(n.text())) { m_reportedUnknown.insert(n.text()); error(dcl->loc, "S101", "unknown scope in '" + n.text() + "'"); } return 0; }
        target = q;
        owner = q->kind == Scope::Class ? q->owner : 0;
    }
    bool ctor = owner && name == owner->ctorName() && !n.parts.back().hasTemplateArgs;
    bool dtor = owner && !name.empty() && name[0] == '~';
    bool conv = n.parts.back().convType != 0;
    if (ctor || dtor) ft = m_types.function(m_types.basic(Type::Void), ft->params, ft->variadic, ft->constMethod);
    else if (conv) {
        const cxx::TypeId* ct = n.parts.back().convType;
        const Type* rt = typeOf(ct->spec, target);
        if (ct->decl) rt = applyDeclarator(rt, *ct->decl, target, 0);
        ft = m_types.function(rt, ft->params, ft->variadic, ft->constMethod);
    }
    // an existing declaration with the same signature?
    std::vector<Symbol*> existing;
    if (target->kind == Scope::Class) { const std::vector<Symbol*>* v = target->find(name); if (v) existing = *v; }
    else { const std::vector<Symbol*>* v = target->find(name); if (v) existing = *v; }
    for (size_t i = 0; i < existing.size(); ++i) {
        Symbol* e = existing[i];
        if (e->kind == Symbol::Function && sameSignature(e->type, ft)) {
            if (n.parts.size() > 1 || e->scope == target) {
                dcl->semaSym = e;
                // an out-of-line definition supplies the parameter names
                if (!params.empty() && e->params.size() == params.size()) for (size_t k = 0; k < params.size(); ++k) if (e->params[k]->name.empty()) e->params[k]->name = params[k]->name;
                if (!params.empty()) {
                    // default arguments given by the declaration stay with the function
                    if (e->params.size() == params.size()) for (size_t k = 0; k < params.size(); ++k) if (!params[k]->init && e->params[k]->init) params[k]->init = e->params[k]->init;
                    e->params = params;
                }
                return e;
            }
        }
    }
    if (n.parts.size() > 1 && !existing.empty() && target->kind == Scope::Class) {
        bool anyFn = false;
        for (size_t i = 0; i < existing.size(); ++i) if (existing[i]->kind == Symbol::Function) anyFn = true;
        if (anyFn) { error(dcl->loc, "S116", "'" + n.text() + "' does not match any declaration in its class"); }
    } else if (n.parts.size() > 1 && target->kind == Scope::Class) {
        error(dcl->loc, "S116", "'" + n.text() + "' is not a member of its class");
    }
    Symbol* fn = m_prog.newSymbol(Symbol::Function, name, target);
    fn->loc = dcl->loc; fn->access = access; fn->type = ft; fn->params = params; fn->declAst = d; fn->ownerClass = owner;
    fn->isStatic = spec.isStatic; fn->isExtern = spec.isExtern; fn->isExplicit = spec.isExplicit;
    fn->isInline = spec.isInline || (in->kind == Scope::Class && d->kind == cxx::Decl::Function);   // defined in the class body
    fn->isVirtual = spec.isVirtual; fn->isConstructor = ctor; fn->isDestructor = dtor; fn->isConversion = conv;
    fn->isOperator = name.compare(0, 8, "operator") == 0 && !conv;
    fn->isDependentContext = in->kind == Scope::TemplateParams || (owner && owner->isDependentContext);
    fn->requiredParams = static_cast<int>(params.size());
    for (size_t k = 0; k < params.size(); ++k) if (params[k]->init) { fn->requiredParams = static_cast<int>(k); break; }
    // default arguments are analysed with the bodies, in the class or declaring scope (a member may be declared later)
    if (!fn->isDependentContext) for (size_t k = 0; k < params.size(); ++k) if (params[k]->init) { PendingBody pb; pb.fn = params[k]; pb.def = d; pb.scope = owner && owner->members ? owner->members : target; m_pending.push_back(pb); }
    for (size_t k = 0; k < params.size(); ++k) params[k]->scope = 0;
    if (owner) {
        if (ctor) { owner->hasUserCtor = true; if (params.empty() || fn->requiredParams == 0) owner->hasDefaultCtor = true;
                    if (params.size() >= 1 && fn->requiredParams <= 1) { const Type* p0 = params[0]->type; if (p0->kind == Type::Reference && m_types.unqualified(p0->inner) == owner->type) owner->hasCopyCtor = true; } }
        if (dtor) owner->hasDtor = true;
        if (fn->isOperator && name == "operator=" && params.size() == 1) { const Type* p0 = params[0]->type; if ((p0->kind == Type::Reference && m_types.unqualified(p0->inner) == owner->type) || m_types.unqualified(p0) == owner->type) owner->hasCopyAssign = true; }
    }
    dcl->semaSym = fn;
    ++m_stats.symbols;
    return fn;
}

void Sema::declareSimple(cxx::Decl* d, Scope* in, cxx::Access access)
{
    // an inline class / enum definition in the specifiers
    Symbol* defined = 0;
    if (d->spec.kind == cxx::DeclSpec::Class && d->spec.classDef) { defined = declareClass(d->spec.classDef, in, access, 0); d->semaSym = defined; }
    else if (d->spec.kind == cxx::DeclSpec::Enum && d->spec.enumDef) { defined = declareEnum(d->spec.enumDef, in, access); d->semaSym = defined; }
    else if (d->spec.kind == cxx::DeclSpec::Named && !d->spec.elaborated.empty() && d->spec.elaborated != "typename" && d->spec.elaborated != "enum" && d->declarators.empty()) {
        // "class X;" declares X in this scope unless this scope has it already: an X of an outer scope does not count
        if (d->spec.name.parts.size() == 1 && !d->spec.name.global) {
            const std::string& nm = d->spec.name.parts[0].ident;
            Symbol* here = 0;
            const std::vector<Symbol*>* v = in->find(nm);
            if (v) for (size_t k = 0; k < v->size() && !here; ++k) if ((*v)[k]->kind == Symbol::Class || (*v)[k]->kind == Symbol::ClassTemplate) here = (*v)[k];
            if (!here) {
                Symbol* cls = m_prog.newSymbol(Symbol::Class, nm, in);
                cls->loc = d->spec.loc; cls->access = access; cls->type = m_types.classType(cls);
                cls->isStruct = d->spec.elaborated != "class"; cls->isUnion = d->spec.elaborated == "union";
                cls->isDependentContext = in->kind == Scope::TemplateParams || (in->kind == Scope::Class && in->owner && in->owner->isDependentContext);
                d->semaSym = cls;
            }
            return;
        }
        namedType(d->spec.name, in, d->spec.elaborated, d->spec.loc);
        return;
    }
    if (d->declarators.empty()) return;
    const Type* base;
    if (defined) base = m_types.qualified(defined->kind == Symbol::Class ? m_types.classType(defined) : m_types.enumType(defined), d->spec.isConst, d->spec.isVolatile);
    else base = typeOf(d->spec, in);

    for (size_t i = 0; i < d->declarators.size(); ++i) {
        cxx::InitDeclarator& id = d->declarators[i];
        cxx::Declarator* dcl = id.decl;
        if (!dcl) continue;
        Scope* dscope = in;
        if (dcl->name.parts.size() > 1) { Scope* q = scopeForQualifier(dcl->name, in); if (q) dscope = q; }
        std::vector<Symbol*> params;
        const Type* t = completedArray(applyDeclarator(base, *dcl, dscope, &params), id.init);
        const std::string& name = dcl->name.parts.empty() ? std::string("") : dcl->name.parts.back().ident;
        if (d->spec.isTypedef) {
            Symbol* td = m_prog.newSymbol(Symbol::Typedef, name, in);
            td->type = t; td->loc = dcl->loc; td->access = access; dcl->semaSym = td;
            continue;
        }
        if (t->kind == Type::Function) {
            Symbol* fn = declareFunction(d, dcl, d->spec, in, access, t, params);
            if (fn) { if (id.pureVirtual) { fn->isPureVirtual = true; fn->isVirtual = true; } if (d->spec.isFriend) fn->isFriendDecl = true; }
            continue;
        }
        // variable, field or static member
        Scope* target = in;
        Symbol* var = 0;
        if (dcl->name.parts.size() > 1) {
            // out-of-line definition of a static data member
            Scope* q = scopeForQualifier(dcl->name, in);
            if (q) { std::vector<Symbol*> out; if (q->kind == Scope::Class) lookupMember(q->owner, name, out); for (size_t k = 0; k < out.size() && !var; ++k) if (out[k]->kind == Symbol::Variable) var = out[k]; }
            if (!var) { error(dcl->loc, "S116", "'" + dcl->name.text() + "' is not a static member of its class"); continue; }
            var->isDefinition = true;
            if (var->type && var->type->kind == Type::Array && var->type->arraySize < 0 && t->kind == Type::Array) var->type = t;
        } else {
            // "extern int x;" after "int x;" refers to the same variable
            const std::vector<Symbol*>* v = target->find(name);
            if (v && target->isNamespaceLike()) for (size_t k = 0; k < v->size() && !var; ++k) if ((*v)[k]->kind == Symbol::Variable) var = (*v)[k];
            if (!var) {
                var = m_prog.newSymbol(Symbol::Variable, name, target);
                var->type = t; var->loc = dcl->loc; var->access = access;
                var->isStatic = d->spec.isStatic; var->isExtern = d->spec.isExtern; var->isMutable = d->spec.isMutable;
                var->isMember = target->kind == Scope::Class;
                var->ownerClass = var->isMember ? target->owner : 0;
                if (var->isMember && !var->isStatic) { var->index = static_cast<int>(target->owner->fields.size()); target->owner->fields.push_back(var); }
                var->isDependentContext = in->kind == Scope::TemplateParams || (var->ownerClass && var->ownerClass->isDependentContext);
                ++m_stats.variables;
            }
            var->isDefinition = var->isDefinition || !d->spec.isExtern;
        }
        dcl->semaSym = var;
        if (d->spec.align > var->declAlign) var->declAlign = d->spec.align;
        if (!var->declAst || id.init.kind != cxx::Initializer::None) var->declAst = d;   // the declaration carrying the initializer
        if (id.init.kind != cxx::Initializer::None) {
            var->init = id.init.args.empty() ? 0 : id.init.args[0];
            if (!var->isDependentContext && !var->isMember) { PendingBody pb; pb.fn = var; pb.def = d; pb.scope = in; m_pending.push_back(pb); }
            else if (var->isMember && var->isStatic && var->init && !var->isDependentContext) { PendingBody pb; pb.fn = var; pb.def = d; pb.scope = in; m_pending.push_back(pb); }
        }
    }
}

void Sema::declareTemplate(cxx::Decl* d, Scope* in, cxx::Access access)
{
    if (d->decls.empty()) return;
    cxx::Decl* inner = d->decls[0];
    // the template parameter scope
    Scope* tscope = m_prog.newScope(Scope::TemplateParams, in, 0);
    std::vector<Symbol*> tparams;
    for (size_t i = 0; i < d->templateParams.size(); ++i) {
        const cxx::TemplateParam* p = d->templateParams[i];
        Symbol* ps = m_prog.newSymbol(Symbol::TemplateParam, p->name.empty() ? std::string("$T") : p->name, tscope);
        ps->loc = p->loc; ps->index = static_cast<int>(i);
        if (p->kind == cxx::TemplateParam::NonType) { ps->kind = Symbol::Variable; ps->type = p->nonType ? typeOf(p->nonType->spec, tscope) : m_types.basic(Type::Int); ps->isDependentContext = true; ps->hasConstValue = false; ps->init = p->nonType ? p->nonType->defaultValue : 0; ps->kind = Symbol::TemplateParam; }
        else ps->type = m_types.dependent(ps);
        tparams.push_back(ps);
    }
    if (inner->kind == cxx::Decl::Template) { declareTemplate(inner, tscope, access); return; }   // member template of a template

    // which entity?
    cxx::ClassDecl* classAst = 0;
    if (inner->kind == cxx::Decl::Class) classAst = inner->classDef;
    else if (inner->kind == cxx::Decl::Simple && inner->spec.kind == cxx::DeclSpec::Class && inner->declarators.empty()) classAst = inner->spec.classDef;
    if (!classAst && inner->kind == cxx::Decl::Simple && inner->spec.kind == cxx::DeclSpec::Named && inner->declarators.empty()
        && (inner->spec.elaborated == "class" || inner->spec.elaborated == "struct" || inner->spec.elaborated == "union")
        && inner->spec.name.parts.size() == 1 && !inner->spec.name.parts[0].hasTemplateArgs) {
        // "template <...> class X;" declares the template without a definition: its specialisations may follow,
        // and an instantiation before a definition is an incomplete class
        const std::string& name = inner->spec.name.parts[0].ident;
        Symbol* ct = 0;
        const std::vector<Symbol*>* v = in->find(name);
        if (v) for (size_t i = 0; i < v->size() && !ct; ++i) if ((*v)[i]->kind == Symbol::ClassTemplate && !(*v)[i]->patternArgs.size()) ct = (*v)[i];
        if (!ct) {
            ct = m_prog.newSymbol(Symbol::ClassTemplate, name, in);
            ct->loc = d->loc; ct->access = access; ct->templateAst = d; ct->templateParams = tparams; ct->isDependentContext = true;
            tscope->owner = ct;
            ++m_stats.symbols;
        }
        d->semaSym = ct; inner->semaSym = ct;
        return;
    }
    if (classAst) {
        std::string name = classAst->name.parts.empty() ? std::string("") : classAst->name.parts.back().ident;
        bool specialization = !classAst->name.parts.empty() && classAst->name.parts.back().hasTemplateArgs;
        Symbol* primary = 0;
        {
            std::vector<Symbol*> out;
            Scope* target = in;
            if (classAst->name.parts.size() > 1) { Scope* q = scopeForQualifier(classAst->name, in); if (q) target = q; }
            const std::vector<Symbol*>* v = target->find(name);
            if (v) for (size_t i = 0; i < v->size() && !primary; ++i) if ((*v)[i]->kind == Symbol::ClassTemplate && !(*v)[i]->patternArgs.size()) primary = (*v)[i];
            if (!specialization) {
                Symbol* ct = primary;
                if (!ct) { ct = m_prog.newSymbol(Symbol::ClassTemplate, name, target); ct->loc = d->loc; ct->access = access; ++m_stats.symbols; }
                ct->templateAst = d; ct->templateParams = tparams; ct->classAst = classAst; ct->isDependentContext = true;
                tscope->owner = ct;
                if (classAst->isDefinition) ct->isDefinition = true;
                d->semaSym = ct; inner->semaSym = ct;
                return;
            }
            if (!primary) { error(d->loc, "S111", "specialisation of unknown template '" + name + "'"); return; }
            if (d->explicitSpecialization) {
                // a full specialisation is an ordinary class registered under its argument text
                std::vector<const Type*> types; std::vector<long> ints; std::string key;
                if (!templateArgText(classAst->name.parts.back().templateArgs, in, types, ints, key)) { error(d->loc, "S111", "bad specialisation arguments"); return; }
                cxx::ClassDecl copy = *classAst;
                copy.name.parts.back().hasTemplateArgs = false;
                copy.name.parts.back().ident = name + key;
                Scope* iscope = m_prog.newScope(Scope::Instantiation, in, primary);
                Symbol* cls = m_prog.newSymbol(Symbol::Class, name + key, iscope);
                cls->type = m_types.classType(cls); cls->instantiatedFrom = primary; cls->templateArgs = types; cls->templateIntArgs = ints;
                classAst->name.parts.back().ident = name + key; classAst->name.parts.back().hasTemplateArgs = false;   // so that out-of-line members match
                declareClass(classAst, iscope, access, cls);
                primary->instantiations[key] = cls;
                d->semaSym = cls;
                return;
            }
            // partial specialisation: keep the pattern for matching at instantiation time
            Symbol* ps = m_prog.newSymbol(Symbol::ClassTemplate, name, tscope);
            ps->templateAst = d; ps->templateParams = tparams; ps->classAst = classAst; ps->isDependentContext = true;
            ps->patternArgs = classAst->name.parts.back().templateArgs; ps->instantiatedFrom = primary;
            tscope->owner = ps;
            primary->partialSpecs.push_back(ps);
            d->semaSym = ps;
            return;
        }
    }
    // function template (definition or declaration), or a static member of a class template etc.
    cxx::Declarator* dcl = 0;
    if (inner->kind == cxx::Decl::Function) dcl = inner->declarator;
    else if (inner->kind == cxx::Decl::Simple && !inner->declarators.empty()) dcl = inner->declarators[0].decl;
    if (dcl && dcl->isFunction()) {
        std::string name = dcl->name.parts.back().ident;
        Scope* target = in;
        if (dcl->name.parts.size() > 1) {
            // member of a class template defined out of line: attach to the template so instantiation finds it
            Symbol* ct = 0;
            std::vector<Symbol*> out;
            cxx::QualifiedName q = dcl->name; q.parts.pop_back();
            if (q.parts.size() == 1) { lookupUnqualified(q.parts[0].ident, in, false, &out); for (size_t i = 0; i < out.size() && !ct; ++i) if (out[i]->kind == Symbol::ClassTemplate) ct = out[i]; }
            if (ct) { Symbol* mt = m_prog.newSymbol(Symbol::FunctionTemplate, name, tscope); mt->templateAst = d; mt->templateParams = tparams; mt->declAst = inner; mt->isDependentContext = true; mt->ownerClass = ct; ct->partialSpecs.push_back(mt); tscope->owner = mt; d->semaSym = mt; return; }
            Scope* qs = scopeForQualifier(dcl->name, in);
            if (qs) target = qs;
        }
        // a redeclaration, or the definition after a forward declaration, is the same template (one candidate):
        // the definition takes the symbol over, keeping the default arguments only a declaration gave
        const std::string sig = templateSignature(d, inner, dcl);
        const std::vector<Symbol*>* same = target->find(name);
        for (size_t i = 0; same && i < same->size(); ++i) {
            Symbol* ex = (*same)[i];
            if (ex->kind != Symbol::FunctionTemplate || ex->scope != target || !ex->templateAst || !ex->declAst) continue;
            const cxx::Declarator* exd = ex->declAst->kind == cxx::Decl::Function ? ex->declAst->declarator : (ex->declAst->declarators.empty() ? 0 : ex->declAst->declarators[0].decl);
            if (!exd || !exd->isFunction() || templateSignature(ex->templateAst, ex->declAst, exd) != sig) continue;
            if (inner->kind == cxx::Decl::Function) {
                if (ex->declAst->kind == cxx::Decl::Function) { error(d->loc, "S110", "redefinition of function template '" + ex->qualifiedName() + "'"); d->semaSym = ex; return; }
                std::vector<cxx::Param*>& np = dcl->parts.back().params;
                const std::vector<cxx::Param*>& op = exd->parts.back().params;
                for (size_t k = 0; k < np.size() && k < op.size(); ++k) if (!np[k]->defaultValue) np[k]->defaultValue = op[k]->defaultValue;
                ex->templateAst = d; ex->declAst = inner; ex->templateParams = tparams;
                tscope->owner = ex;
            }
            d->semaSym = ex;
            return;
        }
        Symbol* ft = m_prog.newSymbol(Symbol::FunctionTemplate, name, target);
        ft->loc = d->loc; ft->access = access; ft->templateAst = d; ft->templateParams = tparams; ft->declAst = inner; ft->isDependentContext = true;
        ft->ownerClass = target->kind == Scope::Class ? target->owner : 0;
        tscope->owner = ft;
        d->semaSym = ft;
        ++m_stats.symbols;
        return;
    }
    // other templates (static data members of class templates, typedef templates do not exist in C++98): ignore
}


bool Sema::derivesFrom(Symbol* derived, Symbol* base, int* offset, int depth)
{
    if (!derived || !base || depth > 32) return false;
    if (derived == base) { if (offset) *offset = 0; return true; }
    for (size_t i = 0; i < derived->bases.size(); ++i) {
        int o = 0;
        if (derivesFrom(derived->bases[i].cls, base, &o, depth + 1)) { if (offset) *offset = derived->bases[i].offset + o; return true; }
    }
    return false;
}

namespace {

// the alignment __declspec(align(N)) demands of a type, which #pragma pack cannot lower: declared on its class or
// inherited from an aligned member or base (Symbol::declAlign of a laid-out class)
int declaredFloor(const Type* t)
{
    while (t && t->kind == Type::Array) t = t->inner;
    return t && t->kind == Type::Class && t->decl && t->decl->layoutDone ? t->decl->declAlign : 0;
}

} // namespace

void Sema::layoutClass(Symbol* cls)
{
    if (cls->layoutDone) return;
    // #pragma pack(n) caps the alignment of every member at n; __declspec(align(N)) is a floor it cannot lower
    const int pack = cls->classAst ? cls->classAst->pack : 0;
    int floor = cls->classAst ? cls->classAst->declAlign : 0;
    int off = 0, align = 1;
    bool vptr = false;
    for (size_t b = 0; b < cls->bases.size(); ++b) {
        Symbol* bc = cls->bases[b].cls;
        if (!bc) continue;
        if (!bc->layoutDone) layoutClass(bc);
        if (b == 0 && bc->isPolymorphic) vptr = true;        // share the primary base's vptr
        int a = pack > 0 && bc->align > pack ? pack : bc->align;
        if (bc->declAlign > a) a = bc->declAlign;
        if (bc->declAlign > floor) floor = bc->declAlign;
        off = (off + a - 1) / a * a;
        cls->bases[b].offset = off;
        off += bc->size;
        if (a > align) align = a;
    }
    bool ownVirtuals = false;
    for (size_t i = 0; i < cls->members->order.size(); ++i) { Symbol* m = cls->members->order[i]; if (m->kind == Symbol::Function && m->isVirtual) ownVirtuals = true; }
    for (size_t b = 0; b < cls->bases.size() && !ownVirtuals; ++b) if (cls->bases[b].cls && cls->bases[b].cls->isPolymorphic) ownVirtuals = true;
    cls->isPolymorphic = ownVirtuals;
    if (ownVirtuals && !vptr) {
        // a new vptr at offset 0: shift the bases
        for (size_t b = 0; b < cls->bases.size(); ++b) cls->bases[b].offset += 8;
        off += 8;
        cls->vptrOffset = 0;
        const int va = pack > 0 && pack < 8 ? pack : 8;
        if (align < va) align = va;
    } else if (vptr) cls->vptrOffset = 0;
    for (size_t f = 0; f < cls->fields.size(); ++f) {
        Symbol* fld = cls->fields[f];
        const Type* ft = fld->type;
        const Type* elem = ft;
        while (elem && elem->kind == Type::Array) elem = elem->inner;
        if (elem && elem->kind == Type::Class && elem->decl && !elem->decl->layoutDone) layoutClass(elem->decl);
        int s = m_types.sizeOf(ft), a = m_types.alignOf(ft);
        if (ft->kind == Type::Reference) { s = 8; a = 8; }
        if (pack > 0 && a > pack) a = pack;
        const int need = fld->declAlign > declaredFloor(ft) ? fld->declAlign : declaredFloor(ft);
        if (need > a) a = need;
        if (need > floor) floor = need;
        if (cls->isUnion) { fld->offset = 0; if (s > off) off = s; }
        else { off = (off + a - 1) / a * a; fld->offset = off; off += s; }
        if (a > align) align = a;
    }
    if (floor > align) align = floor;
    if (off == 0) off = 1;
    cls->size = (off + align - 1) / align * align;
    cls->align = align;
    cls->declAlign = floor;
    cls->layoutDone = true;
}

void Sema::completeClass(Symbol* cls)
{
    if (!cls->members) return;
    // implicit special members
    const Type* ct = cls->type;
    if (!cls->hasUserCtor) {
        Symbol* c = m_prog.newSymbol(Symbol::Function, cls->ctorName(), cls->members);
        c->isConstructor = true; c->isImplicit = true; c->ownerClass = cls; c->type = m_types.function(m_types.basic(Type::Void), std::vector<const Type*>(), false, false); c->loc = cls->loc;
        cls->hasDefaultCtor = true;
    }
    if (!cls->hasCopyCtor) {
        std::vector<const Type*> ps; ps.push_back(m_types.reference(m_types.qualified(ct, true, false)));
        Symbol* c = m_prog.newSymbol(Symbol::Function, cls->ctorName(), cls->members);
        c->isConstructor = true; c->isImplicit = true; c->ownerClass = cls; c->type = m_types.function(m_types.basic(Type::Void), ps, false, false); c->loc = cls->loc; c->requiredParams = 1;
        Symbol* p = m_prog.newSymbol(Symbol::Variable, "other", 0); p->type = ps[0]; p->isParam = true; c->params.push_back(p);
    }
    if (!cls->hasCopyAssign) {
        std::vector<const Type*> ps; ps.push_back(m_types.reference(m_types.qualified(ct, true, false)));
        Symbol* c = m_prog.newSymbol(Symbol::Function, "operator=", cls->members);
        c->isOperator = true; c->isImplicit = true; c->ownerClass = cls; c->type = m_types.function(m_types.reference(ct), ps, false, false); c->loc = cls->loc; c->requiredParams = 1;
        Symbol* p = m_prog.newSymbol(Symbol::Variable, "other", 0); p->type = ps[0]; p->isParam = true; c->params.push_back(p);
    }
    if (!cls->hasDtor) {
        Symbol* c = m_prog.newSymbol(Symbol::Function, "~" + cls->ctorName(), cls->members);
        c->isDestructor = true; c->isImplicit = true; c->ownerClass = cls; c->type = m_types.function(m_types.basic(Type::Void), std::vector<const Type*>(), false, false); c->loc = cls->loc;
        for (size_t b = 0; b < cls->bases.size(); ++b) { Symbol* bc = cls->bases[b].cls; if (!bc) continue; std::vector<Symbol*> out; lookupMember(bc, "~" + bc->ctorName(), out); for (size_t k = 0; k < out.size(); ++k) if (out[k]->isVirtual) c->isVirtual = true; }
    }
    if (cls->isDependentContext) return;
    layoutClass(cls);
    // vtable: the primary base's table first, then overrides and new virtuals
    if (!cls->bases.empty() && cls->bases[0].cls) cls->vtable = cls->bases[0].cls->vtable;
    for (size_t i = 0; i < cls->members->order.size(); ++i) {
        Symbol* m = cls->members->order[i];
        if (m->kind != Symbol::Function || m->isStatic) continue;
        // does it override a virtual function of a base?
        Symbol* over = 0;
        for (size_t b = 0; b < cls->bases.size() && !over; ++b) {
            std::vector<Symbol*> out;
            lookupMember(cls->bases[b].cls, m->isDestructor ? "~" + cls->bases[b].cls->ctorName() : m->name, out);
            for (size_t k = 0; k < out.size() && !over; ++k)
                if (out[k]->kind == Symbol::Function && out[k]->isVirtual && (m->isDestructor || sameSignature(out[k]->type, m->type))) over = out[k];
        }
        if (over) { m->isVirtual = true; m->overrides = over; }
        if (!m->isVirtual) continue;
        bool placed = false;
        for (size_t s = 0; s < cls->vtable.size() && !placed; ++s) {
            Symbol* v = cls->vtable[s];
            if (v == over || (over && v->overrides && v->name == m->name && sameSignature(v->type, m->type)) || (m->isDestructor && v->isDestructor)) { cls->vtable[s] = m; m->vtableSlot = static_cast<int>(s); placed = true; }
        }
        if (!placed) { m->vtableSlot = static_cast<int>(cls->vtable.size()); cls->vtable.push_back(m); }
    }
    cls->isAbstract = false;
    for (size_t s = 0; s < cls->vtable.size(); ++s) if (cls->vtable[s]->isPureVirtual) cls->isAbstract = true;
}


const Type* Sema::instantiateClassTemplate(Symbol* tmpl, const std::vector<cxx::TemplateArg>& args, Scope* from, const cxx::Loc& loc)
{
    std::vector<const Type*> types; std::vector<long> ints; std::string key;
    if (!templateArgText(args, from, types, ints, key)) { error(loc, "S111", "bad template arguments for '" + tmpl->name + "'"); return m_types.error(); }
    for (size_t i = 0; i < types.size(); ++i) if (types[i] && types[i]->isDependent()) return m_types.dependent(tmpl);
    if (key.find('?') != std::string::npos) return m_types.dependent(tmpl);
    // defaults
    size_t nparams = tmpl->templateParams.size();
    if (types.size() < nparams && tmpl->templateAst) {
        for (size_t i = types.size(); i < nparams; ++i) {
            const cxx::TemplateParam* p = tmpl->templateAst->templateParams[i];
            Scope* tmp = m_prog.newScope(Scope::Instantiation, tmpl->scope, tmpl);
            for (size_t k = 0; k < i; ++k) { Symbol* b = m_prog.newSymbol(types[k] ? Symbol::Typedef : Symbol::Enumerator, tmpl->templateParams[k]->name, tmp); if (types[k]) b->type = types[k]; else { b->type = m_types.basic(Type::Int); b->hasConstValue = true; b->constValue = ints[k]; } }
            if (p->kind == cxx::TemplateParam::Type && p->defaultType) { const Type* t = applyDeclarator(typeOf(p->defaultType->spec, tmp), *p->defaultType->decl, tmp, 0); types.push_back(t); ints.push_back(0); key = key.substr(0, key.size() - 1) + (types.size() > 1 ? ", " : "") + t->text() + ">"; }
            else if (p->kind == cxx::TemplateParam::NonType && p->nonType && p->nonType->defaultValue) { long v; expr(p->nonType->defaultValue, tmp); if (!constantValue(p->nonType->defaultValue, v)) v = 0; types.push_back(0); ints.push_back(v); std::ostringstream s; s << v; key = key.substr(0, key.size() - 1) + (types.size() > 1 ? ", " : "") + s.str() + ">"; }
            else { error(loc, "S111", "too few template arguments for '" + tmpl->name + "'"); return m_types.error(); }
        }
    }
    if (types.size() != nparams) { error(loc, "S111", "wrong number of template arguments for '" + tmpl->name + "'"); return m_types.error(); }
    std::map<std::string, Symbol*>::iterator it = tmpl->instantiations.find(key);
    if (it != tmpl->instantiations.end()) return it->second->type;
    if (!tmpl->classAst || !tmpl->classAst->isDefinition) {
        // only declared: an incomplete class type
        Symbol* cls = m_prog.newSymbol(Symbol::Class, tmpl->name + key, tmpl->scope);
        cls->type = m_types.classType(cls); cls->instantiatedFrom = tmpl; cls->templateArgs = types; cls->templateIntArgs = ints; cls->loc = loc;
        tmpl->instantiations[key] = cls;
        return cls->type;
    }
    // choose a partial specialisation whose pattern matches
    Symbol* chosen = tmpl;
    std::map<std::string, const Type*> bind; std::map<std::string, long> ibind;
    for (size_t p = 0; p < tmpl->partialSpecs.size(); ++p) {
        Symbol* ps = tmpl->partialSpecs[p];
        if (ps->kind != Symbol::ClassTemplate || ps->patternArgs.size() != types.size()) continue;
        std::map<std::string, const Type*> b; std::map<std::string, long> ib;
        bool ok = true;
        for (size_t i = 0; i < types.size() && ok; ++i) {
            const cxx::TemplateArg& pa = ps->patternArgs[i];
            if (pa.type) {
                if (!types[i]) { ok = false; break; }
                // pattern "T", "T*", "const T&": deduce against the argument type
                cxx::Param tmp; tmp.spec = pa.type->spec; tmp.decl = pa.type->decl;
                Scope* tscope = ps->templateParams.empty() ? 0 : ps->templateParams[0]->scope;
                if (!deduce(&tmp, types[i], tscope, b, ib)) ok = false;
            } else if (pa.expr) {
                if (types[i]) { ok = false; break; }
                if (pa.expr->kind == cxx::Expr::Name && pa.expr->name.parts.size() == 1) { ib[pa.expr->name.parts[0].ident] = ints[i]; }
                else { long v; if (!constantValue(pa.expr, v) || v != ints[i]) ok = false; }
            }
        }
        if (ok) { chosen = ps; bind = b; ibind = ib; break; }
    }
    // the instantiation scope binds the parameters
    Scope* iscope = m_prog.newScope(Scope::Instantiation, chosen->scope->kind == Scope::TemplateParams ? chosen->scope->parent : chosen->scope, chosen);
    if (chosen == tmpl) {
        for (size_t i = 0; i < nparams; ++i) {
            Symbol* b = m_prog.newSymbol(types[i] ? Symbol::Typedef : Symbol::Enumerator, tmpl->templateParams[i]->name, iscope);
            if (types[i]) b->type = types[i]; else { b->type = tmpl->templateParams[i]->type ? tmpl->templateParams[i]->type : m_types.basic(Type::Int); b->hasConstValue = true; b->constValue = ints[i]; }
        }
    } else {
        for (size_t i = 0; i < chosen->templateParams.size(); ++i) {
            const std::string& pn = chosen->templateParams[i]->name;
            Symbol* b = m_prog.newSymbol(bind.count(pn) ? Symbol::Typedef : Symbol::Enumerator, pn, iscope);
            if (bind.count(pn)) b->type = bind[pn]; else { b->type = m_types.basic(Type::Int); b->hasConstValue = true; b->constValue = ibind.count(pn) ? ibind[pn] : 0; }
        }
    }
    Symbol* cls = m_prog.newSymbol(Symbol::Class, tmpl->name + key, iscope);
    cls->type = m_types.classType(cls); cls->instantiatedFrom = tmpl; cls->templateArgs = types; cls->templateIntArgs = ints; cls->loc = loc;
    cls->isImplicitInstantiation = true;
    tmpl->instantiations[key] = cls;
    ++m_stats.classInstantiations;
    // the class template's name inside the instantiation means the instantiation (injected class name)
    Symbol* inj = m_prog.newSymbol(Symbol::Typedef, tmpl->name, iscope); inj->type = cls->type;
    cxx::ClassDecl* ast = cxx::cloneClassDecl(chosen->classAst, nodeArena());   // annotations belong to this instantiation
    Symbol* savedClass = m_currentClass;
    declareClass(ast, iscope, cxx::AccessPublic, cls);
    cls->name = tmpl->name + key;
    m_currentClass = savedClass;
    // members defined out of line ("template <class T> void Stack<T>::push(...)")
    for (size_t p = 0; p < tmpl->partialSpecs.size(); ++p) {
        Symbol* mt = tmpl->partialSpecs[p];
        if (mt->kind != Symbol::FunctionTemplate || !mt->declAst) continue;
        Scope* mscope = m_prog.newScope(Scope::Instantiation, iscope, cls);
        for (size_t i = 0; i < nparams && i < mt->templateParams.size(); ++i) {
            Symbol* b = m_prog.newSymbol(types[i] ? Symbol::Typedef : Symbol::Enumerator, mt->templateParams[i]->name, mscope);
            if (types[i]) b->type = types[i]; else { b->type = m_types.basic(Type::Int); b->hasConstValue = true; b->constValue = ints[i]; }
        }
        Symbol* inj2 = m_prog.newSymbol(Symbol::Typedef, tmpl->name, mscope); inj2->type = cls->type;
        cxx::Decl* def = mt->declAst;
        if (def->kind == cxx::Decl::Function && def->declarator) {
            // a copy of the definition for this instantiation, with "Stack<T>::push" rewritten to the
            // instantiation's name so that the member is found
            cxx::Decl* copy = cxx::cloneDecl(def, nodeArena());
            cxx::Declarator* dcopy = copy->declarator;
            if (dcopy->name.parts.size() >= 2) { dcopy->name.parts[dcopy->name.parts.size() - 2].ident = cls->name; dcopy->name.parts[dcopy->name.parts.size() - 2].hasTemplateArgs = false; }
            declare(copy, mscope, cxx::AccessPublic);
        }
    }
    return cls->type;
}


namespace {

void dumpScope(const Scope* s, int depth, std::ostringstream& out)
{
    std::string pad(static_cast<size_t>(depth * 2), ' ');
    if (depth > 40) { out << pad << "...\n"; return; }
    for (size_t i = 0; i < s->order.size(); ++i) {
        const Symbol* y = s->order[i];
        switch (y->kind) {
        case Symbol::Namespace: out << pad << "namespace " << (y->name.empty() ? "(anonymous)" : y->name) << "\n"; if (y->members && y->members->owner == y) dumpScope(y->members, depth + 1, out); break;
        case Symbol::Class:
            out << pad << (y->isStruct ? "struct " : "class ") << y->name;
            for (size_t b = 0; b < y->bases.size(); ++b) out << (b ? ", " : " : ") << (y->bases[b].cls ? y->bases[b].cls->name : std::string("?")) << "@" << y->bases[b].offset;
            if (y->layoutDone) out << "  size " << y->size << " align " << y->align << (y->isPolymorphic ? " vptr" : "") << (y->isAbstract ? " abstract" : "");
            out << "\n";
            if (y->members) dumpScope(y->members, depth + 1, out);
            break;
        case Symbol::Enum: out << pad << "enum " << y->name << "\n"; break;
        case Symbol::Typedef: out << pad << "typedef " << (y->type ? y->type->text() : std::string("?")) << " " << y->name << "\n"; break;
        case Symbol::Variable:
            out << pad << (y->isStatic ? "static " : "") << (y->type ? y->type->text() : std::string("?")) << " " << y->name;
            if (y->isMember && !y->isStatic) out << "  @" << y->offset;
            if (y->hasConstValue) out << " = " << y->constValue;
            out << "\n";
            break;
        case Symbol::Function:
            if (y->isImplicit) break;
            out << pad << (y->isVirtual ? "virtual " : "") << (y->isStatic ? "static " : "") << (y->type ? y->type->text() : std::string("?")) << " " << y->name;
            if (y->vtableSlot >= 0) out << "  slot " << y->vtableSlot;
            out << (y->hasBody ? "  {body}" : "") << "\n";
            break;
        case Symbol::Enumerator: out << pad << "enumerator " << y->name << " = " << y->constValue << "\n"; break;
        case Symbol::ClassTemplate: {
            out << pad << "template class " << y->name << "  (" << y->instantiations.size() << " instantiation(s))\n";
            for (std::map<std::string, Symbol*>::const_iterator it = y->instantiations.begin(); it != y->instantiations.end(); ++it) {
                const Symbol* c = it->second;
                out << pad << "  instantiation " << c->name << (c->layoutDone ? "  size " : "") ; if (c->layoutDone) out << c->size; out << "\n";
                if (c->members) dumpScope(c->members, depth + 2, out);
            }
            break;
        }
        case Symbol::FunctionTemplate: {
            out << pad << "template function " << y->name << "  (" << y->instantiations.size() << " instantiation(s))\n";
            for (std::map<std::string, Symbol*>::const_iterator it = y->instantiations.begin(); it != y->instantiations.end(); ++it)
                out << pad << "  instantiation " << it->second->name << " " << (it->second->type ? it->second->type->text() : std::string("")) << "\n";
            break;
        }
        default: break;
        }
    }
}

} // namespace

std::string Sema::dumpSymbols() const
{
    std::ostringstream out;
    dumpScope(m_prog.global, 0, out);
    return out.str();
}

} // namespace sema
} // namespace cppparse
