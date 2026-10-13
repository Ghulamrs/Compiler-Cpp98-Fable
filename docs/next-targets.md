# Next targets (2026-10-13)

The open items of [review-2026-10-08.md](review-2026-10-08.md), re-checked on 0ed10be (table-based exceptions
the default), with the items found since. Each was reproduced with a small program built by cppparse
(`--compile6 -O0` and `-O2`, `--msvc-mangling`) and by cl /O2 as the reference.

## Fixed since the review

- 17: a throw inside a catch handler. Every handler body is a try region whose landing pad calls
  `cppparse_catch_unwind` (IrGen.cpp, runtime); correct in table and sjlj mode at -O0 and -O2.
- 18, call side: float arguments of a varargs call are copied into the integer registers (X86.cpp).
- 19, compares and negation: floats are widened to double first; exact, only slower than cl.
- 20, mostly: file-scope `static` functions and variables and the anonymous namespace are internal (the
  namespace carries a per-unit id); an out-of-class `inline` definition is `f i`.
- 22, partly: a missing include is fatal; an undeclared function is error S103.
- 23, partly: a class used before its layout is done and array fields of class type are laid out as cl does.
- Lazy instantiation of class template member bodies (MatrixTest.cpp's first blocker).
- 19, stack-passed float parameters: a `float` after the fourth parameter is homed with movss into its 4-byte
  slot, and the copy goes through xmm5, not xmm0 (which still held the first parameter at -O2) (X86.cpp).
- 21, destructors of globals: after a global (or a static local) with a destructor is built, an internal thunk
  that destroys it is registered with atexit, as cl does, so they run after main newest first (IrGen.cpp).
- 18, callee side: a variadic function keeps its parameters in their home slots and its prologue spills rcx,
  rdx, r8 and r9 there (named floats from xmm0-xmm3); `include/cstdarg`, `stdarg.h` and `vprintf`/`vfprintf`/
  `vsprintf` in `cstdio` give va_list/va_start/va_arg/va_end over that area, as MSVC's vadefs.h (X86.cpp).
- 20, static locals: cl's names (`?n@?1??counterA@@YAHXZ@4HA`, MangleSema.cpp) and Static linkage, External in
  one COMDAT only for an inline function, so cl and cppparse copies share the variable. A static in a nested
  block gets a unique block number that is not always cl's.
- README "known limit" (untyped library results): with the headers included the version 6 front end types
  `strtod` and `std::string::size`; the paragraph describes the legacy self-compile.

## Plan

Three lanes, worked in parallel on their own branches; each item gets a regression test, and each fix runs
the full suite plus matrix, lib, life, selfhost, mix, cprog and mapcompare before its commit.

| # | Item | Effect today | Size | Lane |
|---|---|---|---|---|
| 1 | 4: object written after errors (driver/main.cpp:313) | broken objects reach the linker | small | front end |
| 3 | 3: sizeof of an incomplete type accepted, sizeof(T&) = 8 (SemaExpr.cpp:1256) | silent 8 | small | front end |
| 4 | a local variable does not hide a function template (CxxParser.cpp:211-217) | wrong result | small-medium | front end |
| 5 | `using namespace std; printf(...)` ambiguous: candidates not deduplicated (SemaExpr.cpp:390-444) | false error | small | front end |
| 8 | 5: empty base takes a byte (Sema.cpp:1076-1088) | wrong size | small | layout |
| 9 | 23: bit-fields ignored; anonymous union/struct members cannot be named | wrong size, false error | medium | layout |
| 10 | 23: polymorphic base not first: own vptr, not moved first (Sema.cpp:1080, 1093, 1155) | wrong layout | medium | layout |
| 11 | MatrixTest.cpp: injected class name with arguments (Sema.cpp:368-379); a redeclared function template is a second candidate (Sema.cpp:1033); member templates do not see the enclosing parameters (SemaExpr.cpp:544-572) | does not compile | small-medium | front end |
| 12 | 21: constant pointer tables initialised dynamically in .data, not .rdata with relocations | start-up order | medium | back end |
| 14 | 23: virtual bases laid out inline, no vbptr | wrong values | large | layout (later) |
| 15 | 22: `_MSC_VER`, `_MT`, `_CPPUNWIND` not predefined (common/Target.cpp) | by choice so far | small | decision |

## Deferred

- 15 (MSVC ABI parity), only needed for mixed links with cl objects: classes of 1/2/4/8 bytes passed and
  returned in registers (IrGen.cpp:658, 671, 1930); vtable slots grouped by overload as MSVC does
  (Sema.cpp:1169-1174); the destructor slot holding a scalar deleting destructor.
- design-table-exceptions.md section 7: an exception edge from every call to its landing pad, then GVN and
  register values across handlers; the main lever on code size (1.57 times cl's on the matrix sample).
- mapcompare.ps1: take cl's functions from its objects, not from link's map, so methods inlined everywhere
  (`Matrix::rows()`, 3 bytes in both compilers) and whole classes can be compared.
