// Compiler.cpp - version 3 driver, version 5: the per-function phases run on the worker pool.
//
// Phases (each one runs the functions in parallel unless noted):
//   1. body parse, type check, IR build        per function; shared structures are locked
//   2. template instantiation rounds           the same, for bodies requested while type checking
//   3. local optimisation (-O1 passes, LICM)   per function
//   4. inlining and unrolling                  sequential: reads the IR of other functions
//   5. SSA passes and final clean-up           per function
//   6. VM64 lowering and x86-64 code           per function; the module pools are locked
// Results are merged in a fixed order, so the outputs do not depend on thread timing.
#include "Compiler.hpp"
#include "BodyParser.hpp"
#include "Instantiate.hpp"
#include "Ir.hpp"
#include "Lower.hpp"
#include "Optimizer.hpp"
#include "Parallel.hpp"
#include "PathUtil.hpp"
#include "TypeCheck.hpp"
#include "Types.hpp"
#include "X86.hpp"

#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

namespace cppparse {

Compiler::Compiler(CodeBase& cb, const CompileOptions& opt) : m_cb(cb), m_opt(opt) {}

Compiler::~Compiler() {}

namespace {

struct Unit {
    FunctionInfo*             fn;
    const std::vector<Token>* tokens;
    bool                      instantiated;
};

void collectUnits(CodeBase& cb, std::vector<Unit>& out)
{
    const std::vector<SourceFile*>& files = cb.files();
    for (size_t f = 0; f < files.size(); ++f) {
        SourceFile* sf = files[f];
        for (size_t k = 0; k < sf->functions.size(); ++k)
            if (sf->functions[k].hasBody) { Unit u; u.fn = &sf->functions[k]; u.tokens = &sf->tokens; u.instantiated = false; out.push_back(u); }
        for (size_t c = 0; c < sf->allClasses.size(); ++c)
            for (size_t m = 0; m < sf->allClasses[c]->methods.size(); ++m)
                if (sf->allClasses[c]->methods[m].hasBody) { Unit u; u.fn = &sf->allClasses[c]->methods[m]; u.tokens = &sf->tokens; u.instantiated = false; out.push_back(u); }
    }
}

// everything one function produces in phase 1; merged into the compiler state in unit order
struct UnitResult {
    std::vector<Finding> findings;
    std::string          astText;
    IrFunction*          ir;
    bool                 parseFailed, typeFailed;
    UnitResult() : ir(0), parseFailed(false), typeFailed(false) {}
};

// parses, type checks and lowers one function body to IR (runs on a worker thread)
void compileUnit(const CodeBase& cb, TypeTable& types, Layouts& layouts, const std::set<std::string>& macros, bool emitAst,
                 FunctionInfo& fn, const std::vector<Token>& tokens, UnitResult& r)
{
    AstArena arena;
    FunctionAst ast;
    BodyParser parser(cb, types, arena, fn, tokens);
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
        r.parseFailed = true;
        for (size_t e = 0; e < ast.errors.size() && e < 3; ++e) {
            Finding f;
            f.severity = Finding::Warning;
            f.code = "T191";
            f.file = fn.file;
            f.line = ast.errors[e].size() > 1 ? atoi(ast.errors[e].c_str() + 1) : fn.line;
            f.message = "body of " + fn.displayName() + " could not be parsed: " + ast.errors[e].substr(ast.errors[e].find(':') + 2);
            f.detail = "the function is not type checked or compiled";
            r.findings.push_back(f);
        }
    }
    TypeChecker checker(cb, types, r.findings, macros);
    bool checked = ast.parsed && checker.check(ast);
    if (ast.parsed && !checked) r.typeFailed = true;
    fn.typeChecked = ast.parsed;   // typed checks replace the count-based ones even when they found errors

    if (emitAst) {
        std::ostringstream s;
        dumpFunctionAst(ast, s);
        r.astText = s.str() + "\n";
    }
    if (!ast.parsed) return;

