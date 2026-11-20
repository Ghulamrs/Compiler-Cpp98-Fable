# Design: table-based exceptions (2026-10-12)

Status: done (2026-10-12), steps 1-5. Table mode is the default; `--eh=sjlj` keeps the setjmp model.
Results on the matrix sample at -O2: 278.6 ms and 70,053 bytes of code (setjmp model: 295.1 ms, 75,607 bytes).
In the map comparison, cppparse's code is 1.57 times cl's for the 43 functions both compilers emit (1.71 with setjmp).
Not done, as planned: the optimiser still keeps what handlers read in memory and runs no GVN in functions with
regions. Lifting that needs an exception edge from every call to its landing pad (section 7).

## 1. The problem in one paragraph

Today every `try`, and every block that owns an object with a destructor, costs code on the normal path,
even when nothing is ever thrown:

    lea rcx, [frame] ; call cppparse_push_frame        ; on entry to the region
    lea rcx, [frame] ; xor edx, edx ; call _setjmp      ; saves all registers into a 272-byte buffer
    test eax, eax    ; jne dispatch
    ...
    lea rcx, [frame] ; call cppparse_pop_frame          ; at every exit of the region (return, break, ...)

Because `longjmp` only brings back the callee-saved registers, such a function may not use the volatile
registers r8-r11, the optimiser may not run GVN in it, and locals read by a handler stay in memory.

In the matrix sample at -O2 (matrix + solver) there are **80 such regions**. Only 3 of them have a `catch` clause
(2 in matrix, 1 in solver); all the others exist only to run destructors. That is where cppparse's
2-7x size against cl comes from in functions such as `mtxSizeMismatchThrows` (127 vs 905 bytes) and
`mtxSingularThrows` (169 vs 988). It also explains the 4 values the `operator*` inner loop keeps in memory.

cl and clang pay **nothing** on the normal path. The compiler writes tables instead of code, and the
tables are only read when something is thrown. This design does the same.

## 2. The idea

1. For every function, the compiler writes an **unwind table entry**: "this function pushed rbx and rsi,
   then took 120 bytes of stack". With it, Windows can step from any function back to its caller and
   restore that caller's registers exactly. These are the standard Windows x64 `.pdata` / `.xdata`
   sections; cl writes them for every function too.
2. For each function with try or cleanup regions, the compiler also writes a small **range table**:
   "code from A to B belongs to a region; if an exception passes through there, continue at landing pad P".
3. `throw` calls the runtime, which **walks the stack** with the Windows functions `RtlLookupFunctionEntry`
   and `RtlVirtualUnwind`. In each frame it looks up the return address in that function's range table.
4. When a range matches, the runtime **jumps to the landing pad** with `RtlRestoreContext`. All callee-saved
   registers then hold the values they had in that frame when the call was made. The landing pad is
   ordinary code in the function: it runs the destructors, then tests the catch clauses (the same
   `cppparse_match` calls as today). If none matches, it calls `cppparse_resume` and the walk continues
   in the caller.

So a landing pad behaves like a second return point of the call. To the register allocator, a value
needed at the landing pad is just a value that lives across the call, which it already handles.

## 3. What the generated code looks like

    Before (setjmp)                              After (tables)
    ---------------                              --------------
    lea  rcx, [rbp-288]                          ; nothing: the region is a range in a table
    call cppparse_push_frame
    lea  rcx, [rbp-288]
    xor  edx, edx
    call _setjmp
    test eax, eax
    jne  dispatch
    ... body ...                                 ... body ...
    lea  rcx, [rbp-288]                          ; nothing
    call cppparse_pop_frame
    ...
    dispatch:                                    landing_pad:
    lea  rcx, [rbp-288]                            ... destructors (as today) ...
    call cppparse_pop_frame                        lea rcx, "Error" ; call cppparse_match ; jne handler
    ... destructors, cppparse_match ...            call cppparse_resume       ; not ours: continue walking

The normal path loses 2 calls on entry, 1 call at each exit and the 272-byte buffer. My estimate is about
75 bytes of code per region, or about 6 KB of the 75 KB in the matrix sample. On top of that, try
functions get r8-r11 and GVN back.

## 4. The tables

**Unwind info (standard Windows format, every function with a frame):**

    .pdata   : begin RVA, end RVA, unwind-info RVA                     (12 bytes per function)
    .xdata   : version 1, flags EHANDLER (only in functions with regions), prologue size,
               unwind codes (PUSH_NONVOL, ALLOC_SMALL/LARGE, SET_FPREG, SAVE_XMM128),
               handler RVA = cppparse_eh_handler, then our range table

Frameless leaf functions (no stack, no callee-saved register) need no entry; Windows already assumes
"return address at [rsp]" for them.

