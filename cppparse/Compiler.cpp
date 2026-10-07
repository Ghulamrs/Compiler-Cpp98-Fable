// Compiler.cpp - version 3 driver.
#include "Compiler.hpp"
#include "BodyParser.hpp"
#include "Instantiate.hpp"
#include "Ir.hpp"
#include "Lower.hpp"
#include "Optimizer.hpp"
#include "PathUtil.hpp"
#include "TypeCheck.hpp"
#include "Types.hpp"
#include "X86.hpp"

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

namespace {

struct CompileContext {
    CodeBase&       cb;
    TypeTable&      types;
    TypeChecker&    checker;
    IrBuilder&      builder;
    CompileSummary& summary;
    std::vector<Finding>& findings;
    bool            emitAst;
    std::ostringstream& astOut;
    std::vector<IrFunction*>& irFunctions;
    std::map<std::string, IrFunction*>& byName;
    CompileContext(CodeBase& c, TypeTable& t, TypeChecker& ch, IrBuilder& b, CompileSummary& s, std::vector<Finding>& f, bool ea,
                   std::ostringstream& ao, std::vector<IrFunction*>& irf, std::map<std::string, IrFunction*>& bn)
        : cb(c), types(t), checker(ch), builder(b), summary(s), findings(f), emitAst(ea), astOut(ao), irFunctions(irf), byName(bn) {}
};

// parses, type checks and lowers one function body to IR
void compileUnit(CompileContext& ctx, FunctionInfo& fn, const std::vector<Token>& tokens)
{
    AstArena arena;
    FunctionAst ast;
    BodyParser parser(ctx.cb, ctx.types, arena, fn, tokens);
    if (fn.isConstructor && fn.hasInitializerList && fn.bodyBegin > 0) {
        // the initializer list sits between the ':' after the parameter list and the body's '{'
        size_t k = fn.bodyBegin;
        int depth = 0;
        size_t colon = 0;
        bool found = false;
        while (k > 0) {
            --k;
            const Token& t = tokens[k];
            if (t.type != TokPunct) continue;
            if (t.text == ")") ++depth;
            else if (t.text == "(") { if (depth == 0) break; --depth; }
            else if (t.text == ":" && depth == 0) { colon = k; found = true; break; }
        }
        if (found) parser.parseInitializers(ast, colon + 1, fn.bodyBegin);
    }
    parser.parse(ast);
    if (!ast.parsed) {
        ++ctx.summary.parseFailures;
        for (size_t e = 0; e < ast.errors.size() && e < 3; ++e) {
            Finding f;
            f.severity = Finding::Warning;
            f.code = "T191";
            f.file = fn.file;
            f.line = ast.errors[e].size() > 1 ? atoi(ast.errors[e].c_str() + 1) : fn.line;
            f.message = "body of " + fn.displayName() + " could not be parsed: " + ast.errors[e].substr(ast.errors[e].find(':') + 2);
            f.detail = "the function is not type checked or compiled";
            ctx.findings.push_back(f);
        }
    }
    bool checked = ast.parsed && ctx.checker.check(ast);
    if (ast.parsed && !checked) ++ctx.summary.typeFailures;
    fn.typeChecked = ast.parsed;   // typed checks replace the count-based ones even when they found errors

    if (ctx.emitAst) {
        std::ostringstream s;
        dumpFunctionAst(ast, s);
        ctx.astOut << s.str() << "\n";
    }
    if (!ast.parsed) return;

    IrFunction* irf = new IrFunction;
    ctx.builder.build(ast, *irf);
    ctx.irFunctions.push_back(irf);
    ctx.byName[irf->name] = irf;
    ++ctx.summary.functionsCompiled;
}

} // namespace

