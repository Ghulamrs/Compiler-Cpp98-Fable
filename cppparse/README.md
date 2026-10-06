# cppparse

A C++98 tool that reads a C++ project and writes a complete report about it:
the files, every preprocessor directive, all classes with their inheritance,
methods, fields and nested types, the functions with their parameters and
local variables, globals, macros, exception handling, and a class diagram.

Written in strict C++98 with no dependencies beyond the standard library.

## Input

```
cppparse [options] <input>...
```

| input                   | what happens                                                      |
|-------------------------|-------------------------------------------------------------------|
| `Project.sln`           | every C++ project in the solution is read                         |
| `Project.vcxproj`       | Visual Studio 2010 and later: file list, include dirs, defines   |
| `Project.vcproj`        | Visual Studio 2005/2008                                           |
| a folder                | scanned recursively for `.h .hpp .hxx .hh .inl` and `.cpp .cxx .cc .c` |
| `a.h b.cpp ...`         | the files given                                                   |

Files reached through `#include "..."` that are not part of the input are
parsed too and marked as *discovered* (disable with `--no-follow`).

## Output

Written next to the first input, or into the `-o` directory:

| file                   | content                                                         |
|------------------------|-----------------------------------------------------------------|
| `<name>-report.txt`    | the complete text report (8 sections, see below)                 |
| `<name>-health.txt`    | version 2 health check: errors, warnings, info with codes        |
| `<name>-symbols.map`   | linker-style map: modules, symbols, cross reference, unreferenced |
| `<name>-symbols.json`  | symbol table for tools (one object per symbol)                   |
| `<name>-symbols.txt`   | symbol table, tab-separated                                      |
| `<name>-report.html`   | the same report as one self-contained page with the diagram      |
| `<name>-report.pdf`    | the same report as PDF: title page, diagram pages, text pages    |
| `<name>-diagram.svg`   | the class diagram picture                                       |
| `<name>-diagram.dot`   | Graphviz source: `dot -Tpng -o diagram.png <name>-diagram.dot`   |

Report sections: 1 project, 2 files with every directive, 3 classes in
detail (methods with parameters, local variables and exceptions; fields;
typedefs; enums; friends; nested classes), 4 inheritance tree, 5 global
definitions (free functions, out-of-line member definitions, global
variables, static member definitions, typedefs, enums, macros, namespaces),
6 exception handling, 7 health findings, 8 totals.

Diagram legend: solid line with hollow triangle = inherits (red when
private, orange when protected, labelled when virtual); dashed blue = has a
member of that type; dotted grey = nested in; yellow box = abstract; grey
dashed box = base class not defined in the parsed files.

## Options

```
-o <dir>         output directory
-n <name>        base name of the output files
--no-follow      do not parse #include'd files outside the input
--no-directives  omit the per-file preprocessor listing
--no-locals      omit local variables
--text-only      write only the .txt, health and symbol files
--stdout         print the text report, write nothing
--health         print the health report, write nothing
--strict         exit code 3 when the health check finds errors (for build scripts)
-q               quiet
```

## Version 2: health checker

`<name>-health.txt` (also section 7 of the reports) lists what is wrong or
risky in the code, each finding with a code, severity, location, message
and a one-line explanation. Severities: **E** will not compile or link,
**W** likely bug or risky construct, **I** worth knowing.

