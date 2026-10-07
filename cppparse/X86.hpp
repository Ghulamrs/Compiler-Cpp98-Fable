// X86.hpp - version 4: x86-64 code generation in MASM syntax (Windows x64 ABI).
//
// Register allocation: linear scan over the callee-saved registers RBX, RSI,
// RDI, R12-R15 (integer/pointer temporaries) and XMM6-XMM13 (floating
// point); RAX, RCX, RDX, R8-R11 and XMM0-XMM5 are scratch and argument
// registers. Locals, spilled temporaries and saved registers live in the
// frame below RBP; a 32-byte shadow space plus outgoing stack arguments sit
// at [RSP]. Arguments: RCX, RDX, R8, R9 / XMM0-XMM3 by position, the rest
// on the stack; results in RAX / XMM0.
//
// Emitted file: EXTERN declarations, .data (globals, vtables), .const
// (strings, float constants), .code (one PROC per function). The runtime
// helpers map to the C library: __new -> malloc, __delete -> free,
// __copy -> memcpy, __throw -> cppparse_throw (abort with a message).
//
// Strict C++98.
#ifndef CPPPARSE_X86_HPP
#define CPPPARSE_X86_HPP

#include "Ir.hpp"
#include "Lower.hpp"
#include "Parallel.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace cppparse {

class X86Module {
public:
    X86Module(const CodeBase& cb, TypeTable& types, Layouts& layouts, bool msvcNames = false);
    void        registerFunction(const FunctionInfo* fn);   // version 5: give a compiled function its MSVC name (before codeFor)
    std::string symbol(const std::string& irName);          // symbol of an IR function or callee name
    std::string globalSymbol(const std::string& irName);    // symbol of a variable, vtable or function address
    void        addFunction(const IrFunction& f, LowerStats& stats);     // codeFor + appendCode
    std::string codeFor(const IrFunction& f, LowerStats& stats);         // safe to call from several threads
    void        appendCode(const std::string& code);
    std::string finish();                       // the complete .asm text

private:
    void emitGlobals();
    void emitVtables();

    const CodeBase&          m_cb;
    TypeTable&               m_types;
    Layouts&                 m_layouts;
    std::string              m_code;
    std::string              m_data;
    std::map<std::string, std::string> m_strings;   // label -> literal; labels are derived from the contents so
    std::map<std::string, double>      m_floats;    // that the output is the same whatever the thread timing
    std::set<std::string>    m_externs;
    std::set<std::string>    m_defined;         // PUBLIC symbols defined in this module
    std::set<std::string>    m_vtablesNeeded;   // qualified class names
    std::set<std::string>    m_globalsUsed;     // mangled global names referenced
    Mutex                    m_mutex;           // guards the pools above while functions are generated in parallel
    bool                               m_msvc;            // version 5: MSVC-compatible names
    std::map<std::string, std::string> m_functionNames;   // IR function name -> decorated name (read-only while generating)
    std::map<std::string, std::string> m_globalNames;     // qualified variable name -> decorated name
    std::set<std::string>              m_dataSymbols;     // symbols already made PUBLIC in .data
    std::string floatLabel(double v);
    std::string stringLabel(const std::string& text);
    void        useGlobal(const std::string& name) { Lock l(m_mutex); m_globalsUsed.insert(name); }
    void        useExtern(const std::string& name) { Lock l(m_mutex); if (!m_defined.count(name)) m_externs.insert(name); }
    friend class X86Function;
};

} // namespace cppparse

#endif
