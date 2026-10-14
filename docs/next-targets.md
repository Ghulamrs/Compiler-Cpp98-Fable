# Next targets (2026-10-13)

The open items of [review-2026-10-08.md](review-2026-10-08.md), re-checked on 0ed10be (table-based exceptions
the default), with the items found since. Each was reproduced with a small program built by cppparse
(`--compile6 -O0` and `-O2`, `--msvc-mangling`) and by cl /O2 as the reference.

## Fixed since the review

- 17: a throw inside a catch handler. Every handler body is a try region whose landing pad calls
  `cppparse_catch_unwind` (IrGen.cpp, runtime); correct in table and sjlj mode at -O0 and -O2.
- 18, call side: float arguments of a varargs call are copied into the integer registers (X86.cpp).
- 19, compares and negation: negation widens a float to double first (exact, only slower than cl); compares are
  `ucomiss` on floats since 2026-10-13 (see the floating-point entry below).
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
- Destructors that throw (C++98; cl with `/Zc:implicitNoexcept-` is the reference): a destructor's members and
  bases are cleanups of its body (IrGen::destructorParts), so `delete (WB*)new WD` with a throwing `~WD` runs ~WD,
  ~WB, ~WA and the int reaches the handler, and a stack object's throwing destructor is caught; a cleanup's flag is
  cleared before its destructor runs (runCleanup), so it is not destroyed twice; `delete` does not free when the
  destructor throws, as cl. An exception leaving a destructor run by a landing pad calls terminate (handlerCleanups:
  the pad's destructors are a region whose handler calls `cppparse_terminate`, runtime). eh.ps1: ehDtorThrow and
  terminate.cpp, table and sjlj, -O0..-O3.
- 22: an element destructor that throws in `delete[]` (`??_E` and the non-virtual path) or at the end of an array's
  scope: the elements before it are destroyed, newest first, then the exception goes on (cl: 54321, no free); `new
  T[n]` whose k-th constructor throws destroys the k built elements and frees through the matching `operator
  delete[]` (forEachElement's cleanup of the elements [0, index), registerFree). eh.ps1: ehArrays.
- 21, the throwing constructor: `new T` frees through the class's `operator delete` (or the global one, `::new`
  the global one) when the constructor or initializer throws; placement new frees nothing (registerFree). ehArrays.
- 14c, catch by value: `catch (D x)` copy-constructs the handler's object from the thrown one and destroys it when
  the handler ends (also on return, break, continue and a throw out of the handler). eh.ps1: ehCatchValue. A copy
  constructor that throws while it initializes the parameter calls terminate, as cl (terminate-catch.cpp).
- Review of the exception commits: an exception leaving a by-value handler destroys the handler's objects (its
  parameter, then the body's) in the handler's landing pad, before the handled exception is released, as cl; a
  handler for a base that is not the first gets the base subobject (the throw descriptor carries each base's
  offset, `B@16`, which cppparse_match/catch_begin apply); `new P[n]` of a class without destructor and with a
  sized `operator delete[]` frees through it with count * size. Refused with S132 (sample-front/goto-handler.cpp,
  new-refused.cpp): a goto out of a catch handler, a placement new of a class with its own operator new, new and
  delete[] of a multidimensional array of a class with its own operator new[] or delete[]. eh.ps1: ehNoCookie,
  ehHandlerOrder, ehCatchBase, terminate-handler.cpp.
- S3, S4, S5 (lane A, 2026-10-14): a thrown pointer is caught as a pointer to a public unambiguous base (adjusted
  unless null), with more cv-qualifiers on the pointee and as `void*`; a class or pointer is no longer caught as a
  private or ambiguous base (cl does not either); `throw "x"` is caught by `char*` as with cl; throwing a pointer to
  a class with virtual bases is refused (S132: the offset depends on the dynamic type). New- and delete-expressions
  call the global operators by cl's names with cl's sizes, so replacements anywhere are called (cl's default
  `operator new` throws `std::bad_alloc` through cl's exception handling, which cppparse frames do not take part
  in). `delete[]` without a cookie calls the class's `operator delete[]`. eh.ps1 ehCatchPointer, ehGlobalFree;
  test 13 sample-layout TR/TQ/TS and the replaced global operators.
- 16 (plan 16): a second polymorphic base gets a secondary vftable (`??_7D@@6BPB@@@`), with MSVC's this adjustors
  and adjustor thunks (`?f@D@@WBA@EAAHXZ`) (Sema.cpp completeClass, IrGen.cpp storeVptr/callFunction/prologue,
  X86.cpp addThunk); calls through every base, by both compilers, are in sample-layout.
- 15, destructor slot: a virtual destructor's slot holds the scalar deleting destructor (`??_G`, IrGen.cpp
  genDeletingDestructors); `delete` calls it with flags 1, so a secondary base pointer frees the complete object.
  It frees through the class-specific `operator delete` lookup finds in the complete class, with its size, and so
  does a non-virtual `delete` (IrGen.cpp freeObject; review repro u.cpp, KD : SA, K).
