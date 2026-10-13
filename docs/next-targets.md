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
  block gets cl's block number (IrGen::numberStaticBlocks).
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
| 14 | 23: virtual bases. `--compile6` refuses a class with one (S130, driver/main.cpp) instead of laying it out inline; left: the vbptr and vbtable (`??_8D@@7B@`), virtual base subobjects placed once at the end of the most-derived object (shared in a diamond), conversions and member access through the vbtable, the constructors' hidden "most derived" flag (virtual bases built once), destruction order, vtordisp where MSVC emits it | refused (S130) | large | layout (later) |
| 15 | 22: `_MSC_VER`, `_MT`, `_CPPUNWIND` not predefined (common/Target.cpp) | by choice so far | small | decision |
| 17 | `delete[]` of an array of a polymorphic class across compilers: cl calls the vector deleting destructor (`??_E`, flags bit 1) through the slot, which holds cppparse's scalar one | wrong free in mixed links | small | layout |
| 20 | vftable names: two tables introduced by the same class are now named after the bases that tell them apart (`??_7G@@6BQ1@@@`, `??_7G@@6BQ2@@@`, IrGen.cpp registerMetadata); left: cl's full rule (clang's MicrosoftVTableContext MangledPath), e.g. `struct F : X, D` (D : A, B, C) where cl names one table `??_7F@@6B@` and cppparse `??_7F@@6BX@@@` (review repro t2.cpp); paths longer than one class | wrong names in mixed links (both COMDATs kept) | small | layout |
| 18 | member pointers to virtual functions (cl's vcall thunks `??_9`) | not supported | medium | layout |
| 19 | covariant return through a secondary base (`CD* CD::self()` over `SB* SB::self()`, SB at 16): needs cl's return-adjusting thunk and its slot layout. `--compile6` refuses it (S131, driver/main.cpp, sample-front/covariant.cpp; review repro u.cpp); only the first overridden function is checked | refused (S131) | medium | layout |

## Deferred

- 15 (MSVC ABI parity), only needed for mixed links with cl objects: classes of 1/2/4/8 bytes passed and
  returned in registers is done (IrGen::regParamType/regReturnType, sample-abi in Test 13); static locals
  guarded with cl's thread-safe-statics protocol and names is done (sample-statics in Test 13, with
  _Init_thread_abort for a throwing initializer); class results through pointers to functions,
  `(*fp)(x)`, variadic class arguments and cl's own-members rule for results in rax are done (sample-abi);
  a static local cl initialises as a constant (`static int u = int();`, `static double d = 1.0/4;`,
  `static float f = 1.5f*2;`, value-initialisation, casts) is static data with cl's folded bytes and no
  guard (IrGen::foldArith/convertFolded; the bits go out as MASM's encoded reals), so the `$TSS<n>` after
  it match cl's (konstK in sample-statics, cl inlining it); statics in nested blocks get cl's block
  numbers (IrGen::numberStaticBlocks: the body 2, then in source order each compound statement, if and
  each of its branches, for/while/do/switch; checked against cl for blocks, branches, else-if chains,
  loops, switch, try/catch, labels and conditions with declarations; nestedN in sample-statics); open:
  statics inside the member functions of a local class are not checked against cl; exceptions do not cross between cl's
  and cppparse's runtimes (a cl throw into a cppparse frame fails fast); vtable slots grouped by overload as MSVC does
  (Sema.cpp:1169-1174) (the destructor slot holding the scalar deleting destructor is done).
- design-table-exceptions.md section 7: an exception edge from every call to its landing pad, then GVN and
  register values across handlers; the main lever on code size (1.57 times cl's on the matrix sample).

## Other known limits (found by the reviews of 2026-10-13)

- A destructor that throws (allowed in C++98): in cppparse's own code a throw out of a destructor reached by
  `delete` through a base pointer skips the base destructors, and one from a stack object's destructor is not
  caught at all ("unhandled exception of type int"). Repro: review-v w.cpp.
- Calls through pointers to members (`(obj.*pmf)(x)`, `(p->*pmf)(x)`) are rejected by sema (an error, not wrong code).
- A float literal with an `f` suffix is converted decimal -> double -> float (IrGen::foldArith and the
  assembler's REAL4), which can differ from cl's direct decimal -> float rounding in rare halfway cases: a
  decimal just above a midpoint between two floats whose double is the midpoint itself. Repro (checked):
  `float* hf() { static float h = 1.000000059604644775390625000000001f; return &h; }` (1 + 2^-24 plus a
  little): cl stores 0x3F800001, cppparse rounds the double's tie to even, 0x3F800000.

## Code size against cl /O2 (analysis of the matrix sample, 2026-10-13)

mapcompare part C: the ten largest-ratio Matrix members are 1788 bytes against cl's 611 (2.93); the whole
class 1.85, all 95 functions both compilers emit 1.64. The causes, by extra bytes across the sample:

| | Cause | Bytes | Where | Size / risk |
|---|---|---|---|---|
| A | exception code in the body: constructed flags, destructors expanded in every landing pad, pads where nothing after construction can throw | ~1700 | IrGen registerCleanup (table states instead of flags, design-table-exceptions.md section 7); Optimizer inlineCalls (call `??1` in pads instead of inlining) | large / medium |
| B | register allocator: float pool only callee-saved xmm6-13, rax/rcx/rdx not in the pool, intervals ending at a call treated as crossing it, spills not weighted by loop depth, no memory operands, copies not coalesced | ~1100 | X86.cpp allocate, Lower.cpp | medium / medium |
| C | `throw std::invalid_argument("...")` builds a heap std::string temporary; cl uses the `const char*` constructor and `_CxxThrowException` (35 bytes) | ~600 | include/stdexcept, runtime helper | small / low |
| D | instruction selection: base+index*8 addressing, `add r,8` pointer steps, `xorps` for 0.0, setcc and jump threading for bools, redundant movsxd (widen int counters), FP-to-integer copies only for variadic callees, hot slots within rbp-128 | ~400 | X86.cpp, Ssa/Optimizer | small each / low |
| E | no NRVO and no elision of a copy from a returned temporary (transpose, operator*, `Matrix p = a*b`) | ~320 | IrGen (return slot, destination address) | medium / medium |
| F | inlining thresholds counted on unoptimised IR (sameSize 26 instructions for 20 bytes) | ~150 | Optimizer.cpp:1039 | small / low |
| G | rbp frame in leaf functions, no tail calls (`~Matrix`: cl `add rcx,8; jmp _Tidy`) | ~100 | X86.cpp | small / low |
| H | `fabs` called instead of `andpd`, `std::max(const&)` kept through memory after inlining | ~70 | Optimizer | small / low |

Order: C and F (small, low risk), then B, then A, then D, E, G, H. Expected Matrix members ratio: about 1.65
after A+B+C, about 1.15-1.3 after A-F; `~Matrix` needs G and `index`/`sameSize` need D to reach 1.25; the
free functions depend mostly on A. Note: cl moves cleanup and catch code into `dtor$`/`catch$` symbols, which
mapcompare does not add to the function (mtxSingularThrows is 465 bytes with them, not 207).