    IrBuilder builder(cb, types, layouts);
    r.ir = new IrFunction;
    builder.build(ast, *r.ir);
}

struct FrontJob : public ParallelJob {
    const CodeBase*               cb;
    TypeTable*                    types;
    Layouts*                      layouts;
    const std::set<std::string>*  macros;
    bool                          emitAst;
    std::vector<Unit>             units;
    std::vector<UnitResult>       results;
    virtual void run(size_t i) { compileUnit(*cb, *types, *layouts, *macros, emitAst, *units[i].fn, *units[i].tokens, results[i]); }
};

// optimisation phases over the IR functions
struct OptJob : public ParallelJob {
    Optimizer*                             opt;
    int                                    phase;      // 0 local, 1 interprocedural, 2 SSA
    std::vector<IrFunction*>*              functions;
    std::vector<std::vector<std::string> >* logs;
    std::vector<std::string>*              ssaTexts;
    virtual void run(size_t i)
    {
        IrFunction& f = *(*functions)[i];
        if (phase == 0) opt->runLocal(f, (*logs)[i]);
        else if (phase == 1) opt->runInterprocedural(f, (*logs)[i]);
        else opt->runSsa(f, (*logs)[i], &(*ssaTexts)[i]);
    }
};

// lowering to the VM64 listing and to x86-64 text
struct LowerJob : public ParallelJob {
    std::vector<IrFunction*>* functions;
    Layouts*                  layouts;
    X86Module*                x86;
    std::vector<std::string>  vmText, x86Text, afterText;
    std::vector<LowerStats>   vmStats;
    virtual void run(size_t i)
    {
        IrFunction& f = *(*functions)[i];
        afterText[i] = printIr(f);
        vmText[i] = lowerFunction(f, *layouts, vmStats[i]);
        if (x86) { LowerStats xs; x86Text[i] = x86->codeFor(f, xs); }
    }
};

} // namespace