- A destructor stores its own class's vptrs on entry (IrGen.cpp genFunction), so a virtual call in a base's
  destructor reaches the base's function; sample-layout checks it in constructors and destructors, both compilers.
- 15, vtable slots grouped by overload: a class's new virtual functions are ordered as cl orders them, by
  declaration but with the overloads of a name together where the name is first declared in the class (an
  overrider counts), the last declared first (Sema.cpp completeClass); sample-layout OV/OW and cl's CW : OW, called
  through base pointers by both compilers at -O0..-O3.
- 17, `delete[]` of a polymorphic class across compilers: the destructor slot holds cl's vector deleting destructor
  (`??_E`, COMDAT; flags bit 1: the count from the 8-byte cookie, the elements destroyed last first, freed from the
  cookie through the class's `operator delete[]` with count * size + 8, or the global one), and `delete[]` calls it
  with flags 3, an empty array freed directly, as cl (IrGen.cpp genDeletingDestructors, freeArray, Delete);
  sample-layout AV/AW arrays allocated by each compiler and deleted by the other, empty arrays too.
- 21, class-specific allocation: a new-expression calls the class's own (or inherited) `operator new` / `new[]`
  with cl's size (count * size + 8 with a cookie), and `delete[]` of a class with a cookie frees through its
  `operator delete[]` (count * size + 8); `::new` and `::delete` bypass them (CxxParser.cpp unary,
  Expr::globalScope, IrGen.cpp New/Delete); sample-layout TN/TM, allocated by each compiler and deleted by the other.
- Review of the slot and `??_E` work: the overload grouping counts a non-virtual declaration of the name (ZG); an
  override of a function that a middle class hides is found in the bases' tables, not by name lookup (YC::f(int)
  over YA::f(int) past YB::f(double); before, YC got a new slot and `YA*` calls ran YA::f) (Sema.cpp
  completeClass); a class declaring both `operator delete[](void*)` and the sized form uses the first, as cl, and
  likewise `operator delete` (IrGen.cpp classOperatorDelete); all in sample-layout, both compilers.
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
- 22, predefined macros (decided 2026-10-13): the Windows target defines `_MT=1` and `_CPPUNWIND=1`, as cl does for a
  `/MT /EHsc` build (common/Target.cpp); `_MSC_VER` stays undefined, so code under it, written for MSVC's extensions,
  is not compiled; test 17 (`sample-front/front.cpp`) checks the macros against cl.
- Table model, -O2/-O3: a catch inside a loop crashed or lost a value (review repro: `for (k...) { bad = k; try {
  printf("a%d ", 1); thr(1); printf("b%d ", 2); thr(2); } catch (int e) { printf("caught %d\n", e); } }`, access
  violation on the third pass). The runtime was right (`RtlVirtualUnwind` gives each frame its callee-saved
  registers back); the register allocator was not: its liveness had no exception edges, so the inlined throw laid
  out after the loop took rbx, which held the hoisted `"a%d "` that the code after the landing pad still needed.
  The back end now adds an edge from every block whose calls or throws may reach a table landing pad to the
  handlers of that region and of the regions around it (X86.cpp computeEhEdges; liveness and loop extension in
  computeIntervals; Ir.cpp computeLiveness takes the extra edges). eh.ps1: ehCatchLoop, ehNestedLoops
  (probe.cpp), failing at -O2/-O3 before.
