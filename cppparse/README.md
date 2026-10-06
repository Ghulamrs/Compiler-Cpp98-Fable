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
--text-only      write only the .txt report
--stdout         print the text report, write nothing
-q               quiet
```

## Building and testing

```
powershell -ExecutionPolicy Bypass -File cpp\sandbox\build-cppparse.ps1
```

Compiles every `cppparse/*.cpp` with MSVC at `/W4 /permissive-` (warnings
fail the build), then runs three tests: the Visual Studio 2010 solution in
`sample/` with 30 checks on the report, the `cpp` folder itself as a folder
input (cppparse parses its own sources), and `Vector.hpp` + `VectorTest.cpp`
as plain files. Any C++98 compiler works for the sources themselves, e.g.
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

## Planned for version 2

A health checker: syntax errors in class definitions and implementations,
missing include files, calls to functions that do not exist, declarations
without definitions, and simple semantic checks, for projects and folders.
