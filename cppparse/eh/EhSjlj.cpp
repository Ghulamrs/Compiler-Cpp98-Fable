// EhSjlj.cpp - see EhSjlj.hpp.
#include "EhSjlj.hpp"

namespace cppparse {

void EhSjlj::tryBegin(EhEmitter& out, const std::string& frame, const std::string& dispatchLabel)
{
    out.ehExtern("cppparse_push_frame");
    out.ehExtern("_setjmp");
    out.ehLine("lea rcx, " + frame);
    out.ehLine("call cppparse_push_frame");
    out.ehLine("lea rcx, " + frame);
    out.ehLine("xor edx, edx");
    out.ehLine("call _setjmp");
    out.ehLine("test eax, eax");
    out.ehLine("jne " + dispatchLabel + "        ; exception: dispatch to the handlers");
}

void EhSjlj::tryEnd(EhEmitter& out, const std::string& frame)
{
    out.ehExtern("cppparse_pop_frame");
    out.ehLine("lea rcx, " + frame);
    out.ehLine("call cppparse_pop_frame");
}

void EhSjlj::dispatchPop(EhEmitter& out, const std::string& frame)
{
    out.ehExtern("cppparse_pop_frame");
    out.ehLine("lea rcx, " + frame);
    out.ehLine("call cppparse_pop_frame");
}

} // namespace cppparse