void Compiler::run()
{
    long start = millisecondsNow();
    int threads = m_opt.threads < 1 ? 1 : m_opt.threads;
    TypeTable types(m_cb);
    Instantiator instantiator(m_cb);
    types.setInstantiator(&instantiator);
    Layouts layouts(types, m_cb);
    std::set<std::string> macros;
    TypeChecker::collectMacroNames(m_cb, macros);

    std::vector<Unit> all;
    collectUnits(m_cb, all);

    std::ostringstream astOut, irOut, logOut, asmOut, layoutOut;
    std::vector<IrFunction*> irFunctions;
    std::map<std::string, IrFunction*> byName;

    logOut << "cppparse version 3 optimisation log  (-O" << m_opt.optLevel << ", " << threads << " thread(s))\n\n";

    long tFront = millisecondsNow();
    // phase 1: the functions written in the sources
    FrontJob job;
    job.cb = &m_cb; job.types = &types; job.layouts = &layouts; job.macros = &macros; job.emitAst = m_opt.emitAst;
    for (size_t u = 0; u < all.size(); ++u) {
        FunctionInfo& fn = *all[u].fn;
        ++m_summary.functionsSeen;
        std::string title = fn.displayName() + "(" + fn.paramText + ")" + (fn.isConst ? " const" : "") + "   [" + m_cb.displayPath(fn.file) + ":" + intToString(fn.line) + "]";
        std::string skip;
        if (fn.tmpl.isTemplate || (fn.ownerClass && fn.ownerClass->tmpl.isTemplate)) skip = "template (compiled per instantiation, see the instantiated functions below)";
        else if (fn.exceptions.functionTryBlock) skip = "function-try-block";
        else if (fn.isFriend) skip = "friend function defined inside a class";
        else if (fn.bodyEnd <= fn.bodyBegin || fn.bodyEnd > all[u].tokens->size()) skip = "body tokens not available";
        if (!skip.empty()) {
            ++m_summary.functionsSkipped;
            astOut << "function " << title << "\n  skipped: " << skip << "\n\n";
            continue;
        }
        job.units.push_back(all[u]);
    }

    // phase 2 repeats phase 1 for template instantiations requested while type checking (they may request more)
    for (int round = 0; round < 9 && !job.units.empty(); ++round) {
        job.results.clear();
        job.results.resize(job.units.size());
        runParallel(job, job.units.size(), threads);
        for (size_t i = 0; i < job.units.size(); ++i) {
            UnitResult& r = job.results[i];
            m_findings.insert(m_findings.end(), r.findings.begin(), r.findings.end());
            if (job.units[i].instantiated) astOut << "; instantiated\n";
            astOut << r.astText;
            if (r.parseFailed) ++m_summary.parseFailures;
            if (r.typeFailed) ++m_summary.typeFailures;
            if (r.ir) { irFunctions.push_back(r.ir); byName[r.ir->name] = r.ir; ++m_summary.functionsCompiled; }
        }
        job.units.clear();
        if (round == 8) break;
        std::vector<FunctionInfo*> pending;
        instantiator.takePending(pending);
        for (size_t i = 0; i < pending.size(); ++i) {
            const std::vector<Token>* toks = instantiator.tokensFor(pending[i]);
            if (!toks) continue;
            ++m_summary.functionsSeen;
            Unit u; u.fn = pending[i]; u.tokens = toks; u.instantiated = true;
            job.units.push_back(u);
        }
    }
    tFront = millisecondsNow() - tFront;
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

    // phases 3 to 5: optimise
    size_t n = irFunctions.size();
    std::vector<int> before(n, 0);
    std::vector<std::string> beforeText(n), ssaTexts(n);
    std::vector<std::vector<std::string> > logs(n);
    for (size_t i = 0; i < n; ++i) { before[i] = irFunctions[i]->instCount(); m_summary.irBefore += before[i]; beforeText[i] = printIr(*irFunctions[i]); }
    Optimizer optimizer(m_opt.optLevel, byName);
    long tLocal = 0, tInter = 0, tSsa = 0;
    if (m_opt.optLevel > 0) {
        OptJob oj;
        oj.opt = &optimizer; oj.functions = &irFunctions; oj.logs = &logs; oj.ssaTexts = &ssaTexts;
        long t0 = millisecondsNow();
        oj.phase = 0; runParallel(oj, n, threads);
        long t1 = millisecondsNow();
        oj.phase = 1; runParallel(oj, n, 1);          // inlining reads other functions: one thread
        long t2 = millisecondsNow();
        oj.phase = 2; runParallel(oj, n, threads);
        long t3 = millisecondsNow();
        tLocal = t1 - t0; tInter = t2 - t1; tSsa = t3 - t2;
    }

    // phase 6: lower
    X86Module x86(m_cb, types, layouts);
    LowerJob lj;
    lj.functions = &irFunctions; lj.layouts = &layouts; lj.x86 = m_opt.x86 ? &x86 : 0;
    lj.vmText.resize(n); lj.x86Text.resize(n); lj.afterText.resize(n); lj.vmStats.resize(n);
    long tLower = millisecondsNow();
    runParallel(lj, n, threads);
    tLower = millisecondsNow() - tLower;

    for (size_t i = 0; i < n; ++i) {
        IrFunction& f = *irFunctions[i];
        for (size_t l = 0; l < logs[i].size(); ++l) logOut << logs[i][l] << "\n";
        m_summary.irAfter += f.instCount();
        irOut << "================================================================================\n";
        if (m_opt.optLevel > 0) {
            irOut << "--- before optimisation (" << before[i] << " instructions) ---\n" << beforeText[i];
            if (!ssaTexts[i].empty()) irOut << "--- SSA form after the SSA passes ---\n" << ssaTexts[i];
            irOut << "--- after -O" << m_opt.optLevel << " (" << f.instCount() << " instructions) ---\n" << lj.afterText[i] << "\n";
        } else {
            irOut << beforeText[i] << "\n";
        }
        asmOut << lj.vmText[i];
        m_summary.asmInstructions += lj.vmStats[i].instructions;
        m_summary.spills += lj.vmStats[i].spills;
        if (m_opt.x86) x86.appendCode(lj.x86Text[i]);
    }
    if (m_opt.x86) m_x86 = x86.finish();
    if (m_opt.optLevel == 0) logOut << "(no optimisation requested: use -O1, -O2 or -O3)\n";
    logOut << "\ntotal IR instructions: " << m_summary.irBefore << " -> " << m_summary.irAfter << "\n";
    logOut << "phase times (ms, " << threads << " thread(s)): front end " << tFront << ", local optimisation " << tLocal
           << ", inlining/unrolling " << tInter << ", SSA " << tSsa << ", lowering " << tLower << "\n";

    for (size_t i = 0; i < irFunctions.size(); ++i) delete irFunctions[i];

    m_ast = astOut.str();
    m_ir = irOut.str();
    m_log = logOut.str();
    m_asm = "; cppparse version 3 lowering - target VM64 (R0-R7 integer, F0-F7 float, FP, SP)\n"
            "; calling convention: R0-R3 / F0-F3 then stack, result R0 / F0, all registers caller-saved\n\n" + asmOut.str();
    m_layouts = layoutOut.str();
    m_summary.compileMs = millisecondsNow() - start;
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