- Floating point against cl (review of 2026-10-13, test 17 `fp::` in `sample-front/front.cpp`, -O0 and -O2, also
  checked at -O1 and -O3): double <-> long double was `cvtsd2ss` (IrGen::convert: no conversion now, float <-> long
  double as float <-> double); float compares and conversions to bool used `ucomisd` on a float register and ignored
  PF (`(bool)-0.0f` true, `(bool)NaN` false, `NaN == NaN` true): `ucomiss`/`ucomisd` by the operands' type, and
  ==/!= test PF in the setcc (a `jp` around it) and in the fused branch (X86.cpp emitInst Cmp/CondBr, invertCc);
  `x + 0.0` was folded to `x` (-0.0 gave -0.0): only x*1, x/1, x-(+0.0), x+(-0.0) are kept (Optimizer::algebraic);
  a constant's text, the CSE/GVN key, had 6 digits (`x+0.1` and `x+0.1000001` were one value) and the phi
  simplification took -0.0 for 0.0 (IrOperand::str, Ssa phi); `float op float` was a double (TypeTable::
  arithmeticResult promoted float, and unary -/+ did too: `sizeof(-f)` 8): it is a float computed with addss etc.,
  `-f` is negated through a double (the back end's sign mask is a double's), and folded float operations, float
  operands and conversions are rounded to float (Optimizer::constantFold).

## Silent differences found by the reviews of 2026-10-14

Two reviews re-checked the review fixes of 2026-10-13 and the gap list on d9309fa, each case built by cppparse
(-O0, -O2, table and sjlj) and by cl (/Od, /O2, `/Zc:implicitNoexcept-` for throwing destructors). The review
fixes themselves held (catch of a non-first base, handler order, no-cookie frees, the S132 refusals, the
virtual-base fixes, C2584/C4584). These are wrong results with no diagnostic, none recorded before:

| # | Case | Repro | cl | cppparse | Where |
|---|---|---|---|---|---|
| S1 | value-initialisation `T()` of a class without a user constructor | `struct Q { int* f; int x; }; Q q = Q();` (also `Q q((Q()))`, `return Q();`, `q = Q();`, `new Q()`) | all zero | stack garbage at -O0 and -O2 | IrGen materialize/construct: the implicit default constructor is called, nothing zeroes |
| S2 | a `float` argument of a variadic function | `float f = third(); printf("%.9g", f);` | 0.333333343 | 5.19e-315 at -O0; 0.333333312 at -O2 (folded through a double inexactly) | SemaExpr call: no default argument promotion for `...` (float -> double, char/short/bool -> int) |
| S3 | `catch (B*)` of a thrown `D*` | `struct XP { int a; }; struct HB { int b; }; struct MB : XP, HB {}; MB m; try { throw &m; } catch (HB* p) {}` | handler taken, `p` adjusted to the HB subobject | was `catch (...)`, or terminate; fixed (lane A): the same | fixed: IrGen throwDescriptorFor/catchableBases list a pointer's public unambiguous bases (`HB*+4`) and `void*`, runtime cppparse_match/catch_begin check the pointee's cv-qualifiers and adjust a non-null pointer in a slot behind the copy; sample-eh ehCatchPointer |
| S4 | replaced global `operator new` / `delete` / `new[]` / `delete[]` | `void* operator new(size_t n) { ++g; return malloc(n); }`, then `new int(5)` | called | was ignored (`__new`/`__delete` went to malloc/free); fixed (lane A): called | fixed: X86 runtimeName gives cl's names (`??2@YAPEAX_K@Z`, `??_U@YAPEAX_K@Z`, sized `??3@YAXPEAX_K@Z` always, `??_V@YAXPEAX_K@Z` with a cookie and on throwing-constructor paths, `??_V@YAXPEAX@Z` without), a definition of one gets that name in either mangling (replacedAllocationName); IrGen freeGlobal/freeArray pass cl's sizes; exception objects stay malloc (`__malloc`); sample-layout (replacements in the cl driver and in layout.cpp), sample-eh ehGlobalFree |
| S5 | `delete[]` of a class without cookie and with its own `operator delete[]` | `struct R { static void operator delete[](void* p) { log(); free(p); } }; delete[] new R[1];` (also the sized form) | R's operator (the sized form gets one element's size: the count is unknown) | was the global one; fixed (lane A): R's | fixed: IrGen Delete calls freeArray without a cookie (count unknown); sample-layout TR/TQ/TS, deleted by each compiler |
| S6 | multicharacter constants | `'ab'`, `'abcd'` | 24930, 1633837924 | 97, 97 | SemaExpr CharLit |
| S7 | a hex escape before a concatenated literal | `"\x41" "b"` | "Ab", strlen 2 | one char 0x1B, strlen 1 | CxxParser joins the texts before the escapes are decoded |
| S8 | out-of-range floating constants in static initialisers (undefined, cl keeps a value) | `static double v = (unsigned char)300.0 + 0.5;`, `static int n = (int)-2147483649.0;` | 300.5, 2147483647 | 44.5, -2147483648 | IrGen foldArith / Cast: only a bare literal folds, the rest runs cvttsd2si |
| S9 | literal casts with a non-literal constant operand | `(int)-2147483649.0`, `(int)+3e9`, `(int)(2.0 * 1.5e9)`, `(int)(float)1e10`, `const double cd = 3e9; (int)cd` | folded modulo 2^N (7fffffff, b2d05e00, ...) | 80000000h | IrGen value, Cast (`lit->kind == FloatLit` only) |
| S10 | member-pointer size of a class completed later in the unit | `struct Inc; int a = sizeof(int Inc::*); struct Inc { int x; };` | 12 (24 for a function) | 4 (8) | TypeTable::sizeOf decides when the IR is built, not at the point of use |
| S11 | a namespace-scope object of a template with a member-pointer field | `template <class T> struct H { int T::* f; }; H<P> gh;` | f = -1 | 0, not refused | driver/main.cpp: the Variable check looks at the variable's own type only |
| S12 | the halfway float literal (was filed as mixed links only) | `1.00000005960464477550f` as a global, local or static | 0x3F800001 | 0x3F800000 | decimal -> double -> float; see "Other known limits" |

