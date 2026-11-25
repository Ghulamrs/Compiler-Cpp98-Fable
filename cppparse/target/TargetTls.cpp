// TargetTls.cpp - thread-local storage of the Windows x64 target: the refusals' messages and the listing's sequence
// (see TargetTls.hpp).
//
// Strict C++98.
#include "TargetTls.hpp"

namespace cppparse {

std::string TargetTls::message(Refusal r, const std::string& name, const std::string& detail)
{
    const std::string what = "'" + name + "': ";
    switch (r) {
    case BlockScope:      return what + "'__declspec(thread)' on a variable of block scope: a thread-local is a variable of namespace scope (cl C2928 for an automatic one)";
    case Member:          return what + "'__declspec(thread)' on a data member of a class: a thread-local is a variable of namespace scope";
    case Function:        return what + "'__declspec(thread)' applies to data only (cl C2483)";
    case Type:            return what + "'__declspec(thread)' on a type: it applies to a variable of namespace scope";
    case Typedef:         return what + "'__declspec(thread)' on a typedef: it applies to a variable of namespace scope";
    case Parameter:       return what + "'__declspec(thread)' on a parameter: a thread-local is a variable of namespace scope";
    case Added:           return what + "redeclared with '__declspec(thread)', first declared without it (cl C2370)";
    case Dropped:         return what + "redeclared without '__declspec(thread)', first declared with it (cl C2370)";
    case Dynamic:         return what + "a thread-local needs a constant initialiser and no constructor or destructor to run (cl C2482 before C++11)";
    case Alignment:       return what + "a thread-local may be aligned to 16 bytes at most, its type or declaration asks for " + detail;
    case AddressConstant: return what + "the address of the thread-local '" + detail + "' is not a constant: it cannot initialise a variable with static storage";
    }
    return what + "'__declspec(thread)' is refused here";
}

void TargetTls::addressSequence(const std::string& rd, const std::string& symbol, std::vector<std::string>& lines)
{
    lines.push_back(std::string("mov ecx, dword ptr [") + indexSymbol() + "]");
    lines.push_back("mov " + rd + ", qword ptr gs:[58h]");
    lines.push_back("mov " + rd + ", qword ptr [" + rd + "+rcx*8]");
    lines.push_back("mov edx, SECTIONREL " + symbol);
    lines.push_back("add " + rd + ", rdx");
}

} // namespace cppparse
