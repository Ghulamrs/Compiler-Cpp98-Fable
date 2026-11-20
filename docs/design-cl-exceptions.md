# Design: exceptions across cl-compiled frames (row M7, 2026-11-18)

Status: done, stages 0 to 5 (2026-11-18). `--eh=cl` is the third model; the default stays `table` (stage 5's
numbers below: the size criterion does not hold). Go-ahead of 2026-11-18 for option (a) with FH3 tables as `--eh=cl`. The independent review of
the first text (commit c89d752) is folded in: section 10 lists every finding and what was done. Decisions of the
go-ahead: the default changes after stage 5 only if its criteria hold, else it stays `table` and the numbers are
recorded; `table` and `sjlj` both stay in this version; FH4 is not part of M7; the exception code lives in its
own folder, cppparse/eh/ (section 9), and moving the existing code there is stage 0.

The stages as they were done:

- Stage 0 (2026-11-18): cppparse/eh/ holds the table and setjmp models' exception code as the classes of section
  9's table that are marked "from" an existing place (`EhModel`, `EhRegions`, `EhEmitter`, `EhSjlj`, `EhRanges`
  and `EhPads` in EhTable, `EhThrow`, `EhUnwindInfo` and `EhRangeTable`); X86.cpp and Coff.cpp call them. Checked
  with the compiler of the commit before against this one: 804 compilations (every sample source and libcpp at
  -O0..-O3, the compiler's own sources at -O0 and -O2, each with `--eh=table` and `--eh=sjlj`) wrote the same
  bytes in every .asm, .obj, IR, symbol and map file (the map's time stamp apart), and the .asm and .obj files a
  full harness run leaves under sandbox\out are the same as the run before the move, the self-host's own objects
  apart (their sources changed).
- Stage 0b (2026-11-18): the runtime's crossing stop (runtime/cppparse_rt.cpp `crossingStop`). The walk of
  `dispatch()` asks the system for a frame's handler of either kind (EHANDLER or UHANDLER: cl registers a frame
  that only has cleanups as UHANDLER) and, when it is `__CxxFrameHandler3`, `__CxxFrameHandler4` or their /GS
  forms (compared through import and incremental-link thunks), reads the frame's state at the return address
  from cl's tables: the FuncInfo's IP map for FH3, the compressed one for FH4 (a function with separated code
  counts as having a state). A state other than -1 stops the program with a message that names both models.
  `cppparse_eh_handler` does the same for a C++ exception of cl's (code 0xE06D7363) that passes a table-model
  frame inside one of its ranges. Exit test, sample-ehx `cross` by scripts\ehx.ps1 (run from eh.ps1), side A
  from cl with FH4 and with FH3 tables, side B from cppparse at -O0 and -O2: with `--eh=table` all six crossing
  paths of section 1 end with the message; with `--eh=sjlj` the three where cppparse throws into cl's frames do.
  A setjmp frame has no unwind handler, so a cl exception that passes one (paths 3 and 5 with `--eh=sjlj`) is
  still not seen; path 1 there is cl's own stop for an exception no handler takes. Path 0 prints what the all-cl
  build prints in every combination.
- Stage 1 (2026-11-18): `--eh=cl` is accepted (`EhModelCl`; it has the table model's regions) and refused with
  `--assemble`; its listing starts with `.ERR`, which stops ml64 and which the object writer skips. A `throw`
  builds its object in a temporary of the frame, one per throw expression, and calls `_CxxThrowException` with the
  type's ThrowInfo; `throw;` passes two nulls (IrGen.cpp `throwInfoFor` describes the type as an `EhThrown`,
  eh/EhClThrowInfo lays out and names the tables, eh/EhThrow has the calls). The names are those of 5.1 with the
  FNV-1a hash. Exit test, sample-ehx `throw` (ehx.ps1 section 2): 27 kinds of thrown type from each side's
  throwers (throw.cpp: scalars, an enum, a literal, `char*` beside `const char*` and `volatile char*`, pointers
  with conversions, classes with bases at offset 0 and 16, virtual bases, an inaccessible and an ambiguous base, a
  pointer to function, const and volatile objects) against four sets of cl handlers in main.cpp (exact types;
  bases and pointer conversions; by value; a rethrow through the side's `throw;`): at -O0..-O3 the builds with
  side A, side B or both from cppparse print what the all-cl build prints (which handler, the value it sees, the
  copies and destructions). The test compares behaviour where the first plan compared fields with cl's listing:
  the names differ from cl's by design, and every field is read by the runtime in some case. A frame of this
  model that owns objects still has the table model's landing pads until stage 2: only throwers are built with
  it yet.
- Stage 2 (2026-11-18): the cleanups. A function with regions is emitted with `PROC FRAME:__CxxFrameHandler3`,
  the 8-byte unwind-help slot set to -2 behind the prologue, and its landing pads as cleanup funclets behind its
  own code: eh/EhClFunclets finds the owner of every block (the function or one funclet; a block reached under two
  owners is refused), numbers the states by stack of regions, lays the blocks out and reports the values that
  cross a funclet's border, which X86.cpp keeps in the frame (`Interval::forceMem`); each funclet pushes the
  callee-saved registers its own code writes and saves xmm6 and up in its own frame. A pad that is one destructor
  call on a local is `lea rcx, [rdx+n] / jmp`. IrGen ends a pad with a funclet return in place of the rethrow,
  without `cppparse_unwind_enter` / `leave`; the function's unwind region (the flagged objects) is a cleanup
  funclet too. The listing carries the funclets and tables as `;@funclet`, `;@pushreg` ..., `;@ehcl`,
  `;@ehstate`, `;@ehtry`, `;@ehcatch`, `;@ehip` lines; Coff.cpp resolves their labels and hands them to
  eh/EhClFuncInfo, which first checks invariants (i) to (vi) of 5.2 (`EhClCheck`: a violation is an error of the
  object writer, naming the function) and then writes the unwind info of the function and of each framed funclet,
  the FuncInfo and its maps, and the function table entries in address order. A RIP-relative operand that names a
  label of its own PROC is settled by the object writer. The checker works on the tables as the object writer
  receives them, for every function compiled in this model; it is not a reader of object files (the plan said
  "over an object's bytes": checking before encoding names the function and the rule, and the encoding itself is
  exercised by every test that throws). `int 3` follows a call that ends a function table range. Try statements
  are still refused in this model. Exit test, sample-ehx `unwind` (ehx.ps1 section 2, with `throw`): main.cpp
  throws through each side's frames and catches: plain objects, a temporary bound to a reference, the arms of
  `?:`, `&&`, an array partly built and one fully built, a constructor that throws in a new-expression (the
  class's operator delete frees) and one that throws after its base and members, a static local whose initialiser
  throws (the next call builds it), a destructor with floating-point values around a call inlined into a cleanup,
  a loop with a class parameter passed by value: at -O0..-O3 every build with a side from cppparse prints what the
  all-cl build prints (what was destroyed, in which order, the doubles the catching function kept).
- Stage 3 (2026-11-18): the handlers. IrGen::tryStatementCl: a handler is a funclet whose catchparam carries its
  HandlerType (adjectives, type descriptor, the variable's frame local; none for a handler without a name); its
  code is a region without a pad (the handler's own state), opened right behind the catchparam; a region around
  the try's body has the flagged objects' cleanup as its pad and is taken out again when the body has none; no
  handler-unwind region, no terminate region, no copy into a by-value variable (the runtime's work). X86.cpp splits
  a block behind a catchend (the funclet returns there, the rest is the function's code again), the catchend
  returns `lea rax, <the branch's target>`, and the allocator keeps what crosses in the frame. The compiler takes
  its default model from the environment variable CPPPARSE_EH when no `--eh=` is given, and `harness.ps1 -Eh cl`
  sets it for every check (the older pipeline, which has no handlers of this model, keeps the table model then).
  Exit tests:
  - eh.ps1 has the model `cl` beside the other four: probe.cpp and probe-mix.cpp print what cl's build prints
    (89 lines) at -O0..-O3, and the four terminate programs end with cl's output and exit code at -O0 and -O2.
  - sample-ehx `catch` and `cross` with `throw` and `unwind` (ehx.ps1 section 2), side A, side B or both from
    cppparse at -O0..-O3 against the all-cl build: each side's handlers for the other's 27 kinds of throw (exact
    types, bases and pointer conversions, by value with virtual bases, without a name: no copy; a rethrow through
    the other side's `throw;`), an exception that leaves a handler into the try around it, a try inside a handler,
    two exceptions alive in one frame, handlers in a loop left by break, continue and falling through, return and
    goto out of a handler, a class result built in a handler, a constructor's function-try-block, a throw whose
    copy constructor throws, and the seven paths of section 1. Run against cl /EHsc at all four levels and
    against cl /EHa at -O2.
  - The self-host in the model: `selfhost.ps1 -Lvl 0` and `-Lvl 2` with CPPPARSE_EH=cl: all 47 own units compile,
    generation two is identical to generation one, and no function of the own sources, libcpp, sample-eh,
    sample-lib, sample-life, sample-matrix or sample-front has regions that do not nest (each would be an error of
    the object writer).
  What the tests showed about cl itself: cl 19.52 /EHsc at /O1 and up miscompiles two exceptions alive in one
  frame (`try { throw 1; } catch (int& r) { try { throw 2; } catch (int) {} return r; }` reads through a null
  pointer; /EHa and /Od run it; with FH3 tables too). cl's /EHsc copy of that one function in the sample is a
  stand-in that states the result; the /EHa pass has cl run the code. cppparse's copy is the real code in both.
  Finding 6's extension of the hand-written probe was not made: the cases it was to cover (a handler funclet that
  calls a function of cl's which throws, with and without a try in the handler) run in sample-ehx with the
  compiler's own funclets (`catch ... leaving` and `catch ... in handler`, the thrower in main.cpp).
  `harness.ps1 -Eh cl` on this stage's tree: every check passes in the model (suite, selfhost -O0 and -O2,
  suite-self, mix, maps, cprog, matrix, strict98, lib, life, eh, mapcompare).
- Stage 4 (2026-11-18): one runtime state, the link variants, the fault injection.
  - The terminate handler is the C runtime's in every model: `cppparse_set_terminate` is `set_terminate`,
    `cppparse_terminate` and `cppparse_unexpected` end in `terminate` (per thread, as cl's).
    `cppparse_uncaught_exception` answers the runtime's own state or `__uncaught_exception()`.
  - Every object carries `/FAILIFMISMATCH:"cppparse_eh=cl"` or `"cppparse_eh=cpp"` (table and setjmp) in a
    `.drectve` section (`;@drectve` line, Coff.cpp): link refuses the two families in one program (LNK2038).
  - The driver catches `std::bad_alloc`: "cppparse: out of memory", exit code 4.
  Exit tests:
  - sample-ehx `state` (ehx.ps1 section 2, all builds, -O0..-O3 against /EHsc, -O2 against /EHa):
    `std::uncaught_exception` in a destructor of one side while the other's exception unwinds it;
    `std::runtime_error` and `std::logic_error` of each side's library caught by the other by reference; and, each a
    run of its own, a terminate handler one side installs, reached when the other side's throw finds no handler
    (exit code 7 from the handler).
  - The whole sample once more at -O2 linked three other ways: cl /MD with the DLL runtime, /DEBUG /INCREMENTAL,
    /OPT:REF,ICF; and once with both sides on cppparse's library (cl /X with cppparse's headers), where the
    standard exceptions are caught by value as well. 83 checks in ehx.ps1, all pass.
  - link refuses an `--eh=cl` object beside an `--eh=table` one.
  - `sandbox\scripts\failnew.ps1` (not part of the harness, about ten minutes): the compiler's own sources
    compiled by cppparse at -O0 in each model, linked with an operator new that throws `std::bad_alloc` at the k-th
    allocation; 60 values of k spread over the run's allocations (51,129 in the table model, 51,120 in cl's), each
    run compiles a sample with `-j 1`. All 120 runs end with "cppparse: out of memory" and exit code 4: the
    exception unwinds through whatever frames are active, in both models.
  Not made: `std::bad_alloc` from libcpp's operator new caught by cl's code with cl's library (two libraries: by
  reference it is the case of `std::runtime_error` above; whether its CatchableType needs cl's property 10h was
  not looked at). `/guard:ehcont` is not supported (limit 2).
- Stage 5 (2026-11-18): measured, default decided. Object sections of cppparse's objects (`.text*`; `.pdata` +
  `.xdata`; `.rdata` + `.data`, where the model's FuncInfo, maps and ThrowInfo are), table model against cl's model:

  | sources | level | table: code / unwind tables / data / sum | cl: code / unwind tables / data / sum | cl against table |
  |---|---|---|---|---|
  | sample-eh probe.cpp | -O2 | 27,341 / 6,312 / 2,987 / 36,640 | 26,214 / 16,028 / 5,290 / 47,532 | +29.7 % |
  | sample-matrix (2 files) | -O2 | 30,811 / 5,660 / 2,140 / 38,611 | 30,644 / 8,152 / 2,220 / 41,016 | +6.2 % |
  | libcpp iostream.cpp | -O2 | 74,954 / 13,232 / 8,308 / 96,494 | 75,291 / 17,532 / 8,411 / 101,234 | +4.9 % |
  | sample-lib (4 files) | -O2 | 1,074,724 / 181,592 / 101,024 / 1,357,340 | 1,080,055 / 256,684 / 102,940 / 1,439,679 | +6.1 % |
  | own sources (47 files) | -O0 | 3,744,110 / 680,624 / 203,398 / 4,628,132 | 3,722,394 / 1,046,440 / 206,954 / 4,975,788 | +7.5 % |
  | own sources (47 files) | -O2 | 9,722,751 / 615,964 / 203,606 / 10,542,321 | 9,649,045 / 944,404 / 207,162 / 10,800,611 | +2.5 % |

  The code is within 1 % either way (0.8 % smaller for the own sources at -O2: no landing-pad glue, no resume
  calls); the spills are equal (own sources at -O2: 266,864 against 265,425). What grows is the unwind data: one
  `.pdata`/`.xdata` entry per funclet, and FH3's uncompressed maps (FH4 would be the lever; not part of M7).
  Time, best of several runs on the idle machine:

  | what | cl /O2 /EHsc | cppparse `--eh=table` | cppparse `--eh=cl` |
  |---|---|---|---|
  | matrix sample, whole run, -O2 (best of 15) | | 11.2 ms | 11.1 ms |
  | a throw through 10 frames with one object each (probe t.cpp) | 5,378 ns | 7,084 ns | 5,857 ns |
  | the same path without a throw | 34.8 ns | 39.9 ns | 45.5 ns |
  | a throw out of one frame with 1,000 objects alive | 56,261 ns | 3,125,428 ns | 103,169 ns |
  | that function without a throw | 1,679 ns | 3,170 ns | 3,377 ns |

  A throw is cheaper in cl's model (17 % through ten frames; 30 times through 1,000 regions, where the table
  model takes 3.1 ms; the cause was not looked into). The path without a throw is 14 % slower in the ten-frame
  probe (the unwind-help slot's store in every frame with a state, and values that cross a funclet's border live
  in the frame) and 7 % in the 1,000-object function; the matrix sample shows no difference.
  (2026-11-19, row G24: the cause was the table model's runtime, a dispatch and a linear search of the range table
  for every landing pad of the frame; mended there, the table model's throw takes 0.075 ms out of the 1,000-object
  frame and 5.6 us through ten frames, less than this model's 0.098 ms and 5.8 us; since row G30, 2026-11-20,
  0.013 ms, the generated code going from pad to pad and no return address written. The table and its story:
  docs/design-table-exceptions.md, section 6.)
  The decision by the go-ahead's criteria: every gate passes in both models (harness.ps1 and `harness.ps1 -Eh cl`
  on stage 4's tree: 15 checks each in a copy, crc32 apart), the matrix time is within its spread, but code +
  tables are not within 3 % of the table model's (matrix +6.2 %, own sources at -O0 +7.5 %; only the own sources at
  -O2 are, +2.5 %). **The default stays `table`.** `--eh=cl` is the model for a program with cl's code in it.
  What would change the decision: FH4 tables (cl's own default, compressed), and funclets that share one unwind
  entry; both are rows of their own.
- Limits that stay (see section 2): a setjmp-model frame is not seen by the crossing stop; standard exception
  classes cross by value only when both sides use one library; `--eh=cl` needs `--obj` and `--compile6`;
  `/guard:ehcont` is not supported; cl 19.52 /EHsc itself miscompiles two exceptions alive in one frame (stage 3).
Measured with cl 19.52.36725 (x64, preview), link and ml64 of the same toolset, and cppparse 6.0 of 2026-11-13
(sandbox\cppparse.exe of main). x64 Windows only, as the rest of the back end.

## 1. The problem, measured

A program of three objects: unit A and unit B each hold a thrower, a middle frame with a destructor, a function
with `catch (int)`, `catch (const Err&)`, `catch (...)`, and a function with `catch (...) { Guard g; throw; }`;
`main` runs seven paths with three thrown types (int, a class derived from Err, `const char*`). A is compiled by
cl /O2 /EHsc, B by cl (the reference) or by cppparse -O2.

| path | cl + cl | cl + cppparse `--eh=table` | cl + cppparse `--eh=sjlj` |
|---|---|---|---|
| A throws, A catches | ok | ok | ok |
| A (cl) throws, B catches | ok | exit 0xC0000409 | exit 0xC0000409 |
| B throws, A (cl) catches | ok | exit 0xC0000409 | exit 0xC0000409 |
| A throws, through B's frame, A catches | guards 111 | guards 101: **B's destructor is skipped, no message** | the same |
| B throws, through A's frame, B catches | guards 111 | guards 101: **cl's destructor is skipped, no message** | the same |
| A throws, B's `catch (...)` rethrows, A catches | guards 1101 | guards 101: **B's handler is skipped** | the same |
| B throws, A's `catch (...)` rethrows, B catches | guards 1101 | guards 101: **cl's handler is skipped** | the same |

So the row's "terminates (0xC0000409)" is half of it. When thrower and catcher are compiled by the same compiler
and only the frames between belong to the other, the program goes on with destructors and handlers left out.
That is worse than a stop, and the reason not to leave M7 as a documented limit.

Why: the two compilers have two unrelated mechanisms. cl raises a Windows exception (code 0xE06D7363) and the
system calls each frame's handler; cppparse's frames answer "continue search" (`cppparse_eh_handler`) and have
nothing the system could run. cppparse's `throw` never tells the system: `dispatch()` walks the stack itself
and jumps (`RtlRestoreContext`, `longjmp`), over cl's frames as if they were plain C.

## 2. Goals and limits

Goals, in a link of cl objects (/EHsc or /EHa) and cppparse objects, both directions:

1. A `throw` of either compiler is caught by a handler of the other: by value, by reference, by pointer with the
   conversions of 15.3/3 (derived to base, also through virtual bases, qualification, `void*`), `catch (...)`.
2. An exception that passes through frames of the other compiler runs their destructors, in order, and their
   handlers that do not match are tried; `throw;` in either compiler's handler rethrows the same object.
3. One exception object, built once, destroyed once, as in a program compiled by cl alone (the copies and
   destructor calls a program can count must be the same numbers).
4. `std::uncaught_exception`, `std::terminate`, `std::set_terminate` mean one thing in the whole program. An
   exception that leaves a destructor during unwinding, a throwing copy of a by-value catch parameter and an
   unhandled exception call terminate with cl's output and exit code.
5. No cost on the normal path beyond cl's own, code size and speed at cl /O2's level (the project's goal).

Limits that stay:

- Types are matched by the decorated name in the type descriptor (`.?AUErr@geom@@`), as cl's runtime does when
  two descriptors are not the same object. cppparse's descriptors already have cl's names and layout: for twelve
  types (int, double, unsigned long, an enum, classes with `struct` and `class` keys in a namespace, a template
  class, pointers to them, `const char*`, `void*`, a pointer to function) cl and cppparse define the same twelve
  `??_R0...@8` symbols (probe h.cpp). Nothing to change there.
- The standard library. A cl object compiled with MSVC's headers and a cppparse object compiled with
  cppparse's both have a class named `std::exception`, with the same descriptor name and different contents
  (MSVC's is 24 bytes: vfptr, message pointer, flag; cppparse's is 8, the message lives in `logic_error` and
  `runtime_error`). The names match, so a handler `catch (std::exception& e)` of one side takes a
  `std::runtime_error` of the other, and `e.what()` works because both classes declare the destructor first and
  `what` second (two vftable slots in that order; to be checked by a test in stage 4). Caught **by value** the
  thrower's copy constructor would write its own layout into the catcher's smaller variable: undefined, as any
  one-definition-rule violation. The rule stays the one mix.ps1 already follows: objects that exchange library
  types are compiled against one library (cl with `/X` and cppparse's headers). The design does not try to make
  two standard libraries one.
- /EHsc semantics. cppparse's frames behave like cl /EHsc frames: `catch (...)` takes C++ exceptions only, a
  structured exception of another code (an access violation, another language's) passes through without
  destructors. cl /EHa objects in the link keep their own behaviour. Measured (probe c.cpp): a code 0xE0000001
  through a cl /EHsc frame with a destructor does not run it and is not taken by `catch (...)`; under /EHa both
  happen.
- A cl function that calls a function declared `extern "C"` assumes it does not throw: cl /EHsc removed the
  `try` around such a call in a probe, and the exception was unhandled. cppparse makes the same assumption (row
  A1). An exception must not leave an `extern "C"` function, whoever compiled it.
- Two handler matches follow cl's runtime, not C++98, because the runtime compares the descriptors of the inner
  levels of a pointer by name: a thrown `char**` is not taken by `catch (const char* const*)` (a qualification
  conversion at a deeper level, 4.4; the table model takes it), and a thrown `D*` is taken by `catch (B*&)` (not
  C++98; the table model takes it too). Measured by the review's probe u.cpp with cl /O2 /EHsc /d2FH4-.
- cl keeps the terminate handler per thread; cppparse's has been one for the process. With the handlers unified
  (stage 4) a program sets it per thread, as with cl.
- link options that depend on tables cppparse does not write: `/guard:ehcont` (cl lists every continuation
  address in `.gehcont`; under CET strict mode an address not listed fails fast). Not supported with `--eh=cl`.
- x64 only.

## 3. What cl's model is, measured

All numbers from cl /O2 on the probes; `/d2FH4-` selects `__CxxFrameHandler3` (FH3), the default is
`__CxxFrameHandler4` (FH4, the same model with compressed tables).

**The handler runs on top of the stack, the frames below still alive.** In a cl program (probe c.cpp), relative
to a local of the catching function: the thrower's local lies 168 bytes deeper, the exception object 208 bytes
deeper (it is a temporary in the thrower's frame), a local declared in the handler 8 bytes *above* (the handler
addresses the catching function's frame), and a destructor run by the unwinding has its locals 8,448 bytes
deeper. So cl's handlers and cleanups are not code a frame is resumed into. They are small functions
("funclets") the runtime calls with the address of the frame they belong to in `rdx`, while everything between
the throw and the handler is still on the stack. Only when the handler returns (it returns the address to
continue at) is the stack cut back. This one fact decides the choice in section 4.

**What a function carries** (FH3 listing of the probe, `int a_catch(void (*f)(int), int kind)` with a Guard, a
try and three handlers):

    unwind info   flags EHANDLER|UHANDLER (UHANDLER alone when the function has cleanups and no try: the search
                  phase then passes the frame by), handler __CxxFrameHandler3, handler data = RVA of FuncInfo
    FuncInfo      40 bytes: magic 19930522h, state count, RVA UnwindMap, try count, RVA TryBlockMap,
                  IP-to-state count, RVA IpToStateMap, dispUnwindHelp, 0, EHFlags (1: /EHs)
    UnwindMap     8 bytes a state: { the state it unwinds to, RVA of the action funclet or 0 }
    TryBlockMap   20 bytes a try: { tryLow, tryHigh, catchHigh, handler count, RVA HandlerType array }
    HandlerType   20 bytes a handler: { adjectives (1 const, 2 volatile, 8 reference, 40h catch (...)),
                  RVA type descriptor, dispCatchObj (frame offset of the handler's variable), RVA of the
                  catch funclet, dispFrame (offset in the funclet's frame where it saved rdx) }
    IpToStateMap  8 bytes an entry: { RVA, state }, ascending; an address has the state of the last entry not
                  above it
    code          one store in the prologue: mov qword ptr [frame + dispUnwindHelp], -2
    funclets      a destructor: `lea rcx, [rdx+60] / jmp ??1Guard@@QEAA@XZ`, 9 to 12 bytes, no unwind entry
                  a handler: `mov [rsp+16], rdx / push rbp / sub rsp, 32 / mov rbp, rdx / ... /
                  lea rax, continuation / add rsp, 32 / pop rbp / ret`, with its own .pdata entry and an
                  unwind info that names the same handler and the same FuncInfo

**What a throw carries**: `lea rdx, _TI... / lea rcx, object / call _CxxThrowException`, the object a temporary
of the frame, and per thrown type

    ThrowInfo           16 bytes: attributes (1: the object, or a thrown pointer's pointee, is const; 2 volatile),
                        RVA of the destructor (the vbase destructor `??_D` for a class with virtual bases),
                        0, RVA CatchableTypeArray
    CatchableTypeArray  count, then RVAs of CatchableType: the type itself, then every accessible base (for a
                        pointer: the pointers to them, then void*)
    CatchableType       28 bytes: properties (1 scalar, 4 has virtual bases), RVA type descriptor, the base's
                        place { mdisp, pdisp (-1: not in a virtual base), vdisp }, size, RVA copy constructor

Measured details that cppparse must reproduce (probe p.cpp): a thrown `const char*` has ThrowInfo attributes 1
and the catchable types `char*` and `void*`, and `catch (const char*)` is HandlerType { adjectives 1, descriptor
of `char*` }: the pointee's qualifiers travel in the attributes and adjectives, the descriptors are those of the
unqualified pointer; `throw c` of a `const Plain c` has attributes 0; `Err` in `struct VD : virtual Err` is
{ 0, 0, 4 } with properties 4 on VD; `Err` at offset 16 in `Derived : Other, Err` is { 16, -1, 0 }; a thrown
`Derived*` lists `Derived*`, `Other*`, `Err*` (mdisp 16), `void*`. A string literal thrown under cl's default
options is a `char*` (attributes 0); cppparse's is `const char*`: both are caught by `catch (const char*)`, and
`catch (char*)` is not C++98 for a literal anyway.

**What cl's runtime asks of a throw it did not compile** (probe c.cpp, test 1): a function that builds its own
type descriptor of int (a second object with the name `.H`), its own CatchableType, array and ThrowInfo in plain
data, and calls `RaiseException(0xE06D7363, EXCEPTION_NONCONTINUABLE, 4, { 19930520h, &object, &throwInfo,
image base })` is caught by cl's `catch (int)` with the value, after the destructor of the cl frame between.
Nothing else is checked: no registration, no cl-emitted symbol.

**What cl's runtime asks of a frame it did not compile** (probe f.asm, assembled by ml64, linked with cl code,
static and DLL runtime): a function in *cppparse's* frame shape (`push rbp / mov rbp, rsp / push rbx /
sub rsp, 72`, locals at `[rbp-x]`, unwind codes without a frame register) with FH3 tables written by hand, the
funclets placed after the function in the same section, each starting `lea rbp, [rdx+80]` so that the body's
`[rbp-x]` addresses stay valid. Result, equal to the cl-only numbers:

    cl throws int, the hand-written catch (int) takes it:        r=42 guards=111
    cl throws Err, its catch (...) takes it:                     r=-1 guards=111 copies=0 dtors=1
    a cl handler throws again, into the hand-written handler:    r=5  guards=1111
    cl throws double through a hand-written frame with a Guard:  the destructor funclet ran, cl caught 2.5

Three contract points came out of that probe:
- the frame offsets in the tables (dispUnwindHelp, dispCatchObj, the funclet's `[rdx+n]`) count from the
  establisher frame, which for cppparse's unwind codes is `rsp` after the prologue, `rbp - (allocation + pushes
  after rbp)`;
- cl puts a `nop` after a call that ends a state's range, because the runtime looks up the return address, which
  is the next range's first byte. Entries written as `label + 1` need no nop (the probe has none): no code for
  it. cppparse's epilogue starts with `lea rsp, [rbp-n]`, which Windows does not take for an epilogue when the
  unwind info has no frame register, so a call followed by the epilogue is safe too;
- link refuses an object whose .pdata entries are not in address order (LNK1223): the funclets' entries follow
  their function's.

**FH3 or FH4.** Both are in the static runtime; in the DLL runtime FH3 and `_CxxThrowException` are in
VCRUNTIME140.dll, FH4 in VCRUNTIME140_1.dll (dumpbin /imports of the probes). FH3's tables are fixed-size
records that cl's `/FAs /d2FH4-` listing prints field by field, so every table can be compared with cl's; FH4's
are variable-length encodings, and it drops the `-2` store in most functions (0 of 4 in probe a.cpp, 5 of 52 in
sample-eh's probe.cpp). The layouts of both are in the toolset's own headers (`include\ehdata.h`,
`ehdata4_export.h`) and the runtime's source is installed (`crt\src\vcruntime\frame.cpp`, `throw.cpp`,
`risctrnsctrl.cpp`), which settled two questions by reading: the /EHs test above is `frame.cpp` line 296-311,
and a function with FuncInfo must store -2 in its unwind-help slot (`GetCurrentState`).

**Sizes**, sample-eh's probe.cpp (26 KB of exception tests), bytes:

| | code | .pdata | unwind + EH tables | throw tables | total of these |
|---|---|---|---|---|---|
| cl /O2 /EHsc (FH4) | 20,756 | 2,196 | 4,586 | 2,820 | 30,358 |
| cl /O2 /EHsc /d2FH4- (FH3) | 21,001 | 2,196 | 8,648 | 2,820 | 34,665 |
| cl /O2 /EHa (FH4) | 22,775 | 2,616 | 6,384 | 2,820 | 34,595 |
| cppparse -O2 `--eh=table` | 27,341 | 1,476 | 4,836 | (type strings, in .rdata 2,252) | 33,653 + strings |
| cppparse -O2 `--eh=sjlj` | 42,498 | 1,476 | 1,828 | (the same) | 45,802 + strings |

cl's FH3 output for it: 47 functions with FuncInfo, 36 with a try, 150 states, 107 IP-to-state entries, 61
handlers, 64 destructor funclets (22 with a frame and an unwind entry of their own, 42 two-instruction jumps),
52 `-2` stores, 107 throws. cppparse's table output: 51 functions with regions, 364 ranges, 90 one-destructor
pads, 74 pads with more code, 57 `cppparse_match` stubs, 100 `cppparse_catch_begin` calls, 40 handler-unwind
pads, 92 throws and 111 rethrow calls.

**Speed** (probe t.cpp, best of 5): a throw through 10 frames with a destructor each to a `catch (int)`:
cl 5.4 microseconds (FH3 and FH4 alike), cppparse table 7.1, cppparse sjlj 5.3. The same 11 calls without a
throw: cl 35.2 ns (35.8 with FH3: the eleven `-2` stores are within the noise), cppparse table 40.2, sjlj 167.6.

## 4. The options

### (a) cppparse emits cl's own model

FuncInfo tables, funclets, `_CxxThrowException` with ThrowInfo, the handler `__CxxFrameHandler3`. cl's runtime
does everything at run time; to it a cppparse frame is a cl frame.

- For: every goal of section 2 holds by construction, because one runtime executes one protocol: object lifetime,
  rethrow, nested exceptions, terminate, uncaught_exception, longjmp through frames, debuggers' first-chance
  handling. Section 3's hand-written probe shows that cl's runtime accepts cppparse's frame shape as it is. The
  runtime needs no new exception code at all.
- Against: the largest compiler change of the three. Handlers and cleanups become funclets; values that cross a
  funclet's border live in memory (cl has the same rule, so parity with cl is not at risk, but cppparse's table
  model can keep such a value in a callee-saved register today); states must nest the way FH3 wants. P1 (the
  runtime compiled by cppparse itself) must then contain an implementation of the FH3 handler, where today's
  walker is 60 lines.

### (b) keep the landing pads, make the runtimes cooperate

`cppparse_throw` raises 0xE06D7363 with a real ThrowInfo; `cppparse_eh_handler` takes part in the system's
dispatch, recognises cl's exceptions, and enters cppparse's landing pads.

Rejected, on the measurement at the head of section 3. A landing pad is code inside its function that runs with
the frame as the top of the stack; cl's protocol needs handlers and cleanups to run while the thrower's frame is
alive below them:

- A cl exception object lies in the thrower's frame (208 bytes below the catcher in the probe). Entering a
  landing pad cuts the stack back first, so the object must be copied to the heap before: one copy construction
  and one destruction more than cl makes, visible to any class that counts them (goal 3), and impossible for a
  class cl threw without a usable copy constructor.
- cl's second phase is one "consolidated" unwind (`RtlUnwindEx` with a STATUS_UNWIND_CONSOLIDATE record that
  points into the runtime's own frames below). A cppparse cleanup pad in the middle of it would have to cut the
  stack to its frame, which destroys that record and the frames it points to; there is no way to go on with cl's
  unwind after the pad. libgcc's SEH personality does this trick (a second `RtlUnwindEx` from the pad), but only
  because both ends are its own and its objects are on the heap.
- Whether a pad catches is decided by the pad's code (`cppparse_match` calls), so the handler cannot answer the
  system's search phase. The tables would need the handler types anyway, which is a large part of (a).
- The one way out, running pads on top of the stack with `rbp` set by a trampoline, is a funclet in everything
  but the prologue, with unwind tables that are wrong while it runs.

So (b) can at best give "cppparse throws, cl catches or unwinds"; the other direction stays broken or inexact.

### (c) funclets with cppparse's own tables and handler

The compiler work of (a) (funclets, states), but CPPX-style compact tables and a handler of our own in
cppparse_rt.cpp that implements cl's protocol.

Rejected for now: it has all of (a)'s compiler cost and adds a reimplementation of `frame.cpp` that must agree
with cl's runtime about its private per-thread state (the current exception for `throw;`, the processing-throw
count, the catch depth), which changes between toolset versions. Its only gain over (a) is table size, and FH4
is the better way to that (below). It is, in effect, P1's runtime, and belongs there.

### Recommendation: (a), with FH3 tables, as a third model `--eh=cl`

- The model is `--eh=cl`, beside `table` and `sjlj`; `table` stays the default until stage 5 has measured the
  new model against it. Then the default changes if the numbers allow (criteria in section 6, stage 5), because
  the usual cppparse program is a mixed link already: every test driver, the runtime and the C library are cl's.
- FH3 first: fixed layouts, comparable with cl's listings field by field, the same handler in every vcruntime.
  FH4 is a later, separate step that changes only the table writer (Coff.cpp) and removes most `-2` stores:
  about 45 % smaller EH tables by cl's own numbers above.
- An exception must not cross between `--eh=cl` frames and `table`/`sjlj` frames, exactly today's limit between
  cppparse and cl. Until stage 4 nothing but the runtime's message of stage 0b guards that; from stage 4 on link
  refuses to put objects of the two families into one program (`/FAILIFMISMATCH`). `table` and `sjlj` keep
  mixing with each other.

Effect on size, against today's table model, estimated from the counts in section 3 for probe.cpp. The table
rows were recomputed after the review (finding 4): the first text had the EH tables at +4,500 bytes, about half of
what the counts give. To be replaced by measurements at stages 2 to 5:

| item | bytes |
|---|---|
| 57 match stubs (`lea / call cppparse_match / test / jne`), about 20 each | -1,100 |
| catch_begin and catch_end calls, 40 handler-unwind pads with `cppparse_catch_unwind` | -1,800 |
| 90 one-destructor pads become `lea rcx, [rdx+n] / jmp dtor` (16 to 19 bytes -> 9 to 12) | -600 |
| unwind_enter / unwind_leave / rethrow calls of the other 74 pads | -1,200 |
| 92 throws: no malloc, no copy into it, three instructions each | -1,000 |
| about 60 handler funclets: prologue, epilogue, continuation address, about 22 each | +1,300 |
| frames for the 74 larger cleanup funclets, about 10 each | +700 |
| 51 `-2` stores, 8 bytes each (FH3 only) | +400 |
| code, net | about -3,300 (-12 %) |
| IP-to-state map: 364 ranges, 4 bytes more each | +1,500 |
| FuncInfo: 51 x 40 | +2,000 |
| UnwindMap: about 250 states x 8 | +2,000 |
| HandlerType 61 x 20, TryBlockMap 36 x 20 | +1,900 |
| about 60 handler funclets: .pdata 12 and unwind info with handler data about 20 each | +1,900 |
| 74 cleanup funclets with a frame: .pdata and unwind info, about 24 each | +1,800 |
| the CPPX pad tables and headers that go away | -1,300 |
| EH tables, net | about +9,800 |
| ThrowInfo and CatchableTypes against the descriptor strings | about +1,000 |
| total | about +7,500 (+20 %) with FH3 |

cl's own FH3 tables for the file are 8,648 bytes with 107 IP entries and 22 framed cleanup funclets, where
cppparse has 364 ranges and 74 such pads, so cl's figure is a lower bound. The own sources at -O0 (53,000
regions, the reason the CPPX entries were packed into 4 bytes) grow by at least 16 bytes a region, several hundred
KB of objects. With FH3 the stage 5 criterion "within 3 %" will therefore most likely not hold, and the default
will stay `table`; stage 5 measures and records that. FH4 (about 45 % smaller tables by cl's numbers) is the way
to the default, and stays its own row after M7.

Against cl /O2 the remaining difference is then the code generator's (27.3 KB of code against cl's 21.0 KB on
this file, before the 3.3 KB above), not the exception model's.

Effect on speed: the normal path loses one store per function with FuncInfo (not measurable in cl's own FH3 and
FH4 numbers), and every value a funclet reads lives in the frame for its whole life: `this` in a constructor with
member cleanups, the counter of an array under construction, the pointer of a new-expression. The table model
keeps those in callee-saved registers today, so this is extra loads and stores at -O2 against today, though not
against cl, which has the same rule. Stage 2 records the spill count (`LowerStats::spills`) beside the table
model's. A throw calls malloc no more; cl's 5.4 against cppparse's 7.1 microseconds through 10 frames was
measured with cl's frames, and FH3 finds a state by a linear walk of the IP map, several times per frame, where
cppparse has about 3.4 times cl's entries: stage 5 measures the throw with cppparse's frames, and once through a
function of 1,000 regions.

Effect on self-hosting: none on the method. The own sources compiled with `--eh=cl` are the model's largest
test (53,000 regions at -O0); generation two must stay identical, which only needs the table writer to be
deterministic. mix.ps1 (one cppparse object among cl's) becomes a link in which exceptions may really cross.

Effect on P1: with `--eh=cl` a program without cl's runtime needs `__CxxFrameHandler3`, `_CxxThrowException`
and `__uncaught_exception` from P1's runtime: the search over the FuncInfo, type matching with CatchableTypes,
building the catch object, the consolidated unwind (`RtlUnwindEx` with a callback record), rethrow and nested
exceptions. It can be written in C++98 and compiled by cppparse in this very model: where cl's source uses
`__try`/`__finally` around the call of a handler funclet, a local object's destructor does the same job. That
does not cover cl's `__except` filters (`ExFilterRethrow`, `FrameUnwindFilter`), which run in the search phase,
before any unwinding, and decide there between rethrow and terminate; P1's handler would have to be the handler
of its own frames to do the same. 1,000 to 1,500 lines is plausible; the time is open until that is designed, and
is more than the three to four weeks of the first text. Today's table walker is far smaller.
P1 may instead keep `--eh=table`, since a program without cl's runtime has no cl frames and so no M7. That
decision is P1's and is not needed for M7; it is the reason this design keeps the table model.

## 5. Contracts

### 5.1 Throw

    throw e;      the operand is built in a temporary of the frame (directly, for a prvalue), then
                  lea rcx, [temporary] / lea rdx, <ThrowInfo> / call _CxxThrowException      (does not return)
    throw;        xor ecx, ecx / xor edx, edx / call _CxxThrowException

ThrowInfo, CatchableTypeArray and CatchableType as in section 3, built from the list IrGen already makes for
the descriptor string (`Class;Base1;Base2@16`, `D*;B1*;B2*+16;V*+v0:4;void*`): one CatchableType per entry,
mdisp from `@n` / `+n`, pdisp and vdisp from `+vP:E`, the size, the copy constructor's symbol (0 for a scalar or
a class copied bit for bit; properties 4 and the constructor that takes the most-derived flag for a class with
virtual bases), the destructor (the vbase destructor for a class with virtual bases). They go to `.rdata` as
COMDAT "any" sections under names of cppparse's own, so that the units of a program share them and no symbol
competes with cl's `_TI...`. The type descriptors are the existing `??_R0...@8`. Relocations:
IMAGE_REL_AMD64_ADDR32NB for every RVA field.

The linker keeps one COMDAT of a name, so a name must stand for one content (review, finding 1: the descriptor of
a thrown `const char*` is that of `char*`, and `Err` lies at 16 in one class and at 0 in another):

    $cppCT$<descriptor>$<mdisp>$<pdisp>$<vdisp>$<hash>     hash: of properties, size and the copy constructor's name
    $cppCTA$<the thrown type's descriptor>$<hash>          hash: of the CatchableType names, in order
    $cppTI$<descriptor>$<hash>                             hash: of attributes, the destructor's name, the array's name

The hash is FNV-1a, 32 bits, in hex. Every field of a table is in its name, spelled out or hashed.

One temporary per throw expression, scalars included (finding 2): the temporary *is* the exception object and
lives until its last handler ends, so two throws of one frame must not share a slot (`try { throw 1; } catch
(int& r) { try { throw 2; } catch (int) {} use(r); }` reads 1). The table model's single `m_throwSlot` is not
used. The temporary has no cleanup region: the runtime destroys the object through the ThrowInfo's destructor.
Rule for any later slot colouring: no sharing for throw temporaries, for handler variables (the runtime writes
them while the try body's objects are still alive) or for anything a funclet addresses.

### 5.2 A function with regions

Unwind info: flags EHANDLER|UHANDLER with a try block, UHANDLER alone otherwise; handler
`__CxxFrameHandler3`; handler data the RVA of the FuncInfo. The FuncInfo and its maps follow in the function's
`.xdata` contribution (the associative COMDAT section of a COMDAT function, as the range table today).

States come from the regions cppparse already has (one per object with a destructor, one per try, one per
handler body, row A3). The back end knows one region *stack* per block (X86 computeRegions, which fails when two
paths reach a block with different stacks). A state is a distinct stack as its owner sees it (finding 5): the
states form the trie of the stacks, a state unwinds to the stack without its last region, and its action is that
region's pad as a funclet. So every state has one `toState` whatever order IrGen suspends and reopens regions in,
and a region that occurs under two stacks is two states with one funclet. Numbering is the pre-order walk of the
trie; at a try, after all states of its body, one state per handler (the handler's own code), each followed by
the states of the regions that handler opens. The invariants the runtime relies on, and checks only by failing
fast (0xC0000409, no message):

    (i)   toState < state for every state
    (ii)  a try's body is exactly tryLow..tryHigh, and every state in it unwinds through tryLow; its handlers are
          exactly tryHigh+1..catchHigh, and a handler's own state unwinds to what the try's state unwinds to
    (iii) the TryBlockMap is in post-order: a try in the body or in a handler of T comes before T
    (iv)  the IP map ascends, and every state in it is below the state count
    (v)   every address of a handler funclet has a state in (tryHigh, catchHigh] of its try, and the funclet's
          address is the BeginAddress of its own function table entry (the runtime finds the function's frame
          from a handler's frame only then; otherwise it takes the handler's own rsp for it, silently)
    (vi)  no function table range ends with a call (the return address would be the next range's first byte, and
          the walk would unwind the caller of a throw with the funclet's tables): `int 3` after such a call

An offline validator (eh/EhClCheck, run by the tests on every object they build in this model) checks (i) to (vi)
from the bytes of the object, so that a wrong table is a failed test, not a fail-fast on the day something is
thrown through that function.

The IP-to-state map is the range list (`;@ehrange` today) with states in place of pads, each entry at
`label + 1`; code outside every region is state -1. The frame gets one more 8-byte slot, set to -2 in the
prologue (the runtime writes the state at +0 and the try in progress at +4 during the search, and -2 again after
every catch); dispUnwindHelp is its offset from the establisher frame.

Funclets are emitted after the function's own blocks, in its section, under cl's names as local labels
(`?dtor$3@?0??f@@YAXH@Z@4HA`, `?catch$1@...`), so maps and debuggers show them as cl's:

    one destructor:   lea rcx, [rdx + offset of the object] / jmp <destructor>          no unwind entry
                      (an object that is a local of the frame; a member of *this or a heap object has its
                      address in a slot and takes the next shape)
    other cleanups:   push rbp / (push of the callee-saved registers its code writes) / sub rsp / (xmm saves) /
                      lea rbp, [rdx + K] / the pad's code / (xmm restores) / add rsp / pops / ret
                      unwind entry, no handler
    handler:          mov [rsp+16], rdx / push rbp / (push of the callee-saved registers it uses) /
                      sub rsp / lea rbp, [rdx + K] / the handler's code /
                      lea rax, <continuation> / add rsp / pops / ret
                      unwind entry with __CxxFrameHandler3 and the function's FuncInfo; dispFrame names the
                      slot of the saved rdx

K is the constant that makes `rbp` the function's own (`allocation + pushes after rbp`), so every `[rbp-x]` in
the funclet's code is the address it would be in the function. Nothing in a function body addresses the frame
through `rsp` except the outgoing argument area (X86.cpp emitCall), which a funclet has for itself.

A funclet saves what it writes in its own frame (finding 8): the callee-saved integer registers by pushes, xmm6
and up in its own allocation with its own `.savexmm128` codes, each funclet with its own set. It writes into the
function's frame only the slots the IR names, never the function's save areas.

A handler's variable is a frame local with a fixed offset (dispCatchObj): the runtime copy-constructs a class
caught by value into it, stores a caught scalar or pointer (converted) in it, or stores the object's address for
a reference. The funclet destroys a by-value class at its end, as today. A handler without a variable name has
dispCatchObj 0 and the runtime builds nothing, as cl's (`catch (E)` copies nothing with cl, and did copy in the
table model). `catch (...)` has adjectives 40h and no descriptor.

Objects built on one path only (the flagged ones: an arm of `?:`, an operand of `&&`) that are alive in a try
body when it is left by an exception: the table model has each handler destroy them first (IrGen
handlerCleanups). Here they are a state inside the try, around its whole body, whose action tests the flags and
destroys what is set (finding 9), so the runtime destroys them while it unwinds to tryLow, with
`uncaught_exception()` true and terminate if one throws. No terminate region exists in this model: a destructor
that throws during unwinding, a throwing copy into a handler's variable and the array cleanup inside a pad are
all the runtime's own cases (IrGen asserts that it opens none).

Leaving a handler: every edge from a handler's blocks to code outside it (the join after the try, `return`,
`break`, `continue`, `goto`) gets a continuation block in the function. The funclet stores what the target
needs in frame slots and returns the continuation's address; the continuation loads from the slots and jumps
on. A return value travels the same way. The runtime restores the callee-saved registers to their values at the
call that threw before it enters the continuation, which is what the table model's landing pads rely on today.

Register rules (X86.cpp, at -O1 and up; at -O0 everything is in memory already):
- a value live into a funclet, or live out of one, has a frame slot for its whole life (no register);
- a value live across the try and into the continuation, untouched by the handler, may stay in a callee-saved
  register (it has the call site's value there);
- inside a funclet any register may be used; callee-saved ones are saved by the funclet's prologue.

### 5.3 What changes where

The exception-specific parts named below are classes of cppparse/eh/ (section 9); the files named here call them.

| file | change |
|---|---|
| driver/main.cpp, Compiler.cpp | `--eh=cl` (a three-valued model in place of the bool `tableEh`), help text; `table` stays the default until stage 5 |
| middle/IrGen.cpp | throw: operand built in a frame temporary, ThrowInfo symbol, no malloc copy; the metadata for ThrowInfo / CatchableTypes (beside registerMetadata's type descriptors); catch: the handler's variable as the catch object, with descriptor and adjectives on the CatchParam, no `copyConstruct` and no terminate region for a by-value parameter (the runtime copies and terminates); no handler-unwind region and no `catch.handled` slot (`cppparse_catch_unwind` is the runtime's job); cleanup pads end with a funclet return instead of a rethrow, without `cppparse_unwind_enter/leave`; a terminate region becomes a try with a `catch (...)` that calls terminate. Regions, flags, exits, the order of destruction: unchanged |
| middle/Optimizer.cpp | handler and cleanup blocks are not merged with, or moved among, blocks outside their funclet; `CatchEnd` stays the border; removeDeadRegions / removeDeadUnwind unchanged in meaning |
| backend/X86.cpp | funclet partition and layout, the three funclet shapes, continuation blocks, the register rules, the unwind-help slot and its store, state numbering from the region tree, `;@eh...` lines for the tables (states, tries, handlers, IP ranges), `_CxxThrowException` calls. A function whose regions do not nest (computeRegions fails; today it falls back to setjmp frames) is an internal error in this model: none of sample-eh's functions falls back today |
| backend/Coff.cpp | builds FuncInfo, UnwindMap, TryBlockMap, HandlerType and IpToStateMap from the `;@eh...` lines into the function's .xdata, with ADDR32NB relocations (as it builds the CPPX table from `;@ehrange`); the handler data word; the funclets' .pdata entries in address order after their function's; `DD imagerel symbol` in data, for the throw tables; UHANDLER flag |
| backend/MangleSema.cpp | names of the ThrowInfo family and of the funclets |
| runtime/cppparse_rt.cpp | nothing for the exceptions themselves. `cppparse_terminate` and `cppparse_set_terminate` become the C runtime's (`terminate`, `set_terminate`; per thread, as cl's), so a mixed program has one terminate handler. `cppparse_uncaught_exception` answers "the table model's own state, or `__uncaught_exception()`": the table and sjlj models' throws never pass through vcruntime, so the C runtime's answer alone would be false during their unwinding (finding 3). Stage 0: `dispatch()` and `cppparse_eh_handler` stop with a message when an exception of one family reaches a frame of the other that has something to destroy or a handler to try (finding 10). The table and sjlj code stays |
| include/exception, libcpp | unchanged sources (they call the three runtime functions above); libcpp/iostream.cpp is compiled in the program's model, as every script does already. `operator new`'s `std::bad_alloc` goes through `_CxxThrowException` like any throw |
| the ml64 path | the FuncInfo lines are `;@` comments to ml64, as `;@ehrange` is, so an ml64-built object of an `--eh=cl` listing would have funclets and no FuncInfo: exceptions would pass its frames with nothing run and no message. An `--eh=cl` listing therefore starts with a line ml64 refuses (`.ERR`, which the COFF writer skips), and `--assemble` with `--eh=cl` is an error of the driver (finding 16). Test 6 keeps `--eh=sjlj` |
| sandbox scripts | eh.ps1: model `cl` in the matrix and the cross-compiler cases (section 6); a `-Eh` switch for lib.ps1, life.ps1, matrix.ps1, selfhost.ps1 and build-cppparse.ps1 so the whole harness can run in the new model before it is the default |

## 6. Plan

Every stage ends with harness.ps1 16 of 16 in the default model (the /W4 build, selfhost -O0 and -O2 with
generation two identical, strict98, crc32.txt rewritten) and its own exit test. The cross-compiler program
("ehx": the seven paths of section 1 with the three thrown types, extended per stage) is built four ways, cl+cl
as reference, cl+cppparse, cppparse+cl, cppparse+cppparse, at -O0 to -O3, and every output must equal the
reference's.

| stage | what | exit test |
|---|---|---|
| 0 | Source layout (section 9): the folder cppparse/eh/ and its classes; the table and sjlj models' exception code moves there from X86.cpp and Coff.cpp, nothing else changes | every .asm and .obj the suite, lib, life, eh and matrix scripts and the self-host produce is byte for byte what the commit before produced (a script compares the two trees); harness 16 of 16 |
| 0b | The stopgap of finding 10: the runtime stops with a message when an exception of cppparse's models reaches a C++ frame of cl's with a state to unwind or a try to search, and when a cl exception reaches a table-model frame inside a region | the seven paths of section 1: the four silent ones and the two fail-fast ones now end with the message and terminate, the first still passes; harness 16 of 16 (no check may rely on a silent crossing) |
| 1 | `--eh=cl` accepted (refused with `--assemble`). Throw side: ThrowInfo family with content names, one temporary per throw, `_CxxThrowException`, rethrow | sample-ehx `throw`: cppparse functions throw every kind of type of sample-eh's probe.cpp (scalars, classes, bases at offsets, virtual bases, pointers with conversions, const and volatile, `char*` beside `const char*` and `volatile char*`, `Err` at two offsets in two classes) from two objects; cl's handlers take each as they take cl's own throw (which handler, the value, the copies and destructions), at -O0..-O3 |
| 2 | The FH3 table checker first (eh/EhClCheck over objects). Cleanups: states by region stack, UnwindMap, IpToStateMap, FuncInfo, the unwind-help slot, cleanup funclets of both shapes with their own register saves, UHANDLER, `int 3` at range ends. Functions with a try are still refused in this model | the checker passes on every object of the own sources, libcpp and the samples compiled with `--eh=cl` where no function has a try, and reports the count of functions whose regions do not nest (must be 0). ehx `unwind`: cl throws through cppparse's frames, cl catches: plain objects, temporaries of `?:` and `&&` (flagged), arrays partly built, a constructor that throws in a new-expression, members of a throwing constructor, a throwing static initialiser, a pad that uses xmm registers at -O2: destructor counts and order equal the all-cl build at -O0..-O3; unwind.ps1 passes on the new frames; the spill counts against the table model's are recorded |
| 3 | First the f.asm probe extended: its handler funclet calls a cl function that throws, once with a try in the handler, once without (finding 6). Handlers: catch funclets, HandlerTypes, TryBlockMap, continuations, the register rules; nested tries, a try in a handler, function-try-blocks, loops around handlers, return / break / goto out of a handler; flagged objects as a state of the try | eh.ps1 with model `cl` for probe.cpp and probe-mix.cpp at -O0..-O3: output equal to cl's; the four terminate programs equal cl's output and exit code; ehx `catch` in all four builds, with the cases of finding 17 (a rethrow from a function a handler calls, both directions; a throw whose copy constructor throws; by-value catch of a class with virtual bases from the other compiler; an exception leaving a handler into an outer try of the same function; the two-throws program of finding 2; a throw in a loop with the handler in the same function and the counter in a register); ehx with cl /EHa on the cl side; the checker on every object. The self-host with `-Eh cl` at -O0 and -O2, generation two identical, and the suite under that compiler (finding 12) |
| 4 | One runtime state: terminate, set_terminate, uncaught_exception; the library; `/FAILIFMISMATCH` per model family in every object's `.drectve` | ehx cases: uncaught_exception in a destructor of either side during the other's unwinding; a terminate handler set by one side, reached from the other; `std::runtime_error` across with one library (cl /X) by value and by reference, and with two libraries by reference; `std::bad_alloc` from libcpp's operator new caught by cl (and whether its CatchableType needs cl's property 10h). ehx linked four more ways: /MD, /DEBUG /INCREMENTAL, /OPT:REF,ICF (finding 15). link refuses an `--eh=cl` object with a `table` one. The harness with `-Eh cl` everywhere: suite, mix, lib, life, matrix, cprog. Fault injection (finding 12): the self-built compiler of each model with an operator new that throws at the k-th allocation, k swept over a mid-size input: every run ends with the compiler's out-of-memory message, none with 0xC0000409 or an access violation |
| 5 | Measure; decide the default by the criteria | table against cl model: matrix sample (time, code bytes, mapcompare's ratio to cl), own sources at -O0 and -O2 (object bytes, selfhost time, spills), probe.cpp sizes as in section 3, the throw cost of probe t.cpp with cppparse's frames and through a function of 1,000 regions. Default becomes `cl` only when code + tables are within 3 % of the table model's, the matrix time within its run-to-run spread, and every gate passes in both models; otherwise it stays `table` and the numbers are recorded. README, help text, next-targets, this document's status |
| later, own rows | FH4 tables; retiring `sjlj`; P1's handler | |

eh.ps1's model combinations after stage 3: `sjlj`, `table`, `table+sjlj`, `sjlj+table` as today, and `cl` (both
probe files in it). `cl` is not combined with `table` or `sjlj` in one program: an exception crossing the two
families stops with the runtime's message (stage 0b), and from stage 4 on link refuses the mix.

## 7. Risks and estimate

1. **State shape.** FH3 wants try states contiguous and nested; cppparse's regions are opened, suspended at the
   arms of `?:` and reopened under a flag (IrGen suspendRegions, openDeferredRegions). States numbered by region
   stack (5.2) have one `toState` each whatever IrGen does; what remains is a function whose regions do not nest
   the same way on every path (X86 computeRegions fails, and the table model falls back to setjmp frames for
   it). Stage 2 counts those over the own sources and all samples before any funclet exists; each one is an
   IrGen fix.
2. **A table error fails fast without a message.** The runtime checks its tables only by `_VCRT_VERIFY`
   (0xC0000409, the exit code M7 starts from). The checker of stage 2 is the answer; whatever it does not check
   will be found late.
3. **The optimiser and funclet borders.** Block merging, jump threading and inlining must not make a block that
   is entered both from a handler and from outside it without passing a continuation. A verifier in X86.cpp
   (every block belongs to exactly one funclet or to the function; every edge out of a funclet is a
   continuation) turns a miscompilation into an internal error.
4. **Errors that show only when something is thrown through that very function**, as with the unwind codes in
   the table design. Covered by eh.ps1 at four levels, the harness in the new model, and the own sources.
5. **cl is a preview build.** eh.ps1 already documents a crash of cl 19.52's own /EHsc code at /O1 and up
   (the reference is built with /EHa for it). The probes here ran correctly with both FH3 and FH4; FH3's
   runtime path is the older one. A toolset update may move numbers in section 3, not the formats.
6. **Debug information and maps** for funclets (CodeView procedure lengths, mapcompare.ps1's function lists):
   small, but they are harness checks.
7. **Two standard libraries in one link** stay a limit (section 2); the design can document it, not remove it.

Estimate, in working days of one lane, each with its harness runs: stage 0: 3 to 4; stage 0b: 1; stage 1: 3 to
4; stage 2 with the checker: 6 to 8; stage 3 with the probe and the self-host: 9 to 13; stage 4 with the link
variants and the fault injection: 5 to 7; stage 5: 2 to 3. Together 29 to 40 days, six to eight weeks; the first
text had 22 to 31 without stage 0, the checker, the self-host in stage 3 and the fault injection. Stage 3 holds
most of the uncertainty (the register rules and the continuations at -O2 and -O3). After stage 3 M7's row is
functionally closed behind `--eh=cl`; stages 4 and 5 make it whole and measure it.

## 8. Questions, answered by the go-ahead

1. The default after stage 5: `cl` only when the criteria of stage 5 hold, else `table` stays and the numbers
   are recorded.
2. `table` and `sjlj` both stay in this version; removing one is a later decision.
3. FH4 is not part of M7; its own row afterwards.

## 9. Source layout

The exception handling code is in a folder of its own, cppparse/eh/, as classes that X86.cpp, Coff.cpp and
IrGen.cpp call through small interfaces (the user's direction of 2026-11-18). Stage 0 moves what exists there
without changing a byte of output; everything `--eh=cl` needs is written there from the start. The folder is one
more entry of the source folder lists (build-cppparse.ps1 and scripts\toolset.ps1 `$SourceDirs`, the Visual
Studio project, README).

| file | class | what | from |
|---|---|---|---|
| EhModel.hpp | `EhModel` | the three models and their names (`sjlj`, `table`, `cl`) in place of two bools | driver, X86Module |
| EhRegions.hpp/.cpp | `EhRegions` | the region stack at each block of a function, the regions around each region, the exception edges (a call in a region may continue at its handlers and those around) | X86Function::computeRegions, computeEhEdges |
| EhEmitter.hpp | `EhEmitter` | the interface the models write code through: an instruction line, a comment line for the assembler, a block's label, an external's use. X86Function implements it | new |
| EhSjlj.hpp/.cpp | `EhSjlj` | the setjmp model's code: the frame's size, try begin (`cppparse_push_frame`, `_setjmp`), try end, the pop at a dispatch stub | X86Function::emitInst TryBegin/TryEnd, emitBody |
| EhTable.hpp/.cpp | `EhRanges` | the table model's code ranges (the labels that open and close them, the `;@ehrange` lines) | X86Function::setRegions, emitBody |
| EhThrow.hpp/.cpp | `EhThrow` | a throw's code in each model: `cppparse_throw`, `cppparse_throw_owned`, `cppparse_rethrow`; `_CxxThrowException` | X86Function::emitInst Throw |
| EhTable.hpp/.cpp (also) | `EhPads` | the landing pads' protocol of the two older models: the dispatch stub's clauses, a handler's begin and end | X86Function::emitBody, emitInst |
| EhUnwindInfo.hpp/.cpp | `EhUnwindInfo`, `EhRangeTable` | the bytes of an UNWIND_INFO from a prologue's steps; the `CPPX` range table behind the handler | Coff.cpp buildUnwindInfo |
| EhClThrowInfo.hpp/.cpp | `EhClThrowInfo` | ThrowInfo, CatchableTypeArray, CatchableType: names and data from a neutral description of the thrown type (IrGen supplies it from the semantic types) | new (stage 1) |
| EhClFunclets.hpp/.cpp | `EhClFunclets` | cl's model for one function: the owner of every block, the funclets, the states by region stack, the TryBlockMap, the block order, the values that cross a funclet's border; the funclets' prologues and epilogues, the labels of the IP map, the `;@eh` lines | new (stages 2, 3) |
| EhClFuncInfo.hpp/.cpp | `EhClFuncInfo`, `EhClCheck` | the FuncInfo and its maps as bytes and relocations, with the unwind info of the funclets; the checker of invariants (i) to (vi) of section 5.2 over the tables the object writer gets | new (stage 2) |

What stays where it is: the register allocator and the frame layout (X86.cpp), which ask `EhRegions` and
`EhClFunclets` for edges and constraints; the object writer (Coff.cpp), which hands a PROC's prologue steps and
`;@` lines to the builders and places the bytes they return; IrGen's regions and pads, which are the language's
semantics and the same in all models; the runtime (runtime/cppparse_rt.cpp), which is not part of the compiler.

## 10. Review and changes

The independent review of commit c89d752 (2026-11-18) agreed with option (a) and with section 3's facts, and
found two contract errors, a regression, a size estimate half too low and several open funclet details. Each
finding and what was done:

| # | finding | done |
|---|---|---|
| 1 blocker | the COMDAT names of the throw tables did not identify their contents (`char*` and `const char*` share a descriptor; `Err` at two offsets) | names carry every field, spelled or hashed (5.1); stage 1 tests the colliding cases from two objects |
| 2 blocker | one shared slot for thrown scalars would be the exception object of two live exceptions | one temporary per throw expression, no cleanup region for it, a no-sharing rule for slots a funclet or the runtime addresses (5.1); the two-throws program is a stage 3 test |
| 3 | `uncaught_exception` "one with the C runtime's in every model" would be false during a table-model unwind | the runtime answers its own state or the C runtime's (5.3); `set_terminate` per thread written down as a limit (2) |
| 4 | tables estimated at half of what the counts give; the 3 % criterion cannot hold with FH3 | the table in section 4 recomputed (+20 % instead of +6 %); stage 5 stays a measurement with the criteria as set, the expected outcome (default stays `table`, FH4 the way on) is stated; spill counts recorded in stage 2 |
| 5 | number states by region stack, state the FH3 invariants, check them offline | 5.2: states are the trie of the stacks; invariants (i) to (vi); the checker EhClCheck is built first in stage 2 and run on every object of the tests; the count of functions whose regions do not nest is reported |
| 6 | every address of a handler funclet must lie in the catch range and the funclet must begin its own function table entry; the probe never had the hand-written handler call something that throws | invariant (v); the probe is extended before stage 3 and both cases are ehx tests |
| 7 | a call that does not return at the end of a function table range | invariant (vi), `int 3` at every range end |
| 8 | funclets must save the registers they write, xmm in their own frames; `this` and parameters in registers; the one-jump shape only for frame locals | 5.2: the shapes and the rule; a value used on both sides of a funclet's border lives in the frame; a pad with xmm registers is a stage 2 test |
| 9 | flagged objects at a handler's entry should be states; the terminate-region rewrite was wrong in kind | 5.2: one state around the try's body with a flag-testing action; no terminate regions in this model |
| 10 | the silent crossing stays between cppparse's own model families, and with cl until stage 5 | stage 0b now: the runtime stops with a message; `/FAILIFMISMATCH` per family in stage 4 |
| 11 | unnamed by-value handler copies where cl does not; two matches differ from C++98 | dispCatchObj 0 without a name (5.2); the two matches are limits (2) |
| 12 | the self-host came late and hardly unwinds | self-host in the model moved to stage 3; fault injection in stage 4 |
| 13 | the appendix's IP map destroyed `g` twice if its destructor threw | corrected: the continuation's destructor call is state -1 |
| 14 | Coff.cpp needs more than tables: funclets in the function's COMDAT section, static symbols, CodeView, the implicit copy constructor and `??_D` emitted where a type is thrown | funclets are code of the same PROC under local labels (no symbols, one section, one CodeView procedure); IrGen marks the special members a ThrowInfo names as needed (5.1) |
| 15 | link variants | /MD, /DEBUG /INCREMENTAL, /OPT:REF,ICF in stage 4; `/guard:ehcont` not supported, a limit (2) |
| 16 | the ml64 path should refuse `--eh=cl` | `.ERR` at the head of the listing, `--assemble` refused (5.3) |
| 17 | tests the stage list did not name | in stages 3 and 4 |
| 18 | P1's estimate rested on destructors standing in for `__except` filters | text corrected (4) |
| 19 | the throw cost was measured with cl's frames | text corrected (4); measured with cppparse's frames in stage 5 |

Two facts from the review's reading of the runtime, recorded in 5.2: the runtime puts -2 back into the
unwind-help slot after every catch, and writes the state and the try in progress into it during the search, so
the slot is the full 8 bytes.

## Appendix: the probes

The probe sources were scratch files; the two that define contracts are short enough to restate.

The hand-built throw (probe c.cpp), accepted by cl's `catch (int)`:

    struct TypeDesc     { const void* vftable; void* spare; char name[8]; };       // ??_7type_info@@6B@, 0, ".H"
    struct CatchableT   { unsigned props; int type; int mdisp, pdisp, vdisp; int size; int copy; };   // 1, rva, 0, -1, 0, 4, 0
    struct CatchableArr { int count; int types[1]; };
    struct ThrowInf     { unsigned attrs; int unwind; int forward; int array; };
    ULONG_PTR args[4] = { 0x19930520, (ULONG_PTR)&object, (ULONG_PTR)&throwInfo, (ULONG_PTR)&__ImageBase };
    RaiseException(0xE06D7363, EXCEPTION_NONCONTINUABLE, 4, args);

The hand-written frame (probe f.asm), `int asm_catch(void (*f)(int), int kind) { Guard g(100); try { f(kind); }
catch (int v) { return v; } catch (...) { return -1; } return 0; }` in cppparse's frame shape; rbp is the
establisher frame + 80, g at [rbp-20], v at [rbp-24], the unwind-help slot at [rbp-32], the result at [rbp-40]:

    asm_catch:  push rbp / mov rbp, rsp / push rbx / sub rsp, 72
                mov qword ptr [rbp-32], -2
                mov dword ptr [rbp-20], 100 / mov rax, rcx / mov ecx, edx
    try_b:      call rax
    try_e:      lea rcx, [rbp-20] / call ??1Guard@@QEAA@XZ / xor eax, eax
    fn_exit:    lea rsp, [rbp-8] / pop rbx / pop rbp / ret
    cont:       lea rcx, [rbp-20] / call ??1Guard@@QEAA@XZ / mov eax, [rbp-40] / jmp fn_exit
    dtor0:      lea rcx, [rdx+60] / jmp ??1Guard@@QEAA@XZ
    catch1:     mov [rsp+16], rdx / push rbp / sub rsp, 32 / lea rbp, [rdx+80]
                mov eax, [rbp-24] / mov [rbp-40], eax
                lea rax, cont / add rsp, 32 / pop rbp / ret
    catch2:     the same, with mov dword ptr [rbp-40], -1

    unwind info of asm_catch   19h, 9, 3, 0 / codes 9,82h 5,30h 1,50h / RVA __CxxFrameHandler3 / RVA funcinfo
    unwind info of catch1, 2   19h, 10, 2, 0 / codes 10,32h 6,50h / RVA __CxxFrameHandler3 / RVA funcinfo
    funcinfo     19930522h, 3, RVA unwindmap, 1, RVA trymap, 4, RVA ip2state, 48, 0, 1
    unwindmap    { -1, RVA dtor0 }  { 0, 0 }  { 0, 0 }
    trymap       { 1, 1, 2, 2, RVA handlers }
    handlers     { 0, RVA descriptor of int, 56, RVA catch1, 56 }  { 40h, 0, 0, RVA catch2, 56 }
    ip2state     { try_b+1, 1 }  { try_e+1, -1 }  { catch1+1, 2 }  { catch2+1, 2 }
                 (the probe as run also had { cont+1, 0 }, which would run dtor0 a second time if ~Guard threw
                 in `cont`: a call that ends an object's life lies outside its state, as at try_e)
    .pdata       asm_catch, catch1, catch2, in address order

A frame with a destructor and no try is the same with unwind flags 11h (UHANDLER only), one state, no try map.