Also found, errors on valid code (not wrong code): `int a[(int)(unsigned char)300.0 + 1];` is S114 (a floating
literal cast to an integral type is an integral constant expression); an explicit template argument on a call
`run<PS>(3)` is S107; `n(sv)` with `S::operator int()` and overloads `n(int)`, `n(long)` is S106 ambiguous (cl
picks `n(int)`). Code size at d9309fa (mapcompare's counting on the matrix sample, -O2): the 17 Matrix members
both emit 1.79 of cl's bytes, the whole class 1.60, all 95 common functions 1.55, the ten largest 3.02.

## Plan

Three lanes, worked in parallel on their own branches; each item gets a regression test, and each fix runs
the full suite plus matrix, lib, life, selfhost, mix, cprog and mapcompare before its commit.

| # | Item | Effect today | Size | Lane |
|---|---|---|---|---|
| 14 | 23: virtual bases. Sema now lays them out as cl (Sema::layoutClass, layoutVbtables: own or shared vbptr injected as cl does, virtual bases once after nvSize in cl's order, vbtable entries and names; test 17 checks `sample-front/vbase-layout.cpp` against `/d1reportAllClassLayout`); `--compile6` compiles non-polymorphic ones (vbtable data, conversions through the vbptr, the "most derived" flag as the constructors' last parameter, `??1` plus the vbase destructor `??_D`; IrGen::toBase, storeVbptrs, vbaseDestructor; nested virtual bases with their vbptrs in the virtual part; test 13 sample-layout across both compilers). Still refused (S130, driver/main.cpp virtualBaseRefusal): polymorphic classes with virtual bases (the vfptr inside the virtual base, a class's own vfptr only for new virtuals, overrides through adjustor thunks, vtordisp and `$4` thunks), empty or over-aligned virtual bases (cl's padding), `#pragma pack`, vbtable names with paths of several bases | refused (S130) for those | large | layout (later) |
| 14b | polymorphic virtual bases, what row 14 leaves refused (S130, driver/main.cpp virtualBaseRefusal, sample-front/vbase.cpp): Sema::layoutClass still gives a class a vfptr of its own for a polymorphic virtual base (cl: only for new virtual functions; the vfptr stays in the virtual base subobject) and completeClass takes a virtual base as primary base; left: vftables of the virtual bases at their places (`??_7D@@6BV@@@`), overrides reached through them with adjustor thunks computed from the virtual base's offset, vtordisp fields (4 bytes before a virtual base whose virtual function a class with a user constructor or destructor overrides) and the `$4` vtordisp thunks, the vptr stores in constructors and destructors for the virtual part, the vbtable-relative this of the vbase destructor `??_D` for a virtual `~D`; also empty or over-aligned virtual bases (cl's 4-byte padding), `#pragma pack`, and vbtable names with paths of several bases | refused (S130) | large | layout |
| 14c | review of the virtual-base commits: now refused with S132 (IrGen::unsupported, driver/main.cpp; sample-front/vbase-refused.cpp): `catch` of a class used as a virtual base anywhere in the unit (the handler would need the thrown object's vbptr), `.*`/`->*` on a class with virtual bases, a cast from a virtual base down (cl C2635); toBase reports an internal error instead of returning the address. Fixed: `throw` of a class with virtual bases registers `??_D`; `V v = D()` no longer builds D into V's storage, and a class prvalue bound to a base reference or sliced is evaluated once. Diagnosed as cl (lane C, Sema::declareClass, sample-front ambiguous-base.cpp): a direct base that is also a base of another direct base, S141 error where cl gives C2584 (`struct F : virtual V, W {}`, `struct H2 : V, WV {}`), S142 warning where cl gives C4584 (`struct G2 : V, W {}`, `struct H3 : WV, V {}`), nothing for `struct F2 : W, virtual V {}`. Ambiguous conversions to a base (C2594: S143 for casts, S105 for implicit conversions) and unqualified access to a member of a base held more than once (C2385: S144) are errors (Sema::baseSubobjects, sample-front ambiguous.cpp), and `d.C::a` / `d.C::f()` go through the named base (IrGen namingBase; they took the first path). Left: a name found in a derived class on one path and in its base on another (`struct B : A { int a; }; struct D : B, C {}` with `C : A`, then `d.a`: cl C2385, cppparse takes B::a) and unqualified names inside member functions (implicit `this->a`) are not checked for ambiguity; implicit conversions report S105 "cannot initialise", not cl's wording; (`takeV(mk())` and `D d = makeD()` now match cl, see 14d; `catch (D x)` by value copies, see lane A's fixes); pre-existing: pointers to members (void* mangling, `->*` reading garbage) are now refused with S132, see row 18 | refused / partly fixed | medium | layout |
| 14d | copies and temporaries (test 17 `sample-front/copies.cpp`: logging classes; -O0 against cl /Od, -O2 against cl /O2, which differ by NRVO). Fixed: a constructor initializer calls the constructor sema chose (`B(o)` in a copy constructor called `B(int)` with garbage when declared first; SemaExpr MemInit::semaSym, IrGen::constructorInit); the callee destroys its by-value parameters left to right, as cl (IrGen::prologue); NRVO at -O1 and up for one local of the result class declared directly in the body and named by every `return` (IrGen::namedResult, localDecl, Return; destroyed only when an exception leaves the function). `takeV(mk())`, `V v = D()`, `const V& r = D()`/`mkD()`, `?:` temporaries, member initialisers from temporaries already matched cl. Left: (1) cl builds the class temporaries of all arguments before it calls a function inside another argument: `Holder h(mkV(35), takeV(V(36)))` cl: V(36), takeV(36), ~V(36), V(35) (checked 2026-10-14; the order written here before was wrong); cppparse evaluates the left argument first; likewise `takeRef(V(30)) + takeRef(V(31))` (cl: V(30), V(31), takeRef(V31), takeRef(V30); cppparse evaluates the left operand first) - unspecified in C++98, but the logs differ; (2) cl /O2 also applies NRVO to a local in a nested block and to different locals returned on disjoint paths (`V f(int x) { if (x > 0) { V a(x); return a; } V b(0); return b; }`: cl /O2 no copy, cppparse -O2 one copy, as cl /Od); (3) `template <class T> T tmake(int v) { T t(v); return t; }` called as `tmake<V>(42)` (template parameter only in the return type) is rejected by sema with S107; (4) arguments are evaluated left to right, cl goes right to left (`takeTwo(mkV(11), l)` with `takeTwo(V, V)`: cl copies `l` first, cppparse calls `mkV` first). Evaluating them right to left in IrGen::callFunction (tried: argExprs evaluated from the last, pushed in order) matches cl on copies.cpp, but selfhost.ps1 then fails its bootstrap (generation two differs in CxxParser, Sema and IrGen: the self-built IrGen numbers temporaries differently, e.g. `condbr` on a pointer without `cmp ne` in genVbaseDestructor), so some call in cppparse's own sources depends on the order of its arguments; find those calls (two IR-emitting calls as arguments of one call) before switching | extra copy where cl elides (allowed); a different order where C++98 leaves it unspecified | medium | copies |
| 20 | vftable names: two tables introduced by the same class are now named after the bases that tell them apart (`??_7G@@6BQ1@@@`, `??_7G@@6BQ2@@@`, IrGen.cpp registerMetadata); left: cl's full rule (clang's MicrosoftVTableContext MangledPath), e.g. `struct F : X, D` (D : A, B, C) where cl names one table `??_7F@@6B@` and cppparse `??_7F@@6BX@@@` (review repro t2.cpp); paths longer than one class | wrong names in mixed links (both COMDATs kept) | small | layout |
| 21 | left of class-specific allocation (a throwing constructor in a new-expression now frees, see above): placement forms of a class's operator new (`new (arena) T` with `T::operator new(size_t, Arena&)`) are not looked up (refused, S132; before, the placement argument was taken as the address); a class placement `operator new`, once looked up, also needs its matching placement `operator delete` on the throwing-constructor path. Placement: `struct A {}; struct P { static void* operator new(size_t n, A&) { return malloc(n); } }; A a; P* p = new (a) P;`: cl calls P's operator new, cppparse refuses it. Fixed (lane A, row S5): `delete[]` of a class without a cookie calls its own `operator delete[]`; the two-parameter form gets one element's size, as cl (checked: `struct Q { int a; ... operator delete[](void*, size_t n) }; delete[] new Q[3]` logs 4 with cl /Od and /O2; the earlier "cl logs 2" was not reproduced); a null pointer is passed to it, as cl. Explicit calls `::operator new(n)` / `::operator delete(p)` compile now (`<new>` declares the global operators); unqualified `operator new[](n)` is still a parse error | refused (S132) | small | layout |
| 18 | pointers to members in code: refused with S132 since lane C (IrGen::memberPointerCheck: `.*`/`->*` and every value of a member pointer type; driver/main.cpp: a class with a member of such a type; sample-front memptr-refused.cpp, memptr-field.cpp); their sizes are cl's (TypeTable::sizeOf/alignOf, memberPointerModel; front.cpp `mpsize::`). Left, in cl's representation: data, an `int` offset with null -1 (`int P::* pm = 0;` is FFFFFFFFh), conversion base -> derived adds the base's offset (not for null); functions, a code pointer (single inheritance) or {code pointer, this adjustment int} (multiple), virtual functions through vcall thunks `??_9P@@$BA@AA` (slot 0; `$B7` slot 1); `.*`/`->*` (data: base + offset; functions: this + adjustment, then call), comparisons with null and each other, mangling (`$$Q`/`P8P@@...` instead of void*), calls through them (sema S109 today). Repro: `struct P { int a; int b; virtual int f() { return 1; } }; struct M { int m; }; struct Q : M, P {}; int P::* pm = &P::b; int (P::*pf)() = &P::f; Q q; int Q::* qm = pm; return q.*qm + (q.*pf)();` | refused (S132) | medium | layout |
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
  and cppparse's runtimes (a cl throw into a cppparse frame fails fast); (vtable slots grouped by overload and the
  destructor slot holding the deleting destructor are done).
