// Compiler.cpp - version 3 driver.
#include "Compiler.hpp"
#include "BodyParser.hpp"
#include "Ir.hpp"
#include "Lower.hpp"
#include "Optimizer.hpp"
#include "PathUtil.hpp"
#include "TypeCheck.hpp"
#include "Types.hpp"

#include <cstdlib>
#include <map>
#include <sstream>

namespace cppparse {

Compiler::Compiler(CodeBase& cb, const CompileOptions& opt) : m_cb(cb), m_opt(opt) {}

Compiler::~Compiler() {}

namespace {

struct Unit {
    FunctionInfo* fn;
    const SourceFile* file;
};

void collectUnits(CodeBase& cb, std::vector<Unit>& out)
{
    const std::vector<SourceFile*>& files = cb.files();
    for (size_t f = 0; f < files.size(); ++f) {
        SourceFile* sf = files[f];
        for (size_t k = 0; k < sf->functions.size(); ++k)
            if (sf->functions[k].hasBody) { Unit u; u.fn = &sf->functions[k]; u.file = sf; out.push_back(u); }
        for (size_t c = 0; c < sf->allClasses.size(); ++c)
            for (size_t m = 0; m < sf->allClasses[c]->methods.size(); ++m)
                if (sf->allClasses[c]->methods[m].hasBody) { Unit u; u.fn = &sf->allClasses[c]->methods[m]; u.file = sf; out.push_back(u); }
    }
}

} // namespace

void Compiler::run()
{
    TypeTable types(m_cb);
    Layouts layouts(types, m_cb);
    TypeChecker checker(m_cb, types, m_findings);
    IrBuilder builder(m_cb, types, layouts);

    std::vector<Unit> units;
    collectUnits(m_cb, units);

    std::ostringstream astOut, irOut, logOut, asmOut, layoutOut;
    std::vector<IrFunction*> irFunctions;
    std::map<std::string, IrFunction*> byName;

    logOut << "cppparse version 3 optimisation log  (-O" << m_opt.optLevel << ")\n\n";

    for (size_t u = 0; u < units.size(); ++u) {
        FunctionInfo& fn = *units[u].fn;
        ++m_summary.functionsSeen;
        std::string title = fn.displayName() + "(" + fn.paramText + ")" + (fn.isConst ? " const" : "") + "   [" + m_cb.displayPath(fn.file) + ":" + intToString(fn.line) + "]";
        std::string skip;
        if (fn.tmpl.isTemplate || (fn.ownerClass && fn.ownerClass->tmpl.isTemplate)) skip = "template (bodies are compiled per instantiation; not in the version 3 subset)";
        else if (fn.exceptions.functionTryBlock) skip = "function-try-block";
        else if (fn.isFriend) skip = "friend function defined inside a class";
        else if (fn.bodyEnd <= fn.bodyBegin || fn.bodyEnd > units[u].file->tokens.size()) skip = "body tokens not available";
        if (!skip.empty()) {
            ++m_summary.functionsSkipped;
            astOut << "function " << title << "\n  skipped: " << skip << "\n\n";
            continue;
        }

        AstArena arena;
        FunctionAst ast;
        BodyParser parser(m_cb, types, arena, fn, units[u].file->tokens);
        if (fn.isConstructor && fn.hasInitializerList && fn.bodyBegin > 0) {
            // the initializer list sits between the ':' after the parameter list and the body's '{'
            const std::vector<Token>& toks = units[u].file->tokens;
            size_t k = fn.bodyBegin;
            int depth = 0;
            size_t colon = 0;
            while (k > 0) {
                --k;
                const Token& t = toks[k];
                if (t.type != TokPunct) continue;
                if (t.text == ")") ++depth;
                else if (t.text == "(") { if (depth == 0) break; --depth; }
                else if (t.text == ":" && depth == 0) { colon = k; break; }
            }
            if (colon > 0) parser.parseInitializers(ast, colon + 1, fn.bodyBegin);
        }
        parser.parse(ast);
        if (!ast.parsed) {
            ++m_summary.parseFailures;
            for (size_t e = 0; e < ast.errors.size() && e < 3; ++e) {
                Finding f;
                f.severity = Finding::Warning;
                f.code = "T191";
                f.file = fn.file;
                f.line = ast.errors[e].size() > 1 ? atoi(ast.errors[e].c_str() + 1) : fn.line;
                f.message = "body of " + fn.fullName() + " could not be parsed: " + ast.errors[e].substr(ast.errors[e].find(':') + 2);
                f.detail = "the function is not type checked or compiled";
                m_findings.push_back(f);
            }
        }
        bool checked = ast.parsed && checker.check(ast);
        if (ast.parsed && !checked) ++m_summary.typeFailures;
        fn.typeChecked = ast.parsed;   // typed checks replace the count-based ones even when they found errors

        if (m_opt.emitAst) {
            std::ostringstream s;
            dumpFunctionAst(ast, s);
            astOut << s.str() << "\n";
        }
        if (!ast.parsed) continue;

        IrFunction* irf = new IrFunction;
        builder.build(ast, *irf);
        irFunctions.push_back(irf);
        byName[irf->name] = irf;
        ++m_summary.functionsCompiled;
    }

    // class layouts
    const std::vector<ClassInfo*>& classes = m_cb.classes();
    for (size_t c = 0; c < classes.size(); ++c)
        if (classes[c]->isDefinition && !classes[c]->tmpl.isTemplate) {
            std::string d = layouts.describe(classes[c]);
            if (!d.empty()) layoutOut << d << "\n";
        }

    // optimise and lower
    Optimizer optimizer(m_opt.optLevel, byName);
    for (size_t i = 0; i < irFunctions.size(); ++i) {
        IrFunction& f = *irFunctions[i];
        int before = f.instCount();
        m_summary.irBefore += before;
        std::string beforeText = printIr(f);
        std::vector<std::string> log;
        optimizer.run(f, log);
        for (size_t l = 0; l < log.size(); ++l) logOut << log[l] << "\n";
        m_summary.irAfter += f.instCount();
        irOut << "================================================================================\n";
        if (m_opt.optLevel > 0) {
            irOut << "--- before optimisation (" << before << " instructions) ---\n" << beforeText;
            irOut << "--- after -O" << m_opt.optLevel << " (" << f.instCount() << " instructions) ---\n" << printIr(f) << "\n";
        } else {
            irOut << beforeText << "\n";
        }
        LowerStats st;
        asmOut << lowerFunction(f, layouts, st);
        m_summary.asmInstructions += st.instructions;
        m_summary.spills += st.spills;
    }
    if (m_opt.optLevel == 0) logOut << "(no optimisation requested: use -O1, -O2 or -O3)\n";
    logOut << "\ntotal IR instructions: " << m_summary.irBefore << " -> " << m_summary.irAfter << "\n";

    for (size_t i = 0; i < irFunctions.size(); ++i) delete irFunctions[i];

    m_ast = astOut.str();
    m_ir = irOut.str();
    m_log = logOut.str();
    m_asm = "; cppparse version 3 lowering - target VM64 (R0-R7 integer, F0-F7 float, FP, SP)\n"
            "; calling convention: R0-R3 / F0-F3 then stack, result R0 / F0, all registers caller-saved\n\n" + asmOut.str();
    m_layouts = layoutOut.str();
}

void Compiler::summaryLines(std::vector<std::string>& out) const
{
    out.push_back("Version 3 compile (-O" + intToString(m_opt.optLevel) + "): functions seen " + intToString(m_summary.functionsSeen) +
                  ", compiled " + intToString(m_summary.functionsCompiled) + ", skipped " + intToString(m_summary.functionsSkipped) +
                  " (templates, function-try-blocks), parse failures " + intToString(m_summary.parseFailures) +
                  ", functions with type errors " + intToString(m_summary.typeFailures));
    out.push_back("IR instructions: " + intToString(m_summary.irBefore) + " -> " + intToString(m_summary.irAfter) + " after optimisation;  assembly instructions " +
                  intToString(m_summary.asmInstructions) + ", spilled temporaries " + intToString(m_summary.spills));
}

} // namespace cppparse
