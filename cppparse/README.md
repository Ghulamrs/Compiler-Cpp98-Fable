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
| `<name>-ast.txt`       | version 3: annotated AST of every function body                  |
| `<name>-ir.txt`        | version 3: class layouts, IR before/after `-On`                   |
| `<name>-opt.log`       | version 3: optimisation passes and instruction counts            |
| `<name>-asm.txt`       | version 3: lowered VM64 assembly                                  |
| `<name>.asm`           | version 4: x86-64 MASM (Windows x64 ABI); `--assemble` adds `.obj` via ml64 |
| `<name>.obj`           | version 5: COFF object written by cppparse itself (`--obj`) |
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
-O0 .. -O3       version 3 optimisation level (default -O0)
--no-compile     skip version 3 and 4
--no-x86         do not write the x86-64 .asm file
--assemble       run ml64 on the .asm to produce the .obj (ml64 on PATH)
--obj            write the .obj with cppparse's own COFF writer (version 5)
-j <n>           worker threads (default: one per processor for parsing, at most 4 for compiling)
src\*.cpp        wildcard patterns are expanded by cppparse itself (more than 50 files is fine)
@list.txt        response file: one path, pattern or option per line; '#' starts a comment
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

## Version 3: AST, type checking, IR, optimisation and lowering

Every function body (templates excepted) goes through a small compiler
back half, all in C++98:

```
tokens -> AST (BodyParser) -> type check (TypeCheck) -> IR (Ir) -> passes (Optimizer) -> VM64 assembly (Lower)
```

Outputs: `<name>-ast.txt` (annotated syntax tree per function),
`<name>-ir.txt` (class layouts with field offsets and vtables, then the IR
before and after optimisation), `<name>-opt.log` (what each pass changed
per function, with instruction counts) and `<name>-asm.txt` (the lowered
assembly). Select the level with `-O0` (default), `-O1`, `-O2` or `-O3`;
`--no-compile` skips version 3 entirely.

**Type checker** findings (T codes, in the health report): T100
undeclared identifier, T101 no such member, T102 no viable overload by
argument types, T103 ambiguous call, T104 bad conversion, T105 invalid
operands, T106 non-const method on const object, T107 assignment to const,
T108 return type mismatch, T109 condition not convertible to bool, T110
member access on non-class, T111 bad subscript, T112 bad dereference, T114
unknown type, T115 signed/unsigned comparison, T116 narrowing, T117 `.`
on pointer or `->` on object, T118 unused expression result, T119 private
member access, T121 lvalue required, T122 break outside loop, T123 to
T125 switch problems, T146 call to an undeclared (library) function, T190
and T191 constructs outside the subset. For bodies the checker handles,
the typed checks replace the count-based E142 to I146 of version 2.

**IR**: temporaries assigned once, locals as memory slots with
`loadlocal`/`storelocal`, `addrfield` with real offsets from the class
layout, `vcall` through vtable slots, `br`/`condbr`/`ret`, `trybegin`,
`throw`, `catchparam`. Objects are handled by address; `__copy`, `__new`
and `__delete` are runtime calls.

**Passes**: `-O1` constant propagation and folding, store-to-load
forwarding, copy propagation, algebraic simplification and strength
reduction (`x * 8` to `x << 3`), local common-subexpression elimination,
dead code and dead store elimination, branch folding, unreachable block
removal, block merging. `-O2` adds loop-invariant code motion (dominators,
natural loops, preheaders) and inlining of small functions. `-O3` inlines
larger functions and fully unrolls loops with a constant trip count of at
most 8. The folder `sample-opt/` shows each transformation; the test
checks, for example, that `constants()` folds to `ret #14` at `-O1`,
`square` is inlined at `-O2` and `unroll()` becomes `ret #6` at `-O3`.

**Lowering** targets a virtual 64-bit machine with eight integer and
eight floating registers: linear-scan register allocation with spilling,
frame layout for locals and spill slots, arguments in R0-R3/F0-F3 then on
the stack, virtual calls through the vptr, `TRY`/`ENDTRY` pseudo
instructions for exception regions.

Subset limits: function-try-blocks, brace initialisers, goto and local
classes are reported and skipped; standard-library types are opaque and
never produce findings; calls through unknown functions are emitted as
opaque calls.

## Version 4: SSA, template instantiation, x86-64

**SSA** (`Ssa.cpp`): after the version 3 passes every scalar local that
never has its address taken is promoted to SSA temporaries with phi
instructions placed by iterated dominance frontiers; global value
numbering runs over the dominator tree, trivial phis are folded, and the
phis are removed again with parallel copies on split critical edges
before lowering. `-ir.txt` shows the SSA form of every function.