| area            | codes |
|-----------------|-------|
| preprocessor    | E100 include not found, E101 unbalanced `#if`, W102 no include guard, I103 circular include |
| syntax          | E110 missing `;` after class body, W111 unrecognised construct, I112 macro invocation, E113 unterminated body |
| class design    | E120 duplicate class, E121 duplicate/self base, W122 base not defined, E123 duplicate member, E124 incomplete type, W125 virtual methods without virtual destructor, W126 static member never defined, I127 derived class still abstract, W128 hides a base virtual |
| implementation  | E130 definition without declaration (reports const/parameter mismatch), E131 duplicate definition, W132/W133 declared but never defined, E134 return type mismatch, E135 default argument repeated, E136 initializer names unknown member, W137 initializer order, W138 member not initialised |
| calls and flow  | W140 missing return, W141 return value in void, E142 wrong number of arguments, E143 no such member, E144 `.` on pointer or `->` on object, E145 no matching constructor, I146 call to undeclared function, W147 throw in destructor |
| bodies          | W150 unused parameter, W151 unused local, W152 shadowing, W153 `if (x = y)`, W154 `if (...);`, W155 catch by value, E156 handler after `catch(...)`, W157 unreachable code |
| one definition  | E160 global defined twice, W161 extern never defined, E162 duplicate enumerator, E163 function defined twice, W164 global defined in a header, W165 non-inline function in a header |

The checks are static and declaration-level: macros are not expanded and
`#if` conditions are not evaluated, so code that depends on them can give
false positives. Argument checks compare counts, not types; full type
checking needs an expression parser and is planned for version 3 together
with an AST and a three-address IR. The folder `sample-bad/` contains one
example of every finding and is part of the regression test.

## Symbol table and map file

`<name>-symbols.map` follows the layout of a linker map: a module table
(one line per file), the symbols of each module with a `module:ordinal`
reference, kind, name with signature, line range and the link to the
definition or declaration, then all symbols sorted by name, a cross
reference (every function and global with the places that call or use it),
the functions and globals never referenced anywhere, and the entry point.
`<name>-symbols.json` and `-symbols.txt` hold the same symbols for tools.

## Building and testing

```
powershell -ExecutionPolicy Bypass -File cpp\sandbox\build-cppparse.ps1
```

Compiles every `cppparse/*.cpp` with MSVC at `/W4 /permissive-` (warnings
fail the build), then runs four tests: the Visual Studio 2010 solution in
`sample/` with checks on the report, symbols and map, the `cpp` folder
itself as a folder input (cppparse parses and health-checks its own
sources), `Vector.hpp` + `VectorTest.cpp` as plain files, and the broken
folder `sample-bad/`, which must produce every health code. Any C++98 compiler works for the sources themselves, e.g.
`g++ -std=c++98 -pedantic -Wall -o cppparse cppparse/*.cpp`.

## How it works

`Lexer` turns a file into tokens, keeping each preprocessor line as one
token. `Parser` walks the tokens once: it records every directive and keeps
the stack of active `#if` conditions, skips `#if 0` regions, recognises
include guards, and parses namespaces, classes (templates, specializations,
multiple and virtual inheritance, nested classes), functions (declarations,
definitions, constructor initializer lists, function-try-blocks), variables,
typedefs, enums, friends and usings. Function bodies are skipped by brace
matching while counting `try`/`catch`/`throw` and recording local variable
declarations. `CodeBase` loads the project, follows includes and links
out-of-line definitions to their declarations, base names to classes, and
static member definitions to fields. `Diagram`, `Report`, `Html` and `Pdf`
write the outputs.

It is a declaration-level parser, not a compiler: it does not expand macros
or evaluate `#if` conditions (it reports them instead), and constructs it
cannot classify are listed as diagnostics and skipped.

## Limits

* Macros that hide syntax (`BEGIN_MESSAGE_MAP(...)`, `DECLARE_...()`) are
  recognised heuristically as macro invocations and listed as diagnostics.
* `struct X y;` versus `class MACRO Name;` is decided by a heuristic
  (an all-caps first word is treated as an attribute macro).
* Local variable detection is syntactic; `a * b;` at statement start is
  taken as a declaration of `b`, as a compiler would.
* Property sheets (`.props`) referenced by a `.vcxproj` are not evaluated.

## Planned for version 3

An expression and statement parser producing an AST, a type checker over
the symbol table (type-based argument checks, conversions, const
correctness), and lowering to a three-address IR with a control-flow graph
as a starting point for optimisation work.
