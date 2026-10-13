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
- 16 (plan 16): a second polymorphic base gets a secondary vftable (`??_7D@@6BPB@@@`), with MSVC's this adjustors
  and adjustor thunks (`?f@D@@WBA@EAAHXZ`) (Sema.cpp completeClass, IrGen.cpp storeVptr/callFunction/prologue,
  X86.cpp addThunk); calls through every base, by both compilers, are in sample-layout.
- 15, destructor slot: a virtual destructor's slot holds the scalar deleting destructor (`??_G`, IrGen.cpp
  genDeletingDestructors); `delete` calls it with flags 1, so a secondary base pointer frees the complete object.
  It frees through the class-specific `operator delete` lookup finds in the complete class, with its size, and so
  does a non-virtual `delete` (IrGen.cpp freeObject; review repro u.cpp, KD : SA, K).
- A destructor stores its own class's vptrs on entry (IrGen.cpp genFunction), so a virtual call in a base's
  destructor reaches the base's function; sample-layout checks it in constructors and destructors, both compilers.
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

## Plan

Three lanes, worked in parallel on their own branches; each item gets a regression test, and each fix runs
the full suite plus matrix, lib, life, selfhost, mix, cprog and mapcompare before its commit.

| # | Item | Effect today | Size | Lane |
|---|---|---|---|---|
| 14 | 23: virtual bases. `--compile6` refuses a class with one (S130, driver/main.cpp) instead of laying it out inline; left: the vbptr and vbtable (`??_8D@@7B@`), virtual base subobjects placed once at the end of the most-derived object (shared in a diamond), conversions and member access through the vbtable, the constructors' hidden "most derived" flag (virtual bases built once), destruction order, vtordisp where MSVC emits it | refused (S130) | large | layout (later) |
| 15 | 22: `_MSC_VER`, `_MT`, `_CPPUNWIND` not predefined (common/Target.cpp) | by choice so far | small | decision |
| 17 | `delete[]` of an array of a polymorphic class across compilers: cl calls the vector deleting destructor (`??_E`, flags bit 1) through the slot, which holds cppparse's scalar one | wrong free in mixed links | small | layout |
| 20 | vftable names: two tables introduced by the same class are now named after the bases that tell them apart (`??_7G@@6BQ1@@@`, `??_7G@@6BQ2@@@`, IrGen.cpp registerMetadata); left: cl's full rule (clang's MicrosoftVTableContext MangledPath), e.g. `struct F : X, D` (D : A, B, C) where cl names one table `??_7F@@6B@` and cppparse `??_7F@@6BX@@@` (review repro t2.cpp); paths longer than one class | wrong names in mixed links (both COMDATs kept) | small | layout |
| 18 | member pointers to virtual functions (cl's vcall thunks `??_9`) | not supported | medium | layout |
| 19 | covariant return through a secondary base (`CD* CD::self()` over `SB* SB::self()`, SB at 16): needs cl's return-adjusting thunk and its slot layout. `--compile6` refuses it (S131, driver/main.cpp, sample-front/covariant.cpp; review repro u.cpp); only the first overridden function is checked | refused (S131) | medium | layout |

## Deferred

- 15 (MSVC ABI parity), only needed for mixed links with cl objects: classes of 1/2/4/8 bytes passed and
  returned in registers (IrGen.cpp:658, 671, 1930); vtable slots grouped by overload as MSVC does
  (Sema.cpp:1169-1174).
- design-table-exceptions.md section 7: an exception edge from every call to its landing pad, then GVN and
  register values across handlers; the main lever on code size (1.57 times cl's on the matrix sample).
- mapcompare.ps1: take cl's functions from its objects, not from link's map, so methods inlined everywhere
  (`Matrix::rows()`, 3 bytes in both compilers) and whole classes can be compared.