void Compiler::run()
{
    TypeTable types(m_cb);
    Instantiator instantiator(m_cb);
    types.setInstantiator(&instantiator);
    Layouts layouts(types, m_cb);
    TypeChecker checker(m_cb, types, m_findings);
    IrBuilder builder(m_cb, types, layouts);

    std::vector<Unit> units;
    collectUnits(m_cb, units);

    std::ostringstream astOut, irOut, logOut, asmOut, layoutOut;
    std::vector<IrFunction*> irFunctions;
    std::map<std::string, IrFunction*> byName;
    CompileContext ctx(m_cb, types, checker, builder, m_summary, m_findings, m_opt.emitAst, astOut, irFunctions, byName);

    logOut << "cppparse version 3 optimisation log  (-O" << m_opt.optLevel << ")\n\n";

    for (size_t u = 0; u < units.size(); ++u) {
        FunctionInfo& fn = *units[u].fn;
        ++m_summary.functionsSeen;
        std::string title = fn.displayName() + "(" + fn.paramText + ")" + (fn.isConst ? " const" : "") + "   [" + m_cb.displayPath(fn.file) + ":" + intToString(fn.line) + "]";
        std::string skip;
        if (fn.tmpl.isTemplate || (fn.ownerClass && fn.ownerClass->tmpl.isTemplate)) skip = "template (compiled per instantiation, see the instantiated functions below)";
        else if (fn.exceptions.functionTryBlock) skip = "function-try-block";
        else if (fn.isFriend) skip = "friend function defined inside a class";
        else if (fn.bodyEnd <= fn.bodyBegin || fn.bodyEnd > units[u].file->tokens.size()) skip = "body tokens not available";
        if (!skip.empty()) {
            ++m_summary.functionsSkipped;
            astOut << "function " << title << "\n  skipped: " << skip << "\n\n";
            continue;
        }
        compileUnit(ctx, fn, units[u].file->tokens);
    }

    // template instantiations requested while type checking: compile them (they may request more)
    for (int round = 0; round < 8; ++round) {
        std::vector<FunctionInfo*> pending;
        instantiator.takePending(pending);
        if (pending.empty()) break;
        for (size_t i = 0; i < pending.size(); ++i) {
            const std::vector<Token>* toks = instantiator.tokensFor(pending[i]);
            if (!toks) continue;
            ++m_summary.functionsSeen;
            astOut << "; instantiated\n";
            compileUnit(ctx, *pending[i], *toks);
        }
    }
    m_summary.classInstantiations = instantiator.classCount();
    m_summary.functionInstantiations = instantiator.functionCount();

    // class layouts
    const std::vector<ClassInfo*>& classes = m_cb.classes();
    for (size_t c = 0; c < classes.size(); ++c)
        if (classes[c]->isDefinition && !classes[c]->tmpl.isTemplate) {
            std::string d = layouts.describe(classes[c]);
            if (!d.empty()) layoutOut << d << "\n";
        }
    for (size_t c = 0; c < instantiator.classes().size(); ++c) {
        std::string d = layouts.describe(instantiator.classes()[c]);
        if (!d.empty()) layoutOut << d << "   (instantiated)\n";
    }

    // optimise and lower
    Optimizer optimizer(m_opt.optLevel, byName);
    X86Module x86(m_cb, types, layouts);
    for (size_t i = 0; i < irFunctions.size(); ++i) {
        IrFunction& f = *irFunctions[i];
        int before = f.instCount();
        m_summary.irBefore += before;
        std::string beforeText = printIr(f);
        std::vector<std::string> log;
        std::string ssaText;
        optimizer.run(f, log, &ssaText);
        for (size_t l = 0; l < log.size(); ++l) logOut << log[l] << "\n";
        m_summary.irAfter += f.instCount();
        irOut << "================================================================================\n";
        if (m_opt.optLevel > 0) {
            irOut << "--- before optimisation (" << before << " instructions) ---\n" << beforeText;
            if (!ssaText.empty()) irOut << "--- SSA form after the SSA passes ---\n" << ssaText;
            irOut << "--- after -O" << m_opt.optLevel << " (" << f.instCount() << " instructions) ---\n" << printIr(f) << "\n";
        } else {
            irOut << beforeText << "\n";
        }
        LowerStats st;
        asmOut << lowerFunction(f, layouts, st);
        m_summary.asmInstructions += st.instructions;
        m_summary.spills += st.spills;
        if (m_opt.x86) { LowerStats xs; x86.addFunction(f, xs); }
    }
    if (m_opt.x86) m_x86 = x86.finish();
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
    out.push_back("Template instantiations: " + intToString(m_summary.classInstantiations) + " classes, " + intToString(m_summary.functionInstantiations) + " functions");
}

} // namespace cppparse