- design-table-exceptions.md section 7: an exception edge from every call to its landing pad, then GVN and
  register values across handlers; the main lever on code size (1.55 times cl's over all common functions of the matrix sample at d9309fa). The
  register allocator has these edges now (computeEhEdges); the IR and the optimiser do not (a call inside a region
  does not end its block), so pinHandlerLocals, the GVN guard and handlersSelfContained stay.

## Other known limits (found by the reviews of 2026-10-13)

- cl builds a handler's by-value parameter before it unwinds the frames and handlers the exception leaves (MSVC's
  two-phase handling); cppparse unwinds first, so the order of copies and destructors differs when a handler is
  left by a throw that an outer by-value handler catches. Repro: `struct B { int v; B(int x) : v(x) {} B(const B&
  o) : v(o.v + 10) { printf("cB%d ", v); } ~B() { printf("~B%d ", v); } };` then `try { try { throw B(10); } catch
  (B b) { throw b; } } catch (B b2) { printf("got%d ", b2.v); }`: cl `cB20 cB30 cB40 ~B20 ~B10 got40 ~B40 ~B30`,
  cppparse `cB20 cB30 ~B20 ~B10 cB40 got40 ~B40 ~B30` (the same objects, cB40 later).
- Calls through pointers to members (`(obj.*pmf)(x)`, `(p->*pmf)(x)`) are rejected by sema (an error, not wrong code);
  every other use of pointers to members is refused by --compile6 (S132, row 18).