**Template instantiation** (`Instantiate.cpp`): `FixedStack<double, 32>`
or `Buffer<int, 4>` used in a declaration becomes a real class with the
parameters substituted in fields, signatures, bases, typedefs and every
method body (in-class or out-of-line); `maxOf(7, 2)` deduces `T` from the
argument types and instantiates the function template. Instantiated
bodies are parsed, type checked and compiled in extra rounds; integral
arguments such as `R - 1` are folded; dependent uses inside other
templates are left alone; a user-written explicit specialisation wins.
Member templates and partial specialisations are not instantiated.

**x86-64** (`X86.cpp`): `<name>.asm` holds MASM for the Windows x64 ABI:
callee-saved registers allocated by linear scan, frame with shadow space,
arguments in RCX/RDX/R8/R9 and XMM0-XMM3, SSE2 floating point, vtables in
`.data`, strings and float constants in `.const`, `malloc`/`free`/`memcpy`
for the runtime helpers, `cppparse_throw` for `throw` (exceptions are not
implemented on this target). `--assemble` runs `ml64 /c` to produce
`<name>.obj`. Symbols use cppparse's own mangling (`square(int x)` is
`square_int_x`); `extern "C"` functions keep their name.

Test 6 assembles the `sample-opt` output with `ml64`, links it with the
MSVC-compiled `sample-opt-driver/driver.cpp` and runs it at `-O0`, `-O2`
and `-O3`: all 19 result checks pass, templates included.

## Version 5: parallel pipeline, many input files

**Worker pool** (`Parallel.cpp`): C++98 has no threads, so the pool uses
`CreateThread` on Windows (a sequential fallback elsewhere). A job is a
`ParallelJob` with `run(index)`; `runParallel(job, count, threads)` hands
the indices to the threads through an interlocked counter.

**Parallel loading** (`CodeBase::loadFiles`): every input list (project,
solution, folder, or the files on the command line) is read, lexed and
parsed on the pool, one file per job, writing only into its own
`SourceFile`. Files are registered in list order, then the `#include`s
they discovered form the next round, until nothing new appears. On the
49 files of cppparse itself parsing takes 32 ms on 12 threads instead
of 125 ms.

**Parallel compiling** (`Compiler.cpp`): the per-function phases run on
the pool: body parse + type check + IR build (and the template
instantiation rounds), the local optimisation passes, the SSA passes, and
VM64 plus x86-64 lowering. Inlining and unrolling read the IR of other
functions, so that phase runs on one thread. The shared structures (type
table, template instantiator, class layouts, the x86 constant pools) are
guarded by one recursive lock; results are merged in function order and
x86 constant labels are derived from their contents (`fconst_<bits>`,
`str_<hash>`), so every output file is byte-identical whatever `-j` is.
The optimisation log ends with the time of each phase.

The compile phases allocate heavily and all threads share the C runtime
heap: on a 12-processor machine they are fastest at 2 to 4 threads and
get slower beyond, which is why the default compile thread count stops
at 4 (`-j` overrides it). Arena allocation for the IR and the generated
text is on the version 6 list.

**Many files**: `src\*.cpp` and `include\*.hpp` are expanded by cppparse
(no shell globbing is needed on Windows), and `@list.txt` reads paths,
patterns and options from a file, one per line, so a command line is
never too long. All plain files are parsed together as one project.

**COFF object writer** (`Coff.cpp`, `--obj`): `<name>.obj` is written by
cppparse itself, so ml64 is no longer needed to get an object file. The
writer is a small assembler for exactly the MASM subset that `X86.cpp`
emits: it parses the text, encodes every PROC on the worker pool (REX,
ModRM, SIB, RIP-relative operands, the SSE2 scalar instructions, and
jump relaxation to the 2-byte form where the target is near), then lays
out `.text`, `.data` and `.rdata`, the relocations (`REL32` for calls,
`lea` and RIP-relative loads, `ADDR64` for vtable entries), the symbol
table (functions, globals, vtables, string and float constants, externs)
and the string table. Lines it cannot encode are reported with their
line number and the file is not written. Test 8 links the object with
the MSVC-compiled driver at `-O0` and `-O3`, runs it (all 19 checks
pass) and compares `dumpbin /disasm` of cppparse's object with ml64's:
the instruction streams are identical.

Known limit: when cppparse compiles its own 51 sources, 23 of about
290,000 generated lines mix an XMM register with an integer operand.
They come from calls to library functions that the front end has no
declaration for (`strtod`, `std::string::size`, ...): their results carry
no type, so the register allocator and the instruction emitter disagree
about the register class. ml64 rejects those lines as well; the COFF
writer reports them by line number. Real declarations for library
functions are part of the version 6 front end.