**Range table (ours, follows the handler RVA):**

    u32 magic "CPPX", u32 entry count, u32 pad count
    entry count x u32 { code offset in the function << 12 | pad number }    sorted by offset
    pad count x u32 landing-pad RVA

An entry holds from its code offset (20 bits, relative to the function's start, which the runtime takes
from the RUNTIME_FUNCTION) to the next entry's; a pad number of 0 marks code outside every region (a gap
between two ranges, and the end of the last one, which is always an entry); pad n is the n-th RVA of the
pad table. The entries are plain words, only the pad table needs relocations. The object writer builds
this from the `;@ehrange begin, end, pad` lines of the listing (2026-10-30; before that every range was
a begin, end, pad triple of RVAs, 12 bytes and three relocations: with a region per object, A3, the
tables of the own sources at -O0, 53,000 ranges, would have cost more than the code they save).

Nested regions need no extra field. The `cppparse_resume` call of an inner landing pad lies inside the
outer region's range, so the walk finds the outer pad next by itself.

`cppparse_eh_handler` is a function in the runtime that Windows may call during its own SEH dispatch
(an access violation, or a cl `throw` passing through). It always answers "continue search". The runtime
uses its address only to recognise "this frame was compiled by cppparse, its range table follows".

**COFF:** every entry needs the IMAGE_REL_AMD64_ADDR32NB relocation (image-relative; today the writer has
REL32 and ADDR64). A COMDAT function's `.pdata`/`.xdata` must go in an *associative* COMDAT section
(selection 5), so the linker drops them together with the function. cl does the same.

## 5. The frame shape

**As built (step 1):** no layout change was needed after all. `rsp` never moves in a function body, so the
unwind codes can describe the whole frame from `rsp` without a frame register. `rbp` is declared as one more
pushed register (`.pushreg rbp`), `mov rbp, rsp` needs no code of its own, and each xmm save at `[rbp-x]` is
described as `.savexmm128 xmmN, frameSize-x` (its offset from `rsp` after the prologue). The text below was the
original plan, kept for reference.



Windows unwind codes can only describe saves at **positive** offsets from the frame base. Today's
prologue sets `rbp` first and stores xmm6-15 below it (`movdqu [rbp-x], xmm6`), which the format cannot
express. The new shape is the one cl and clang use:

    push rbp                     ; UWOP_PUSH_NONVOL
    push rbx / rsi / ...         ; UWOP_PUSH_NONVOL each
    sub  rsp, N                  ; UWOP_ALLOC_SMALL / LARGE (with __chkstk for N >= 4096, as today)
    movaps [rsp+x], xmm6         ; UWOP_SAVE_XMM128 (x/16)
    mov  rbp, rsp                ; UWOP_SET_FPREG, offset 0
    ... locals at [rbp+x], positive offsets ...
    epilogue: movaps xmm6, [rbp+x] ; lea rsp, [rbp+N] ; pop ... ; pop rbp ; ret

This is the official epilogue form, so even an asynchronous unwind in the epilogue is correct. What changes is
`layoutFrame`, the prologue/epilogue code and the frame addresses in X86.cpp: most go through `localRef`,
plus about 15 places that write `[rbp-...]` directly.

A later option, not part of this project: no frame pointer at all (address through rsp, like cl). That
gives the allocator one more register.

## 6. The runtime (cppparse_rt.cpp)

    cppparse_throw / cppparse_throw_owned / cppparse_rethrow   same interface as today
      -> record the exception (as today)
      -> phase 1 (search): walk the stack without changing anything until a frame whose range table
         covers its return address is found. If there is none: "unhandled exception of type X", abort
         (as today, and before any destructor has run, as C++ allows).
      -> phase 2: walk again and RtlRestoreContext into the first landing pad.
    cppparse_resume       called by a landing pad whose clauses did not match: continue phase 2 from its caller
    cppparse_eh_handler   returns ExceptionContinueSearch
    cppparse_match / catch_begin / catch_end                   unchanged

Phase 1 cannot tell whether a landing pad will catch (its clauses are code), so "handled" means "some
cppparse frame has a region here". An unhandled exception therefore runs destructors before aborting.
That is allowed by C++ ("implementation-defined whether the stack is unwound").

`cppparse_frame_depth()` returns 0 in table mode (no frames are pushed), so the driver checks stay valid.

