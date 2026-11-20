// EhThrow.cpp - see EhThrow.hpp.
#include "EhThrow.hpp"
#include "PathUtil.hpp"

namespace cppparse {

void EhThrow::rethrow(EhEmitter& out)
{
    out.ehExtern("cppparse_rethrow");
    out.ehLine("call cppparse_rethrow            ; throw;");
}

void EhThrow::objectIn(EhEmitter& out, const std::string& reg)
{
    if (reg != "rcx") out.ehLine("mov rcx, " + reg);
}

void EhThrow::scalarIn(EhEmitter& out, const std::string& reg, bool isFloat, const std::string& slot)
{
    if (isFloat) out.ehLine("movsd real8 ptr " + slot + ", " + reg);
    else out.ehLine("mov qword ptr " + slot + ", " + reg);
    out.ehLine("lea rcx, " + slot);
}

void EhThrow::descriptor(EhEmitter& out, const std::string& typeDescriptor, int size)
{
    out.ehLine("lea rdx, " + out.ehString("\"" + typeDescriptor + "\""));
    out.ehLine("mov r8, " + intToString(size));
}

void EhThrow::callOwned(EhEmitter& out, const std::string& destructorReg)
{
    if (destructorReg != "r9") out.ehLine("mov r9, " + destructorReg);
    out.ehExtern("cppparse_throw_owned");
    out.ehLine("call cppparse_throw_owned");
}

void EhThrow::call(EhEmitter& out)
{
    out.ehExtern("cppparse_throw");
    out.ehLine("call cppparse_throw");
}

void EhThrow::clThrow(EhEmitter& out, const std::string& reg, const std::string& throwInfoSymbol)
{
    out.ehExtern("_CxxThrowException");
    if (reg != "rcx") out.ehLine("mov rcx, " + reg);
    out.ehLine("lea rdx, " + throwInfoSymbol);
    out.ehLine("call _CxxThrowException");
}

void EhThrow::clRethrow(EhEmitter& out)
{
    out.ehExtern("_CxxThrowException");
    out.ehLine("xor ecx, ecx");
    out.ehLine("xor edx, edx");
    out.ehLine("call _CxxThrowException            ; throw;");
}

} // namespace cppparse
