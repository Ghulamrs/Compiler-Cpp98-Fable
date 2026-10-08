// X86.hpp - version 4: x86-64 code generation (MASM syntax, Windows x64 ABI).
//
// Every IR function becomes one PROC: a linear-scan allocator over the
// callee-saved registers (rbx, rsi, rdi, r12-r15; xmm6-xmm13 for doubles),
// a fixed frame below rbp with the locals, spill slots and saved registers,
// shadow space and stack arguments for calls, vtables in .data, string and
// float constants in .const. The module knows nothing about the front end:
// vtables, globals and decorated names are given to it explicitly, so both
// the version 3 and the version 6 front ends drive it.
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

struct IrVtable {
    std::string              className;    // qualified class name
    std::vector<std::string> entries;      // IR function names in slot order; "" for a pure virtual slot
};

struct IrGlobal {
    std::string name;        // qualified variable name as the IR refers to it
    int         size;
    bool        isFloat;
    std::string init;        // numeric initialiser text, or empty for zero
    bool        linkOnce;    // a static local of an inline function: every unit emits it, one copy survives (COMDAT)
    IrGlobal() : size(8), isFloat(false), linkOnce(false) {}
};

class X86Module {
public:
    explicit X86Module(bool msvcNames = false);
    void        addVtable(const IrVtable& v) { m_vtables.push_back(v); }
    void        addGlobal(const IrGlobal& g) { m_globals.push_back(g); }
    void        addInitializer(const std::string& irName) { m_initializers.push_back(irName); }   // run by the CRT before main
    void        registerSymbolName(const std::string& irName, const std::string& decorated);   // MSVC name of a function
    void        registerGlobalName(const std::string& irGlobalName, const std::string& decorated);
    std::string symbol(const std::string& irName);          // symbol of an IR function or callee name
    std::string globalSymbol(const std::string& irName);    // symbol of a variable, vtable or function address
    void        addFunction(const IrFunction& f, LowerStats& stats);     // codeFor + appendCode
    std::string codeFor(const IrFunction& f, LowerStats& stats);         // safe to call from several threads
    void        appendCode(const std::string& code);
    std::string finish();                       // the complete .asm text

private:
    void emitGlobals();
    void emitVtables();

    std::string              m_code;
    std::string              m_data;
    std::map<std::string, std::string> m_strings;   // label -> literal; labels are derived from the contents so
    std::map<std::string, double>      m_floats;    // that the output is the same whatever the thread timing
    std::set<std::string>    m_externs;
    std::set<std::string>    m_defined;         // PUBLIC symbols defined in this module
    std::set<std::string>    m_globalsUsed;     // mangled global names referenced
    Mutex                    m_mutex;           // guards the pools above while functions are generated in parallel
    bool                               m_msvc;
    std::map<std::string, std::string> m_functionNames;   // IR function name -> decorated name (read-only while generating)
    std::map<std::string, std::string> m_globalNames;     // qualified variable name -> decorated name
    std::set<std::string>              m_dataSymbols;     // symbols already made PUBLIC in .data
    std::vector<IrVtable>              m_vtables;
    std::vector<IrGlobal>              m_globals;
    std::vector<std::string>           m_initializers;
    std::string floatLabel(double v);
    std::string stringLabel(const std::string& text);
    void        useGlobal(const std::string& name) { Lock l(m_mutex); m_globalsUsed.insert(name); }
    void        useExtern(const std::string& name) { Lock l(m_mutex); if (!m_defined.count(name)) m_externs.insert(name); }
    friend class X86Function;
};

} // namespace cppparse

#endif
