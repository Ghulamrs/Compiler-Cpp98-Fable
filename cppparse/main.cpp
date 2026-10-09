// main.cpp - cppparse command line.
//
//   cppparse [options] <input>...
//
//   input    a Visual Studio solution (.sln), a C++ project (.vcxproj, .vcproj),
//            a folder (scanned recursively for headers and sources), or
//            individual header / source files
//
// Outputs, written next to the first input (or into the -o directory):
//   <name>-report.txt     complete text report
//   <name>-health.txt     version 2 health check findings
//   <name>-symbols.json   symbol table (also -symbols.txt, tab-separated)
//   <name>-symbols.map    linker-style map: modules, symbols, cross reference
//   <name>-ast.txt        version 3: abstract syntax tree of every function body
//   <name>-ir.txt         version 3: class layouts and the IR before/after -On
//   <name>-opt.log        version 3: what each optimisation pass changed
//   <name>-asm.txt        version 3: lowered assembly for the VM64 target
//   <name>.asm            version 4: x86-64 MASM (Windows x64 ABI); --assemble adds <name>.obj via ml64
//   <name>-report.html    self-contained HTML report with the class diagram
//   <name>-report.pdf     the same report as PDF (diagram + text)
//   <name>-diagram.svg    class diagram picture
//   <name>-diagram.dot    Graphviz source (dot -Tpng -o diagram.png <name>-diagram.dot)
//
// Strict C++98.
#include "CodeBase.hpp"
#include "Compiler.hpp"
#include "Diagram.hpp"
#include "Health.hpp"
#include "Html.hpp"
#include "PathUtil.hpp"
#include "Pdf.hpp"
#include "Report.hpp"
#include "Symbols.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace cppparse;

namespace {

void usage()
{
    std::cout <<
        "cppparse - C++98 project reader and report generator\n"
        "\n"
        "usage: cppparse [options] <input>...\n"
        "\n"
        "  <input>          .sln | .vcxproj | .vcproj | folder | header/source files\n"
        "\n"
        "options:\n"
        "  -o <dir>         output directory (default: next to the first input)\n"
        "  -n <name>        base name of the output files (default: input name)\n"
        "  --no-follow      do not parse #include'd files that are not part of the input\n"
        "  --no-directives  omit the per-file preprocessor listing from the reports\n"
        "  --no-locals      omit local variables from the reports\n"
        "  --text-only      write only the .txt report (and the health report)\n"
        "  --stdout         print the text report to the console, write no files\n"
        "  --health         print the health report to the console, write no files\n"
        "  --strict         exit code 3 when the health check finds errors\n"
        "  -O0 .. -O3       version 3: optimisation level for the IR (default -O0)\n"
        "  --no-compile     skip version 3 (AST, type check, IR, lowering)\n"
        "  --no-x86         version 4: do not write the x86-64 MASM file (<name>.asm)\n"
        "  --assemble       version 4: run \"ml64 /c\" on <name>.asm to produce <name>.obj (ml64 must be on PATH)\n"
        "  -q               quiet: print only errors\n"
        "  -h, --help       this text\n";
}

std::string stem(const std::string& path)
{
    std::string b = baseName(normalizePath(path));
    if (b.empty()) b = "project";
    size_t dot = b.rfind('.');
    std::string ext = extension(b);
    if (dot != std::string::npos && (ext == ".sln" || ext == ".vcxproj" || ext == ".vcproj" || isHeaderExtension(ext) || isSourceExtension(ext)))
        b = b.substr(0, dot);
    return b;
}

bool writeOut(const std::string& path, const std::string& content, bool quiet, std::vector<std::string>& written)
{
    if (!writeFile(path, content)) { std::cerr << "error: cannot write " << path << "\n"; return false; }
    written.push_back(path + "  (" + intToString(static_cast<int>(content.size() / 1024)) + " KB)");
    if (!quiet) std::cout << "  wrote " << path << "\n";
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    std::vector<std::string> inputs;
    std::string outDir, outName;
    bool follow = true, textOnly = false, toStdout = false, quiet = false, healthOnly = false, strict = false, compile = true, assemble = false;
    CompileOptions copt;
    ReportOptions opt;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help" || a == "/?") { usage(); return 0; }
        else if (a == "-o" && i + 1 < argc) outDir = argv[++i];
        else if (a == "-n" && i + 1 < argc) outName = argv[++i];
        else if (a == "--no-follow") follow = false;
        else if (a == "--no-directives") opt.directives = false;
        else if (a == "--no-locals") opt.locals = false;
        else if (a == "--text-only") textOnly = true;
        else if (a == "--stdout") toStdout = true;
        else if (a == "--health") healthOnly = true;
        else if (a == "--strict") strict = true;
        else if (a == "--no-compile") compile = false;
        else if (a == "--no-x86") copt.x86 = false;
        else if (a == "--assemble") assemble = true;
        else if (a.size() == 3 && a[0] == '-' && a[1] == 'O' && a[2] >= '0' && a[2] <= '3') copt.optLevel = a[2] - '0';
        else if (a == "-q") quiet = true;
        else if (!a.empty() && a[0] == '-') { std::cerr << "unknown option: " << a << "\n"; usage(); return 2; }
        else inputs.push_back(a);
    }
    if (inputs.empty()) { usage(); return 2; }