- A float literal with an `f` suffix is converted decimal -> double -> float, which differs from cl's direct
  decimal -> float rounding when the double is exactly halfway between two floats. The folder
  (IrGen::foldArith, floatRoundsOnce) no longer folds such a literal, but it then takes the guarded dynamic
  path (one more `$TSS<n>` than cl, so later guards of the function shift) and the run-time value, like the
  code's own float constants and the assembler's REAL4, is still double-rounded. Repro (checked):
  `float* hf() { static float h = 1.00000005960464477550f; return &h; }`: cl 0x3F800001, cppparse a guard
  and 0x3F800000. Fix: a correctly rounded decimal -> float conversion (exact big-decimal comparison with the
  midpoint).
- Out-of-range float -> integer conversions in constants (undefined; cl keeps the value, e.g.
  `static double v = (unsigned char)300.0 + 0.5;` is 300.5 in cl's data) are not folded: guarded dynamic path,
  so the guards after them shift against cl's.
- (fixed) At -O2 a run-time double -> unsigned conversion of 3000000000.0 gave 2147483648: the optimiser folded
  it through a 32-bit `long`. Floating <-> integer conversions now follow cl's code at every level and in the
  folder (X86.cpp Conv, Optimizer.cpp floatToInt; sample-front `conv::`).
- (fixed) 64-bit integer literals: `4294967295ULL` was sign-extended to 2^64-1 and wider literals kept their low
  32 bits. Sema reads all 64 bits (integerLiteralValue) and types them as cl (int, unsigned int for hexadecimal
  and octal, long, unsigned long, long long, unsigned long long); IrGen builds a value outside the 32-bit
  sign-extended immediates with bitConstant; a case label converts from its own type (`case 4294967295U:` of an
  unsigned long long switch compared with 2^64-1). Left, refused or merely different from cl: Expr::semaValue
  is still a 32-bit `long`, so such a literal is no constant expression: `case 4294967296ULL:`, an array bound
  or a template argument of that value is an error (S114 and the like, not wrong code); a global or static
  initialised with it gets a dynamic initialiser (a static local a guard) where cl emits static data
  (`unsigned long long b = 4294967295ULL;`: cl `DQ 0FFFFFFFFh`, cppparse `DQ 0` plus an initialiser; the value
  is right once main runs). Fix: a 64-bit semaValue (SemaExpr folding, constantValue, IrGen constant tables).
- (review of the lane C commits, fixed) the C2584/C4584 check took a non-virtual base of a virtual base for a
  virtual one (cl looks only at the edge to the base: `W : V; XV : virtual W; T7 : V, XV` warns, `T8 : virtual
  V, XV` is an error); the member-pointer field refusal fired on implicit instantiations cl accepts
  (`sizeof(Holder<P>*)`; instantiations are no longer refused, using their member still is); member-pointer
  sizes for an incomplete class (cl's unspecified model: data 12, functions 24) and for a vfptr added in front
  of a non-polymorphic base (multiple model, 16); namespace-scope and static member pointers to members are
  refused (S132, memptr-global.cpp); a cast of a floating literal to an integer type is folded modulo 2^N as cl
  folds it (`(int)3e9` b2d05e00h, `(unsigned short)1e10` e400h; IrGen value, Cast). Left (checked 2026-10-14, rows S8-S11): a negated literal
  (`(int)-2147483649.0`) is converted at run time (80000000h; cl /Od and /O2 give 7fffffff, as a printf argument
  and as a global initialiser), likewise unary plus, constant arithmetic, cast chains and const variables; a
  template instantiation with a member-pointer field is not refused when copied or value-initialised: `template
  <class T> struct H { int T::* f; }; H<P> h = H<P>();` gives cl 0 (value-initialisation zeroes, cl's -1 is only
  for `pm = 0`), cppparse garbage at -O0 (row S1); as a namespace-scope object `H<P> gh;` cl -1, cppparse 0
  (row S11); the incomplete-class sizes hold only for a class never completed in the unit (row S10).
- Statics in a member function of a local class: the mangled name lacks the enclosing function (cl nests
  it), so such a static is not shared with cl's copy; found by the review of 92a19bb (`int t4(int a) { struct L { static int f(int b) { static int s1 = b; return s1; } }; return L::f(a); }`).

## Code size against cl /O2 (analysis of the matrix sample, 2026-10-13)

mapcompare part C: the ten largest-ratio Matrix members are 1788 bytes against cl's 611 (2.93); the whole
class 1.85, all 95 functions both compilers emit 1.64. The causes, by extra bytes across the sample:

| | Cause | Bytes | Where | Size / risk |
|---|---|---|---|---|
| A | exception code in the body: constructed flags, destructors expanded in every landing pad, pads where nothing after construction can throw | ~1700 | IrGen registerCleanup (table states instead of flags, design-table-exceptions.md section 7); Optimizer inlineCalls (call `??1` in pads instead of inlining) | large / medium |
| B | register allocator: float pool only callee-saved xmm6-13, rax/rcx/rdx not in the pool, intervals ending at a call treated as crossing it, spills not weighted by loop depth, no memory operands, copies not coalesced | ~1100 | X86.cpp allocate, Lower.cpp | medium / medium |
| C | `throw std::invalid_argument("...")` builds a heap std::string temporary; cl uses the `const char*` constructor and `_CxxThrowException` (35 bytes) | ~600 | include/stdexcept, runtime helper | small / low |
| D | instruction selection: base+index*8 addressing, `add r,8` pointer steps, `xorps` for 0.0, setcc and jump threading for bools, redundant movsxd (widen int counters), FP-to-integer copies only for variadic callees, hot slots within rbp-128 | ~400 | X86.cpp, Ssa/Optimizer | small each / low |
| E | no NRVO and no elision of a copy from a returned temporary (transpose, operator*, `Matrix p = a*b`); NRVO done 2026-10-13 (row 14d), a returned temporary was already built in place | ~320 | IrGen (return slot, destination address) | medium / medium |
| F | inlining thresholds counted on unoptimised IR (sameSize 26 instructions for 20 bytes) | ~150 | Optimizer.cpp:1039 | small / low |
| G | rbp frame in leaf functions, no tail calls (`~Matrix`: cl `add rcx,8; jmp _Tidy`) | ~100 | X86.cpp | small / low |
| H | `fabs` called instead of `andpd`, `std::max(const&)` kept through memory after inlining | ~70 | Optimizer | small / low |

Order: C and F (small, low risk), then B, then A, then D, E, G, H. Expected Matrix members ratio: about 1.65
after A+B+C, about 1.15-1.3 after A-F; `~Matrix` needs G and `index`/`sameSize` need D to reach 1.25; the
free functions depend mostly on A. Note: cl moves cleanup and catch code into `dtor$`/`catch$` symbols, which
mapcompare does not add to the function (mtxSingularThrows is 465 bytes with them, not 207).
