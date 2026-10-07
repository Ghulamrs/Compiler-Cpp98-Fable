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
#include "Coff.hpp"
#include "Compiler.hpp"
#include "Diagram.hpp"
#include "Health.hpp"
#include "Html.hpp"
#include "Parallel.hpp"
#include "PathUtil.hpp"
#include "Pdf.hpp"
#include "Pp.hpp"
#include "CxxParser.hpp"
#include "Report.hpp"
#include "Symbols.hpp"

#include <cctype>
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
        "  --obj            version 5: write <name>.obj with cppparse's own COFF writer (no ml64 needed)\n"
        "  --msvc-mangling  version 5: MSVC-compatible decorated names (?square@@YAHH@Z) so MSVC code links with plain C++ declarations\n"
        "  -j <n>           worker threads (default: one per processor for parsing, at most 4 for compiling)\n"
        "  -E               version 6: preprocess the input files only and print the result (like cl /EP)\n"
        "  --parse6         version 6: preprocess and parse with the new front end; print counts and syntax errors,\n"
        "                   and write <name>-ast6.txt when -o or -n is given\n"
        "  -I <dir>         version 6: include directory for the preprocessor (also -Idir)\n"
        "  -D NAME[=value]  version 6: predefine a macro (also -DNAME=value); -U NAME undefines one\n"
        "  src\\*.cpp        wildcard patterns are expanded by cppparse itself\n"
        "  @list.txt        response file: one path, pattern or option per line\n"
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
    bool follow = true, textOnly = false, toStdout = false, quiet = false, healthOnly = false, strict = false, compile = true, assemble = false, obj = false;
    int threads = hardwareThreads();
    bool threadsGiven = false;
    bool ok0 = true;
    bool preprocessOnly = false, parse6 = false;
    PpOptions ppOpt;
    CompileOptions copt;
    ReportOptions opt;

    // response files (@list.txt) are expanded into the argument list first
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.size() > 1 && a[0] == '@') {
            std::vector<std::string> more;
            if (!readResponseFile(a.substr(1), more)) { std::cerr << "error: cannot read response file " << a.substr(1) << "\n"; return 2; }
            args.insert(args.end(), more.begin(), more.end());
        } else args.push_back(a);
    }
    for (size_t i = 0; i < args.size(); ++i) {
        std::string a = args[i];
        if (a == "-h" || a == "--help" || a == "/?") { usage(); return 0; }
        else if (a == "-j" && i + 1 < args.size()) { threads = atoi(args[++i].c_str()); threadsGiven = true; }
        else if (a.size() > 2 && a[0] == '-' && a[1] == 'j' && isdigit(static_cast<unsigned char>(a[2]))) { threads = atoi(a.c_str() + 2); threadsGiven = true; }
        else if (a == "-o" && i + 1 < args.size()) outDir = args[++i];
        else if (a == "-n" && i + 1 < args.size()) outName = args[++i];
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
        else if (a == "--obj") obj = true;
        else if (a == "--msvc-mangling") copt.msvcNames = true;
        else if (a == "-E" || a == "--preprocess") preprocessOnly = true;
        else if (a == "--parse6") parse6 = true;
        else if (a == "-I" && i + 1 < args.size()) ppOpt.includeDirs.push_back(args[++i]);
        else if (a.size() > 2 && a[0] == '-' && a[1] == 'I') ppOpt.includeDirs.push_back(a.substr(2));
        else if (a == "-D" && i + 1 < args.size()) ppOpt.defines.push_back(args[++i]);
        else if (a.size() > 2 && a[0] == '-' && a[1] == 'D') ppOpt.defines.push_back(a.substr(2));
        else if (a == "-U" && i + 1 < args.size()) ppOpt.undefines.push_back(args[++i]);
        else if (a.size() == 3 && a[0] == '-' && a[1] == 'O' && a[2] >= '0' && a[2] <= '3') copt.optLevel = a[2] - '0';
        else if (a == "-q") quiet = true;
        else if (!a.empty() && a[0] == '-') { std::cerr << "unknown option: " << a << "\n"; usage(); return 2; }
        else {
            std::vector<std::string> expanded;
            if (expandWildcards(a, expanded) == 0) { std::cerr << "error: no file matches " << a << "\n"; ok0 = false; }
            inputs.insert(inputs.end(), expanded.begin(), expanded.end());
        }
    }
    if (inputs.empty()) { usage(); return 2; }

    if (parse6) {
        // version 6: preprocess and parse every input with the new front end; print counts and syntax errors
        int totalErrors = 0;
        // calling-convention macros of windows.h, empty when the header is not available
        ppOpt.defines.push_back("WINAPI=");
        ppOpt.defines.push_back("CALLBACK=");
        ppOpt.defines.push_back("APIENTRY=");
        for (size_t i = 0; i < inputs.size(); ++i) {
            Preprocessor pp(ppOpt);
            pp.run(inputs[i]);
            cxx::CxxArena arena;
            cxx::TranslationUnit tu;
            tu.path = inputs[i];
            cxx::Parser parser(pp.tokens(), arena);
            // typedefs of the C library (and of windows.h when it was not found) that sources use without
            // the headers being available: until cppparse ships its own library headers they are assumed
            static const char* const kCTypes[] = { "size_t", "ptrdiff_t", "FILE", "time_t", "clock_t", "va_list", "jmp_buf", "fpos_t", "div_t", "ldiv_t",
                                                   "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "intptr_t", "uintptr_t", 0 };
            static const char* const kWinTypes[] = { "DWORD", "LONG", "ULONG", "LPVOID", "HANDLE", "BOOL", "BYTE", "WORD", "UINT", "INT", "LPCSTR", "LPSTR",
                                                     "SYSTEM_INFO", "CRITICAL_SECTION", "LPCRITICAL_SECTION", "LPTHREAD_START_ROUTINE", "SIZE_T", "HMODULE", "HWND", "LPARAM", "WPARAM", "LRESULT", 0 };
            for (int k = 0; kCTypes[k]; ++k) parser.assumeType(kCTypes[k]);
            bool winMissing = false;
            for (size_t k = 0; k < pp.missingIncludes().size(); ++k) if (toLower(pp.missingIncludes()[k]) == "windows.h") winMissing = true;
            if (winMissing) for (int k = 0; kWinTypes[k]; ++k) parser.assumeType(kWinTypes[k]);
            int errors = parser.parse(tu);
            totalErrors += errors;
            cxx::Counts c;
            cxx::countTranslationUnit(tu, c);
            std::cout << inputs[i] << ": " << pp.tokens().size() << " tokens, " << errors << " syntax error(s); namespaces " << c.namespaces
                      << ", classes " << c.classes << ", enums " << c.enums << ", functions defined " << c.functionsDefined << ", declared " << c.functionsDeclared
                      << ", variables " << c.variables << ", typedefs " << c.typedefs << ", templates " << c.templates << ", statements " << c.statements
                      << ", expressions " << c.expressions << "\n";
            const std::vector<cxx::ParseDiag>& d = parser.diagnostics();
            for (size_t k = 0; k < d.size() && k < 20; ++k)
                std::cout << "  " << (d[k].file < static_cast<int>(pp.files().size()) ? pp.files()[static_cast<size_t>(d[k].file)] : "?") << "(" << d[k].line << "): error: " << d[k].message << "\n";
            if (!outDir.empty() || !outName.empty()) {
                std::string base = joinPath(outDir.empty() ? "." : outDir, (outName.empty() ? baseName(inputs[i]) : outName) + (inputs.size() > 1 ? "-" + intToString(static_cast<int>(i)) : ""));
                std::vector<std::string> written;
                writeOut(base + "-ast6.txt", cxx::dumpTranslationUnit(tu), quiet, written);
            }
        }
        return totalErrors == 0 ? 0 : 1;
    }

    if (preprocessOnly) {
        // version 6: run the preprocessor alone and print the result like cl /EP
        int rc = 0;
        for (size_t i = 0; i < inputs.size(); ++i) {
            Preprocessor pp(ppOpt);
            if (!pp.run(inputs[i])) rc = 1;
            std::cout << pp.text();
            const std::vector<PpDiag>& d = pp.diagnostics();
            for (size_t k = 0; k < d.size(); ++k)
                std::cerr << d[k].file << "(" << d[k].line << "): " << (d[k].severity == PpDiag::Error ? "error" : "warning") << ": " << d[k].message << "\n";
        }
        return rc;
    }

    CodeBase cb;
    cb.setFollowIncludes(follow);
    if (threads < 1) threads = 1;
    cb.setThreads(threads);
    // the per-function compile phases allocate heavily and share one heap: beyond four threads
    // they slow down on the measured machines, so the default stops there unless -j says otherwise
    copt.threads = threadsGiven ? threads : (threads > 4 ? 4 : threads);
    bool ok = ok0;
    // plain files are parsed together in parallel; projects, solutions and folders handle their own lists
    std::vector<std::string> plainFiles;
    for (size_t i = 0; i < inputs.size(); ++i) {
        std::string ext = extension(inputs[i]);
        bool structured = ext == ".sln" || ext == ".vcxproj" || ext == ".vcproj" || isDirectory(inputs[i]);
        if (!structured) { plainFiles.push_back(inputs[i]); continue; }
        if (!quiet && !toStdout) std::cout << "reading " << inputs[i] << "\n";
        if (!cb.addInput(inputs[i])) ok = false;
    }
    if (!plainFiles.empty()) {
        if (!quiet && !toStdout) std::cout << "reading " << plainFiles.size() << " file(s) with " << threads << " thread(s)\n";
        if (!cb.addFiles(plainFiles)) ok = false;
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
            if (obj) {
                CoffResult cr;
                std::string objPath = base + (assemble ? "-cppparse.obj" : ".obj");   // --assemble keeps <name>.obj for ml64
                if (writeCoffObject(compiler.x86Text(), objPath, copt.threads, cr)) {
                    if (!quiet) std::cout << "  wrote " << objPath << "  (" << cr.functions << " functions, " << cr.instructions << " instructions, .text "
                                          << cr.textBytes << " bytes, .data " << cr.dataBytes << ", .rdata " << cr.rdataBytes << ", "
                                          << cr.relocations << " relocations, " << cr.symbols << " symbols)\n";
                } else {
                    std::cerr << "error: " << objPath << " not written, " << cr.errors.size() << " line(s) could not be encoded:\n";
                    for (size_t e = 0; e < cr.errors.size() && e < 10; ++e) std::cerr << "  " << cr.errors[e] << "\n";
                    ok = false;
                }
            }
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
        std::cout << "timing: parse " << cb.parseMilliseconds() << " ms" << (compile ? ", compile " + intToString(static_cast<int>(compiler.summary().compileMs)) + " ms" : std::string(""))
                  << " (parse threads " << threads << ", compile threads " << copt.threads << ")\n";
        for (size_t i = 0; i < cb.messages().size(); ++i) std::cout << "  note: " << cb.messages()[i] << "\n";
    }
    if (strictExit) return strictExit;
    return ok ? 0 : 1;
}