    CodeBase cb;
    cb.setFollowIncludes(follow);
    bool ok = true;
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (!quiet && !toStdout) std::cout << "reading " << inputs[i] << "\n";
        if (!cb.addInput(inputs[i])) ok = false;
    }
    if (cb.files().empty()) {
        std::cerr << "error: no source files were read\n";
        for (size_t i = 0; i < cb.messages().size(); ++i) std::cerr << "  " << cb.messages()[i] << "\n";
        return 1;
    }
    cb.link();
    Compiler compiler(cb, copt);
    if (compile) compiler.run();          // sets FunctionInfo::typeChecked before the health check runs
    cb.checkHealth();
    if (compile) cb.addFindings(compiler.findings());
    int errors = 0, warnings = 0, infos = 0;
    countFindings(cb.findings(), errors, warnings, infos);
    const int strictExit = (strict && errors > 0) ? 3 : 0;

    if (healthOnly) { std::cout << renderHealthText(cb); return strictExit ? strictExit : (ok ? 0 : 1); }
    std::string text = renderTextReport(cb, opt);
    if (toStdout) { std::cout << text; return strictExit ? strictExit : (ok ? 0 : 1); }

    if (outDir.empty()) outDir = cb.baseDirectory();
    if (outName.empty()) outName = stem(inputs[0]);
    outDir = normalizePath(outDir);
    std::string base = outDir + "/" + outName;

    std::vector<std::string> written;
    if (!writeOut(base + "-report.txt", text, quiet, written)) ok = false;
    if (!writeOut(base + "-health.txt", renderHealthText(cb), quiet, written)) ok = false;
    if (!writeOut(base + "-symbols.json", renderSymbolsJson(cb), quiet, written)) ok = false;
    if (!writeOut(base + "-symbols.txt", renderSymbolsText(cb), quiet, written)) ok = false;
    if (!writeOut(base + "-symbols.map", renderSymbolsMap(cb), quiet, written)) ok = false;
    if (compile) {
        if (!writeOut(base + "-ast.txt", compiler.astText(), quiet, written)) ok = false;
        if (!writeOut(base + "-ir.txt", "; class layouts\n" + compiler.layoutText() + "\n" + compiler.irText(), quiet, written)) ok = false;
        if (!writeOut(base + "-opt.log", compiler.optLog(), quiet, written)) ok = false;
        if (!writeOut(base + "-asm.txt", compiler.asmText(), quiet, written)) ok = false;
        if (copt.x86) {
            if (!writeOut(base + ".asm", compiler.x86Text(), quiet, written)) ok = false;
            if (assemble) {
                std::string cmd = "ml64 /nologo /c \"/Fo" + base + ".obj\" \"" + base + ".asm\"";
                if (!quiet) std::cout << "  running " << cmd << "\n";
                int rc = std::system(cmd.c_str());
                if (rc != 0) { std::cerr << "error: ml64 failed (exit code " << rc << ")\n"; ok = false; }
            }
        }
    }
    if (!textOnly) {
        DiagramLayout layout;
        layout.build(cb);
        if (!writeOut(base + "-diagram.svg", renderSvg(layout, cb.title() + " - class diagram", true), quiet, written)) ok = false;
        if (!writeOut(base + "-diagram.dot", renderDot(cb), quiet, written)) ok = false;
        if (!writeOut(base + "-report.html", renderHtmlReport(cb, layout, opt), quiet, written)) ok = false;
        if (!writeOut(base + "-report.pdf", renderPdfReport(cb, layout, text), quiet, written)) ok = false;
    }

    if (!quiet) {
        Totals t;
        computeTotals(cb, t);
        std::cout << "\n" << cb.title() << ": " << t.files << " files (" << t.headers << " headers, " << t.sources << " sources), "
                  << t.lines << " lines, " << (t.classes + t.structs + t.unions) << " classes, " << t.methods << " methods, "
                  << t.freeFunctions << " free functions, " << t.globals << " globals, " << t.macros << " macros\n"
                  << "health: " << errors << " errors, " << warnings << " warnings, " << infos << " info\n";
        if (compile) {
            std::vector<std::string> lines;
            compiler.summaryLines(lines);
            for (size_t i = 0; i < lines.size(); ++i) std::cout << lines[i] << "\n";
        }
        for (size_t i = 0; i < cb.messages().size(); ++i) std::cout << "  note: " << cb.messages()[i] << "\n";
    }
    if (strictExit) return strictExit;
    return ok ? 0 : 1;
}
