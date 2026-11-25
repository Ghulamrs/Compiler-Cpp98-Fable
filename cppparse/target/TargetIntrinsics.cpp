// TargetIntrinsics.cpp - the compiler intrinsics of the Windows x64 target: recognition and code (see
// TargetIntrinsics.hpp).
//
// Strict C++98.
#include "TargetIntrinsics.hpp"
#include "Sema.hpp"

namespace cppparse {

namespace {

// "long volatile*": a pointer to a volatile long
bool volatileLongPointer(const sema::Type* t)
{
    return t && t->kind == sema::Type::Pointer && t->inner && t->inner->kind == sema::Type::Long && t->inner->isVolatile && !t->inner->isConst;
}

bool plainLong(const sema::Type* t) { return t && t->kind == sema::Type::Long && !t->isVolatile; }

} // namespace

TargetIntrinsics::Kind TargetIntrinsics::byName(const std::string& n)
{
    if (n == "_InterlockedIncrement") return Increment;
    if (n == "_InterlockedDecrement") return Decrement;
    if (n == "_InterlockedExchange") return Exchange;
    if (n == "_InterlockedCompareExchange") return CompareExchange;
    if (n == "_AddressOfReturnAddress") return AddressOfReturnAddress;
    return None;
}

const char* TargetIntrinsics::name(Kind k)
{
    switch (k) {
    case Increment: return "_InterlockedIncrement";
    case Decrement: return "_InterlockedDecrement";
    case Exchange: return "_InterlockedExchange";
    case CompareExchange: return "_InterlockedCompareExchange";
    case AddressOfReturnAddress: return "_AddressOfReturnAddress";
    default: return "";
    }
}

TargetIntrinsics::Kind TargetIntrinsics::of(const sema::Symbol* fn)
{
    if (!fn || fn->kind != sema::Symbol::Function || !fn->isExternC || fn->hasBody || fn->ownerClass) return None;
    const Kind k = byName(fn->name);
    if (k == None) return None;
    const sema::Type* ft = fn->type;
    if (!ft || ft->kind != sema::Type::Function || ft->variadic || !ft->inner) return None;
    const std::vector<const sema::Type*>& p = ft->params;
    const sema::Type* r = ft->inner;
    switch (k) {
    case Increment: case Decrement:
        return plainLong(r) && p.size() == 1 && volatileLongPointer(p[0]) ? k : None;
    case Exchange:
        return plainLong(r) && p.size() == 2 && volatileLongPointer(p[0]) && plainLong(p[1]) ? k : None;
    case CompareExchange:
        return plainLong(r) && p.size() == 3 && volatileLongPointer(p[0]) && plainLong(p[1]) && plainLong(p[2]) ? k : None;
    case AddressOfReturnAddress:
        return r->kind == sema::Type::Pointer && r->inner && r->inner->kind == sema::Type::Void && p.empty() ? k : None;
    default:
        return None;
    }
}

std::string TargetIntrinsics::addressMessage(const std::string& n)
{
    return "'" + n + "': the address of an intrinsic cannot be taken: it has no code of its own, a call of it is emitted inline (docs/design-selfhost.md 4.2)";
}

void TargetIntrinsics::sequence(Kind k, const std::string& p, const std::string& v, const std::string& c, std::vector<std::string>& lines)
{
    switch (k) {
    case Increment:
        lines.push_back("mov eax, 1");
        lines.push_back("lock xadd dword ptr [" + p + "], eax");
        lines.push_back("inc eax");
        break;
    case Decrement:
        lines.push_back("mov eax, -1");
        lines.push_back("lock xadd dword ptr [" + p + "], eax");
        lines.push_back("dec eax");
        break;
    case Exchange:
        if (v != "eax") lines.push_back("mov eax, " + v);
        lines.push_back("xchg dword ptr [" + p + "], eax");
        break;
    case CompareExchange:
        if (c != "eax") lines.push_back("mov eax, " + c);
        lines.push_back("lock cmpxchg dword ptr [" + p + "], " + v);
        break;
    case AddressOfReturnAddress:
        lines.push_back("lea rax, [rbp+8]");
        break;
    default:
        break;
    }
}

} // namespace cppparse