**As built, and the cost of a throw (2026-11-19, row G24).** There is one phase: `dispatch` walks from the
runtime function the generated code called to the first frame whose range table has a landing pad for its call
site and enters it with `RtlRestoreContext`; a pad that does not keep the exception calls `cppparse_rethrow`, or
`cppparse_unwind_destroy(object, destructor)` when its cleanup is one destructor call, and the walk goes on from
there. M7 measured a throw out of one frame with 1,000 objects alive at 3.1 ms (cl 0.06 ms). The cause, by cycle
counters in the runtime: each of the 1,000 pads started a dispatch of its own, and of its 7,000 cycles 5,100
were `rangePad` reading the range table from the front (the pads lie behind the function's body, so the call site
in a pad is behind nearly all of the 2,500 entries), 800 the lookups and virtual unwinds of four frames (three of
them the runtime's own: dispatch, cppparse_rethrow, cppparse_unwind_destroy) and 900 `RtlRestoreContext`, the pad
and the destructor. Three changes, all in the runtime, no object byte different:

1. `rangePad` is a binary search (the entries are sorted by code offset).
2. A frame's landing pads follow one another without a dispatch: when the call of a pad lies in the range of an
   outer region of the same function (the object built before), the next landing pad is one of the same frame. The
   pad finds what `RtlRestoreContext` would give it: the frame's stack pointer (it never moves in a body) and the
   callee-saved registers as they are at the call. A frame is left with one dispatch however many objects it
   holds. (In this row's first form `cppparse_unwind_destroy` looked its own call site up and returned into the
   next pad by writing its return address; row G30, below, took that out: the generated code goes to the pad.)
3. The walk starts in the runtime function that the generated code called (`DISPATCH` captures the context
   there), so it passes one frame of the runtime where it passed three.

Best of three runs of the probe (each the best of 15 loops; idle machine; frames at `-O0` / `-O2`, cl `/Od` / `/O2`):

| what | cl /EHsc | `--eh=table` before | `--eh=table` after | `--eh=cl` |
|---|---|---|---|---|
| a throw out of one frame with 1,000 objects alive | 61.4 / 60.7 us | 2,966 / 2,967 us | 74.4 / 76.2 us | 98.5 / 97.5 us |
| that function without a throw | 3.87 / 3.50 us | 4.46 / 4.58 us | 4.46 / 4.57 us | 4.51 / 4.94 us |
| a throw through 10 frames with one object each | 6.12 / 5.72 us | 7.00 / 7.16 us | 5.53 / 5.64 us | 5.77 / 5.80 us |
| the same path without a throw | 56.5 / 48.5 ns | 78.4 / 58.1 ns | 78.5 / 57.1 ns | 80.4 / 59.3 ns |

The objects are the same bytes before and after (the 1,000-object unit: 47,924 bytes of code and 16,100 of unwind
tables at `-O0`, 62,010 and 16,116 at `-O2`; cl's model 40,980 and 24,184, 55,082 and 24,200; cl 59,992 and
30,864 at `/Od`, 42,672 and 9,955 at `/O2`). `RtlRestoreContext` into a pad that no `/guard:ehcont` table names
does not pass that check (limit 2 of docs/design-cl-exceptions.md): for such a program the model is `--eh=cl`.
eh.ps1 `ehManyObjects` runs a frame of
18 objects with an inner block, a destructor that throws and catches on its own between two pads, and a value the
handler reads, in every model at `-O0`..`-O3`. Left: what a pad with more than one cleanup costs (its
`cppparse_unwind_enter`, destructors, `cppparse_unwind_leave` and `cppparse_rethrow` are a dispatch for each pad,
as before; such pads are the flagged temporaries of a full expression, few to a frame), and the range table's
fields (20 bits of code offset, 12 bits of pad number: the object writer refuses a function of more than 4,095
pads or 1 MB of code, "too many landing pads or too much code for the range table").

**The pads of a frame, by the generated code (2026-11-20, row G30).** Change 2 above, as first built, made
`cppparse_unwind_destroy` write its own return address: the function returned into the next pad. A return to an
address other than the call's is what a shadow stack (CET) refuses, and nothing in the runtime kept the write in
bounds; the user asked for it to be resolved. The next pad of a frame is known where the code is generated: it is
the pad of the innermost region open at the pad's own call (`EhRanges::now`), the block the range table names or
the region's dispatch stub. So the pad of an object in a region of its function ends with

    call cppparse_unwind_destroy_next     ; destroys the object while the exception is parked, and returns
    jmp  <that region's landing pad>

(`EhPads::isDestroy`, `destroyNext` and `next` in cppparse/eh/EhTable; `X86Function::nextPad`), and a pad in no
region keeps `call cppparse_unwind_destroy`, which does not return: the exception leaves the frame through
`dispatch`. The layout places such a pad right before the pad it goes on at (`X86Function::orderPads`: the pads of
objects built one after another lie newest first), so the jump is not written and the 1,000-object unit has the
bytes of code it had (47,924 and 62,010). The runtime lost its look-up of the call site and the function it noted
for it; it reads its return address in one place (`cppparse_unwind_enter` notes the frame) and writes none. An
object compiled before this row calls `cppparse_unwind_destroy` in every pad and is sent on by a dispatch for each,
as before row G24 but with the binary search. IR, optimiser and register allocator are as they were: the pad's call
was an exception edge to the pads around it already, and the pad that follows finds the frame and the callee-saved
registers as at that call. Measured with the probe of the table above (best of three runs, `-O0` / `-O2`, cl `/Od` /
`/O2`; another lane was building on the machine, which leaves cl's and `--eh=cl`'s figures a few percent above the
idle ones of that table):

| what | cl /EHsc | `--eh=table`, row G24 | `--eh=table` now | `--eh=cl` |
|---|---|---|---|---|
| a throw out of one frame with 1,000 objects alive | 63.2 / 65.1 us | 74.4 / 76.2 us | 13.4 / 12.4 us | 103.2 / 105.0 us |
| that function without a throw | 3.99 / 3.72 us | 4.46 / 4.57 us | 4.73 / 4.72 us | 4.61 / 5.23 us |
| a throw through 10 frames with one object each | 6.22 / 6.11 us | 5.53 / 5.64 us | 5.54 / 5.65 us | 6.01 / 6.09 us |
| the same path without a throw | 60.7 / 52.6 ns | 78.5 / 57.1 ns | 81.0 / 59.9 ns | 84.2 / 63.0 ns |

The suite reads front.cpp's listings (with the lean landing pads): every call of `cppparse_unwind_destroy_next` is
followed by a jump or by the label of the pad laid out next, and the runtime's source reads its return address in
one place and writes none. eh.ps1 `ehPadChain` runs chains that end in `catch (...)` and in clauses of which none
matches, with an array and a temporary built under a condition among the pads, in the turns of a loop, in every
model at `-O0`..`-O3` (the probe prints 106 lines). Not claimed: this machine's processor has no CET, so no run
under a shadow stack was made. Every return of the model's code and of its runtime goes to the instruction after
its call, which is the shadow stack's rule; a frame's first pad is entered by `RtlRestoreContext` as before (limit 2
of docs/design-cl-exceptions.md, unchanged).

## 7. Optimiser and register allocator

- In table mode, a call that may throw inside a region **ends its block** with two exits: the next block
  and the landing pad (LLVM's "invoke"). Liveness, SSA phis and the allocator then see the real control
  flow, with no special cases.
- In table mode, the setjmp workarounds go away: `pinHandlerLocals`, "no GVN in try functions",
  "no volatile registers in try functions" and the `handlersSelfContained` revert.
- At a landing pad, volatile registers are garbage. That is already true after any call, and values live
  into the pad are live across the call, so they get callee-saved registers or stack slots.
- Done in the back end only (2026-10-13): the register allocator's liveness has an edge from every block
  whose calls or throws lie in a region to the handlers of that region and of the regions around it
  (X86.cpp computeEhEdges, used by computeIntervals). Without it, a value the code after a catch needs (a
  string literal hoisted out of a loop whose body catches) was dead at an inlined throw laid out after the
  loop, which reused its callee-saved register; the landing pad then resumed with the throw's value in it.
  The IR does not split blocks at calls yet, so the optimiser's workarounds stay.

## 8. Limits (same as today)

- Destructors in **cl-compiled** frames between `throw` and `catch` are not run, and a cl `throw` is not
  caught by a cppparse `catch`. Today's `longjmp` has the same limit. Full interoperation would mean
  writing MSVC's own FuncInfo / ThrowInfo / RTTI tables; that is a possible later project.
- Linux (version 7): the same model maps onto `.eh_frame` (CFI instead of unwind codes) and
  `_Unwind_RaiseException` with a personality routine. The range table and the landing pads stay the same.

## 9. Steps (each one: full suite + matrix, lib, life, selfhost, mix, cprog, mapcompare; then commit)

| Step | What | How it is tested | Visible result |
|---|---|---|---|
| 1 | New frame shape + `.pdata`/`.xdata` for every function (both modes), ADDR32NB, associative COMDAT | `dumpbin /unwindinfo` check in build-cppparse.ps1; a runtime test walks the stack from inside nested cppparse calls with `RtlVirtualUnwind` and checks every return address and restored register | Debugger call stacks and crash dumps through cppparse code become correct; no speed change |
| 2 | Runtime: two-phase walker, `cppparse_resume`, `cppparse_eh_handler` | Through step 3 | — |
| 3 | Code generator `--eh=table`: ranges, landing pads, range tables. setjmp stays the default | Every script run in both modes; outputs identical | Normal path without push/setjmp/pop |
| 4 | Optimiser/allocator in table mode: calls end blocks, setjmp workarounds removed | As step 3, at -O0 to -O3 | r8-r11 and GVN in try functions |
| 5 | Table mode becomes the default; `--eh=sjlj` kept for one version; README; size and speed tables | Full retest, mapcompare | New size/speed table against cl |

Risk: a wrong unwind code only shows up when something is thrown through that function. Step 1's stack-walk
test covers every frame shape (with and without pushes, xmm saves, large frames with __chkstk) before
step 3 depends on it.
