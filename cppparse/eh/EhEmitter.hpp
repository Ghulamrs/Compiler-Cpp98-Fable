// EhEmitter.hpp - the interface the exception models write assembly through.
//
// The classes of this folder know which runtime entry a model calls and in what order; where a value is (a register, a
// frame slot) is the code generator's knowledge. X86.cpp's function emitter implements this interface and hands the
// operands over as text.
//
// Strict C++98.
#ifndef CPPPARSE_EHEMITTER_HPP
#define CPPPARSE_EHEMITTER_HPP

#include <string>

namespace cppparse {

class EhEmitter {
public:
    virtual ~EhEmitter() {}
    virtual void        ehLine(const std::string& instruction) = 0;    // one instruction of the function being emitted
    virtual void        ehText(const std::string& text) = 0;           // text as it is: a label's line, a ";@" line for the object writer
    virtual void        ehExtern(const std::string& symbol) = 0;       // the module refers to this external symbol
    virtual std::string ehString(const std::string& quoted) = 0;       // the label of a string literal (given with its quotes)
};

} // namespace cppparse

#endif
