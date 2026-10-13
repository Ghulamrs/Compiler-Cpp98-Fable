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
- 5 (plan 8): empty bases laid out as MSVC does: no space for an empty base, a padding byte between
  zero-sized subobjects, an empty base at the end on the last byte (Sema.cpp layoutClass); test 13 runs
  examples/sample-layout against a cl-compiled driver at -O0..-O3.
- 23 (plan 9): bit-fields laid out as MSVC does (units of the declared type's size, zero-width and unnamed
  bit-fields) and read and written by mask and shift (IrGen loadBits/storeBits); members of anonymous unions
  and structs are named in the enclosing class (Sema declareAnonymousMember). Covered by sample-layout.
- 23 (plan 10): the polymorphic bases are laid out first; the first one is the primary base at offset 0, whose
  vptr the class shares and whose vtable it extends (Sema.cpp layoutClass, completeClass). Covered by
  sample-layout, with virtual calls made by cl on objects built by cppparse and the other way round.
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
- 21, constant tables: a global or static local initialised by constants, string literals and addresses of
  globals and functions (arrays, plain aggregates, pointers) is static data with relocations, in .rdata when
  const, as cl emits it; classes with bit-fields stay on the dynamic path (IrGen.cpp, X86.cpp).
- README "known limit" (untyped library results): with the headers included the version 6 front end types
  `strtod` and `std::string::size`; the paragraph describes the legacy self-compile. Rewritten: the README now
  describes the version 6 behaviour (typed results, S103 and no object without a declaration); test 17 checks both.
- 4 (plan 1): a unit with syntax or semantic errors is not compiled: no object, `.asm` or map (stale ones removed),
  exit code 1 (main.cpp); test 17, `examples/sample-front/errors.cpp`.
- 3 (plan 3): `sizeof(T&)` is `sizeof(T)`; sizeof of a declared-only class, of a class inside its own members, of
  `void` or a function type is S113 (SemaExpr.cpp completeForSizeof); test 17, `sample-front/sizeof-incomplete.cpp`.
- plan 4: variables, parameters and functions are recorded per parser scope; template-name lookup stops at the
  innermost scope that declares the name (CxxParser.cpp isTemplateName); test 17, `sample-front/front.cpp`.
- plan 5: overload candidates are deduplicated by entity before resolution (SemaExpr.cpp resolveOverload), so
  `using namespace std; printf(...)` is no longer ambiguous; test 17, `sample-front/front.cpp`.
- plan 11a: the injected class name with template arguments names that specialisation, as a type and as a
  qualifier (Sema.cpp injectedTemplate); test 17, `sample-front/front.cpp`.
- plan 11b: a function template declared and then defined is one symbol, matched by its written signature; the
  definition keeps the declaration's default arguments (Sema.cpp declareTemplate); test 17, `sample-front/front.cpp`.
- plan 11c: deduction against a template-id in a member template evaluates the arguments that are not its own
  parameters where the template is (the enclosing instantiation's bound R, a literal) and compares them (SemaExpr.cpp
  deduce). MatrixTest.cpp now compiles and prints what cl's build prints at -O0 and -O2 (test 17).
- calls through a function-pointer data member (`e.scale(x)`, `p->scale(x)`, `e.shared(x)` for a static one) called
  an external symbol named after the member: the callee now carries the member's type (SemaExpr.cpp exprCall), so
  the call is indirect through its value; parentheses around the callee are dropped (`(e.scale)(x)`), and `(*fp)(x)`
  and `(*f)(x)` call through the pointer (IrGen.cpp call); test 17, `sample-front/front.cpp`.
- a data member of class type called as a function (`h.f(1)`, `p->f(1)`, `(h.f)(1)`, inherited from a base) called
  operator() with the enclosing object as `this`: it now gets the member's address, converted to operator()'s class
  (IrGen.cpp call); test 17, `sample-front/front.cpp`.
- mapcompare.ps1 (was deferred): cl's functions come from its objects (`dumpbin /symbols`), not from link's map, so
  methods inlined everywhere are listed as "removed by link" in part B (`Matrix::rows()` 3 bytes and `cols()` 4 in
  both compilers), and a part C compares whole classes (`mx::Matrix`: 1.66 times cl's bytes over all its members,
  1.85 over the 17 both emit; cl's link keeps 5 of its 19, ours 13 of 18).
## Plan

Three lanes, worked in parallel on their own branches; each item gets a regression test, and each fix runs
the full suite plus matrix, lib, life, selfhost, mix, cprog and mapcompare before its commit.

| # | Item | Effect today | Size | Lane |
|---|---|---|---|---|
| 14 | 23: virtual bases laid out inline, no vbptr | wrong values | large | layout (later) |
| 15 | 22: `_MSC_VER`, `_MT`, `_CPPUNWIND` not predefined (common/Target.cpp) | by choice so far | small | decision |
| 16 | a second polymorphic base (`D : PA, PB`) gets no secondary vtable: D's override of a PB function is not reached through a PB* (needs a vtable per polymorphic base subobject and this-adjusting thunks) | wrong call | medium | layout |

## Deferred

- 15 (MSVC ABI parity), only needed for mixed links with cl objects: classes of 1/2/4/8 bytes passed and
  returned in registers (IrGen.cpp:658, 671, 1930); vtable slots grouped by overload as MSVC does
  (Sema.cpp:1169-1174); the destructor slot holding a scalar deleting destructor.
- design-table-exceptions.md section 7: an exception edge from every call to its landing pad, then GVN and
  register values across handlers; the main lever on code size (1.57 times cl's on the matrix sample).
