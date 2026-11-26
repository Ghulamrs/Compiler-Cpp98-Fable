# Design: CodeView types and variables (row M10, 2026-11-19)

Status: done, stages 0 to 5 of section 6 (stages 0 and 1 on 2026-11-20, stage 2 on 2026-11-24, stages 3 and 4 on
2026-11-25, stage 5 on 2026-11-26); row M10 is closed. With `-g` an object carries its types in `.debug$T` and in `.debug$S` its variables with static storage
and every function's parameters and locals, at `-O0` each in its place. At `-O1` and up a variable that stays in
memory is at its slot and one with one definition is where that value is, a register or a spill slot, over the code
where it lives; the others are listed without a place. With `-g2` every class also lists the member functions it
declares, as cl's types do; they are an option of their own by stage 4's size criterion (the own sources' `-g`
objects grow by 26.9 % with them at `-O0`). With either option the object's file table names every source file
the unit read with its SHA-256, as cl's /Z7 objects do; a debugger holds a source against it (stage 5). What the
options cost (objects, PDB, compile and link time, the self-host) is the table in stage 5's entry below, and what
is left, as rows of their own and as sessions by hand, stands behind it.
The independent review of the first text (commits 60f813e and 5dfd7f8) is folded in; its findings and what was
done about each are section 11. The user's go-ahead of 2026-11-20 is on the recommended answers to the twelve
questions of section 8. The order is the one of M7 (docs/design-cl-exceptions.md): the text, an independent
review, the user's review (the questions of section 8), then development by the stages of section 6.

The stages as they were done:

- Stage 0 (2026-11-20): cppparse/cv/ holds the CodeView code Coff.cpp had, as the classes of section 9's table
  that are marked "from" an existing place: `CvBytes` with `CvReloc` (CvRecord), `CvSymbols`, `CvLines`,
  `CvFiles`, `CvSection`. Coff.cpp calls them from its sequential loop and copies a section's bytes and
  relocations into the object; which sections an object has is still its matter. Checked with the compiler of
  the commit before (0cf38b9, built by cl) against this one, both run on this commit's sources with the same
  command lines and output names, the files compared by SHA-256:
  - the 52 own sources, libcpp\iostream.cpp and the 38 units of examples\sample-dbg and sample-front (13
    compile, 25 are refused by design) at `-O0`, `-O2`, `-O3`, `-O0 --eh=sjlj`, `-O1 --eh=cl` and `-O2 --eh=cl`:
    396 objects, 396 listings, 396 object maps (their time stamp line apart) and 1,866 dumps (IR, assembly
    text, symbols, tree) are the same bytes from both compilers, none differs, none is written by one only; the
    228 console texts are equal once the output folder's name is replaced;
  - the scripts of the exit test (the suite's 19 tests, selfhost.ps1 at `-O0` and `-O2`, lib.ps1 and life.ps1
    at both levels, eh.ps1, matrix.ps1), run once with each compiler as sandbox\cppparse.exe: 521 objects, 532
    object maps, 2,113 dumps and 533 of 534 listings are the same bytes. The one listing that differs is test
    2's cpp-folder.asm (and the four dumps and reports beside it): the versions 1 to 5 pipeline on the whole
    folder, where no object is written. It is not reproducible from run to run with one compiler: 4,439,820
    and 4,439,226 bytes from two runs of this commit's, 4,439,814 and 4,439,914 from two of the one before.
    The self-host that starts from the compiler before has generation two, compiled by this commit's code,
    identical to generation one at both levels.
  Test 18 (sample-dbg read back through dbghelp) is unchanged and passes; harness.ps1 passes in full in both
  models (`harness.ps1` and `harness.ps1 -Eh cl`), the self-host's generation two identical at `-O0` and `-O2`
  in each. The differences between this text and the code that the move showed are a note in section 11.
- Stage 1, in parts (2026-11-20):
  - The comparator of invariant 1, sandbox\scripts\cvsame.ps1 (5.5/1): two objects, or two folders of objects
    paired by name, with `-Listings` the listings beside them, with `-DebugOnly` the stricter question of the
    spelling commit (nothing added, only `.debug$S` contents differ). One line per pair that is not the same
    bytes, a count, exit code 1 on a violation. `-SelfTest` runs it on cases with known answers made from one of
    cppparse's objects (16: a byte changed in the code, in a relocation, a symbol, the string table, a section's
    flags, a `.debug$S`; a section added behind the last one with and without its symbol, a `.rdata` added, a
    section added in between; listings with the markers, with another instruction, with a marker `-g` does not
    write); test 18 runs that on lines06.obj. One thing 5.5/1 did not say is a note in section 11 (a
    `.debug$S` section symbol's auxiliary record). `-Recurse` pairs the objects of two folder trees by their
    paths, `-DebugNames` leaves the `;@debug` lines out of the listings' comparison.
  - Procedure names in cl's spelling, for every object (question 10): `CvNames` (cv/) has cl's rules, measured on
    cl 19.52's /Z7 objects; `CvFromSema` (middle/), the one file that sees both Sema and cv/, describes a function
    to it from the symbol (`functionName`); IrGen puts the name on the function (`IrFunction::debugName`) and
    X86.cpp writes it into `;@debug`, where its own `displayName` wrote the IR name without the parameter list
    (that rule is `CvNames::fromIrName` now, for a function that has no symbol: the versions 3 to 5 pipeline).
    What changed in a name: template arguments from the instantiation's own arguments (`Arr<int,4>`, `> >`,
    `char const *`, `__int64`, `enum ns::E`, `int (__cdecl*)(int,char)`, `int ns::In::*`; a non-type argument
    by its value, a pointer to member by its representation), a constructor and destructor under the class's
    name with its arguments (`std::vector<int>::vector<int>`, `~vector<int>`), a conversion function under its
    type's name (`operator __int64`, where the typedef's name stood), the unnamed namespace
    (`` `anonymous namespace'::hg ``), the members of a class local to a function (`` `local'::`2'::L::m ``), the
    initializer of a COMDAT variable (`` `dynamic initializer for 'LinkDyn<int>::name'' ``).
    Measured against cl, symbol by symbol (a scratch script pairs the `S_GPROC32` / `S_LPROC32` records of cl's
    /Z7 /Od object and cppparse's by the symbol they are relocated against; cl compiles with /X and cppparse's
    headers, as mix.ps1 arranges): the 52 own sources of the commit before and libcpp\iostream.cpp, 17,006
    procedures that both objects have, 10,496 named differently before this commit and none after; the 49
    example units both compilers take, 10,409 procedures, 16 and none; a probe of 153 shapes (every operator,
    47 type arguments, 12 value arguments, pointers to members, local classes, nested unnamed namespaces), none.
    The exit test, cvsame `-DebugOnly -Listings -DebugNames -Recurse` between the compiler of the commit before
    (1fca7f3, built by cl) and this one, both on this commit's sources: the 54 own sources, libcpp and the 77
    example units that compile alone (132 of 168 units; the rejected ones of sample-front are among the 36), at
    `-O0`, `-O2` and `-O2 --eh=cl`:
    396 pairs, 123 the same bytes, 273 differ in the contents of `.debug$S` sections only, none in anything
    else; the listings are the same lines but for `;@debug`. What the comparison found in the front end and
    what is not measured are a note in section 11.
  - `-g`: the types and the variables with static storage. `CvModule` (cv/) is the description, `CvFromSema`
    (middle/) fills it while IrGen runs and completes the classes at the end of the unit, `CvTypes` writes
    `.debug$T` from it in the object writer's sequential loop, `CvSymbols` the data records, `CvCheck` checks
    the bytes. The listing carries `;@proc T<n> <frame> <saved>` behind `;@debug` and one `;@static` per static
    local; `<name>-cv6.txt` is the description's text. The option is `-g` (`-g0` the default spelled out;
    `CPPPARSE_G=1` in the environment sets it for a run). What the code differs in from this text is a note in
    section 11. The exit tests, measured (the compiler built by cl from this commit; cl 19.52, GRIDE3's link
    and gride-dbg as the harness finds them):
    - types.cpp and types2.cpp (examples\sample-dbg; test 20 of the suite, at `-O0` and `-O2`): the program
      prints 152 facts of its own types and variables (sizes, member offsets measured in objects, bit-fields
      found by storing into them, base offsets, enumerators, the two `Node` classes of the units' unnamed
      namespaces through their variables, two local classes `Local` through static locals); dbgtypes.cpp
      (cl, dbghelp) prints the PDB's: all 152 are among them, from Microsoft's link and from GRIDE's, and both
      linkers say nothing. The driver's description of the program's types, every member with its type, the
      bases, virtual bases with vbptr offset and table index, enumerators, static members, nested types and
      the vftable's slot count (305 lines), is the same text for cl /Z7's build of the two sources. Nothing
      had to be normalised: the driver lists no member functions, and dbghelp names an unnamed namespace
      without its id.
    - mixed: types.cpp by cppparse `-g` and types2.cpp by cl /Z7 (/X, cppparse's headers) in one program, so
      that the classes of types.h are in the PDB from both compilers: the 152 facts hold and the description
      is the same 305 lines, through both linkers.
    - gride-dbg on the design's probe t.cpp from either linker's PDB, the same lines: `?? g_origin` is
      `struct geom::Point {x=1,y=2}`, `g_table` an `int [4] [3]` with its values, `g_counter`, `g_name`,
      `s_hidden`, `kLimit`, `kPi`, `geom::Shape::count`, and the static local `calls` in its function.
    - llvm-pdbutil `dump -types -symbols` on the 135 `-g` objects of the direct comparison (below) at `-O0`:
      none with an error; test 20 runs it on the program's two objects.
    - a generated probe (a class of 4,000 members, whose field list is 11 records continued through `LF_INDEX`;
      an enumeration of 5,000 enumerators; enumerators whose values need each size of numeric leaf; a class
      name of 8,011 characters): dbghelp gives every member with its offset and every value from both
      linkers' PDBs, and the cut name and the unique name `.??@a9bc0b36...@` are the ones cl writes for that
      class.
    - invariant 1, cvsame `-Listings`: the 57 own sources, libcpp and the 77 example units that compile
      alone, each without and with `-g` at `-O0`, `-O2` and `-O2 --eh=cl`: 405 pairs, every one differs in
      debug information only, none violates; no unit has an internal error or a `CvCheck` message. The
      suite's own scripts (build-cppparse.ps1's 20 tests, lib.ps1 and life.ps1 at both levels, eh.ps1,
      matrix.ps1) run once as they are and once with `CPPPARSE_G=1`, every script passing both times: of the
      316 objects cppparse writes in them (66,815,867 bytes, 74,771,211 with `-g`) 302 differ in debug
      information only, 14 are the same bytes (they have no function with a source file, or name `-g`
      themselves), none violates. Test 20 repeats it for the program's two units at both levels and for the
      own sources at `-O0`.
    - invariant 2: `selfhost.ps1 -G` (both generations compile with `-g`): generation two's objects and
      listings are generation one's, 57 of 57 units and libcpp, at `-O0` and at `-O2` (the self-host with `-G`
      took 218 s against 208 s without at `-O0`, 438 s against 445 s at `-O2`: no more than the run-to-run
      difference); the self-host's own 57 objects at `-O2` with and without `-g` by cvsame: none violates; six
      own sources at `-O2 -g` with `-j 1` and with `-j 4`: object, listing and description the same bytes.
    - the `-g` self-host's 59 objects linked with /DEBUG; the program runs:

      | | objects | Microsoft's link | its PDB | GRIDE's link | its PDB |
      |---|---|---|---|---|---|
      | `-O0` | 26,841,811 | 0.3 s | 12,824,576 | 1.1 s | 14,163,968 |
      | `-O0 -g` | 31,296,367 (+16.6 %) | 0.3 s | 13,930,496 (+8.6 %) | 1.1 s | 15,405,056 (+8.8 %) |
      | `-O2` | 29,993,195 | 0.3 s | 12,849,152 | 1.0 s | 14,106,624 |
      | `-O2 -g` | 34,447,591 (+14.9 %) | 0.3 s | 13,955,072 (+8.6 %) | 1.5 s | 15,376,384 (+9.0 %) |

    - cost, the direct comparison's runs (four processes at a time, the machine busy with a harness, so the
      times are rough): the own sources' objects grow by 16.4 % at `-O0` (27,388,875 to 31,888,179 bytes) and
      14.7 % at `-O2`, the examples' by 9 %; all three variants of the 135 units took 166 s without `-g` and
      182 s with (+10 %). Risk 3's limit (more than double) is far: the rule "every class a pointer leads
      to that is complete at the end of the unit" stays. The whole of harness.ps1 passes in both models.
- Stage 2 (2026-11-24): parameters and locals. Measured first (section 11 has the numbers): cl's `S_FRAMEPROC`
  flags, the DIA reader on cl's /O2 object, DIA's parameters of a member and of a variadic function. Then: every IR
  instruction carries its block scope (`IrInst::scope`, set where `line` is, copied with it), IrGen marks the
  source's parameters and locals with their blocks and types, X86.cpp writes `;@proc`, `;@block`, `;@local`,
  `;@gone`, `;@static`, `;@prologend`, `;@epilog`, `;@scope`, the encoder records the position markers' offsets, and
  `CvScopes` (cv/) gives each procedure its form and records; `CvCheck` (b) and the frame part of (c); the vftables'
  `S_GDATA32` and the IR dump of the new fields, which stage 1 had left. Where the code differs from this text is
  section 11's note of 2026-11-24, above all: the ranged form has no `S_BLOCK32`. The exit tests, measured (the
  compiler built by cl from this commit; cl 19.52, GRIDE's link and gride-dbg as the harness finds them):
  - the value harness (test 21): examples\sample-dbg\vars.cpp, 19 checkpoints in functions of every shape section 6
    names (a checkpoint in a handler, in a destructor run while an exception passes, in a destructor at a block's
    end), at `-O0` with `--eh=table`, `sjlj` and `cl`: every declared parameter listed in order, every variable
    with a place right (141 values) in the checkpoint's caller, in the frames above it that announce themselves
    (three) and at the first address of the caller's next line, the two never-read parameters without a place;
    nothing listed with a place that the program does not name. The same through Microsoft's link (`/DEBUG
    /INCREMENTAL:NO`, `/DEBUG` incremental, `/DEBUG /OPT:REF,ICF`, `/DEBUG:FULL`, the object from a static
    library) and GRIDE's (`/DEBUG`, `/OPT:REF,ICF`, a static library): no linker says anything.
  - DIA (diavars.exe) at each checkpoint's caller: the same variables at the same places as dbghelp, 18 addresses,
    in each model and from both linkers' PDBs.
  - gride-dbg: breakpoints on nine checkpoint lines, `dv /t /V` at each: the same lines from both linkers'
    programs (unplaced variables show as `static ... <memory not readable>`, request D5).
  - invariant 1, cvsame `-Listings`: the 58 own sources, libcpp and the 81 example units that compile alone, each
    without and with `-g` at `-O0`, `-O2` and `-O2 --eh=cl`: 420 pairs, every one differs in debug information
    only, none violates; no unit has an internal error or a `CvCheck` message; llvm-pdbutil reads all 420 `-g`
    objects without an error. Test 21 repeats cvsame for vars.cpp in the three models.
  - invariant 2: `selfhost.ps1 -G` generation two identical at `-O0` (212 s) and `-O2` (423 s), 58 of 58 units and libcpp; the harness's new
    check selfhost-g-O0 at `-O0`; six own sources with `-g` at `-O0` and `-O2` with `-j 1` and `-j 4`: object,
    listing and description the same bytes.
  - counted on the own sources at `-O0` (llvm-pdbutil over their `-g` objects): of 20,318 procedures with a type
    17,151 are in the plain form, 2,892 in the ranged form, 275 have no variable; 38,240 `S_REGREL32` and 789
    `S_BLOCK32`; 18,643 `S_LOCAL`, 7,552 of them parameters, 2,143 without a place (all of them parameters the
    function never reads, the count question 12 asked for); 8,250 `S_DEFRANGE_REGISTER_REL`, 5,914 of them with
    gaps or several to a variable (a block in pieces: a try, an object with a destructor, a branch placed behind).
  - cost: the own sources' objects grow with `-g` by 24.2 % at `-O0` (27,736,440 to 34,460,968 bytes; stage 1:
    16.4 %), 19.1 % at `-O2`, 18.7 % at `-O2 --eh=cl`; the examples' by 15 to 17 %. Compile time: the self-host with `-G` took 212 s at `-O0` and 423 s at `-O2`, against stage 1's 218 s and 438 s with `-g` and 208 s and 445 s without: within the run-to-run difference.
  - by hand, not done here: a session in Visual Studio's debugger on vars.exe (the Locals pane, a watch with a member
    access and a cast to a template's name, the call stack's frames, the two sibling blocks of one name in
    `blocks`, the never-read parameters of `params`, an object passed by address shown as a reference in `refs`).
- Stage 3 (2026-11-25), in three parts:
  - The optimiser's tables (5.4). `IrFunction::debugTemps` holds, for a variable of the source that has one
    definition, the temporary that is that definition; `IrLocal::promoted` and `IrLocal::staleStores` say why a
    frame slot is no place (Ssa.cpp made the local temporaries; a pass removed a store to it);
    `IrFunction::debugAliases` names a result built in the caller's object and the local that holds the hidden result
    pointer. The pair is recorded where the variable leaves memory: at Ssa.cpp's promotion, and where dead store
    elimination removes the one store of a variable whose every read took the stored value itself, which is how most
    single-assignment locals go, before SSA is built. `replaceTempUses` renames an entry when a pass puts one temporary
    in another's place, a constant ends it, and a verifier after the last pass keeps an entry only if its temporary is
    defined exactly once and, where it is not the temporary the variable was assigned from, if that definition lies
    on no cycle of the function. A store to a local leaves the code through `IrFunction::removeStore` alone. The
    tables are kept under `-g` only and nothing reads them to decide anything; the IR dump prints them. What differs
    from 5.4 is section 11's note of 2026-11-25. Measured: cvsame `-Listings` over the own sources, libcpp and the
    example units without and with `-g` at `-O1`, `-O2`, `-O3`, `-O2 --eh=cl` and `-O2 --eh=sjlj`: 735 pairs, none
    violates; vars.cpp's tables at `-O2`, read in the IR dump: every parameter that is not assigned has its entry
    (the incoming value; in `Both::get` the adjusted `this`), `sum`, `total`, `r`, `w`, `mine`, `got`, `half`,
    `twice`, `flag` theirs from the dead store, `changed`, `top`, `observed` and the loop's `s` and `i` none.
  - The places (5.3, 5.4). X86.cpp lists a variable of an optimised function in one of three ways: in the frame
    (`;@local`) when the code keeps it there (not promoted, no store removed, and stored: not a parameter the
    function never reads or reads from its register alone); by its one definition (`;@var`, with `;@loc` markers
    among the code) when `debugTemps` has it and the temporary has a place and holds a value of the variable's size
    and kind; else without a place (`;@gone`). A named result is listed by address at the hidden result pointer's
    place. `placeOf` answers "where is this temporary" for `use()`, `def()` and the markers alike: its interval's
    register or its spill slot, for its whole life. A place is announced from behind the defining instruction (at a
    block's start when the temporary is live into the block) to the last instruction of the block that reads it:
    before that instruction, behind a call instruction itself for a callee-saved register, behind the instruction for
    a slot; a volatile register never over a call, a throw or a try operation; nothing in an epilogue, a funclet's
    entry or the stubs behind the blocks. `CvScopes` turns the markers into `S_LOCAL` with `S_DEFRANGE_REGISTER`
    (the part of the register that holds the value: `CvRegisters`, a table per register) or
    `S_DEFRANGE_REGISTER_REL` for a spill slot, cut to the variable's block, the holes as gaps; a function without
    a frame has its variables in registers and no `S_FRAMEPROC`. `CvCheck` (b) holds every register against the
    table and the variable's type; (c) holds every register range against what the encoder recorded: the registers
    each instruction writes (`writtenRegisters`, beside `Encoder::instruction`; a call writes every volatile
    register, a mnemonic it does not name writes them all) and the prologue and epilogue code. What differs from
    5.3 and 5.4 is section 11's second note of 2026-11-25. Measured (the compiler built by cl from this commit):
    cvsame `-Listings` over the own sources, libcpp and the example units (148 that compile alone) without and
    with `-g` at `-O1`, `-O2`, `-O3`, `-O2 --eh=cl` and `-O2 --eh=sjlj`: 740 pairs, none violates invariant 1; no
    unit has an internal error or a `CvCheck` message, so every register range of the 740 objects passes (c);
    llvm-pdbutil reads all 740 `-g` objects without an error. Against the compiler of the first part, the same 148
    units: the objects and listings at `-O0` with `-g`, and at `-O2` and `-O2 --eh=cl` without, are the same bytes
    (888 files), so nothing changed but the debug information of optimised code. `selfhost.ps1 -G -Lvl 2`:
    generation two's objects and listings are generation one's (506 s beside a running harness). Test 21 compiles
    vars.cpp at `-O1`, `-O2` and `-O3` with `-g`: cvsame, the compiler's own check silent, 66 places in a register in
    the listing, 48 `S_DEFRANGE_REGISTER` records read by llvm-pdbutil. The own sources' objects grow with `-g` by
    25.1 % at `-O2` (stage 2: 19.1 %), 30.1 % at `-O1`, 18.6 % at `-O3`. A function without a frame (`int
    mulAdd(int a, int b) { int product = a * b; return product + a; }`): no `S_FRAMEPROC`, `a` in ECX over its life,
    `b` and `product`, each consumed by the instruction behind its definition, optimised out.
  - The exit tests (section 6, row 3), measured (the compiler built by cl from this commit; cl 19.52, GRIDE's link
    and gride-dbg as the harness finds them):
    - the value harness (test 21). vars.cpp has 28 checkpoints now: beside stage 2's shapes a named result,
      an `int`, a `bool`, a `char` and a `short` that live across the call (ebx, sil, dil, r12w), ten values of
      which three are spilled, a `float` and a `double` seen from the frame above (xmm7, xmm6), a value whose last
      use is the argument of the call that is on the stack, a variable that is another's value, a variable assigned
      once in a loop from one that changes in every round, the same in a loop that only an exception closes (a
      handler that resumes), an adjusted `this` needed behind the checkpoint. Every
      function asks for its return address (`_AddressOfReturnAddress`), which keeps it out of line at every level:
      the driver finds a checkpoint's caller by name, and an inlined function's variables are not listed. A text
      marks with `+` the variables that must have a place at `-O1` and up. At `-O0`: 222 values right in the
      three models. At `-O1`, `-O2` and `-O3` in each model (nine builds, `dbgvars -optimized`): every declared
      parameter listed in order, no value wrong in the caller, in the frames above it and at the next line, every
      marked variable in its place; 129 values read back (131 in cl's model), a variable without a place 96
      times; no place for a variable the program does not name. DIA gives the same variables the same places at
      all 25 checkpoint callers of each build. The `-O2` program through the link variants (Microsoft's link: `/DEBUG`
      incremental, `/DEBUG /OPT:REF,ICF`, the object from a static library; GRIDE's: `/DEBUG`, `/OPT:REF,ICF`, a
      static library): the same result, no linker says anything. gride-dbg's `dv` at eleven checkpoint lines of
      the `-O2` program: the same lines from both linkers' PDBs (a variable in a register shows as `@reg`, section
      10's D4 to D6).
    - counted on the own sources (62 units with libcpp, the listings' markers; `-O2`): 20,860 procedures with a
      type, 58,259 variables of the source: 6,218 in the frame, 41,653 in their temporary's place over some of
      the code, 10,388 without a place: 82.2 % have a place (3.6's upper bound was 88 %), 90.8 % of the 40,113
      parameters and 63.1 % of the 18,146 locals. At `-O1` 82.5 %, at `-O3` 81.3 %; at `-O0` 96.3 % (the 2,183
      parameters that are never read have none).
    - blocks in pieces (risk 7), the own sources at `-O2`: of 6,370 block scopes with code 3,539 were in
      more than four pieces as the second part left them (the most: 251). The cause was SSA destruction:
      the copies it puts on an edge, the edge's own block and a phi that becomes a copy carried no scope, so each
      cut the blocks around it. They now take the scope of the branch they stand at (fromSsa) and of the block's
      first instruction (a phi); of 6,375, 1,323 blocks are left in more than four pieces (the most: 29),
      5,031 in more than one: what is left is the layout (handlers, landing pads and cold blocks behind
      the function's `ret`; the blocks an inlined call adds, which are numbered behind the caller's and placed by
      their numbers, so that the code of neighbouring scopes alternates: at `-O1`, without inlining, none of the
      6,370 is in more than four pieces, where 2,238 were; at `-O3` 2,161 are, where 4,156 were) and code of the
      enclosing scope between a block's statements. That is the code's order, which `-g` does not touch; a
      variable of such a block is listed over each piece. No code changes with the scopes (cvsame).
    - invariant 1, cvsame `-Listings` over the own sources, libcpp and the example units without and with `-g`:
      at `-O0`, `-O1`, `-O2`, `-O3`, `-O2 --eh=cl` and `-O2 --eh=sjlj`, 888 pairs, none violates; no unit has an
      internal error or a `CvCheck` message at any of them, so every register range of 888 objects passes
      `CvCheck` (c); llvm-pdbutil reads all of the `-g` objects without an error. The suite's scripts
      (build-cppparse.ps1, lib.ps1 and life.ps1 at `-Lvl 2`, eh.ps1, matrix.ps1) once more with `CPPPARSE_G=1`:
      every script passes and no compilation has a `CvCheck` message.
    - invariant 2: `selfhost.ps1 -G -Lvl 2`, generation two's objects and listings are generation one's
      (506 s beside a running harness), the harness's new check `selfhost-g-O2`; test 21 compiles vars.cpp at
      `-O2 -g` with `-j 1` and `-j 4`: the same object, listing and description. The suite under the compiler
      that `selfhost.ps1 -G -Lvl 2` built: every test group passes.
    - cost: with `-g` the own sources' objects grow by 24.5 % at `-O2` (stage 2: 19.1 %), 29.7 % at `-O1`,
      17.8 % at `-O3`; the examples' by 22.6 % at `-O2`.
    - by hand, not done here: the session in Visual Studio's debugger on the `-O2` program (test 21 leaves it as
      `sandbox\out\dbgvars\vars-table-O2-ms.exe`): the Locals pane in `regs`, `floats`, `spill` and `makeBig`
      (`out` as the object), `this` and its members in `Both::get`, the frames of `outer` and `above` selected in
      the call stack while `leaf` and `leaf2` are stopped (`mine`, `p`, `keep`, `part`; `w` of `lastUse` not
      available), `rounds` (`cur`; `once`, `kept`, `i` optimised away), the two `x` of `blocks`, a step over a
      checkpoint line; and stage 2's session at `-O0`.
- Stage 4 (2026-11-25): the member functions in the types, behind `-g2`. Measured first what cl 19.52 writes (three
  probes of 60 classes; section 11's note of this stage has the facts): the field list's order, `LF_ONEMETHOD`,
  `LF_METHOD` with its `LF_METHODLIST`, the kinds and vftable slots, the type of `this`, the function attributes
  and the this adjustment of `LF_MFUNCTION`, a conversion function's name, what cl lists of its own. Then:
  `CvFromSema` lists the functions a complete class declares when it completes the class (`CvMethod` in the
  `CvModule`: type, access, kind, slot), `CvTypes` writes them among the class's members in declaration order,
  `CvCheck` (a) holds each listed function's attributes, its type and an overloaded name's list. A procedure's own
  type keeps the this adjustment 0; the type its class lists says cl's number. Where the code differs from the
  text is section 11's note. The exit tests, measured (the compiler built from this commit by devenv Release|x64
  and by the suite's cl /W4; cl 19.52, the DIA SDK, GRIDE's link as the harness finds them):
  - the tree (test 20). types.cpp has namespace `mem`, 18 classes (one a template with two instances) with every
    kind of member function: overloads, static functions, virtual ones that introduce a slot and that override,
    pure ones (introducing and overriding), const functions, constructors (one of a class with virtual bases)
    and a destructor, eleven operator functions, `new` and `delete` among them, five conversion functions, a variadic function, the members of a class
    template's two instances and of a class nested in it, a union's, overrides first reached through a second
    base's table and through a virtual base's (the this adjustment), a covariant override, class results from
    static and non-static functions. The program calls them and checks the sum (125). The driver
    examples\sample-dbg-driver\diatypes.cpp reads the PDB through DIA and prints every member function of the
    program's classes with its access, kind, vftable slot, type, type of `this`, this adjustment and attributes:
    97 lines, the same for cl /Z7's build and for cppparse `-g2`'s at `-O0` and `-O2`, what cl marks as
    compiler-generated left out; the same 97 from GRIDE's PDB and from the programs mixed of cppparse's types.cpp
    and cl's types2.cpp by either linker; no linker says anything. With `-g` DIA lists none, llvm-pdbutil finds
    no member function in the two objects (101 with `-g2`), and dbghelp's description of the types (411 lines;
    202 facts of the program) is the same for `-g`, `-g2` and cl. The `-g2` listing is `-g`'s byte for byte.
    (Since 2026-11-26, with `mem::RunV`, `RunP` and `RunJ` of next-targets' row G45: 21 classes, the sum 138, 102
    lines from DIA, 106 functions in the two objects, 427 lines and 208 facts from dbghelp.)
  - the records themselves, once, by a scratch script that resolves the type indices of llvm-pdbutil's dumps to
    text: the three probes (as far as cppparse's front end takes them) compiled by cl /Z7 and by cppparse `-g2`,
    one line per listed function with its attributes, slot, result, parameters, class, type of `this`, this
    adjustment and function attributes. cl's objects have 184 lines of functions it did not generate, in 53
    classes, cppparse's 186; 173 are the same in both. The others are the cases section 11 names: three volatile
    member functions (a line of each compiler), eight conversion functions to a template's instance (cl alone),
    two `operator=` that are no copy assignment (cppparse alone), and eight functions of three classes that only
    cppparse's object defines (a class cl leaves a forward reference, an instance cl does not make).
    The independent review compared nine more probes the same way.
  - the size, objects in bytes (`.debug$T` in brackets), the own sources with libcpp (62 units) and
    examples\sample-lib (4 units):

    | | no option | `-g` | `-g2` | `-g2` over `-g` |
    |---|---|---|---|---|
    | own sources, `-O0` | 28,565,597 | 35,621,365 (4,027,780) | 45,191,713 (13,096,724) | +26.9 % |
    | own sources, `-O2` | 31,718,273 | 39,456,045 (4,027,612) | 49,026,529 (13,096,692) | +24.3 % |
    | sample-lib, `-O0` | 1,384,297 | 1,635,997 (110,776) | 1,773,853 (244,616) | +8.4 % |
    | sample-lib, `-O2` | 1,220,937 | 1,505,701 (110,776) | 1,643,557 (244,616) | +9.2 % |

    The criterion of row 4: with the member functions the own sources' objects grow by more than a quarter at
    `-O0`, so they are `-g2` and `-g` stays without them. Most of the growth is not the functions' own records
    but the classes their types name, which are described as cl describes them (section 11, (h) and (9)). The
    66 objects' classes list 138,690 member functions in all (77,942 alone, 60,748 in the lists of 24,780 overloaded
    names: every object repeats the classes of its headers), beside 22,224 procedures. Compile time is not changed by
    either option beyond the run-to-run difference (the 62 units, four processes: 33 s, 34 s and 33 s at `-O0`,
    69 s, 67 s and 69 s at `-O2`).
  - invariant 1. Without an option the objects and listings are the bytes the compiler of the commit before
    (7397c96) writes: the own sources, libcpp and the 74 example units that compile alone (136 of 170 units), the
    objects and the listings at `-O0`, `-O2` and `-O2 --eh=cl`: 816 files, none differs, none is written by one
    compiler only. cvsame over the own sources, libcpp and
    sample-lib without an option against `-g` and against `-g2`, at `-O0` and `-O2`: 264 pairs, every one differs in debug information only, none violates; test 20
    repeats it with the listings for the types program at both levels and for the own sources at `-O0`, with
    `-g` and with `-g2`. llvm-pdbutil reads all 264 `-g` and `-g2` objects of these without an error, and no unit has a `CvCheck` message.
  - invariant 2: `selfhost.ps1 -G2` (both generations with `-g2`, `-O0`): generation two's objects and listings are generation one's, 61 of 61 units, libcpp and the runtime; it is the
    harness's check `selfhost-g2-O0`, beside `selfhost-g-O0` and `selfhost-g-O2`, which run as before.
  - by hand, not done here: Visual Studio's debugger on the `-g2` program (test 20 leaves
    `sandbox\out\dbgtypes\types0-g2-ms.exe`): `g_ops.get()`, `g_impl.first(2)` and `Ops::count()` in a watch,
    the members of `mem::Ops` in the class view; and what it makes of `g_impl.other()`, the override that DIA
    lists without code (section 11, (2)).
- Stage 5 (2026-11-26): the source checksums, and the row's cost table. Measured first what cl 19.52 writes
  (section 11's note of this stage): an entry of the file table with its SHA-256, which files the table names, a
  name that only `#line` gives, a header under two spellings. Then: `CvSha256` (cv/) is the hash; the preprocessor
  tells the driver of the bytes of every file it reads (`PpFileObserver`), the driver puts each file's SHA-256
  into the `CvModule` (`CvSource`), and under `-g` `CvFiles` writes cl's table from it: every file the unit read,
  each entry with kind 3 and the 32 bytes; `CvCheck` holds the table; `cppparse --sha256` prints a file's
  checksum. Without `-g` nothing changes. Where the code differs from the text is section 11's note. The exit
  tests, measured (the compiler built from this commit by devenv Release|x64 and by the suite's cl /W4, which
  write the same object; cl 19.52, llvm-readobj and llvm-pdbutil of the same Visual Studio, GRIDE's link as the
  harness finds it):
  - the hash (test 22): `--sha256` gives the digests the standard has for its examples (the empty message,
    "abc", the messages of 448 and 896 bits, a million "a"), and what `Get-FileHash` gives for 29 files of 1 to
    1,048,576 bytes with lengths around a block's end, the padding's limit (55 and 56 bytes) and the 65,521 bytes
    it reads at a time, and for one of 536,870,913 bytes, whose length in bits is beyond 32 bits. Once by hand:
    files of 2^32 and of 2^32 + 5 bytes, as `Get-FileHash` has them (22 s each, 190 MB a second).
  - the exit test (test 22): examples\sample-dbg\sums.cpp with its header sums.h and `<vector>`, built with `-g`
    and `-g2` at `-O0` and `-O2`. The object's file table has 19 entries, the files cl /Z7's object of the unit
    names (the unit, sums.h, 17 headers of the library), each 40 bytes with kind 3 and the SHA-256 of its file,
    the same bytes cl has for it; the table is read from the object's bytes by the test and by llvm-readobj, and
    `-g2`'s is `-g`'s. Linked with /DEBUG by Microsoft's link and by GRIDE's, neither saying anything:
    `SymGetSourceFileChecksum` returns for the unit, for sums.h and for the other 17 the file's own SHA-256, from
    either PDB (examples\sample-dbg-driver\dbgsum.cpp); from the build without `-g` it returns none for the three
    files that table has. A line added to sums.cpp and to sums.h: the checksums both PDBs hold are no longer the
    files' (they are those of the files as they were built); built again, the table and both PDBs hold the new
    ones. A unit whose `#line` directives name a file that does not exist and one it does not read, and which
    includes a header twice, the second time with its name in capitals: no checksum for either of the first two
    and one entry for the header, in cl's table as in cppparse's.
  - the own sources (test 22, over test 20's objects; 62 units and libcpp): all 2,506 entries of the 63 objects'
    tables are their files' SHA-256 (172 files; 40 a unit, 21.0 MB read and hashed in all), and without the
    option none of the 956 entries has one: a line table names 38 % of the files a unit reads.
  - invariant 1. Without an option the objects and listings are the bytes the compiler of the commit before
    (a351769) writes, both run on this commit's sources: the own sources, libcpp, the runtime and the example
    units that compile alone (153 of 202 units) at `-O0`, `-O2` and `-O2 --eh=cl`: 918 files, none differs, none
    is written by one compiler only. cvsame `-Listings` without an option against `-g` and against `-g2`, the
    same units at the three: 918 pairs, every one differs in debug information only, none violates; no unit has
    a `CvCheck` message; llvm-pdbutil (`dump -types -symbols -l`, which reads the line tables through the file
    table) reads all 918 `-g` and `-g2` objects without an error. Against the `-g` objects of the commit before,
    cvsame `-DebugOnly`: 459 pairs, nothing but the contents of `.debug$S` sections differs (the file table, and
    the offsets by which the line tables name its entries); the 153 `-g` objects grow by 363,700 bytes, 0.7 %
    (the own sources' by 237,092, 0.66 %).
  - invariant 2: `selfhost.ps1` without an option, with `-G` and with `-G2`, at `-O0` and at `-O2`: generation
    two's objects and listings are generation one's in all six runs (their times are in the table below); test
    21's `-j 1` against `-j 4` as before, the description's text now with the sources. A header edited while its
    unit compiles (a probe by hand): the object has the checksum of the text that was compiled.
  - the cost of the row (section 6, row 5), measured on a machine that did nothing else (Core i5-10400T, 6
    cores; the compiler the suite builds with cl /O2): the self-host, which compiles the 62 own sources, libcpp
    and the runtime one after another, twice (stage 1 with the compiler cl built, stage 2 with the one stage 1
    built, itself compiled at that level), one run each; the own sources and libcpp alone, four processes at a
    time, the median of three runs; and generation one's 64 objects linked into the compiler with /DEBUG
    /INCREMENTAL:NO by each linker, the median of three links (every program runs). A percentage is against the
    column without an option at that level:

    | | `-O0` | `-O0 -g` | `-O0 -g2` | `-O2` | `-O2 -g` | `-O2 -g2` |
    |---|---|---|---|---|---|---|
    | the 64 objects, bytes | 28,780,535 | 36,190,975 (+25.7 %) | 45,941,299 (+59.6 %) | 31,913,502 | 40,010,342 (+25.4 %) | 49,760,802 (+55.9 %) |
    | self-host, stage 1 | 76 s | 78 s (+3 %) | 85 s (+12 %) | 178 s | 186 s (+4 %) | 192 s (+8 %) |
    | self-host, stage 2 | 134 s | 143 s (+7 %) | 169 s (+26 %) | 251 s | 260 s (+4 %) | 273 s (+9 %) |
    | self-host, both, as `selfhost.ps1` runs | 210 s | 222 s | 255 s | 429 s | 446 s | 466 s |
    | 63 units, four at a time | 23.4 s | 24.7 s (+6 %) | 26.8 s (+15 %) | 55.1 s | 58.0 s (+5 %) | 59.3 s (+8 %) |
    | Microsoft's link /DEBUG | 0.31 s | 0.33 s | 0.37 s | 0.30 s | 0.34 s | 0.35 s |
    | its PDB, bytes | 13,176,832 | 15,519,744 (+17.8 %) | 17,387,520 (+32.0 %) | 13,201,408 | 15,642,624 (+18.5 %) | 17,510,400 (+32.6 %) |
    | GRIDE's link /DEBUG | 1.13 s | 1.22 s | 1.32 s | 0.94 s | 1.05 s | 1.25 s |
    | its PDB, bytes | 14,450,688 | 16,941,056 (+17.2 %) | 18,894,848 (+30.8 %) | 14,397,440 | 16,977,920 (+17.9 %) | 18,931,712 (+31.5 %) |

    Read: `-g` costs a quarter more object bytes, a sixth more PDB, and 3 to 7 % of compile time; `-g2` three
    fifths more object bytes, a third more PDB, and 8 to 15 % of compile time with the compiler cl built (26 %
    with a compiler that is itself compiled at `-O0`, where describing every class's member functions is slow
    code). Link time grows by a few hundredths of a second with Microsoft's link and by one to three tenths with
    GRIDE's. The times are single runs for the self-host: a second run of the three `-O0` columns, on sources
    three small edits earlier, gave 73, 79 and 85 s for stage 1 and 135, 145 and 170 s for stage 2.
    The checksums themselves cost no time that shows: the compiler of the commit before takes 23.8 s, 25.1 s and
    26.9 s for the three `-O0` columns of the four-at-a-time row and 59.2 s for `-O2 -g`. Without /DEBUG the link of
    the columns without an option takes 0.28 s and 0.26 s with Microsoft's link, 0.99 s and 0.82 s with GRIDE's.
    The sizes of the stages before stand in their entries; they are not comparable row by row (the sources grew,
    and stage 1's 59 objects were without the variables).
  - by hand, not done here: Visual Studio's debugger with "Require source files to exactly match the original
    version" on: test 22 leaves `sandbox\out\dbgsums\sums0-ms.exe`, built before the test edits `src\sums.cpp`
    (the debugger should refuse the edited file for it), and `sums0-edited-ms.exe`, built after (it should take
    it).

What is left of the design, none of it begun: the rows of their own that section 6 names (inlinee records,
which wait for section 10's L3; value tracking for scalars that are assigned more than once, 4.3 c; the source
lines of types, `LF_UDT_SRC_LINE`), the requests of section 10, which are GRIDE3's (D1 to D9, L1 to L5), and the
checks by hand: the two sessions in Visual Studio's debugger on vars.exe, at `-O0` (stage 2's list) and at `-O2`
(stage 3's), with the shorter looks at the `-g2` program (stage 4) and at the checksums (stage 5).

In one paragraph: with a new option `-g` an object carries, beside today's line tables, its types in a `.debug$T`
section of its own (cl's /Z7 form, no type server) and, in `.debug$S`, the globals, the user-defined type names
and every function's parameters and locals. A function whose variables all live in the frame for the whole
function is described as cl describes /Od code: `S_REGREL32` off `rbp`, inner blocks as `S_BLOCK32`. Any other
function (at `-O0` one with a parameter it never reads, or with a block whose code is not in one piece; at `-O1`
and up nearly every one) is described in cl's /O2 form: every variable an `S_LOCAL` with the code ranges where
its place is known, a frame slot or, for a scalar with one definition, a register; a variable without a known
place is listed with no range and the flag "optimised out". A prototype (cppparse's own `-O0` object of a probe,
the records added by a script) was linked by Microsoft's link and by GRIDE's and read back through dbghelp
(GRIDE's debugger) and DIA (Visual Studio's reader): every variable and type came back as from cl's own object
(3.2); a second round after the review measured the forms the review asked for (3.2, probe r). Without `-g` an
object stays what it is today (one open point: the spelling of procedure names, question 10).

Measured with cl 19.52.36725 (x64, preview), its link, llvm-pdbutil and llvm-readobj of the same Visual Studio
(VC\Tools\Llvm\x64), the DIA SDK's DIA2Dump sample, cppparse 6.0 of main 5f463e2 (sandbox\cppparse.exe), GRIDE's
link and gride-dbg 1.1 (GRIDE3 5220ffd, GRIDEd\x64\Release). cdb is not installed on this machine (Windows
Kits\10\Debuggers\x64 holds dbghelp.dll and its helpers only); dbghelp is cdb's and WinDbg's symbol reader. x64
Windows only, as the rest of the back end.

## 1. Where M10 stands, measured

Every version 6 object has `.debug$S` as cl's do (README, "Debug information"): the module's file table
(`DEBUG_S_FILECHKSMS` without checksums, `DEBUG_S_STRINGTABLE`), `S_OBJNAME`, `S_COMPILE3`, and per function
`S_GPROC32` / `S_LPROC32` with type 0, `S_END` and a `DEBUG_S_LINES` table, a COMDAT function's in an associative
section of its own. The listing carries `;@debug "<file>" <name>` and `;@line N`; Coff.cpp's CodeView helpers
turn them into the records. Test 18 reads the lines back through dbghelp.

What a debugger shows for a program built that way (probe t.cpp, 85 lines: an enum, a struct, a union, bit-fields,
a class with virtual functions, a static member and nested types, a derived class, a diamond with a virtual base,
two templates, pointers to members and to functions, globals, a function with five parameters and sixteen
locals, one of them static and two in inner blocks; built by cppparse `-O0`, linked with /DEBUG by either
linker, stopped at line 72 under gride-dbg):

    dv            Unable to enumerate locals
    ?? total      not in scope here
    ?? g_origin   not in scope here
    k             work+0x2bd [t.cpp @ 72] / main+0x7d [t.cpp @ 82]          (lines and stack are right)

The same source from cl /Z7 /Od shows the parameters and the locals in scope with types and values, the
globals, and members through `.` and `->`. That difference is the row.

Two numbers of today's objects, for section 3.5: of four of the compiler's own sources (Coff.cpp, X86.cpp,
IrGen.cpp, Sema.cpp; 4,833 functions) compiled by cppparse at `-O0`, the objects are 7,025,116 bytes, of which
the line tables' `.debug$S` (4,337 sections) are 1,343,128 (19 %); at `-O2` 7,564,859 and 1,367,056 (18 %).

## 2. Goals and limits

Goals, for a program of cppparse objects (alone or mixed with cl's) linked with /DEBUG by Microsoft's link or by
GRIDE's, in Visual Studio's debugger, in cdb / WinDbg and in GRIDE's debugger (cdb itself is not on this machine:
question 9):

1. Types: every type of C++98 a variable can have is described as cl describes it: the arithmetic types,
   pointers, references, arrays, `const` and `volatile`, enums with their enumerators, classes, structs and
   unions with their data members (bit-fields with position and width), static members, nested types, base
   classes at their offsets, virtual bases through the vbptr, the vfptr, pointers to functions, pointers to data
   members and to member functions, instantiations of class templates under cl's display names.
2. Variables: globals, static members and static locals by address; namespace-scope constants by value. Every
   declared parameter and every source local of every function is listed, in its block, at every level. At `-O0`
   each has its place and the right value from the first statement of the function to its return, with one
   exception: a parameter the function never reads is not copied into the frame today, and since `-g` changes
   no code (goal 4) it is listed as optimised out (question 12). At `-O1` and up the variables section 4.3 names
   have a place, each only where the description is true; the others are listed as optimised out.
3. A location is a promise, with the reach cl's own records have: from the end of the prologue (`DbgStart`) to
   the start of an epilogue, and in a funclet from the instruction that sets its `rbp`, a variable the debugger
   is given a place for has its value there, in the top frame and in every frame above it. A variable the
   compiler cannot place is listed without a place, so the debugger says so; it is never described wrongly.
   Outside that reach (limits, below) the frame is not set up and no record form cl uses for /Od code can say
   so; whether M10 should pay for ranged records there is question 8.
4. Debug information changes no code: the object's code, data, unwind and exception tables, symbols and
   relocations are the same bytes with and without `-g` (5.5/1 says exactly what is compared).
5. Deterministic: the same source gives the same object, whatever compiler built cppparse and however many
   threads it runs, so the self-host's second generation stays identical with `-g` as without.
6. Both linkers take the objects without a warning and without leaving records out.

Limits that stay after M10 (each is a later row or a recorded limit):

- The windows of goal 3. In a function's prologue (`push rbp` to behind the parameter copies; a function entered
  with an adjusted `this` also the instructions that put it right), from the first instruction of each epilogue
  on, and in the first instructions of an `--eh=cl` funclet (until `lea rbp, [rdx+frame]`), `rbp` is not the
  frame's and every frame variable reads wrongly. cl's /Od records have the same windows (off RSP). A breakpoint
  on a function's own line lands in the first window in GRIDE's debugger today (section 10, D7). The value
  harness cannot test the windows: its checkpoints are calls.
- At `-O0`: a parameter the function never reads has no place.
- At `-O1` and up: a scalar that is assigned more than once (a loop counter, an accumulator) and is not in
  memory has no place: 12 % of the variables of the four own sources (3.6); nor has a scalar that stayed in
  memory but lost a store to the optimiser (functions SSA skips, 5.3). The debugger lists them as optimised
  out; lines, stack, parameters that are never assigned, single-assignment locals and everything whose address
  is taken are there. A register variable has no place in a frame above the top one where it sat in a volatile
  register at the call.
- A block whose code is not in one piece (it holds a try, or an object with a destructor: the handlers and
  landing pads are placed behind the function's `ret`) is not an `S_BLOCK32`: its variables are listed in the
  enclosing block with their exact ranges. Two such variables of one name are then two records of one scope
  that differ in their ranges; a reader has to go by the range (dbghelp does, 3.2; GRIDE's `??` takes the first
  record today, D5). A static local of such a block is listed in the enclosing block, without a range.
- Inlined calls have no inlinee records (`S_INLINESITE`, the inlinee line tables): code inlined at `-O1` and up
  keeps the call's line, as today, and the inlined function's own variables are not shown. A later row; it
  needs GRIDE's link to carry those records first (section 10).
- No column numbers (cl writes none either: its `DEBUG_S_LINES` have flags 0 in every probe).
- Member functions are listed in their class's type with `-g2` only (stage 4, section 6): the functions the class
  declares. Without it a debugger cannot offer `obj.method()` in an expression. With it one case is left out on
  purpose: an override that is first reached through a later base's table is listed, but a reader that goes by
  DIA does not find its code (section 11, the note of stage 4), so it cannot be called from an expression. Not
  listed: what the compiler generates, the instances of a member template, a conversion function to a template's
  instance, a function, an array or a pointer to member (cl's spelling of those in a member's name is not
  measured). GRIDE's debugger calls no functions at all.
- A class that both cl and cppparse describe in one program (mix.ps1's links) is two records of one name when
  the two differ in detail (cl lists members cppparse does not, such as `__vecDelDtor`); the PDB holds both and
  a debugger takes one. Layout and data members are the same in both when both compiled the same headers, as
  mix.ps1 arranges with /X.
- Two standard libraries in one program (cl's STL in cl's objects, libcpp in cppparse's): `std::` classes of one
  name and different layouts become one name with two definitions, and a debugger may show an object through
  the other library's members. The same limit as for exceptions (M7's design, section 2); question 11.
- An object rebuilt from its listing alone (`--compile6 --obj name.asm`) or assembled by ml64 has the line
  tables and no types or variables (5.4).
- x64 only.

## 3. What cl emits and what the readers need, measured

### 3.1 cl's records

Probe t.cpp, cl /Od, three ways: no option: an object of 11,675 bytes; /Z7: 23,807 bytes, types in the object;
/Zi: 18,271 bytes and a type server PDB of 86,016, the object's `.debug$T` one `LF_TYPESERVER2` record of 144
bytes that names the PDB, its `.debug$S` the same records as /Z7's with indices into that PDB.

The /Z7 object's `.debug$T` is 192 records, 5,680 bytes:

| records | count | bytes | what |
|---|---|---|---|
| `LF_CLASS`, `LF_STRUCTURE`, `LF_UNION`, `LF_ENUM` | 22 | 1,164 | each class twice: a forward reference (size 0, no fields) that every pointer, base and member names, and the definition |
| `LF_FIELDLIST` | 13 | 808 | 16 `LF_MEMBER`, 5 `LF_ENUMERATE`, 3 `LF_BCLASS`, 2 `LF_VBCLASS`, 1 `LF_IVBCLASS`, 1 `LF_VFUNCTAB`, 1 `LF_STMEMBER`, 3 `LF_NESTTYPE`, 9 `LF_ONEMETHOD`, 10 `LF_METHOD` |
| `LF_POINTER`, `LF_MODIFIER`, `LF_ARRAY`, `LF_BITFIELD`, `LF_VTSHAPE` | 54 | 688 | pointers and references (12 bytes; a pointer to member 20), `const`, arrays with their byte size, bit-fields {type, width, position} |
| `LF_PROCEDURE`, `LF_MFUNCTION`, `LF_ARGLIST`, `LF_METHODLIST` | 65 | 1,460 | function types: 33 of the 38 are member functions, most of them never defined (implicit constructors, `operator=`, `__vecDelDtor`) |
| `LF_FUNC_ID`, `LF_MFUNC_ID`, `LF_STRING_ID`, `LF_SUBSTR_LIST`, `LF_BUILDINFO`, `LF_UDT_SRC_LINE` | 38 | 1,560 | ids: a name for every procedure, the command line and directories of the build, the line of every type's definition |

and its `.debug$S` symbols 3,394 bytes: per procedure `S_GPROC32_ID` / `S_LPROC32_ID` (the type field an
`LF_FUNC_ID`), `S_FRAMEPROC` (frame size; "local and parameter base register: RSP"), one `S_REGREL32` {offset,
type, register RSP, name} per parameter and local, the parameters first and in order, `S_BLOCK32` ... `S_END`
around the variables of an inner block, `S_LDATA32` for a static local, `S_CALLSITEINFO`, `S_PROC_ID_END`; at
module level `S_GDATA32` per global (also `geom::Shape::count`, and one per vftable in a `.debug$S` of the
vftable's own), `S_LDATA32` per internal one, `S_CONSTANT` {type, value, name} for `const int kLimit`,
`const double kPi` and the enumerator the code used, `S_UDT` {type, name} per class, enum and typedef,
`S_UNAMESPACE`, `S_BUILDINFO`. The file table has SHA-256 checksums.

At /O2 the variables change form: `S_LOCAL` {type, flags (parameter), name} followed by one record per place:
`S_DEFRANGE_REGISTER` {register, code range, gaps} 40 times in t.cpp, `S_DEFRANGE_REGISTER_REL` {base register,
offset, range} twice, `S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE` {offset} 3 times; the registers are named by size
and kind (`ECX` for an int in rcx, `R8W` for a short, `R9B` for an unsigned char, 162 = `XMM0_0` for a float in
xmm0, 195 = `XMM1L` for a double in xmm1; the numbers are in the DIA SDK's cvconst.h). A variable has ranges
only where it lives: `total` in t.cpp has four ranges in four registers and a gap. Inlined calls are
`S_INLINESITE` ... `S_INLINESITE_END` scopes with `S_INLINEES`. The parameters' home slots are still listed as
`S_REGREL32`.

Three details measured apart (probes q.cpp, q3.cpp):

- A class passed by value that the calling convention passes by address (`int byValue(Big b, ...)`, 16 bytes): cl
  /Od writes `S_REGREL32 b`, type `Big`, at the parameter's home slot, which holds the address. Through dbghelp
  `b` then reads as `{a={177863264, 60, 2, 0}}` where the caller passed `{11, 22, 33, 44}`. cl's own record is
  wrong for a reader that takes it literally.
- A handler of a try block is a procedure of its own in cl's records (`` `tryIt'::`1'::catch$0 ``), with
  `S_FRAMEPROC` naming RBP and the function's variables again as `S_REGREL32` off RBP.
- `long double` is `T_REAL64` (0x41), `wchar_t` 0x71, `long` 0x12, `long long` 0x13, `char` 0x70, `signed char`
  0x10, `bool` 0x30, `void*` 0x603, `char*` 0x670 (a pointer to a basic type is 0x600 + the type, no record).

### 3.2 What the readers need: the prototype

cvpatch.py (appendix) takes cppparse's own `-O0` object of t.cpp, which has `S_GPROC32` / `S_END` and line
tables, and adds what this design proposes: a `.debug$T` built record by record (the types of t.cpp: 50 records,
1,232 bytes, without method lists, ids and source lines), the type of `work` on its `S_GPROC32`, its 20 variables
at the frame offsets the listing's `; frame` comment gives (`n@[rbp-24]` ...), six globals, a constant and 14
`S_UDT`. Seven variants, each linked by both linkers and read by gride-dbg (`dv /t /V`, `?? expr`), two of
them also by DIA2Dump:

| variant | Microsoft link | GRIDE link | dbghelp (gride-dbg) | DIA |
|---|---|---|---|---|
| `S_REGREL32` off RBP (register 334), nothing else | ok | ok | all 20 variables with cl's types and the right values; the globals; `*shape`, `*c` with its base | "rbp Relative", the first five "Param", the rest "Local", the same from both PDBs |
| the same and `S_FRAMEPROC` naming RBP | ok | ok | the same | not run |
| the same, with unique names, `LF_VFUNCTAB` / `LF_VTSHAPE` and two member functions | ok | ok | the same | not run |
| the same, the procedure as `LF_FUNC_ID` + `S_GPROC32_ID` + `S_PROC_ID_END` | ok | ok | the same | not run |
| `S_BPREL32` in place of `S_REGREL32` | ok | ok | no register (`@reg-0x18`), nothing readable | not run |
| `S_LOCAL` + `S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE`, `S_FRAMEPROC` naming RBP | ok | ok | all right, shown as `@rbp-0x18` | the sample lists no `S_LOCAL` at all (none of cl's /O2 object either) |
| `S_LOCAL` + `S_DEFRANGE_REGISTER_REL` RBP over the whole function | ok | ok | all right | not run |

What that settles:

- The minimum is small: types in `.debug$T`, a type on the procedure, `S_REGREL32` off the register the frame
  is addressed by. Not needed by any reader tried, for this one unit: `S_FRAMEPROC`, unique names,
  `LF_VFUNCTAB`, method lists, ids, `LF_UDT_SRC_LINE`, `S_BUILDINFO`. A forward reference is resolved to its
  definition by name (`const geom::Point& p` shows `{x=3, y=4}` though the reference's type names the forward
  record); with two units that is what makes unique names necessary after all (5.2).
- `S_BPREL32` is not for x64.
- cppparse's frame needs nothing new: `rbp` is the frame's base in every function that has a frame, and
  cppparse's unwind codes restore it (`.pushreg rbp`), so a caller's variables are right too: stopped in `work`,
  `.frame 1` / `dv` shows main's `c = {geom::Shape={m_id=9}, m_r=2}`, `p = {x=3, y=4}` (probe f).
- DIA takes the first records of a procedure for its parameters, as many as the procedure's type has: the
  parameters come first and in order. Measured on a free function with five parameters only; member and
  variadic functions are measured at the head of stage 2, and 5.3 does not rest on the rule.
- Both linkers' PDBs give gride-dbg the same lines, with one exception, `?? kLimit` (the `S_CONSTANT`): "memory
  not readable" from Microsoft's PDB, "not in scope" from GRIDE's (section 10, L1 and D3).

The readers on cl's own objects (probes b, b2), which bounds what the prototype can show:

- cl /Z7 /Od through either linker: gride-dbg shows everything, with three errors of its own, the same for
  cl's object and for the prototype's: bit-fields print the whole storage unit (`fl = {a=64785, b=64785,
  c=64785}`), a member of a virtual base is read at offset 0 of the base record instead of through the vbptr
  (`both.v` is garbage), a pointer to a data member prints as `int *`. Section 10, D1, D2, D8.
- cl /Zi through GRIDE's link: every type that is not a basic one comes out as `base0`, without a message:
  GRIDE's link leaves an object's types out when they are in a type server (pdb.cpp `merge_types`).
- cl /O2 (`S_LOCAL` with ranges) through either linker, the same lines from both: dbghelp gives the register of
  the range the address is in (`@rdi struct geom::Point & p`, `@r14 class geom::Shape * shape`); gride-dbg
  prints `@reg` and 0 for the sized registers (`ECX`, `XMM1L`), a register-held reference as a number, and a
  variable without a range at the address as `static ... <memory not readable>` at address 0 (D4, D5, D6).
- A breakpoint on a function's own line stops gride-dbg at the function's first byte, before the frame exists,
  for cl's object as for the prototype's; setting `DbgStart` in the `S_GPROC32` (cl has 45 of 742 bytes for
  `work`) does not change that (D7). Whether Visual Studio goes by it is not measured here.

Not measured: Visual Studio's debugger itself (it reads through DIA; a session is a check by hand, section 6),
and cdb (not installed).

After the review, probe r (cvpatch2.py: the same object, eight more variants, each through both linkers, also
with /OPT:REF,ICF, and under gride-dbg; the same lines from both PDBs in every variant):

| variant | dbghelp (gride-dbg) |
|---|---|
| `S_REGREL32` for all but two variables, those two `S_LOCAL` with flag 0x100 (0x101 for the parameter) and no range | the two are listed without an address (gride-dbg prints `static ... <memory not readable>`, D5). Every `S_REGREL32` that follows an `S_LOCAL` in the procedure is gone: of 20 variables only `n` (before the first `S_LOCAL`) is left |
| every variable an `S_LOCAL`, with `S_DEFRANGE_REGISTER_REL` or with `S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE`, the same two without a range | all 18 right, the two listed without an address |
| every variable an `S_LOCAL`; `total` as two records of that name, the first at a wrong slot over [0, 0x100), the second at its slot from 0x100 on; `area` with two ranges and a hole; `inner` with a range that ends before the stop (at 0x2bd) | `dv` shows `total = 114` at its slot and the first record without an address, `area = 18`, `inner` without an address; `?? total` takes the first record ("memory not readable", D5) |
| one range over the whole function with a gap {start, length}: for `total` and `area` a gap before the stop, for `inner` over it | `total` and `area` right, `inner` without an address: gaps are honoured |
| an associative `.debug$S` (one `S_GDATA32` at a COMDAT function's symbol) behind `.drectve`, its section symbol behind every other symbol | both linkers take it; the record is found by name and reads the COMDAT's bytes |

So: a variable without a place can be listed as such; ranges, several ranges and gaps are read as written and
tell two records of one name apart; added sections and symbols may come last; and one procedure must not mix
`S_REGREL32` with `S_LOCAL` (4.2). Every variable and function of cppparse's object has a symbol of its own to
relocate against, a static local (`?calls@?1??work@@...@4HA`) and an internal variable (`?s_hidden@@3HA`) too.

### 3.3 The linkers

Microsoft's link took every variant without a message. GRIDE's link (toolchain\Windows\LINK\src\pdb.cpp, read,
not changed) does this with an object's CodeView:

- `.debug$T`: every record is copied into the PDB's TPI (ids into IPI) with its type indices rewritten, a record
  already there not again. It must know where the indices are in each kind (`type_refs`): `LF_MODIFIER`,
  `LF_POINTER` (with the class of a pointer to member), `LF_PROCEDURE`, `LF_MFUNCTION`, `LF_ARGLIST`,
  `LF_FIELDLIST` (`LF_MEMBER`, `LF_STMEMBER`, `LF_NESTTYPE`, `LF_ENUMERATE`, `LF_BCLASS`, `LF_VBCLASS`,
  `LF_IVBCLASS`, `LF_VFUNCTAB`, `LF_INDEX`, `LF_ONEMETHOD`, `LF_METHOD`), `LF_METHODLIST`, `LF_BITFIELD`,
  `LF_ARRAY`, `LF_CLASS`, `LF_STRUCTURE`, `LF_UNION`, `LF_ENUM`, `LF_VTSHAPE`, `LF_VFTABLE`, `LF_LABEL`,
  `LF_ALIAS`, and the ids `LF_FUNC_ID`, `LF_MFUNC_ID`, `LF_STRING_ID`, `LF_SUBSTR_LIST`, `LF_BUILDINFO`,
  `LF_UDT_SRC_LINE`, `LF_UDT_MOD_SRC_LINE`. One record of any other kind, an `LF_TYPESERVER2` or an `LF_PRECOMP`
  and the whole object's types are left out, silently. Every kind this design writes is in the list.
- `.debug$S`: relocated (SECREL, SECTION), a COMDAT's section dropped with its COMDAT. Symbols with a type
  field it rewrites: `S_GPROC32` / `S_LPROC32` (and the `_ID` forms, turned into the plain ones), `S_REGREL32`,
  `S_BPREL32`, `S_GDATA32`, `S_LDATA32`, the thread forms, `S_UDT`, `S_CONSTANT`, `S_LOCAL`, `S_REGISTER`,
  `S_FILESTATIC`, `S_BUILDINFO`, `S_CALLSITEINFO`, `S_HEAPALLOCSITE`; carried as they are: `S_END`,
  `S_FRAMEPROC`, `S_BLOCK32`, `S_LABEL32`, `S_THUNK32`, `S_OBJNAME`, `S_COMPILE2/3`, `S_ENVBLOCK`,
  `S_FRAMECOOKIE`, `S_SEPCODE`, the five `S_DEFRANGE_*` forms cl writes for x64, `S_UNAMESPACE`,
  `S_TRAMPOLINE`, `S_ANNOTATION`, `S_SECTION`. Any other kind is left out, and a scope it opens with it:
  `S_INLINESITE` and all inside it ("their sites are left out (no ids)"), `S_INLINEES`. A `S_GDATA32`, `S_UDT`
  (once per name) and a file-scope `S_LDATA32` go to the globals; an `S_CONSTANT` stays in the module's stream.
  `DEBUG_S_LINES`, the file table with whatever checksums it has, and the string table are carried; other
  subsections (the inlinee lines, 0xF6) are skipped.

So GRIDE's link needs nothing for stages 0 to 5 of this design. What it would need for the later inlinee row,
and two things worth having now, are section 10.

### 3.4 Names

cl's display names (probe n.cpp; `S_UDT`, the type records and the procedure symbols agree), against what
cppparse's procedure symbols say today:

| what | cl | cppparse today |
|---|---|---|
| a class in a namespace, a nested class | `geom::Circle`, `geom::Shape::Tag` | the same |
| template arguments | `Arr<int,4>`, `ns::Box<ns::Box<int> >`, `Pair<int,ns::Box<double> >`: no space after a comma, `> >` | `Arr<int, 4>` in `Symbol::name` |
| a qualified pointer argument | `Pair<char const *,unsigned long>` | |
| non-type arguments | `Val<int,-3>`, `Val<bool,1>`, `Val<char,97>`, `Val<enum ns::E,1>` (an enum argument is spelled with `enum`, a class argument without a key), `Fn<&inc>`, `Mem<0>` (a pointer to member: its value) | `Mem<&ns::In::a>::get` |
| a function template's instance, operators | `twice<int>`, `Ops::operator+`, `Ops::operator int`, `Ops::operator()` | the same |
| a member of an unnamed namespace | type `` `anonymous-namespace'::Hidden `` (its `S_UDT`: `?A0xe7d0281f::Hidden`), function `` `anonymous namespace'::hiddenFn `` | `hiddenFn` |
| a class local to a function | `use::__l2::Local` | |
| unique names | the type descriptor's decorated name, `.?AVCircle@geom@@`, `.?AU?$Arr@H$03@@` | MangleSema.cpp has them (the `??_R0` descriptors) |

A type's name is what a user types in a watch (`(Arr<int,4>*)p`) and what makes cl's and cppparse's records of
one class one type in a mixed link. The design writes cl's spelling.

### 3.5 Cost

cl, four of cppparse's own sources (with cl's headers), best of three runs:

| | time | objects | of that `.debug$S` | `.debug$T` |
|---|---|---|---|---|
| cl /Od | 8.00 s | 16,803,927 | 816 | 0 |
| cl /Od /Z7 | 8.29 s (+3.6 %) | 32,111,683 (+91 %) | 7,588,744 | 6,574,572 |
| cl /O2 | 30.42 s | 13,241,402 | 828 | 0 |
| cl /O2 /Z7 | 37.72 s (+24 %) | 37,920,554 (+186 %) | 16,866,972 | 6,589,632 |

cl's types are its library's, with every member function of every class; its `-O2` symbols are dominated by
ranges and inline sites. cppparse's will be smaller on both counts; what can be said before stage 1:

- Symbols at `-O0`: the four sources have 15,781 source variables in 4,833 functions (3.6). An `S_REGREL32`
  is 12 bytes and the name, about 20; `S_FRAMEPROC` 32 a function; a block 28. About 0.5 MB on 7.0 MB of
  objects, +7 %.
- Types: the prototype's 14 classes cost 1,232 bytes where cl spends 5,680 on the same source, about half of
  cl's on functions, ids and source lines. For the own sources it depends on how much of cppparse's library a
  unit's variables reach; stage 1 measures it, with and without member functions (stage 4).
- Time: the writer is one pass over what is already in memory; cl's +3.6 % at /Od is the expectation. Measured
  at stages 1 to 3 on the self-host.

### 3.6 What the compiler knows

- The front end (frontend/Sema.hpp) has every fact a type record needs: a class's size, bases with offsets and
  the virtual flag, fields in order with offset, bit width and bit offset, the vfptr offset and the vftables,
  the vbptr offset and the vbtables with the order of the virtual bases, static members, nested types, access;
  enumerators with values; `TypeTable::sizeOf` and `memberPointerModel` for pointers to members; the template
  arguments of an instantiation.
- The IR does not: `IrLocal::type` is the back end's own `Type` (analyzer/Types.hpp), which has no member
  pointers (`pf` of t.cpp is `int`, `fn` is `double (*)()*` in the IR dump), no typedefs (nor has `sema::Type`, which
  is canonical: 5.2), classes by a `ClassInfo`. The semantic type has to be carried beside it.
- A local or parameter is an `IrLocal` with a name; the compiler's own slots are told apart by a dot in theirs
  (`tmp.cond`, `flag.g`, `throw.object`, `part.Shape`; 7,255 of them in the four sources); a function inlined
  into another brings its locals as `<callee>.<name>`. The block a variable was declared in is not recorded.
- The frame (X86.cpp `layoutFrame`): `push rbp / mov rbp, rsp / pushes / sub rsp`, every `IrLocal` at
  `[rbp-N]`, a variadic function's parameters in their home slots at `[rbp+16+8p]`; a function with nothing in
  memory and no call has no frame at `-O2` (`m_frameless`: the parameters stay in their registers). At `-O0` a
  parameter the function reads is copied into its slot right behind `.endprolog`, and the statements' code
  begins after the copies; one it never reads is not copied (emitBody: `m_paramInReg[p] || !m_paramUsed[p]`),
  its slot holds whatever was there. A virtual function entered with an adjusted `this` stores the object's
  address into the `this` slot as its first IR instructions (IrGen.cpp 5363).
- At `-O1` and up Ssa.cpp turns every scalar local whose address is not taken into temporaries; the allocator
  (linear scan, `Interval` per temporary: a register or a spill slot for the whole interval; coalescing with
  argument registers and rax afterwards) knows where each temporary is, and nothing remembers which temporary
  was which variable. The slot `layoutFrame` still gives a promoted local is never written. A function with a
  try block is only promoted when its handlers' locals can be pinned; else SSA is skipped, its scalars stay
  memory locals, and dead store elimination removes stores to them all the same (Optimizer.cpp `deadStores`;
  review probe p3). A named result (`T r; ... return r;`) is built in the caller's object at `-O1` and up
  (IrGen's `m_nrvo`): it has no local, and IrGen rewrites its symbol's type to a reference.
- What becomes of the variables at `-O1` and up, counted on the `-O0` IR of the four sources (locals.py):

  | | variables | share |
  |---|---|---|
  | in memory for the whole function (a class, an array, address taken) | 2,080 | 13 % |
  | a scalar with one definition (a parameter never assigned, a local assigned once) | 11,848 | 75 % |
  | a scalar with several definitions | 1,853 | 12 % |

  The count is of the `-O0` IR and is an upper bound for 4.3: named results, which have no local at `-O1`
  and up, are in its first row, and so are scalars that lose a store there. Stage 3 measures what is placed.
- IrGen runs one function after another; the optimiser and the code generator run functions in parallel
  (BackEnd.cpp); the object writer encodes the functions in parallel too (Coff.cpp `runParallel`) and then
  places them, and writes their CodeView symbols, in one sequential loop. The object is written from the
  listing's text alone (`writeCoffObject(masmText, ...)`).

## 4. The options

### 4.1 Where the types are

**(a) In each object's `.debug$T`, cl's /Z7.** The linker merges them. For: both linkers do it today, verified
with cl's objects and the prototype's; objects stay self-contained (a library carries its types; no file beside
the object to keep in step; parallel compilations share nothing); a COMDAT's symbol records are dropped with
it (its types stay: `.debug$T` is one section, not associative; harmless).
Against: every object repeats the types of the headers it includes: size (3.5), and link time for the merging.

**(b) A type server: one PDB per compilation, the objects name it (`LF_TYPESERVER2`), cl's /Zi.** For: small
objects. Against: cppparse would have to write the MSF container, the TPI and IPI streams with their hashes, and
update them across compilations, also parallel ones; GRIDE's link does not read type servers (3.2: every type
is lost), so GRIDE3 would have to learn to; what Microsoft's link asks of a type server that cl did not write
is not measured. Rejected: several weeks for a size gain on intermediate files.

**(c) Hashed types (`.debug$H`, clang's `/Zf` extension).** Speeds up lld's merging; neither linker here reads
it. Not considered further.

Recommendation: (a).

### 4.2 Variables in the frame

The forms: `S_REGREL32` off RBP (cl's own form at /Od, with RSP); `S_LOCAL` +
`S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE` (8 bytes more a variable, and it needs `S_FRAMEPROC` to say which
register); `S_LOCAL` + `S_DEFRANGE_REGISTER_REL` (20 bytes and two relocations more, and the only one of the
three that can say where it holds); `S_BPREL32` (does not work, 3.2). The first three read the same through
dbghelp. DIA2Dump lists only the first, but it lists no `S_LOCAL` of cl's /O2 objects either, which Visual
Studio debugs: that is a limit of the sample's enumeration and says nothing against the form; DIA's reading of
`S_LOCAL` is proven on cl's own objects before stage 2 relies on it (section 6).

One measured fact decides how the forms may be combined (probe r, 3.2): in a procedure that has an `S_LOCAL`,
dbghelp no longer lists the `S_REGREL32` records that follow it. cl never mixes them for different variables
(its /O2 `S_REGREL32`s repeat variables its `S_LOCAL`s already describe). So a procedure is written in one form:

- **plain**: `S_REGREL32` with register 334 for every variable, where every variable lives in the frame for the
  whole function and every block is one piece of code: the usual function at `-O0`;
- **ranged**: `S_LOCAL` for every variable, a frame variable with `S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE` (its
  block in one piece) or `S_DEFRANGE_REGISTER_REL` over its block's pieces: every procedure that needs one
  `S_LOCAL`, for a register range, a variable without a place, or a block in pieces.

`S_FRAMEPROC` is written in both (30 bytes: the frame's size, RBP as the base of locals and parameters): cl
always has it, and the full-scope form needs it.

Recommendation: both forms, one per procedure, the plain one wherever it can say everything. If question 8 is
answered for ranged records in the prologue and epilogue windows, the ranged form becomes the only one.

### 4.3 Variables at `-O1` and up

**(a) Nothing: types, globals and lines only above `-O0`.** Honest and free. A debugger in a release build then
shows no parameter at all, though up to 88 % of the variables are easy to place.

**(b) The variables in memory, and the scalars with one definition.** A variable in memory (its address taken,
a class, an array; an unpromoted scalar only if no store to it was removed, 5.3) is described at its slot as at
`-O0`. A promoted scalar with one definition is one temporary: Ssa.cpp records the pair in a table of the
function (not in the instruction stream), the passes that replace a temporary by another keep the table in step
or drop the entry, and X86.cpp writes that temporary's place (its interval's register or spill slot) over the
instructions where it is live, a volatile register never across a call (5.4). A variable with several
definitions is listed as optimised out, without a place. For: up to 88 % of the variables (3.6), among them
nearly every `this`, parameter and reference; nothing is added to the IR's instructions, so goal 4 holds by
construction; a pass that cannot keep the table may always drop an entry (goal 3). Against: loop counters and
accumulators, the variables one watches in a loop, are the 12 % left out.

**(c) Debug-value instructions**, LLVM's way: a pseudo-instruction `dbgvalue local, temp` at every assignment,
carried through every pass, and ranges per definition. Covers the 12 % too. Against: an instruction in the
stream is seen by every pass: instruction counts (the inliner's threshold), empty-block tests, liveness, the
allocator's positions. Each must learn to ignore it, and each place that does not is a difference in code
between `-g` and not, which is the failure goal 4 excludes. Not for M10; a row of its own once (b) and its
test harness exist.

**(d) Under `-g`, keep source variables in memory at every level.** Exact and simple, and it changes the code:
a `-g -O2` program would be a different, slower program than the `-O2` one being debugged. Rejected by goal 4.

Recommendation: (b), and (c) as a later row.

### 4.4 How the information reaches the object writer

Today the object is a function of the listing: `;@debug` and `;@line` are comment lines the writer reads.

**(a) Everything in the listing**, the types too, as `;@type` lines. For: the object stays a function of the
listing (rebuilding from an edited listing keeps full debug information). Against: every listing grows by its
unit's types (thousands of lines that say nothing about the code), and a second text format for types has to be
written, parsed and kept in step, on the hot path of every compilation.

**(b) A side object for what belongs to the module, markers for what belongs to code positions.** The types,
globals and names are a `CvModule` that IrGen fills and the object writer receives beside the text; what has a
code position stays a marker, because only the encoder knows the offsets after it has shortened jumps: a
variable's place, a block's code, the prologue's end, a range's start. Markers name types by a small number
into the `CvModule`. Against: an object built from a listing alone has no types (limit in section 2).

Recommendation: (b). The `CvModule` can be printed (`<name>-cv6.txt`, with the other dumps) for tests and
reading.

### 4.5 Always, or an option

The line tables are always written (19 % of an object's bytes, dropped by a link without /DEBUG). Types and
variables are larger (3.5), cost time, and are of no use to a release link. cl, and every compiler GRIDE drives
(`-g` in its Debug configurations, toolchain.json), make it an option. Recommendation: `-g`, off by default;
the line tables stay as they are, with or without it. An option is also what makes goal 4 testable: every test
source compiled both ways, the objects compared outside the debug sections.

### Recommendation

Unchanged in direction by the review, sharper in the contracts: `-g`; types in the object (/Z7); a procedure in
one of two forms, `S_REGREL32` off RBP where everything lives in the frame, else `S_LOCAL` for every variable
with exact ranges (frame slots, registers for single-definition scalars at `-O1` and up) and "optimised out" for
the rest; block scopes from a scope number every instruction carries, not from line numbers; a `CvModule` beside
the listing and markers in it; new sections and their symbols behind everything an object has today; the
CodeView code in a folder of its own, cppparse/cv/ (section 9). Inlinee records, value tracking for
multiply-assigned scalars and member-function calls from the debugger are named and left to later rows.

## 5. Contracts

### 5.1 The option and the object

`-g` (with `--compile6 --obj`; it means nothing to the older pipeline and to `--assemble`). Without it the
object is what the compiler writes today, byte for byte (procedure names: question 10). `-g2` is `-g` with the
member functions in the classes (stage 4: the size criterion put them behind an option of their own); it changes
`.debug$T` and the type indices and `S_UDT` records of `.debug$S`, nothing else, and the listing is `-g`'s. With
`-g`:

- one section `.debug$T` (flags as `.debug$S`: initialized, discardable, read, 1-byte alignment):
  `CV_SIGNATURE_C13` (4), then the type records, indices from 0x1000 in the order they are written;
- in the module's `.debug$S`, after `S_COMPILE3`: the procedures as today, with the additions of 5.3, then the
  module's `S_GDATA32`, `S_LDATA32`, `S_CONSTANT` and `S_UDT`;
- in a COMDAT function's associative `.debug$S`: its procedure with its variables; a COMDAT variable (a static
  member of a class template's instantiation, a static local of an inline function, a vftable) gets an
  associative `.debug$S` with its `S_GDATA32`, as cl does, so the record goes when the COMDAT goes;
- the file table's entries carry SHA-256 checksums of the sources (type 3, 32 bytes), as cl 19.52's (stage 5): of
  each file's bytes as the preprocessor read them, and the table names every file the unit read, as cl's does,
  where without `-g` it names the files that have code. An entry is then 40 bytes where it is 8 without `-g`, so
  the line tables name their files by other offsets. A name the unit read no file by (one a `#line` directive
  gives) has an entry without a checksum, as in cl's objects.

Where the new things go is part of the contract, because it is what makes goal 4 checkable by bytes (Coff.cpp
numbers the sections .text, .data, .rdata, [.CRT$XCU], [.xdata, .pdata], [.debug$S], each COMDAT with its
associative sections, .drectve, and the symbols in the same order with the undefined externals last; a section
or a symbol put in between renumbers everything behind it, and with it the relocations of the code):

- A section `-g` adds (`.debug$T`; a COMDAT variable's `.debug$S`; the module's `.debug$S` in a unit that has
  none today, one without a function with lines) comes behind every section the object has without `-g`,
  `.drectve` included. The prototype had `.debug$T` there in every variant, and probe r an associative
  `.debug$S`: both linkers take it, also with /OPT:REF,ICF.
- A symbol `-g` adds comes behind every symbol the object has without `-g`. The only ones are the section
  symbols of the added associative sections (a COMDAT section needs one; `.debug$T` and the module's `.debug$S`
  need none).
- A debug relocation names a symbol the object has without `-g`: the variable's or function's own symbol, which
  every one of them has today (static locals and internal variables too, 3.2), or else its section's symbol,
  the offset in the relocated field. A record whose symbol the object does not define is not written (an
  `S_GDATA32` for a variable this unit only declares): a relocation to it would add an undefined external,
  which changes the symbol table and can pull in or fail a link.
- A section that exists without `-g` keeps its place; of those only the `.debug$S` sections change in content.

### 5.2 Types

One record writer, `CvTypes`: `index(record bytes)` returns the index of an equal record already written or
appends it. Records are 4-byte aligned with `LF_PAD` bytes, as cl's. Inside a record a class is always named by
its forward reference, which is written before the first record that names it; so every index a record holds
is a basic type or below the record's own. A symbol names the definition when the class is complete in the
unit, else the forward reference. A class's definition is written when a symbol or a record needs the class
complete (a variable, a member or a base of that type), and for every class a pointer or reference leads to that
is complete at the end of the unit, so that `shape->` opens in the debugger whichever unit it stops in.

| C++ | CodeView |
|---|---|
| `void`, `bool`, `char`, `signed char`, `unsigned char`, `wchar_t`, `short`, `unsigned short`, `int`, `unsigned`, `long`, `unsigned long`, `long long`, `unsigned long long`, `float`, `double`, `long double` | 0x03, 0x30, 0x70, 0x10, 0x20, 0x71, 0x11, 0x21, 0x74, 0x75, 0x12, 0x22, 0x13, 0x23, 0x40, 0x41, 0x41 (no record) |
| a pointer to one of those | 0x600 + the type (no record) |
| `const`, `volatile` | `LF_MODIFIER` {type, 1 const, 2 volatile} |
| pointer, reference | `LF_POINTER` {type, attributes 0x1000C; 0x1002C for a reference; bit 10 for a `const` pointer itself} |
| pointer to data member, to member function | `LF_POINTER` mode 2 or 3 {type, attributes with the size `TypeTable::sizeOf` gives, the class, the model: single, multiple, virtual inheritance or unspecified, from `memberPointerModel`} |
| array | `LF_ARRAY` {element, index type 0x23, size in bytes}; an array of arrays nests |
| function type | `LF_PROCEDURE` {result, convention 0, parameter count, `LF_ARGLIST`}; a member function `LF_MFUNCTION` {result, class, type of `this`, this adjustment 0, `LF_ARGLIST`}; `...` is a last argument of type 0 and is counted, as cl counts it. From stage 4 as cl 19.52 writes them: the type of `this` a plain pointer to the (const) class, none for a static member function; the function attributes: 2 a constructor, 4 with it for a class with virtual bases, 1 a class result that comes back the C++ way (every class result of a non-static member function, a class that is no plain aggregate from any other function) |
| enum | `LF_ENUM` {count, properties, 0x74, `LF_FIELDLIST` of `LF_ENUMERATE` {value, name}, name, unique name} |
| class, struct | forward reference and definition: `LF_CLASS` / `LF_STRUCTURE` {member count, properties, `LF_FIELDLIST`, derived 0, vtable shape, size, name, unique name} |
| union | forward reference and definition: `LF_UNION` {member count, properties, `LF_FIELDLIST`, size, name, unique name} (no derived and no shape field) |
| in the field list | `LF_BCLASS` {base, offset} per non-virtual base; `LF_VBCLASS` / `LF_IVBCLASS` {base, type of the vbptr, vbptr offset, index in the vbtable} per direct / indirect virtual base (from `Symbol::vbtables`); `LF_VFUNCTAB` {pointer to an `LF_VTSHAPE` of the slot count} for a vfptr the class introduces; `LF_MEMBER` {access, type, offset, name} per field, the type an `LF_BITFIELD` {type, width, position} for a bit-field; `LF_STMEMBER` per static member; `LF_NESTTYPE` per nested class, enum and typedef; with `-g2` (stage 4) `LF_ONEMETHOD` {access and kind, `LF_MFUNCTION`, the vftable slot's offset where the function introduces one, name} per member function the class declares, the functions of an overloaded name as one `LF_METHOD` {count, `LF_METHODLIST`, name}; the members in declaration order |
| a field list beyond 64 KB | continued through `LF_INDEX` |

Rules:

- Properties as cl sets them: forward reference 0x80, has unique name 0x200, nested 0x8, contains nested 0x10,
  has constructor or destructor 0x2, scoped (a local class) 0x100.
- Names by 3.4's rules, from a formatter of their own (`CvNames`) over the template arguments.
- Every class, struct, union and enum record carries its unique name (the type descriptor's mangling; property
  0x200), forward references too. This is needed for correctness, not only for agreement with cl: the linkers
  and readers resolve a forward reference by name, and two units may each have a `` `anonymous-namespace'::Node ``
  or a local class `Local` of different layouts. MangleSema already gives each unit's unnamed namespace its
  own `?A0x<id>` and a local class its function's scope; GRIDE's link hashes a scoped class by the unique name
  (pdb.cpp `type_hash`). The `S_UDT` of a class in an unnamed namespace is spelled as cl spells it
  (`?A0x<id>::Hidden`, 3.4), so the one-`S_UDT`-per-name rule of GRIDE's link does not merge two of them.
- The this adjustment of `LF_MFUNCTION` is 0 for every function. cl's functions that are entered with an
  adjusted `this` (an override reached through a secondary base's table) work on the adjusted pointer, and the
  field tells the reader to undo it. cppparse's put `this` right themselves at entry (IrGen.cpp 5363: the
  adjusted value is stored into the `this` slot before the first statement), so the frame's `this` is the
  object and a nonzero field would make a reader adjust twice. What it costs: 5.3, the entry sequence. (Stage 4:
  this holds for the type a procedure's own symbol names. The type a class lists for such a function under `-g2`
  carries cl's number, what a caller adds to the object's address: section 11, the note of stage 4.)
- A record is at most 0xFF00 bytes. A name that would exceed it (deep template names) is cut as cl cuts it, the
  unique name kept whole when it fits; stage 1 measures cl's cut before writing it.
- A typedef has no record of its own in CodeView: it is named by an `S_UDT` (namespace scope) or an
  `LF_NESTTYPE` (class scope), from the Typedef symbols of each scope; `sema::Type` is canonical and does not
  remember the typedef a variable was declared with, so a variable shows its canonical type.
- `CvFromSema` only reads what Sema has when IrGen runs: no `completeClass`, no instantiation, no lookup that
  can create a symbol. Otherwise `-g` could instantiate a template and change the code. A class that is
  incomplete at that point is a forward reference, whatever a debugger would like.
- Type indices are assigned in the object writer's sequential loop only (5.4); the parallel encoding of the
  functions never calls `CvTypes`.

cl's records for anonymous unions and structs, for unnamed bit-fields and for a class without any member are
not in the probes; stage 1 measures them before it writes them.

Not written: ids (`LF_FUNC_ID` and the others: a procedure's symbol carries its type directly, which both
linkers take, 3.2; ids come with the inlinee row), `LF_UDT_SRC_LINE`, `LF_BUILDINFO`.

### 5.3 Symbols

Module level:

    S_GDATA32 / S_LDATA32   {type, SECREL + SECTION of the variable's symbol, name}: every variable of namespace scope
                            and every static member the unit defines; S_LDATA32 for internal linkage
    S_CONSTANT              {type, value, name}: a namespace-scope const of integral or enum type with a constant
                            value and no storage in the object
    S_UDT                   {type, name}: every class and enum whose definition was written, every typedef of
                            namespace or class scope a written type or symbol was declared with

A procedure is written in one of two forms (4.2), never a mixture. The plain form, when every variable of the
procedure lives in the frame for the whole function and every block that declares a variable is one piece of
code:

    S_GPROC32 / S_LPROC32   as today, with the type (LF_PROCEDURE / LF_MFUNCTION), DbgStart = the offset behind the
                            entry sequence, DbgEnd = the offset of the last epilogue
    S_FRAMEPROC             {frame size, 0, 0, bytes of saved registers, 0, 0, flags: RBP (2) as the base of locals
                            and of parameters, and those of cl's flags that apply}
    S_REGREL32              {offset from rbp, type, 334, name} per parameter, then per local of the function's
                            outermost block
    S_LDATA32               per static local
    S_BLOCK32 ... S_END     {code offset and length} per inner block that declares a variable, nested as in the
                            source, with that block's S_REGREL32 / S_LDATA32 inside
    S_END

The ranged form, for every other procedure:

    S_GPROC32 / S_LPROC32   the same, with the flag 0x80 (optimised debug information), as cl sets it on its /O2
                            procedures (review probe p2)
    S_FRAMEPROC             the same; not for a function without a frame
    S_LOCAL                 {type, flags, name} per parameter, then per local of the outermost block, and per
                            variable of an inner block that is in pieces; flags 1: a parameter, 0x100: optimised
                            out. Behind each, its places:
      S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE {offset}      in the frame wherever its block is, the block one piece
      S_DEFRANGE_REGISTER_REL {334, 0, offset, range, gaps}  in the frame, over the pieces of its block
      S_DEFRANGE_REGISTER {register, 0, range, gaps}       in that register over that range, cut to its block
      nothing, and the flag 0x100                          no place the compiler can vouch for
    S_LDATA32               per static local
    S_BLOCK32 ... S_END     per inner block in one piece that declares a variable, its variables inside in the
                            same form
    S_END

Rules:

- Parameters. Every declared parameter has a record, in declaration order, before the first local: `this` first
  in a non-static member function, an unnamed parameter under cl's name `__formal`, nothing for `...` and
  nothing for the hidden result pointer. The count is then the `LF_ARGLIST`'s, less one for `...`, plus one for
  `this`: what cl writes (review probe p2), and independent of DIA's positional rule (3.2), which was measured
  on a free function only; stage 2 measures a member function and a variadic one through DIA before it relies
  on anything.
- A parameter without a place is still a record (`S_LOCAL`, flags 0x101, no range). At `-O0`: one the function
  never reads (X86.cpp `m_paramUsed`: it is not copied into its slot; review probe p1). At `-O1` and up: one
  that is assigned, or whose temporary did not survive. The same record (flags 0x100) for every source local
  without a place. Probe r: dbghelp lists such a record without an address.
- Only source variables: an `IrLocal` IrGen marks as one. The compiler's own slots, the hidden result pointer
  and, until the inlinee row, the locals of inlined functions are not written.
- A class parameter passed by address is described as a reference to the class (`LF_POINTER` 0x1002C) at its
  slot, so that every reader shows the object; the procedure's `LF_ARGLIST` still says the class. This differs
  from cl on purpose (3.1: cl's record reads wrong through dbghelp). Stage 2 checks it in Visual Studio.
- A named result built in the caller's object (IrGen's `m_nrvo`, at `-O1` and up: `std::string s; ... return
  s;`) has no slot of its own: it is described as a reference to its type at the place of the hidden result
  pointer, by the same rule and with the type the variable had before IrGen rewrote it.
- Scopes. A block's code is the instructions that carry its scope number or a descendant's (5.4), not a hull of
  line numbers. If they are one run of bytes the block is an `S_BLOCK32` over it. If not (handlers, landing
  pads and `--eh=cl` funclets come behind the function's `ret` at every level; cold blocks at `-O2`), the
  block has no record: its variables are listed in the nearest enclosing block that has one (in the end the
  procedure), each with ranges cut to the block's pieces, one range from the first piece to the last with the
  holes as gaps (probe r: dbghelp honours gaps and several ranges, and tells two records of one name apart by
  their ranges). The procedure is then in the ranged form.
- In which frames a place holds. A frame slot holds between `DbgStart` and the epilogue (goal 3). Its ranges
  never include prologue or epilogue code when they come from a block in pieces; the full-scope form and
  `S_REGREL32` cannot exclude it.
- The entry sequence ends, and `DbgStart` is, behind the parameter copies and, in a function entered with an
  adjusted `this` (`Symbol::thisAdjust` not 0), behind the store that puts `this` right. At `-O1` and up the
  value that counts as `this` in such a function is the one that store defines, not the incoming register.
- At `-O1` and up a frame slot is a variable's place only if the variable's address is taken or it is a class
  or an array (Ssa.cpp `promotable()` false), or it is a scalar that was not promoted and none of its stores
  was removed (`IrLocal::staleStores` not set, 5.4). "Has a slot" is not the criterion: a promoted local keeps
  a slot nothing writes, and in a function SSA skips ("a catch handler resumes the normal code") dead store
  elimination removes stores to scalars that stay in memory (review probe p3: `observed@[rbp-20]` listed and
  never written).
- A register range: 5.4's `;@loc` rules say where it opens and closes. Its register is named by the value's
  size and kind, as cl does, from an explicit table per register and size (`CvRegisters`); no arithmetic over
  the x86 encoding order, which the 64-bit and the new 8-bit numbers do not follow:

      al cl dl bl 1..4, sil dil 324 325, r8b..r15b 344..351        (ah ch dh bh, 5..8, are never written)
      ax cx dx bx 9..12, si di 15 16, r8w..r15w 352..359
      eax ecx edx ebx 17..20, esi edi 23 24, r8d..r15d 360..367
      rax rbx rcx rdx rsi rdi 328..333, rbp 334, rsp 335, r8..r15 336..343
      a float in xmmN: XMMN_0, 162 + 4N (N < 8), 260 + 4(N-8); a double: XMMNL, 194 + N (N < 8), 292 + (N-8)

- A range is {SECREL + SECTION of the function, length} within the function, at most 65,535 bytes long (the
  field is 16 bits; a longer one is split).
- In a funclet of `--eh=cl` the function's variables are addressed off `rbp` as in the function, from the
  instruction that sets the funclet's `rbp` to the function's (limit in section 2 for the three instructions
  before it): funclets are code of the same PROC. A handler's variable is a frame local like any other, in a
  block whose code is the funclet's.
- A static local of a block in pieces is listed in the enclosing block that has a record (it has an address,
  not a range).

### 5.4 The hand-off

**Front end to back end.** IrGen (one function after another, so every number below is assigned in a fixed order):

- `IrLocal` gets `debugType` (a number into the `CvModule`, 0 for none), `source` (a variable of the source, not
  a slot of the compiler's), `byAddress` (a class parameter passed by address), `block` (the number of the
  block scope that declares it, 0 for the function's) and `staleStores` (set later, below);
- `IrInst` gets `scope`, the number of the innermost block scope that declares a variable or a static local and
  holds the statement the instruction was generated for (a `for` statement's own scope included; 0: the
  function's). It is a field beside `line`, not an instruction, and it is set wherever `line` is set: by IrGen,
  by a pass that makes an instruction from another (it copies both), by the inliner (below). A cleanup's code
  and its landing pad carry the scope of the object they destroy; a handler's code the handler's scope. Sema
  has the scopes (`Symbol::scope`, `Scope::blockIndex`); the AST's statements have no end line (`cxx::Stmt`
  has one `loc`) and need none;
- `IrFunction` gets `debugType` (its function type), `blocks`, a list of {parent} for every such block scope,
  `debugAliases` (a named result built in the caller's object: its name, type number and block, and the local
  that holds the hidden result pointer) and `debugEntry`, the number of leading instructions that put `this`
  right (5.3);
- `IrGlobal` gets `debugType` and `debugName` (the qualified name) and, for a static local, its function and block;
- at `-O1` and up `IrFunction::debugTemps`, a map from a source local that Ssa.cpp promoted to the temporary
  that is its one definition (for a `this` that is adjusted at entry: the adjusted one). Passes that replace
  temporary `a` by `b` (copy propagation, value numbering, phi simplification) rename the entry; a pass that
  removes the temporary, or cannot say, erases it. After the last pass a verifier erases every entry whose
  temporary is not defined exactly once.

What the optimiser owes these fields, and nothing else:

- no pass reads any of them to decide anything (goal 4);
- a pass that removes a store to a local sets that local's `staleStores` (today `deadStores`; forwarding keeps
  the stores);
- the inliner clears `source`, `block` and `debugType` on the locals it copies into the caller, drops the
  callee's `debugTemps` and `debugAliases`, and gives every copied instruction the scope of the call it
  replaces, as it gives it the call's line.

A scope number may always be replaced by an ancestor's and a `debugTemps` entry may always be erased: the
variable is then absent in that code, never wrong.

**`CvModule`** (cppparse/cv): what the module's debug information is made from, free of the front end's
classes, as eh/EhClThrowInfo's description of a thrown type is. `CvFromSema` (middle/, beside IrGen) fills it:
`typeOf(const sema::Type*)` returns the number of a type description, new or known; a class's description
(members, bases, tables) is completed at the end of the unit, when every layout is final; the globals,
constants and typedef names are listed in declaration order.

**Listing** (comment lines to ml64; `T<n>` is a type's number in the `CvModule`):

    ;@debug "<file>" <name>            as today
    ;@proc T<n> <frame size> <saved>   the procedure's type, S_FRAMEPROC's numbers
    ;@prologend                        DbgStart: behind the entry sequence
    ;@epilog                           at the first instruction of each epilogue; DbgEnd is the last one's
    ;@block <id> <parent>              a block scope that declares something
    ;@scope <id>                       from the next instruction on the code belongs to block <id>
    ;@local <block> T<n> [rbp-N] <p|l><r> <name>       a variable in the frame: parameter or local, r: by address
    ;@gone <block> T<n> <p|l> <name>                   a variable without a place
    ;@static <block> T<n> <symbol> <name>
    ;@var <id> <block> T<n> <p|l><r> <name>            a variable with ranges (-O1 and up)
    ;@loc <id> <register | [rbp-N] | ->                from the next instruction on, variable <id> is there / nowhere

X86.cpp writes them when the frame is laid out (`;@proc`, `;@block`, and the variables of each block in
declaration order, the parameters first, whichever of `;@local`, `;@gone`, `;@static`, `;@var` each one is) and
while it emits code (`;@prologend`, `;@epilog`, `;@scope` where the scope of the instruction being emitted
differs from the last one's, as `;@line`; `;@loc`). The instructions X86.cpp adds for an IR instruction (spill
loads, moves between registers) belong to that instruction's scope. Without `-g` none of them is written.

`;@loc` at `-O1` and up, for each entry of `debugTemps` and each alias:

- The place is the allocator's own answer, by the function `emitInst` asks for an operand's register or slot.
  A temporary coalesced into another register for part of its life is reported where it is, by that same
  function, or not at all.
- A range opens behind the instruction that defines the temporary; for a parameter where the function's code
  begins, in its incoming register or its slot. Across blocks by `computeLiveness`: open at a block's start
  when the temporary is live into it.
- A range closes behind the temporary's last use in the block; at that instruction, not behind it, when the
  instruction also writes the register.
- A range in a volatile register (rax, rcx, rdx, r8 to r11, xmm0 to xmm5) closes at the first byte of every
  call instruction, and of every other operation X86.cpp counts as a clobber (`m_clobbers`: calls, throws, the
  catch and try operations that call the runtime). Such a range cannot hold beyond the call anyway; the point
  is the frames above the top one: a debugger looks an outer frame up at its return address less one, inside
  the call instruction, and reads registers from the unwound context, which restores nonvolatile registers
  only. X86.cpp's `hintArguments` and `coalesce` put a value whose last use is an argument into rcx, rdx, r8 or
  r9 on purpose, so without this rule every such variable would read the callee's register in every outer
  frame. A range in a nonvolatile register (rbx, rsi, rdi, r12 to r15, the xmm registers the prologue saves)
  or in a slot spans calls: the unwind codes restore those.

**Object writer.** `writeCoffObject` and `assembleCoff` take a `const CvModule*` (null: today's object). The
encoder, which runs the functions in parallel (Coff.cpp `runParallel`), records the code offset of every
position marker as it records `;@line`'s, and for the checker the registers each instruction writes; it calls
nothing of cv/. The sequential placement loop behind it, which is where `procedureSymbols` runs today, hands
each function's markers, in listing order, to the CodeView builders (cppparse/cv): they compute the scopes'
pieces, choose the procedure's form, and turn the markers and the `CvModule` into the symbol bytes and
relocations, asking `CvTypes` for indices as they go; then the module's symbols in declaration order. A marker
whose `T<n>` the writer cannot resolve (no `CvModule`: an object rebuilt from a listing) is skipped with its
variable.

### 5.5 Invariants, and the checker

1. **No code change.** `-g` adds marker lines, fills fields and tables that no code-generating decision reads,
   and adds sections and symbols behind the existing ones (5.1). Checked for every source the suite compiles,
   by a comparator (cvsame, a sandbox script over the two objects' parsed headers, built first in stage 1):
   - the object without `-g` has N sections and M symbol table entries; the object with `-g` has at least as
     many;
   - its first N section headers are equal in name, flags and alignment, and outside `.debug$S` in size,
     contents and relocation entries, byte for byte (file pointers are not compared: they move);
   - its first M symbol table entries are the same bytes, and the string table of the first object is a prefix
     of the second's; so every symbol index in every relocation of the code means what it meant;
   - the sections behind N are `.debug$T` or `.debug$S`, the symbols behind M are their section symbols;
   - the listings are equal once the `;@` lines are taken out.
2. **Determinism.** Type numbers are given by IrGen in the order it first meets a type; scope numbers by IrGen
   in source order; type indices by the writer's sequential loop in the order of the records' first use,
   walking the functions in listing order, then the module's symbols in declaration order; equal records are
   found by their bytes. Nothing is ordered by an address in memory: no loop over a container keyed by a
   pointer decides an order. Checked by the self-host with `-g` (generation two's objects equal generation
   one's, at `-O0` and `-O2`) and by compiling with `-j 1` and `-j 4`.
3. **A location is a promise** (goal 3). Checked statically by 4 (c) for all code the compiler writes, and by
   running: 6, the value harness.
4. **Well-formed records.** `CvCheck` (cv/, called by the object writer on what it is about to write, as
   EhClCheck is). A violation is an internal error that names the function.
   - (a) Types: every type index a record or symbol holds is a basic type or below the record that holds it;
     no record exceeds 0xFF00 bytes; every class, struct, union and enum record has a unique name; no `T<n>`
     is unresolved under `-g`.
   - (b) Symbols: the parameter records come first, in declaration order, and are as many as the procedure's
     type says, `this` and `...` accounted for (5.3); a procedure has no `S_REGREL32` if it has an `S_LOCAL`;
     every `S_BLOCK32` lies in its parent and apart from its siblings; a variable's ranges lie in the function
     and in its block's code, ascend and do not overlap, their gaps inside them; a register is one of the
     table's, never 5 to 8, and fits the type's kind and size; every relocation of a debug section names a
     symbol that is among the first M (5.1).
   - (c) Places, from what the encoder knows of each instruction: inside a register range no instruction writes
     that register; inside a range in a volatile register there is no call; a range of a frame slot that is
     given by code offsets (the `S_DEFRANGE_REGISTER_REL` form) holds no instruction of the prologue or of an
     epilogue. This covers every function of every object, where the harness covers the shapes it has.

### 5.6 What changes where

| file | change |
|---|---|
| driver/main.cpp, Compiler.cpp | `-g`; the `CvModule` from the compilation to `writeCoffObject`; `<name>-cv6.txt` with the other dumps; from stage 5 the sources' checksums into the `CvModule`, and `--sha256` |
| frontend/ | nothing: the facts exist, and scopes come from Sema's own (no end lines in the AST). (Stage 5: the preprocessor tells an observer of the bytes of every file it reads, for the checksums; section 11) |
| middle/IrGen.cpp | marks source locals, their block and by-address parameters; stamps every instruction's scope; asks `CvFromSema` for type numbers (locals, the function, globals, static locals); fills `IrFunction::blocks`, `debugAliases` (before it rewrites a named result's type), `debugEntry` |
| middle/CvFromSema.cpp (new) | semantic types and symbols to `CvModule` descriptions: classes with bases, fields, tables; names by cl's rules; constants, typedefs; reads Sema only |
| middle/Ssa.cpp, Optimizer.cpp | `debugTemps`: recorded at promotion, renamed or erased by the passes that replace or remove temporaries, verified after the last; `staleStores` where a store is removed; `scope` copied with `line`; the inliner's three duties (5.4) |
| middle/Ir.hpp, Ir.cpp | the new fields; the IR dump prints them under `-g` only |
| backend/X86.cpp | the markers of 5.4; the place of a temporary over its life for `;@loc`, with the volatile-register rule |
| backend/Coff.cpp | new sections and symbols behind the existing ones; the offsets of the position markers; the registers an instruction writes, for `CvCheck`; a COMDAT variable's associative `.debug$S`; calls cv/ from the sequential loop for every `.debug$S` and for `.debug$T`; `CvCheck` |
| cv/ (new) | section 9 |
| backend/MangleSema.cpp | the unique name of a type, given out for cv's use (the mangling exists) |
| runtime/, libcpp/, include/ | nothing |
| sandbox scripts | cvsame (the comparator of invariant 1); build-cppparse.ps1: test 19 (types, variables, the value harness) and the `-g` / no `-g` comparison; selfhost.ps1 `-G`; `cv` among the source folders (toolset.ps1 `$SourceDirs`, the Visual Studio project) |
| README, docs | "Debug information": `-g`, what a debugger shows at each level, the limits |

## 6. Plan

Every stage ends with harness.ps1 passing in full (the /W4 build, selfhost `-O0` and `-O2` with generation two
identical, strict98, crc32.txt rewritten) and its own exit test. From stage 1 on the harness also runs the
comparison of invariant 1 over the suite's sources, and from stage 2 the self-host once more with `-g`.

The test programs (examples\sample-dbg, driver in sample-dbg-driver, both extended):

- **types.cpp** declares the kinds of 2/1 and prints, for each class, what the program itself knows: `sizeof`,
  each member's offset (through a pointer to the member in a real object), each base's offset, an enumerator's
  value, a bit-field's value after a store. The driver reads the same facts from the PDB through dbghelp
  (`SymGetTypeFromName`, `SymGetTypeInfo`: children, offsets, lengths, bit positions, base classes, virtual
  base pointer offsets) and compares. The program is its own oracle, so no expected numbers are kept in a
  script. The same source through cl /Z7 gives a second text of the type tree (names, kinds, sizes, members);
  the two texts are compared after a stated normalisation: member functions and compiler-generated members
  (`__vecDelDtor`, implicit special members) left out, an unnamed namespace's `?A0x<id>` replaced by one
  token, `LF_VTSHAPE` compared by slot count only. With it **types2.cpp**, a second unit: a class of the same
  name and a different layout in each unit's unnamed namespace, and a local class of one name in two
  functions; the driver asks for each from an address in its own unit.
- **mixed types** (with mix.ps1's arrangement, shared headers): one class defined by a cl /Z7 object and by a
  cppparse `-g` object of one program, through both linkers; a variable of the class in each object shows its
  members right from either.
- **The value harness, vars.cpp.** Functions of every shape call `checkpoint(id, ...)`, a function of another
  object compiled by cl, with the current values of their variables, which it records. The shapes, each a rule
  of 5.3 or 5.4: `this`; an override reached through a second polymorphic base (`this->member`), and a
  destructor of such a class; references; class parameters by value and by address; a named result returned
  by value; a parameter that is never read, an unnamed one, one that is assigned; a variadic function; two
  sibling blocks that declare one name, the first holding a try; a block with an object that has a destructor;
  a static local; a handler's variable; a function whose `catch (...)` resumes and whose scalars lose stores
  (the review's p3); variables live across a call in nonvolatile integer and xmm registers; variables whose
  last use is the call's argument; a `bool` and a `char` that land in sil or dil; floats and doubles; more
  than four parameters.
  The driver runs the program under the Win32 debug API (a small debugger: a breakpoint on `checkpoint`, the
  caller's context from one unwind step, `SymSetContext` and `SymEnumSymbols` for the caller's frame and for
  the frames above it) and compares each variable dbghelp gives a place for with the value the program passed.
  Then, in the top frame: it returns from `checkpoint`, steps to the first address of the next line and
  compares again there (the sources change no checked variable between a checkpoint and the next line), which
  is where a stepping user stops and where volatile registers count. Wrong value: failure. Every declared
  parameter must be listed, in order, at every level. At `-O0` every variable but a never-read parameter must
  have its place; at `-O1` to `-O3` a listed minimum must (the parameters that are not assigned, the variables
  whose address is taken, the single-definition locals that live across the call), and any other may be without
  a place but not wrong.
- The same programs linked by GRIDE's link when GRIDE3 is on the machine (the driver's output must be equal to
  the one from Microsoft's link), run through gride-dbg and, where cdb.exe is installed, through cdb with one
  command script (`bp`, `g`, `dv /t /V`, `??`; gride-dbg speaks cdb's commands), and read through DIA
  (a second small reader over `IDiaSession::findChildrenExByRVA`, loaded with `NoRegCoCreate` from the DIA SDK
  when Visual Studio has one), which must name the same places as dbghelp. The DIA reader is an oracle only
  after it has shown cl's own /O2 object of vars.cpp right (`S_LOCAL` with ranges): the first step of stage 2.
- Link variants, from stage 2 on, for vars.exe and types.exe by both linkers where the linker has the option:
  /DEBUG alone (Microsoft's link then links incrementally, through ILT thunks), /DEBUG:FULL /INCREMENTAL:NO,
  /DEBUG /OPT:REF,ICF (COMDATs and their associative `.debug$S` dropped and folded), and the `-g` objects from
  a static library.
- By hand, at the end of stages 2 and 3: a session in Visual Studio's debugger on vars.exe (Locals, Watch with
  a member access and a cast to a template's name, the call stack's frames, the two sibling blocks of one
  name, an optimised-out parameter), its panes recorded as text in the stage's notes.

| stage | what | exit test |
|---|---|---|
| 0 (done 2026-11-20, see the status) | Source layout (section 9): cppparse/cv/ with the classes marked "from"; Coff.cpp calls them; nothing else changes | every .asm and .obj of the suite, lib, life, eh, matrix and the self-host is byte for byte what the commit before wrote (the comparison script of M7's stage 0) |
| 1 (done 2026-11-20, see the status) | First cvsame, the comparator of invariant 1, and with it the rule of 5.1 for added sections and symbols. Then, by question 10, cl's spelling of procedure names for every object, a commit of its own. Then `-g`: `CvTypes` and its records, `CvModule`, `CvFromSema`, `CvNames`; the procedure's type; `S_GDATA32` / `S_LDATA32` (static locals at function level until stage 2), `S_CONSTANT`, `S_UDT`; a COMDAT variable's section; `CvCheck` (a) and the relocation rule of (b); `<name>-cv6.txt`. Before the records are written, the missing measurements of 5.2 (anonymous unions, unnamed bit-fields, empty classes, cl's cut of a long name) | the spelling commit: cvsame says nothing but `.debug$S` changed, in every object of the suite. types.cpp and types2.cpp at `-O0` and `-O2`: dbghelp's facts equal the program's own; the type tree equal to cl /Z7's after the normalisation; the mixed-types program; both linkers without a message, GRIDE's PDB giving the driver the same output; llvm-pdbutil dumps every object without an error. Invariant 1 over the suite; libcpp, the samples and the own sources compile with `-g` and pass `CvCheck`; self-host with `-g`, generation two identical; the `-g` self-host's objects (about 80) linked with /DEBUG by both linkers, PDB size and link time recorded; object bytes and compile time with and without `-g` recorded (own sources, sample-lib) |
| 2 (done 2026-11-24, see the status) | First the measurements: the DIA reader on cl's /O2 object; DIA's parameters of a member function and of a variadic one; the `S_FRAMEPROC` flags cl sets. Then variables at `-O0`: source locals, scope numbers through IrGen, by-address parameters; `;@proc`, `;@prologend`, `;@epilog`, `;@block`, `;@scope`, `;@local`, `;@gone`, `;@static`; `CvScopes`; both procedure forms; `S_FRAMEPROC`, `DbgStart` / `DbgEnd`; `CvCheck` (b) and the frame part of (c); all three exception models | the value harness at `-O0` with `--eh=table`, `sjlj` and `cl` (a checkpoint inside a handler and inside a cleanup's destructor): every parameter listed, every variable but a never-read parameter present and right, in the checkpoint's caller, in the frames above it and at the next line; the DIA reader agrees; gride-dbg's `dv` on vars.exe equal from both linkers; the link variants; the session in Visual Studio; invariant 1; counted on the own sources and recorded: procedures in each form, blocks in pieces, never-read parameters; sizes and time |
| 3 (done 2026-11-25, see the status) | Variables at `-O1` and up: `debugTemps` through Ssa and the optimiser with its verifier; `staleStores`; the inliner's duties; named results; `;@var`, `;@loc` with the volatile-register rule; `S_DEFRANGE_REGISTER`; functions without a frame; `CvRegisters` whole; `CvCheck` (c) | the value harness at `-O1`, `-O2`, `-O3`: no wrong value, in the caller, above it and at the next line; the minimum present; the share of source variables with a place counted on the own sources and recorded (expected below 88 %: section 3.6's count includes named results and scalars with removed stores); self-host with `-g` at `-O2`, generation two identical, `-j 1` and `-j 4` equal; invariant 1 at every level; the own sources and the suite compiled with `-g -O2` pass `CvCheck` (c); the suite under a compiler built with `-g -O2`; the link variants |
| 4 (done 2026-11-25, see the status: behind `-g2`) | Member functions in the types: `LF_ONEMETHOD`, `LF_METHOD`, `LF_METHODLIST`, `LF_MFUNCTION` for declared members, vftable offsets of introducing virtual functions | types.cpp's tree equal to cl's with member functions (cl's compiler-generated entries apart); `.debug$T` bytes with and without them on the own sources and sample-lib: they stay in under `-g` if the objects grow by less than a quarter, else they become `-g2` and the numbers are recorded |
| 5 (done 2026-11-26, see the status) | SHA-256 checksums in the file table; README, help text, next-targets; the measurements as a table in this document's status | dbghelp returns the checksum (`SymGetSourceFileChecksum`) equal to the source's own SHA-256, and no longer equal after the source is edited; the cost table: objects, PDB, compile and link time with and without `-g`, both linkers, the self-host included |
| later, own rows | inlinee records (after section 10's L3); value tracking for multiply-assigned scalars (4.3 c); source lines of types | |

Stages 3 and 4 do not depend on each other. The order stands because the size decision of stage 4 wants the
symbol sizes of stage 3 beside it, and because stage 3 is the one that can fail (question 2's fallback removes
it); the lead may run stage 4 first to close the `-O0` experience earlier.

## 7. Risks and estimate

1. **A wrong location at `-O1` and up.** The allocator's later steps (coalescing with argument registers, results
   left in rax, the second allocation around coalesced intervals) move values after the intervals are made;
   `;@loc` must say what the emitted code does, not what the first allocation planned; and a register that is
   right in the top frame is wrong in the frame above when it is volatile (5.4). Answer: one function answers
   "where is this temporary now" for both the emitter and the marker; the volatile-register rule; `CvCheck`
   (c), a second opinion from the encoder's side on every function of every object, the own sources at `-O2`
   included; the value harness at three levels, in three kinds of frame; and the rule that dropping a variable
   is always allowed.
2. **A pass that forgets `debugTemps` or `staleStores`.** A renamed temporary whose entry was not renamed
   describes a dead or reused temporary; a removed store without the flag leaves a slot described that holds
   an old value. Answer for the first: the verifier after the last pass (an entry whose temporary is not
   defined exactly once is erased; a reused number cannot happen, temporaries are numbered once). It has to
   hold through about ten passes (constant folding, copy propagation, algebraic rewriting, value numbering,
   phi simplification, dead code, CFG simplification, loop-invariant motion, SSA destruction and the level-1
   run behind it, where phi copies make a temporary multiply defined), which is why stage 3 is the heaviest.
   Answer for the second: one helper removes a store to a local and sets the flag, and stage 3 greps that no
   pass erases a `StoreLocal` another way; the shape that showed it (p3) is in the harness.
3. **Size.** Types of the library repeated in every object; unknown until stage 1 measures it. If `-g` more than
   doubles the own sources' objects without member functions, stage 1 stops and the rule "every complete class a
   pointer leads to" (5.2) is narrowed to classes the unit's own code uses complete. The ranged form costs 8
   bytes a variable more than the plain one and a block in pieces 20 and two relocations a variable; stage 2
   counts how many procedures need it.
4. **cl's names.** A name that differs from cl's in one character makes two types of one in a mixed link and
   misses a visualizer. Answer: the type-tree comparison against cl in stage 1 covers the forms of 3.4; forms it
   does not cover (template template arguments, pointers to member functions as arguments) are listed as they
   are found.
5. **Readers have rules nobody wrote down.** DIA and dbghelp are two readers; Visual Studio's expression
   evaluator is a third layer and is only checked by hand. Probe r found one such rule after the review
   (dbghelp drops `S_REGREL32` behind an `S_LOCAL`), and the design had it wrong before. Answer: no record form
   is used that the prototype has not shown through both linkers and dbghelp first, and DIA's reading of
   `S_LOCAL` is proven on cl's objects before stage 2 uses it. The places this design leaves cl's habits: the
   by-address parameter as a reference (fallback: cl's form, one line in X86.cpp); two records of one name in
   one scope, told apart by range, for sibling blocks in pieces (fallback: the later one is left out there);
   the ranged form with flag 0x80 in `-O0` code. Each is in the Visual Studio session of stage 2.
6. **cl is a preview build** (19.52). Formats are stable; counts in section 3 may move.
7. **Scopes in many pieces at `-O1` and up.** Scope numbers travel with `line`; a pass that makes an instruction
   without a source gives it scope 0, which cuts a block in two and turns it into ranges with gaps. That is
   never wrong, but it costs bytes and the `S_BLOCK32` nesting. Stage 3 counts blocks in more than four pieces
   on the own sources; if they are common the passes concerned are made to copy the neighbour's scope.
8. **The self-host's identity** rests on no order coming from memory addresses; a `std::map` keyed by a
   `Symbol*` iterated once in `CvFromSema` would break generation two on some machine only. Answer: invariant 2
   stated in the code, the `-j 1` / `-j 4` and two-generation comparisons in every stage.
9. **`CvCheck` (c) needs to know which registers an instruction writes**, for every form X86.cpp emits (implicit
   ones too: `mul`, `div`, `cdq`, the string instructions, a call's volatile set). A hole there is a missed
   error, a wrong entry a false internal error. Answer: the table lives beside the encoder's own mnemonic
   table, an unknown mnemonic counts as writing everything, and the own sources at `-O2` are its first test.
10. **The windows of goal 3** (prologue, epilogue, funclet entry) are untested by construction and stay wrong
   in the plain form, as with cl. If the user wants them closed (question 8), every procedure takes the ranged
   form with `S_DEFRANGE_REGISTER_REL`: about 0.2 MB of records and 0.3 MB of relocations on the four sources'
   7.0 MB (the review's figures), and stage 2 grows by the epilogue ranges.

Estimate. The unit first, because the review asked what it is worth. M7's design estimated 29 to 40 working
days of one lane; its seven stage commits (0, 0b, 1 to 5) all landed on 2026-11-18 between 16:18 and 20:14,
half an hour to an hour apart, five and a half hours after the design's first text. So a "working day" of
these estimates never was a calendar day: it is a weight that ranks the stages against each other, and M7's
weights ranked them rightly (its stage 3 was the long one). What M7 shows for the calendar is that a stage is
one lane of about an hour when it goes well, and that the lead's review between lanes and the user's answers
set the pace. The estimate is therefore given as weights and as lanes, and the calendar from M7's ratio.

Weights (the first text's in brackets), with what the review added: stage 0: 2 to 3 (2 to 3); stage 1: 9 to 13
(8 to 11): the comparator, the section and symbol order, the spelling commit, unique names and the second unit,
the mixed link, the self-host's PDB; stage 2: 8 to 12 (6 to 9): scope numbers through IrGen, two procedure
forms, the DIA reader's proof, the top-frame checks, the link variants; stage 3: 11 to 16 (8 to 12): the
volatile-register rule, `CvCheck` (c) with the encoder's write sets, `staleStores`, named results, the
inliner, the upkeep of `debugTemps` through ten passes; stage 4: 3 to 5; stage 5: 2 to 3. Together 35 to 52
(29 to 43).

Lanes of the kind M7 ran in (one stage or half a stage a lane, the lead reviewing each): eleven to thirteen
(eight to ten), one after another: stage 0 one; stage 1 three (the comparator with the layout and the spelling
commit; the record writer with `CvCheck`; `CvFromSema` with names and the type tests); stage 2 three (the
measurements and the DIA reader; the compiler; the value harness with the link variants); stage 3 three (the
optimiser's tables; the allocator's ranges and the checker; the harness at three levels and the counts);
stages 4 and 5 one each. By M7's ratio that is eight to ten hours of lanes if each goes as M7's went; one to
two days is the figure to plan with, not counting the two sessions in Visual Studio and the user's answers.
Stage 3 holds most of the uncertainty: unlike any stage of M7 it touches every optimiser pass, so it is the
one most likely to need a lane more. After stage 2 the row's first
half, debugging a `-O0` program with variables and types, is closed.

## 8. Questions for the user

1. `-g` as an option, off by default, the line tables staying always on (4.5)? The alternative is types and
   variables in every object.
2. At `-O1` and up: the variables in memory and those with one definition (4.3 b), with multiply-assigned
   scalars listed as optimised out and left to a later row? Or `-O0` only for M10 (4.3 a), which removes
   stage 3?
3. Member functions in the types: by the size criterion of stage 4, or decided now (in, or never)? (Stage 4,
   2026-11-25: the criterion gave `-g2`; the status has the numbers.)
4. A class parameter passed by address described as a reference (5.3), where cl writes the class itself and
   dbghelp then shows wrong values: agreed, with cl's form as the fallback if Visual Studio needs it?
5. May the cpp repository's tests use GRIDE3's link.exe and gride-dbg.exe when they are on the machine (a path
   in the harness, skipped when absent), or do those checks belong to GRIDE3's own tests?
6. The requests of section 10: are D1 to D9 (GRIDE's debugger) and L1, L2 (its link) taken up in GRIDE3 now,
   beside M10's stages, and L3 with the inlinee row?
7. Is the inlinee row (stepping into inlined code, its variables) wanted after M10, or is "inlined code has the
   call's line" the lasting answer?
8. How strong is goal 3? Recommended: cl's own guarantee, a place holds from the end of the prologue to the
   start of the epilogue, and the windows outside are a recorded limit, with D7 so that a breakpoint on a
   function does not land in one. The alternative closes the windows: every frame variable as a ranged record
   that leaves them out, for about 0.5 MB on 7.0 MB of objects (7 %) and the ranged form in every procedure.
9. cdb and WinDbg are not on this machine. Install the Debugging Tools for Windows (a download from Microsoft)
   so that stage 2 can run them, or word the goal as "debuggers that read through dbghelp", which cdb and
   WinDbg do and which gride-dbg tests? GRIDE's toolchain.json names cdb for its debugger role.
10. Procedure names in cl's spelling (`Arr<int,4>::sum`, not today's `Arr<int, 4>::sum`): for every object,
   with or without `-g`, in a commit of its own that changes nothing but `.debug$S` (recommended: one spelling,
   one formatter), or under `-g` only, so that an object without `-g` stays byte for byte what it is today?
11. Programs with two standard libraries (cl's STL in cl's objects, libcpp in cppparse's): a documented limit
   for debug information as it is for exceptions (recommended), or should cppparse's `std::` types be told
   apart, which would mean leaving cl's names?
12. A parameter a function never reads is not stored in the frame at `-O0` and so has no place there.
   Recommended: leave the code alone and list it as optimised out; stage 2 counts how many there are. The
   alternative is a change of `-O0` code for every object, with and without `-g` (one store per such
   parameter), so that `-O0` shows every parameter as cl /Od does.

## 9. Source layout

The CodeView code is in a folder of its own, cppparse/cv/, as the exception code is in cppparse/eh/ (the user's
direction of 2026-11-18 for new subsystems): classes that Coff.cpp, X86.cpp and IrGen call through small
interfaces, free of the front end's and the back end's types. Stage 0 moves what exists there without changing
a byte of output. The folder joins the source folder lists (toolset.ps1 `$SourceDirs`, build-cppparse.ps1, the
Visual Studio project and its filters, README).

| file | class | what | from |
|---|---|---|---|
| CvRecord.hpp/.cpp | `CvBytes` | the byte helpers: little-endian fields, numeric leaves, padding of symbol and type records, a relocation as {offset, symbol, kind} | Coff.cpp `put16/32` uses in the helpers, `pad4`, `symbolRecord` |
| CvSection.hpp/.cpp | `CvSection` | one `.debug$S`: the signature and its subsections with their relocations | Coff.cpp `debugSubsection`, `fillDebugSection` |
| CvFiles.hpp/.cpp | `CvFiles` | the module's file table: checksum entries and string table; from stage 5 an entry carries the SHA-256 the `CvModule` has for its file | Coff.cpp `DebugFiles` |
| CvSha256.hpp/.cpp | `CvSha256` | SHA-256 (FIPS 180-4): a message in pieces, its digest; the digest of a file on disk (`--sha256`). The driver hashes with it what the preprocessor read | new (stage 5) |
| CvLines.hpp/.cpp | `CvLines` | a function's `DEBUG_S_LINES` from its (offset, line) pairs | Coff.cpp `lineTable` |
| CvSymbols.hpp/.cpp | `CvSymbols` | the symbol records: `S_OBJNAME`, `S_COMPILE3`, a procedure with its `S_END`; from stage 1 the module's data, constants and UDTs; from stage 2 `S_FRAMEPROC`, the plain form (`S_REGREL32`, `S_BLOCK32`) and the ranged form (`S_LOCAL` with frame ranges, full-scope places and none); from stage 3 the register ranges | Coff.cpp `moduleSymbols`, `procedureSymbols`; the rest new |
| CvModule.hpp/.cpp | `CvModule`, `CvType`, `CvClass`, `CvVariable`, `CvTypedef` | the module's debug description (5.4): type descriptions by number, classes with members and bases, globals, constants, typedef names; its text dump. From stage 4 `CvMethod`: a member function as its class lists it (type, access, kind, vftable slot), and a function type's attributes and this adjustment. From stage 5 `CvSource`: a source file the unit read, with its checksum | new (stage 1) |
| CvTypes.hpp/.cpp | `CvTypes` | `.debug$T`: a description to its records, each record once, indices in order of first use, forward references; from stage 4 the member functions of a class (`LF_ONEMETHOD`, `LF_METHOD` with its `LF_METHODLIST`) | new (stage 1) |
| CvNames.hpp/.cpp | `CvNames`, `CvTypeText` | cl's display names (3.4): the spelling rules for a type inside a name, a template-id, a value argument, a scope, a conversion function; `CvFromSema` describes a name to them piece by piece. From stage 4 the type of a conversion function as its class's member list spells it (`operator const char *`, `operator struct ns::S`) | new (stage 1); X86.cpp `displayName` moved here as `fromIrName` |
| CvScopes.hpp/.cpp | `CvScopes` | a function's blocks: each one's pieces of code from the `;@scope` offsets, one piece or several, nesting, the variables of each, the procedure's form | new (stage 2) |
| CvRegisters.hpp/.cpp | `CvRegisters` | CodeView's register number for a register name and a value's size and kind: a table per register, volatile or not | new (stage 2: RBP; stage 3: the rest) |
| CvCheck.hpp/.cpp | `CvCheck` | the invariants of 5.5/4 over what the writer is about to write: (a) types, (b) symbols, (c) places against the instructions' write sets | new (stage 1: a; stage 2: b; stage 3: c; stage 4: in (a) a listed function's attributes, its type an `LF_MFUNCTION`, an overloaded name's list; stage 5: the file table's entries and the files the line tables name) |

What stays where it is: the object writer (Coff.cpp), which records the markers' offsets and each
instruction's written registers in its parallel encoding, and in its sequential loop hands a function's markers
to the builders and places the bytes and relocations they return; the frame and the allocator
(X86.cpp), which write the markers; `CvFromSema` in middle/, the one file that sees both the semantic types and
the `CvModule`; the unique-name mangling in backend/MangleSema.cpp.

## 10. Requests to GRIDE3

GRIDE's link needs nothing to take the objects of stages 0 to 5 (stage 5 measured it for the checksums: the
PDB of GRIDE's link gives dbghelp every file's SHA-256 as Microsoft's does): every record kind this design writes is one
pdb.cpp already carries (3.3; the review read it again: `type_refs` covers every leaf of 5.2, `S_BLOCK32`,
`S_LOCAL` and the `S_DEFRANGE_*` forms are carried with the scopes' parent and end filled in, the TPI hash
stream lets forward references resolve), and the prototype's PDBs from GRIDE's link read the same as
Microsoft's. No exit test of section 6 waits for a request either: the tests read through dbghelp with a
driver of their own. What the requests decide is what a user of GRIDE's debugger sees. Until D4 to D6 are in,
a program built with `-g -O1` or higher shows its register variables as `@reg` and 0 and its optimised-out
ones as `static ... <memory not readable>`; at `-O0` the same for a never-read parameter and, by `??`, for
the second of two same-named variables of sibling blocks in pieces. GRIDE3's sources were read, not changed.

GRIDE's link (toolchain\Windows\LINK\src\pdb.cpp):

- **L1.** A file-scope `S_CONSTANT` into the globals stream, as `S_UDT` goes there today: a debugger finds a
  constant by name only through the globals (`?? kLimit` is "not in scope" from GRIDE's PDB).
- **L2.** A warning when an object's types are left out (`merge_types`: an `LF_TYPESERVER2`, an `LF_PRECOMP`, a
  leaf kind `type_refs` does not know), naming the object and the kind. Today cl /Zi objects lose every type
  without a word; a new leaf from any compiler would too.
- **L3.** For the inlinee row, later: `S_INLINESITE` / `S_INLINESITE_END` kept as scopes with their inlinee id
  remapped into IPI, `S_INLINEES`, `S_CALLEES`, `S_CALLERS` carried, and the `DEBUG_S_INLINEELINES` (0xF6)
  subsection into the module's stream.
- **L4** (found in stage 1, 2026-11-20; nothing waits for it). An `S_UDT` whose name is the name of the class,
  struct, union or enumeration it points at need not go into the globals: Microsoft's link leaves it out, and
  DIA then lists no typedef for it (`DIA2Dump -t` on the types program: 22 such lines for cl's /Z7 objects from
  GRIDE's PDB against 1 from Microsoft's, 25 against 2 for cppparse's). dbghelp's view is the same from both.
- **L5** (found in stage 5, 2026-11-26; nothing waits for it). The source files of a program: dbghelp
  (`SymEnumSourceFiles`) lists one file more from GRIDE's PDB than from Microsoft's for test 22's program, 786
  against 785: the C runtime's `minkernel\crts\ucrt\src\appcrt\tran\copysign.c`, with its checksum. Every other
  file and every checksum is the same from both, cppparse's and cl's objects' alike; not looked into (an
  object of a library that Microsoft's link leaves out of the PDB when nothing of it is kept?).

GRIDE's debugger (toolchain\Windows\DBG\src\main.cpp), each measured on cl's own /Z7 objects, so none is a
matter of cppparse's records:

- **D1.** Bit-fields: the value by `TI_GET_BITPOSITION` and the member's length (today the storage unit:
  `{a=64785, b=64785, c=64785}` for `{1, 2, -3}`).
- **D2.** Members of a virtual base: the base's place through the vbptr (`TI_GET_VIRTUALBASEPOINTEROFFSET`,
  `TI_GET_VIRTUALBASEDISPINDEX`); today read at offset 0 (`both.v` is garbage).
- **D3.** Constants: a symbol with `SYMFLAG_VALUEPRESENT` shown by its value (`?? kLimit`: "memory not
  readable" from Microsoft's PDB).
- **D4.** CodeView's sized registers by name and value: `EAX` ... (17 to 24), `R8D` ... (360 to 367), the 8- and
  16-bit ones (`SIL`, `DIL` 324, 325), `XMMn_0` (a float) and `XMMnL` (a double); today `@reg` and 0. What
  makes register variables readable in GRIDE's debugger at `-O1` and up.
- **D5.** A variable without a place at the current address: "optimised away" or "not available here", where
  today it prints `static` at address 0 and `<memory not readable>`. And `?? name`, when several records of
  the scope have the name: the one that has a place at the address (probe r: `dv` shows the right `total`,
  `?? total` takes the first record and fails).
- **D6.** A reference or pointer held in a register: followed, as one in memory is (today the register's
  number: `@r8 struct geom::Point & p = 30458837424`).
- **D7.** A breakpoint on a function's own line: at the procedure's debug start (`SymTagFuncDebugStart`), where
  the frame exists and the parameters are in their slots; today the function's first byte, inside the first
  window of goal 3.
- **D8.** A pointer to a data member shown as one (`int geom::Point::*`), not as `int *`.
- **D9.** In a frame above the top one, a variable in a volatile register (rax, rcx, rdx, r8 to r11, xmm0 to
  xmm5) is "not available here": the unwound context restores nonvolatile registers only, and today the value
  is read from it all the same (main.cpp `valueOf`, the lookup at the return address less one). cppparse's own
  ranges end before the call (5.4), so this protects against other compilers' records, cl's /O2 included.

Not requested: an adjustment of `this` by `LF_MFUNCTION`'s field (the review's possible D10). cppparse's
functions hold the unadjusted `this` and write 0 there (5.2); whether dbghelp applies cl's nonzero field for
cl's objects is not measured and is not M10's matter.

## 11. Review and changes

The independent review of commits 60f813e and 5dfd7f8 (2026-11-19; its text and its probes p1 to p3 are scratch
files) agreed with the direction and with section 3's facts, and found two contract errors, five contracts
that contradicted goal 3 or the design's own checker, and eleven smaller points. Seventeen findings are taken
as they stand or sharpened; one (11) is taken with the opposite conclusion, on the evidence of IrGen's code.
While checking findings 1, 3 and 4 the author ran a further probe (r, appendix), which confirmed the forms the
review proposed and found one fact neither text had: dbghelp does not list an `S_REGREL32` that follows an
`S_LOCAL` in the same procedure. That fact turned finding 1's "an `S_LOCAL` for the parameter that cannot be
placed" into the rule of one form per procedure (4.2, 5.3).

| # | finding | decision | done |
|---|---|---|---|
| 1 blocker | the parameter rules contradict each other and the code: a reassigned parameter had no record while `CvCheck` and DIA count parameters; a never-read parameter is not stored at `-O0`; unnamed and variadic parameters, `this` | accepted, and extended by probe r | every declared parameter has a record in order, `this` first, `__formal` for an unnamed one, none for `...`; a variable without a place is an `S_LOCAL` with flag 0x100 and no range, locals too (5.3); because of dbghelp's rule a procedure is plain or ranged as a whole (4.2, 5.3); goal 2 and the limits say what `-O0` does not show; `CvCheck` (b) restated; 3.6 corrected; DIA measured for a member and a variadic function at the head of stage 2; question 12 |
| 2 blocker | a range in a volatile register that covers a call gives every frame above the top one the callee's register | accepted | 5.4: such a range closes at the first byte of every call and clobber, by `m_clobbers`; it closes at the last use when that instruction writes the register; `CvCheck` (c) checks both statically for every function; the harness also compares in the frames above the caller and at the next line of the top frame; the `-O1` minimum is "live across the call"; risk 1; request D9 |
| 3 major | invariant 1's byte comparison cannot pass: added sections and symbols renumber the others, and with them the code's relocations | accepted, the review's (a) | 5.1: added sections behind every existing one, added symbols behind every existing one, debug relocations only to symbols the object has without `-g` (which also covers the review's undefined-external case); 5.5/1 says what cvsame compares; cvsame is the first thing stage 1 builds. Evidence that both linkers take the layout: the prototype's `.debug$T` and probe r's associative `.debug$S`, both behind `.drectve` |
| 4 major | block scopes from line hulls overlap whenever handlers and pads are out of line, which is always; the AST has no end lines | accepted | scopes are a number every instruction carries beside its line (5.4), `;@scope` markers, exact pieces in the object writer; one piece: `S_BLOCK32`; several: the variables in the enclosing block with ranges and gaps (5.3; probe r shows dbghelp reads gaps, several ranges, and two records of one name by range); the AST is not changed (5.6); the old risk 7 is gone, the new one is fragmentation at `-O1` and up; D5 extended; a limit in section 2 |
| 5 major | "in memory" at `-O1` and up would take in slots whose stores the optimiser removed | accepted | 5.3: a slot is a place only for address-taken variables and aggregates, or for an unpromoted scalar no store of which was removed (`IrLocal::staleStores`, set where a store is removed, 5.4); otherwise optimised out; the p3 shape is in vars.cpp; risk 2; the 88 % of 3.6 is called an upper bound |
| 6 major | goal 3 promised more than `S_REGREL32` can keep: prologue, epilogue, funclet entry; DIA2Dump's silence was weak evidence against `S_LOCAL` | accepted, the review's first way | goal 3 restated with cl's reach, the windows a limit (section 2); 4.2 rewritten, the DIA2Dump argument withdrawn; `CvCheck` (c) keeps offset-ranged frame records out of the windows; the harness cannot test them, said in section 2; the stronger alternative is question 8 and risk 10 |
| 7 major | a named result built in the caller's object vanishes at `-O1` and up; the inliner would copy the new flags | accepted | 5.3: the result is a reference at the hidden result pointer, its type taken before IrGen rewrites it (`IrFunction::debugAliases`, 5.4); the inliner clears `source`, `block`, `debugType`, drops the callee's tables and gives copied instructions the call's scope (5.4, 5.6); a named result is a shape of vars.cpp |
| 8 minor | the register table read as consecutive by encoding; the 64-bit numbers and sil/dil are not | accepted | 5.3: the table written out per register; `CvRegisters` is a table, no arithmetic (section 9); `CvCheck` (b) rejects 5 to 8; a `bool` in sil is a shape of vars.cpp; D4 names SIL and DIL |
| 9 minor | the encoder runs in parallel; where are type indices assigned | accepted | 3.6 corrected; 5.2 and 5.4: only the sequential placement loop calls cv/, the encoder records offsets and write sets; invariant 2 names the loop |
| 10 minor | unique names are needed for correctness (two units' unnamed namespaces), not only for mixed links | accepted | 5.2: mandatory on every class and enum record, with the reason; 3.2's "not needed" qualified; types2.cpp (section 6); `CvCheck` (a); the `S_UDT` of such a class in cl's `?A0x<id>` spelling, so GRIDE's one-per-name rule does not merge two |
| 11 minor | name `thisAdjust` as the source of `LF_MFUNCTION`'s adjustment; test an override from a second polymorphic base; perhaps a GRIDE request | adapted: the test yes, the field 0 | IrGen.cpp 5363 to 5367: a function entered with an adjusted `this` stores the object's address into the `this` slot before its first statement, so the frame's `this` needs no adjustment and a nonzero field would adjust twice (5.2). What the finding did uncover: until that store `this` is wrong, so the store belongs to the entry sequence and `DbgStart` lies behind it, and at `-O1` and up the adjusted value is the definition that counts (5.3, 5.4 `debugEntry`); the override and a destructor are shapes of vars.cpp; no D10 |
| 12 minor | cl's spelling of procedure names: with or without `-g` | accepted, put to the user | question 10, recommended for every object in a commit of its own in stage 1, proven by cvsame to change `.debug$S` only; 5.1 and the summary point at the question |
| 13 minor | cl sets "optimised debug information" (0x80) on /O2 procedures and flags in `S_FRAMEPROC` | accepted | 5.3: 0x80 on every procedure in the ranged form; cl's `S_FRAMEPROC` flags measured at the head of stage 2 and those that apply copied; the ranged form in `-O0` code is in the Visual Studio session (risk 5) |
| 14 minor | tests: link variants, a mixed /Z7 link, a proof of the DIA reader for `S_LOCAL`, the self-host's PDB, the normalisation of the type-tree comparison, a checksum test that can fail; optional: stage 4 before 3 | accepted; the order kept | section 6: the link variants from stage 2; the mixed-types program and the `-g` self-host's link in stage 1; the DIA reader proven on cl's /O2 object as stage 2's first step; the normalisation stated; stage 5 tests the checksum's value and that an edit changes it. The order of stages 3 and 4 stands, with the reasons and leave to swap, under the table |
| 15 note | the estimate: M7's 29 to 40 days landed in an afternoon; several items are under-weighted | accepted | section 7: the unit explained from M7's log, weights and lanes given apart; stage 1 +1 lane, stage 2 +1, stage 3 +1 (the review asked for one and a half in all; the two procedure forms and the DIA proof are stage 2's extra): eleven to thirteen lanes, weights 35 to 52 |
| 16 note | section 10's opening and D4 disagreed; possible D9, D10; nothing in GRIDE's link blocks the stages | accepted but for D10 | section 10: the opening says what waits for nothing and what a GRIDE user sees meanwhile; D4 reworded; D9 added; D5 and D7 extended; D10 not requested (finding 11) |
| 17 note | four questions to add | accepted | questions 8 (goal 3's strength), 9 (cdb), 10 (spelling), 11 (two standard libraries), and 12 from finding 1; question 6 now D1 to D9; the two-libraries limit in section 2 |
| 18 note | smaller text points | accepted, all six | 4.1 (a): types of a dropped COMDAT stay; 5.2: `LF_UNION` has no shape field, indices strictly below the record that holds them, the 0xFF00 limit, typedefs through `S_UDT` and `LF_NESTTYPE`, `CvFromSema` reads only |

What did not change: the recommendation of section 4 in every option (types in the object, the two-level
treatment of variables, the side `CvModule` with markers, `-g` off by default, cppparse/cv/), the type records
of 5.2 but for the union's layout and the unique names, the stages' number and order.

Notes from the development, where the code and this text differ:

- 2026-11-20, stage 0. (1) Section 9 gives `CvBytes` numeric leaves and the padding of type records. Nothing
  writes either yet: stage 0's `CvBytes` has what Coff.cpp's helpers had (little-endian fields, a name with its
  0, padding to four bytes, a symbol record, the two relocations of an address) and gets the rest in stage 1
  with the first record that needs it. (2) Section 9 names `fillDebugSection` among what becomes `CvSection`.
  Its content moved (`CvSection::write`: the signature, the subsections, their relocations at the section's
  offsets); a function of that name stays in Coff.cpp and copies a `CvSection`'s bytes and `CvReloc`s into the
  writer's own section and relocation types (SECREL, SECTION), which cv/ does not know. The section's name and
  characteristics stay in Coff.cpp with those of the other sections, as .xdata's did in M7's stage 0. (3) A
  `CvSection` keeps its line tables as one run of finished subsections, not as a list: the bytes are the same
  and the module's section is put together once, when every function is placed. (4) The exit test names "the
  comparison script of M7's stage 0": that was a scratch file and is gone. The comparison was made with scratch
  scripts of this lane (two runs with SHA-256 manifests, the status has the numbers); they are not in the
  repository, and stage 1's cvsame is the comparator that stays. (5) cppparse's own map of an object carries a
  time stamp line, so maps are compared without it. (6) The folder lists are four: `$SourceDirs` in
  sandbox\build-cppparse.ps1 and in scripts\toolset.ps1 (selfhost.ps1, strict98.ps1, mix.ps1 and the map scripts
  take the latter), cppparse.vcxproj with its filters, and README's build lines.
- 2026-11-20, stage 1, the comparator. (1) 5.5/1 says the first M symbol table entries are the same bytes. One
  field cannot be: a section's own symbol has an auxiliary record with the section's length and its number of
  relocations, and for a `.debug$S` those are debug information (`-g` adds records to the module's section and
  to a COMDAT function's). cvsame compares such a record without its first six bytes (length, relocation
  count) and every other entry whole. For the same reason a `.debug$S` section header's flags are compared
  without IMAGE_SCN_LNK_NRELOC_OVFL, which says the section has more than 65,534 relocations. (2) "The listings
  are equal once the `;@` lines are taken out" is checked more narrowly: only the marker kinds `-g` writes (5.4's
  list but `;@debug`) are taken out of the second listing, and what is left must be the first listing line for
  line, the markers every object has (`;@line`, `;@comdat`, `;@ehrange` ...) included. (3) The design's
  prototype (cvpatch.py) is no positive case for the comparator: the script wrote every section's relocation
  entries sorted, so its object differs from cppparse's in the relocation entries of `.text`. That was a property
  of the script, not of the layout; cvsame's own positive cases are made by adding a section header and moving
  the file pointers.
- 2026-11-20, stage 1, the names. (1) 5.4 lists what `IrFunction` gets; the name is one more field,
  `debugName`: X86.cpp has the IR name only, and cl's spelling needs the symbol (the instantiation's arguments,
  the conversion's type), which IrGen has. (2) Section 9 has `CvNames` format "a neutral description of a name's
  parts": it is a set of spelling rules that take text and give text (a type is built in two halves around
  its declarator, `CvTypeText`), and `CvFromSema` walks the symbols and calls them; cv/ sees no front-end
  type. (3) Where cppparse's name still differs from cl's, each because the decorated name differs first, so
  the two are not one function to a linker either (found by the comparison, none is this row's): a class in a
  nested block of a function is numbered 2 plus the block's position (`` `local'::`5'::L3 ``; cl counts
  its own way, 6 there), as MangleSema numbers it; an unsigned value argument of a parameter whose type is a
  template parameter (`template <class T, T v>` with `V<unsigned, 4000000000u>`) is decorated and shown as
  the signed value (`-294967296`; cl `$0OOGLCIAA@`, 4000000000); a template template argument is decorated
  with the class key `V` where cl has the template's own (`U` for a struct). And one that stops a unit: an
  array type as a template argument is decorated as a const pointer, so `W<int[4]>` and `W<int* const>` in
  one unit are one symbol twice ("duplicate symbol", the object is not written). (4) Not measured, written
  by the general rule: a pointer to a pointer to a function, a function that returns a pointer to a function,
  an array of unknown bound. cl's names for what cppparse does not emit as procedures (`` `scalar deleting
  destructor' ``, the `dtor$N` funclets of a function) have no counterpart; the module's one initializer
  function keeps its own name (`__cppparse_init_<unit>`), where cl has a function for each variable.
- 2026-11-20, stage 1, `-g`. What 5.2 left to measure, on cl 19.52 (/Z7 /Od, read with llvm-pdbutil and a
  script): (a) an anonymous union or struct in a class has no record: its members are `LF_MEMBER`s of the class
  at the two offsets together (`i` and `f` at 4 behind an `int`); a member of an unnamed class type that has a
  declarator is a class `Anon::<unnamed-type-named>` with an `LF_NESTTYPE`; an unnamed struct or enum with a
  variable is `<unnamed-type-g_unnamed>`, one named by a typedef has the typedef's name; a namespace-scope
  anonymous union is `<unnamed-type-$S1>`. (b) An unnamed bit-field has no member; the positions of the others
  account for it. (c) A class without members has an `LF_FIELDLIST` of no members (a record of four bytes) and
  size 1. (d) A union's definition has the property "sealed" (400h). (e) An enumeration has no forward
  reference. (f) A class local to a function is `use::__l2::Local`, scoped (100h), and its unique name is the
  decorated one followed by a backquote and the unit's id in hexadecimal (`` .?AULocalS@?1??use@@YAHXZ@`3b8b1910 ``).
  (g) The cut of a long name: a name of more than 4,096 characters is its first 4,064 and the 32 hexadecimal
  digits of the MD5 of the whole name (verified by computing it: names of 8,011 and 16,036 characters; 4,007
  is whole); a unique name of more than 4,096 is `.??@<MD5>@`, the MD5 of the decorated name without its dot
  (8,034 characters; 4,040 is whole). The exact boundary between 4,040 and 8,011 is not measured, 4,096 is
  taken. All of these are written as measured, but: cppparse numbers its unnamed classes in the unit
  (`<unnamed-type-$S1>`...) and puts the unit's id behind their unique name, where cl names one after its
  declarator (Sema does not keep the declarator); and Sema's layout of an anonymous member inside another
  anonymous member is not followed further than one level.
  Where the code differs from 5.x and section 9: (1) `IrGlobal` gets no fields (5.4): IrGen lists a variable
  in the `CvModule` itself, with the symbol `X86Module::globalSymbol` gives it, where it registers the global;
  the object writer writes the record only when the object defines that symbol, which it knows. A static
  local travels as `IrFunction::debugStatics` and a `;@static` marker, block 0 until stage 2. (2) `;@proc`
  carries the frame's size and the pushed registers' bytes already; nothing reads the two numbers before
  stage 2, which measures cl's `S_FRAMEPROC` first and may redefine them. (3) "A class's definition is written
  when a symbol or a record needs the class complete, and for every class a pointer leads to": in the code
  every class that any written record names and that the unit has complete gets its definition
  (`CvTypes::finish`), which is the same set. (4) Typedefs: Sema's types are canonical, so no symbol knows the
  typedef it was declared with. An `S_UDT` is written for every typedef name of namespace scope, and an
  `LF_NESTTYPE` for every member typedef, whose type needs no class described that is not described anyway (a
  typedef alone brings no class into the object); cl writes the ones the unit uses. (5) `S_CONSTANT` is
  written where the object has no storage for the constant; cppparse gives a namespace-scope `const int`
  storage whenever it is defined, so such a constant is an `S_LDATA32` today and a debugger reads it from
  memory (`?? kLimit` is 40 from both linkers' PDBs, where the design's prototype had the two differ). (6) A
  vftable has no `S_GDATA32` yet (5.1 lists it among the COMDAT variables): no reader asked for it; a static
  member of a class template's instantiation has its associative `.debug$S`, a static local of an inline
  function its `S_LDATA32` in the COMDAT function's own section. (7) `CvCheck` (a) has two entry points, the
  bytes of `.debug$T` and the symbol records of each `.debug$S` (their type indices against the number of type
  records); "no `T<n>` unresolved" and the relocation rule of (b) are checked by the object writer where it
  resolves them, as internal errors of the same kind. (8) The suite's test for this stage is test 20 (5.6 says
  19, which the self-compile took meanwhile); cvsame over the suite's objects is not a harness check of its
  own: test 20 runs it on the types program and on the own sources, and `CPPPARSE_G=1` lets any script's
  objects be had both ways. (9) `TypeTable::sizeOf` notes on a class that the size of a pointer to its member
  was asked while it was incomplete; `CvFromSema` spells that size out itself, so that it only reads. (10) The
  design's type-tree comparison names a normalisation (member functions, `?A0x<id>`, `LF_VTSHAPE` by slot
  count): the test compares through dbghelp, where the driver lists data members, bases, enumerators, static
  members, nested types and the slot count, so nothing is left to normalise; the records themselves differ
  from cl's in what the design leaves out (member functions, ids, source lines). (11) The IR dump does not
  print the new fields (5.6): the listing's markers and `<name>-cv6.txt` show them. (12) Through DIA (the SDK's
  DIA2Dump on the types program's PDBs): the globals of cppparse's build are the same lines from both linkers'
  PDBs; against cl's build cppparse has one more (it defines `Box<geom::Point>::shared`, which cl does not
  instantiate) and lacks cl's vftables and the enumerators cl lists as constants; the types are the same but
  for that static member and one typedef name. GRIDE's PDB lists a class's `S_UDT` as a typedef there, for
  cl's objects as for cppparse's, where Microsoft's link leaves such a record out (section 10, L4). (13) cvsame
  now takes `-g`'s markers out of both listings (two objects that both have `-g` compare too) and compares no
  listings where neither object has one beside it (an object assembled from a listing of another name).
  Found in the front end on the way, not this row's: a using-directive in a function body (`using namespace
  geom;` in `main`) is not parsed; a function, a variable's address or a reference as a template argument
  (`Fn<inc>`, `Pp<&g>`, `Rr<g>`) is refused (S111).
- 2026-11-24, stage 2. Measured first (cl 19.52, probe fp.cpp of the lane, /Od and /O2 /Ob0, llvm-pdbutil; the
  readers on the linked programs): (a) cl's `S_FRAMEPROC` has "optimised for speed" (bit 20) on every procedure,
  /Od included; "marked inline" (bit 5) on a function declared inline or defined in its class; "has EH" (bit 4)
  where the function has an exception state table (not where an object's lifetime holds no call that may throw);
  "has async EH" (bit 9) on its funclets alone; "security checks" (bit 8) with a /GS buffer; the base registers RSP
  (RBP in funclets); its size is the frame below the saved registers. cppparse writes RBP for both bases, bit 20
  always, bit 5 for a function declared inline, bit 4 for a function with exception regions; `;@proc` carries
  `<frame> <saved> <flags>`: the bytes rsp is lowered by below the pushed registers, the bytes of the pushed
  registers with rbp, and these flags (stage 1's two numbers are redefined so). (b) The DIA reader
  (examples\sample-dbg-driver\diavars.cpp, `findChildrenExByRVA`, msdia140 by `NoRegCoCreate`) reads cl's /O2
  object right: at each address the register cl's `S_DEFRANGE_REGISTER` ranges give, and "none" outside them. DIA
  returns every `S_BLOCK32` of a scope whatever the address: the reader keeps the one that holds it. (c) DIA by
  position on `S_REGREL32`: `Cls::member(int a, int)` lists `this`, `a`, `__formal` as parameters, `variadic(int
  count, ...)` lists `count` alone (the `...` is not counted), as 5.3 has it. (d) dbghelp lists `this` without
  `SYMFLAG_PARAMETER`, for cl's objects as for cppparse's (DIA: the object pointer); the value harness counts it
  as the first parameter.
  Where the code differs from 5.3, 5.4 and section 9: (1) The ranged form has no `S_BLOCK32`. dbghelp does not look
  into a block of a procedure that has an `S_LOCAL` (the first build had `blocks` of vars.cpp in that form: DIA
  listed the variables of its loop's blocks, dbghelp none of them; probe r had no block in a ranged procedure).
  So in the ranged form every variable stands in the procedure, as in cl's /O2 procedures, which have no blocks
  either: a variable of the function's own block by `S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE`, one of an inner block
  by `S_DEFRANGE_REGISTER_REL` over its block's pieces of code (one range or several, the holes as gaps), whether
  the block is one piece or not; the static locals in the procedure. The plain form keeps `S_BLOCK32`. (2) The entry
  sequence is not `IrFunction::debugEntry` but a scope: IrGen gives the instructions it makes before the first
  statement (the entry block's, the store that puts `this` right, the copy of an object passed in a register) the
  scope -1, and X86.cpp writes `;@prologend` before the first instruction of another scope; prologues, epilogues
  and a funclet's entry are scope -1 too (`;@scope -1`), so no block's code holds them by construction. (3)
  `DbgEnd` is the function's own epilogue, the first `;@epilog`; a funclet's exit is an `;@epilog` for `CvCheck`
  alone (the funclets are code of the procedure and lie behind it). (4) A block is numbered when IrGen meets its
  first variable or static local (Sema's scope of the declaration; the function's body and its parameters are
  block 0), its parent the block the code is in then; so the code of a block before its first declaration is its
  parent's. The fields: `IrLocal::source`, `debugName` (`__formal`), `debugType`, `debugParam` (the place among the
  declared parameters, `this` 0: an object passed in a register is the parameter, though its local is made later),
  `block`, `byAddress`; `IrInst::scope`; `IrFunction::debugBlocks`, `debugInline`, `debugOptimized` (set where the
  optimiser ran); `IrDebugStatic::block`. `staleStores`, `debugTemps` and `debugAliases` are stage 3's. (5) A
  cleanup's code carries the block of the scope that runs it (the destructor calls at a block's end its block); the
  landing pads of the table model are made at the end of the function and carry block 0: there a block's variables
  are absent, not wrong. (6) A block without code (its statements generated nothing) is not described, nor are its
  variables. (7) At -O0 a function without a frame (nothing in memory, no call) lists its variables without a place,
  as does a parameter the function never reads; at `-O1` and up, until stage 3, every parameter and local is listed
  without a place (the ranged form). (8) The vftable's `S_GDATA32` (stage 1's note 6): ``Der::`vftable'``, as cl
  names every table of a class, of type an array of const pointers to `void ()` with the table's slots (cl writes
  one type of 16 bytes for every table); each in an associative `.debug$S`. (9) `CvCheck` (b) runs on every
  procedure of every `.debug$S` with the object's `.debug$T` (the parameter count of `LF_PROCEDURE` /
  `LF_MFUNCTION`, `this` counted and `...` not); the frame part of (c) checks the `S_DEFRANGE_REGISTER_REL` ranges
  against what the encoder knows: the prologue up to `.endprolog`, each funclet's up to its own, and each epilogue
  from its `;@epilog` to the end of the `ret` behind it (the encoder records the ends of `ret`s as of calls). (10) The
  value harness is test 21 (5.6's test 19); the self-host with `-g` is the harness's check `selfhost-g-O0`.
- 2026-11-25, stage 3, the optimiser's tables. Where the code differs from 5.3, 5.4 and risk 2: (1) 5.4 has Ssa.cpp
  record the pair at the promotion. Most single-assignment locals never reach it: the passes before SSA forward the
  stored value to every read in the block, the local is then never loaded, and dead store elimination removes its
  store (in vars.cpp at `-O2`: `sum`, `total`, `r`, `w`, `mine`, `got`, `half`, `twice`, `flag`, all but the
  parameters). So the pair is also recorded there, in `deadStores`, when the store that goes is the variable's only
  one and none was removed before; the local is then marked `staleStores` and has its entry. (2) "Passes that replace
  temporary a by b rename the entry" is right only while b is not computed again: b has a's value where a was
  defined, but b is defined elsewhere, and in a loop the variable keeps what it was assigned while b is computed
  anew (`for (...) { a = f(i); if (i == 3) b = a; }`: `b`'s entry would be `a`'s temporary, which changes in every
  round). An entry therefore says whether it is exact, that is, whether its temporary is defined where the variable
  is assigned: the instruction before the store, a constant materialised at the store, a parameter's load at the
  function's start. An exact entry holds wherever its temporary is live. An entry that a pass renamed, or that was
  recorded from a store of another variable's value (`int b = a;`), is not exact, and the verifier keeps it only if
  its temporary's definition lies on no cycle of the function (it runs once, so the variable, once assigned, is
  equal to it; before the assignment the variable has no value to be wrong about). (3) The rename is in one place,
  `replaceTempUses` (Optimizer.cpp), which copy propagation and both store-to-load forwardings go through (and, for
  the entries recorded before it, in Ssa.cpp where the loads of a promoted local become that local's value); value
  numbering, local CSE, phi simplification and the algebraic rewriting turn an instruction into a copy of the same
  result, which copy propagation then removes, so they keep the table without code of their own; a temporary that
  dead code elimination removes and a phi's result that SSA destruction defines by copies end at the verifier
  (not defined exactly once). X86.cpp's own forwarding at lowering (`constantsIntoOperands`) is not taught the
  table: a temporary it drops has no interval and so no place. (4) An adjusted `this` is not `IrFunction::debugEntry`
  (stage 2's note 2 already dropped that field) but a rule: a parameter's one store counts as its one definition when
  it is of scope -1, the entry sequence, which runs once before the first statement. (5) `IrLocal::promoted` is new:
  X86.cpp cannot tell a promoted parameter from one that stays in its slot by the code alone (both are loaded at
  the entry). (6) The tables are filled under `-g` only (`IrFunction::debugType` not 0); invariant 1's comparison
  is what shows that no code depends on them. The verifier takes the successors from the instructions and leaves
  the function's own lists alone. (7) The inliner's duties were done in stage 2 but for the two new marks, which it
  clears with the others; a callee's `debugTemps` and `debugAliases` are simply not copied. (8) The grep risk 2 asks
  for, over cppparse/middle, backend and eh: a `StoreLocal` leaves the code in `IrFunction::removeStore` (called by
  `deadStores` twice and by `removeDeadRegions` for the flags of an unwind region that is gone), in Ssa.cpp's
  promotion (every store of a promoted local; the local is marked `promoted`), and with a block nothing reaches
  (`eraseBlocks`: a store that never ran); nowhere else. X86.cpp's lowering forwards stored values and keeps the
  stores. (9) A named result's alias is recorded by IrGen where it rewrites the variable's type (`localDecl`), with
  the block Sema gives the declaration.
- 2026-11-25, stage 3, the places. Where the code differs from 5.3, 5.4, 5.5 and section 9: (1) `;@loc <n> <register>
  <bytes>` names the register by its 64-bit (or xmm) name and the bytes of the value; `CvRegisters` gives the number of
  that part. (2) 5.4 expects a temporary that is "coalesced into another register for part of its life". There is
  none: the allocator gives an interval one register or one spill slot, and the steps after it (a parameter kept in
  its argument register, a result in rax, an argument computed in its register) settle the whole interval. So a
  variable has one place, and the markers say only where it is valid. (3) Where a range ends: 5.4 has it end behind
  the last use, or at that instruction when it also writes the register. X86.cpp does not ask which: a register's
  range ends before the last instruction of a block that reads the temporary (that instruction's own result may take
  the register over), with one exception, a callee-saved register whose last reader is a call: the range ends
  behind the `call` instruction itself and before the result is moved in, so that the frame is seen with the
  variable from inside the call. A spill slot's range ends behind the last reader (nothing but the temporary's own
  definition writes the slot). (4) A range is opened at a block's start by liveness and closed at the last reader,
  per block; everything is closed at an epilogue, at a funclet's entry and before the dispatch stubs, whose code is
  no instruction of the IR. A range that starts in the entry sequence is cut to `DbgStart`. (5) The volatile
  registers a variable can be in are rax, rcx, rdx, r8 to r11, xmm2 and xmm3; their ranges end before a call, a
  throw, and the catch and try operations (`m_clobbers`), and those in rax, rcx and rdx also before an intrinsic and
  a thread-local's address, whose sequences use the three beside their operands. (6) A variable takes its
  temporary's place only if the temporary's type has the variable's size and kind (integer or pointer of 1, 2, 4 or
  8 bytes; float; double); a temporary of equal value and another width is no place. (7) The encoder has no table of
  mnemonics but a chain of tests (and one table, the SSE instructions'); `writtenRegisters` stands before
  `Encoder::instruction` and names each mnemonic that function encodes. It is recorded for a PROC that has a `;@var`
  only. `CvCheck` (c) also holds the register ranges against the prologue and epilogue windows, which 5.5 asks of
  the frame ranges alone. (8) `CvCheck` (b)'s "fits the type's kind and size" reads the type from `.debug$T`: a
  fundamental type, a pointer or reference, an enumeration, a modifier of one; a class or an array in a register
  would not be held against anything, and none is written. (9) At every level a parameter that the function reads
  from its argument register alone (X86.cpp `m_paramInReg`) is not stored in the frame and so has no frame place;
  stage 2 asked only "never read". At `-O0` no function has such a parameter (the entry block holds no load), and
  the `-O0` objects are the bytes they were. (10) X86.cpp keeps the liveness sets it computes for the intervals
  (`m_liveIn`, `m_liveOut`); the allocator reads them as before. (11) 5.4 opens a parameter's range "where the
  function's code begins, in its incoming register or its slot". It opens behind the load that is the parameter's
  definition, the first instructions of the body; before it (the prologue, which lies before `DbgStart` anyway) the
  incoming register is not described. A parameter that is consumed by the instruction right behind its load has
  an empty range and is listed as optimised out (`b` in `int f(int a, int b) { return a * b + a; }`).
- 2026-11-25, stage 3, the exit tests. (1) Section 6's minimum for `-O1` and up ("the parameters that are not
  assigned, the variables whose address is taken, the single-definition locals that live across the call") cannot be
  asked of a parameter as such: a parameter whose last use lies before the checkpoint is dead there, its register
  may hold something else, and its range has ended (in vars.cpp nearly every parameter). The minimum is therefore
  written per checkpoint, in the program's text: a `+` before a variable that lives in memory or has one definition
  and is needed behind the checkpoint; `dbgvars -optimized` fails when such a variable has no place, and for any
  other accepts a missing place and no wrong value. `this` is often not among them either: a member's address
  computed before the checkpoint is what lives on, not `this` (`Counter::add`); `Both::get` reads a member behind
  the checkpoint so that its adjusted `this` is checked. (2) The harness's functions are kept out of line by the
  `_AddressOfReturnAddress` intrinsic; without it `-O3` inlines `many` and `refs` into `main` and the caller of
  their checkpoints is `main`, whose listing has none of their variables (section 2's limit). The intrinsic costs
  each function one instruction and a register for its unused result. (3) Risk 7's count came out high and its
  remedy was taken: the three instructions SSA destruction makes without a source (the copies on an edge, the
  jump of a split edge, a phi) take a scope from their neighbour, as the risk says; `line` is left alone (the line
  table of an object without `-g` stays what it is). (4) "The suite under a compiler built with `-g -O2`" is the
  suite run with the compiler `selfhost.ps1 -G -Lvl 2` leaves (out\selfhost-g); it is not a check of the harness,
  since by invariant 1 that compiler's code is the bytes of the `-O2` self-host's, which `suite-self` runs. (5) The
  share of variables with a place is counted on the listings' markers (`;@local`, `;@var` with a `;@loc` that names
  a place, `;@gone`), by a script of the lane; a variable counts as placed when it has a place over any of the
  code. (6) The harness has 24 checks with `selfhost-g-O2`.
- 2026-11-25, stage 3, the review of its code. An independent read of the three parts' code (its one question: where
  can a place be wrong) found three ways, each closed in the third part, and two smaller points. (1) A variable with
  one store left after an earlier store was removed (`x = 5; ...; x = v;`, the first overwritten in its block) had
  an entry at Ssa.cpp's promotion; between the two stores the variable is 5 while `v`'s temporary, defined earlier
  (or an exact one moved there by loop-invariant motion), is live. Ssa.cpp now asks what `deadStores` asked already:
  no entry for a local with `staleStores`. The first note's "before the assignment the variable has no value to be
  wrong about" holds for such a local only. (2) The verifier's cycles were those of the branches and of each
  `trybegin`'s own block. A handler that resumes closes a cycle from every block of its region (a loop that tries
  again): the variable keeps the round before's value while the value it will be assigned is computed anew. The
  verifier now gives every block every handler as a successor; `retry` in vars.cpp is the shape, and with the fault
  put back the harness says `x = 21, the program says 11`. (3) In cl's model a value of the function that a catch
  funclet does not touch stays in its callee-saved register, which is right for the code (the runtime restores it at
  the continuation) and wrong as a place while the funclet runs: the register is the runtime's then. In a funclet's
  blocks a register is now a place only for what the funclet computes itself (it opens behind its definition); a
  frame slot stays one. Not reproduced by a test: it needs the runtime to use that register before it calls the
  funclet. (4) The entry `deadStores` records for an adjusted `this` was replaced at the promotion by the incoming
  value's (a lost place, not a wrong one); it stays now. (5) The loads of the parameters and the constants for
  uninitialised reads that Ssa.cpp puts at the function's start carried scope 0, so `;@prologend` came before the
  entry sequence in a promoted function; they carry scope -1, and `DbgStart` is behind the entry sequence again
  (5.3). Sound by the same read: the renames, the inliner and unrolling, `removeStore` as the only way out for a
  store, one slot per spilled temporary, the takeover of a dying operand's register at the last reader, folded
  addresses and fused compares (no interval, no place), the closing at epilogues, funclet entries and stubs, the
  ranges' arithmetic in `CvScopes`, the registers' numbers, and `writtenRegisters` (no mnemonic understated).
- 2026-11-25, stage 4. Measured first, cl 19.52 (/Z7 /Od; the lane's probes mf.cpp, p2.cpp, p3.cpp and p4.cpp, 60
  classes; llvm-pdbutil, llvm-readobj; DIA and dbghelp on the linked programs): (a) A class's field list holds its
  members in the order of their declarations, the functions among the data members: `LF_ONEMETHOD` {attributes,
  type, the slot's offset in the vftable where the function introduces one, name}; an overloaded name is one
  `LF_METHOD` {count, list, name} where the name is first declared, with an `LF_METHODLIST` of {attributes, 0, type,
  the slot's offset} in which the last declared comes first. The class's member count has every function of such a
  name (9 for one data member and eight functions of five names). (b) The attributes are the access and the kind:
  none, 1 virtual (an override), 2 static, 4 introducing virtual, 5 pure virtual, 6 pure and introducing; a
  covariant override, which cl gives a slot of its own, is 1. The slot's offset is eight times its place in the
  vftable that holds it (`D::own` 32 behind four inherited slots; two overloads the last declared first, as Sema
  places them). (c) `LF_MFUNCTION`: the type of `this` is a plain pointer to the class, to the const class in a const
  member function, the very type the function's own `this` record has; cl 19.52 writes no const pointer there, which
  stage 1 had taken from older objects (its own runtime's types still have it). A static member function is an
  `LF_MFUNCTION` without a `this` type. The function attributes: 2 on a constructor, 6 on a constructor of a class
  with virtual bases, 1 ("returns cxx udt") where a class result comes back the C++ way: every class result of a
  non-static member function, and from any other function (a pointer to function's type too) a class that is no
  plain aggregate, which is the code generator's rule for the hidden result pointer without the size (27 result
  types: a plain struct of 16 bytes, one with an array, a static member or a private member function, a union and
  an empty struct are not marked; one with a base, a constructor, a destructor, a copy assignment, a non-public
  data member, a member that has a destructor, a const or a reference member is, and so is a class the unit never
  defines; one defined behind the function's declaration counts by its definition). (d) The this adjustment is not 0
  for an override that a later base's table holds first: `D::h` 16, through a virtual base `L::vf` 24 and `M::vf`
  56 (the base's place in the class's own layout), 0 for a destructor: the numbers Sema keeps as
  `Symbol::thisAdjust`. (e) Names: a constructor under the class's own name with its arguments (`Tm<int>`),
  `~Tm<int>`, operators as in the procedures' names. A conversion function's type is spelled a third way there, as
  a declaration reads: `operator const char *`, `operator struct p3::S`, `operator const class p3::C &`, `operator
  union p3::Un`, `operator enum p3::E`, `operator char **`, `operator const char *const *`, `operator volatile const
  struct p3::S *`, `operator unsigned __int64`; and, measured and not written (5): `operator struct
  p3::Box<struct p3::Box<int> > *`, `operator struct p3::Box<char const *>`, `operator int (__cdecl p3::S::* )(int)
  const`, `operator int (*)[3]`, `operator void (__cdecl *)(void)`. (f) cl lists no member template, none of its
  instances and no friend. It lists, marked compiler-generated (100h), what it generates: the implicit
  constructors and `operator=` of a class that needs them, `__vecDelDtor`, `__local_vftable_ctor_closure`, and for
  a covariant override the return-adjusting thunk as a second function of the name. A class without a name
  (`struct { int z; int zf(); } g;`) lists no function. An `operator=` that is no copy
  assignment is not listed where the class declares no copy assignment (`struct A { A& operator=(int); };` lists
  nothing; beside a declared copy assignment both are listed). (g) Class properties: 4h for any operator function,
  `new`, `delete` and conversions among them, an `operator=` only if it is the copy assignment; 20h for a copy
  assignment (by value and by non-const reference too); 40h for a conversion function; 4h and 20h also where cl
  lists its own `operator=`. (h) cl writes the definition of every complete class that a listed function's type
  names (a parameter by reference, a result by pointer), as for a data member's. Microsoft's link keeps a
  definition in the PDB only when a symbol leads to it (an `S_UDT`, which cl writes for the classes the unit uses).
  (i) The readers: DIA gives a function a class lists its code (an address, and then the procedure's qualified name)
  only when a procedure of that name has the same type index; its flag for a static function is not set from the
  member list (a function without a `this` type is one); dbghelp tells neither access nor kind nor what is
  compiler-generated.
  Where the code differs from 5.1, 5.2, section 6 and section 9: (1) The option. By the criterion of row 4 the
  member functions are `-g2` (`CPPPARSE_G=2`): the status has the numbers. `-g2` describes after the unit's code
  is generated (`CvFromSema::finish`), so its listing is `-g`'s byte for byte and only `.debug$T`, the type
  indices in `.debug$S` and the `S_UDT` records differ. One thing comes with the functions that is not a
  function: a member typedef is an `LF_NESTTYPE` where its type needs no class described for it alone (stage 1's
  note, 4), and the functions' types describe more classes, so a class completed behind them may list a typedef
  under `-g2` that it does not list under `-g` (cl lists it). (2) The this adjustment. 5.2 says 0 for every function,
  for the reason given there, and that stands for the type a procedure's symbol names. The type a class lists for
  the same function says cl's number, because there it tells a caller where the function expects `this`, and
  cppparse's functions are entered as cl's are. A function with an adjustment so has two `LF_MFUNCTION` records
  where cl has one, and by (i) DIA lists it in its class without code. That is the conservative side: with one
  record of 0 a reader that calls the function would pass the object's address where the function expects the
  base's; with one record of cl's number a reader of a frame would move a `this` that is already right. (3) Two
  corrections of stage 1's function types reach `-g` too, since a procedure has one type at both levels: the type
  of `this` is a plain pointer as in (c), and the attributes of (c) are written on `LF_PROCEDURE` and
  `LF_MFUNCTION` (stage 1 wrote 0). `-g`'s objects are therefore not byte for byte stage 3's: the own sources'
  are 21,316 bytes (0.06 %) smaller at `-O0`, the type of `this` being a record the function's `this` variable
  has already. `CvFromSema` asks the code generator the one fact Sema does not record, whether a
  class is a plain aggregate to the calling convention (`CvResultRule`, which IrGen is: `podForReturn`). (4)
  Listed are the functions a class's definition declares; not what the compiler generates (cl lists its own
  under names cppparse does not have), not a member template's instances. A declared `operator=` that is no copy
  assignment is listed where cl by (f) lists none. A volatile member function is listed, with a `this` that is
  not volatile: Sema keeps no volatile on a member function (next-targets, G42). (5) A conversion function is
  listed when its type is one whose spelling (e) has: a fundamental type, a class or enumeration that is no
  template's instance, not local to a function and has a name, a pointer to those or to a pointer, a reference to
  those, with their qualifiers but for a volatile pointer; any other is not listed, since a wrong name would be a
  member the debugger offers and cannot find. (6) The properties 4h, 20h and 40h are set from what the class
  declares, under `-g2` only; cl's 4h and 20h for its own `operator=` are not. (7) The test. Section 6 has the
  type tree through dbghelp; by (i) the member functions are read through DIA by a driver of their own
  (examples\sample-dbg-driver\diatypes.cpp), which leaves out what is marked compiler-generated: test 20 has its
  lines equal for cl's build and for `-g2`'s, from both linkers' PDBs and in the mixed program, and none for `-g`.
  The record trees themselves were compared once by a scratch script over llvm-pdbutil's dumps of both compilers'
  objects (the status). The properties are not compared (cl's bits for its own members). (8) Section 9: no class
  of its own; `CvMethod` stands in CvModule.hpp beside `CvMember`, the records are `CvTypes`', the spelling of (e)
  `CvNames`'. (9) The classes a listed function's type names are described as 5.2 has it for every written
  record, which is cl's rule by (h); it is most of the size (`.debug$T` of the own sources 4.0 MB without the
  member functions, 13.1 MB with). A narrower rule (a class that only a member function's type names stays a
  forward reference) was not tried: it is not what cl writes.
  (10) The self-host with `-g2` is `selfhost.ps1 -G2` and the harness's check `selfhost-g2-O0`: 25 checks.
  The review of the code (an independent read with nine probes against cl's objects; its question: where can a
  wrong record be written) found the byte layouts, the kinds, slots and adjustments over multiple and virtual
  inheritance, the attributes on 39 result types, the names, what is not listed, `CvCheck`'s reader, invariant 1
  and the determinism sound, and five things, all taken: a class result of a type the unit never defines was not
  marked as returned the C++ way, which cl marks (at `-g` too; fixed, (c)); an unnamed class listed its
  functions where cl lists none (fixed, (f)); `Compare-Object` on an empty list would have thrown in the test
  (it fails with a line now); `CvCheck` did not hold a static function against its type's missing `this`, nor
  the list entry's padding (it does); nothing ran invariant 2 for `-g2` ((10)). Two wrong records it found were
  the front end's, each with its row. The volatile member function (G42) stays. The other was an override in a
  class that names a virtual base before the polymorphic base it overrides (`struct J2 : virtual VR, P1 { int
  r(); };` over `VR::r` and `P1::r`): Sema left `P1`'s slot to `P1::r`, so the adjustment listed was 32 where cl
  has 0, which was what cppparse's own code did and not what cl's does (G45). That row is closed (2026-11-26, in
  Sema; nothing in the CodeView code changed): the override is in `P1`'s table, the function takes this at the
  object, and the class's list says 0 as cl's does. types.cpp `mem::RunJ` holds it in test 20: DIA's lines for
  `run` (adjust 0) and `stay` (32, first held by the virtual base's table) are those of cl /Z7's build; with the
  three classes the program's sum is 138 and DIA lists 102 lines (dbghelp 427, 208 facts).
  Found in the front end on the way, not this row's (next-targets G42 to G45): a member function's `volatile` is
  dropped (its decorated name is the non-volatile one); a conversion function to a type that a member typedef
  names, or to a pointer to member, is refused; a qualified name does not find a member of the named namespace's
  unnamed namespace; the override of G45 (fixed since).
- 2026-11-26, stage 5. Measured first, cl 19.52 (/Z7 /Od and /O2; the lane's probes fm.cpp and fn.cpp with seven
  headers, line.cpp, and test 22's sums.cpp; llvm-readobj `--codeview --codeview-subsection-bytes`, dbghelp on the
  linked programs): (a) An entry of `DEBUG_S_FILECHKSMS` is {the offset of the name in the string table (4 bytes),
  20h, 03h, the 32 bytes of the SHA-256, two zeros}: 40 bytes. The checksum is of the file's bytes as they are on
  disk, line ends and a byte order mark included: it is what `Get-FileHash` gives. There is one table, in the
  module's `.debug$S`; the line tables of the COMDAT functions' sections name its entries by their offsets. cl
  without /Z7 writes no `.debug$S` at all. (b) cl's table names every file the unit includes, whether it has code
  or not (a header with a macro alone, one with declarations only, an empty one): 8 files for fn.cpp, 19 for
  sums.cpp, of which its line tables name four. The order is not that of the inclusions. (c) A file that only a
  `#line` directive names has an entry without a checksum (kind 0, 8 bytes), also where a file of that name
  exists. (d) Both linkers carry the entries into the PDB as they are; dbghelp's `SymGetSourceFileChecksum`
  returns the kind and the bytes, and kind 0 with no bytes for an entry without a checksum.
  Where the code differs from 5.1, 5.5, 5.6, section 6 and section 9: (1) Under `-g` and `-g2` only. 5.1 lists the
  checksums among what `-g` adds and says that without it the object is what it was, byte for byte; that decides
  it (the status has the comparison). An object without the option keeps its entries without a checksum, so a
  debugger cannot hold a source against a program built without `-g`. (2) Which files. Under `-g` the table names
  every file the unit read, as cl's does by (b): first the unit and each header in the order of reading, then any
  name a function gives that is none of them (a `#line` name). 5.1 speaks of the entries' checksums only; the
  entries for files without code are new, about three fifths of all on the own sources (the status has the
  count), and with the checksums they cost 0.7 % of the `-g` objects' bytes. The first form of this stage kept
  the table to the files with code; the count showed that cl's table costs next to nothing, and with the
  module's description holding every file read (3) it is one loop. Without `-g` the table is what it was: the
  files a line table refers to. A unit that has variables and no function with lines has the table in the module
  section `-g` gives it. (3) Whose bytes. The checksum is of the bytes the preprocessor read, as cl's is of what
  cl read. The first form of this stage read each file a second time when the object was written; the review
  showed a header edited while its unit compiles getting the edited text's checksum beside line tables of the old
  text, without a word. So the front end changes, where 5.6 says "frontend/: nothing": `Preprocessor` tells a
  `PpFileObserver` of every file it reads, once a file, with the bytes as read (`readFileBytes`; the byte order
  mark is dropped behind it), and only when one is set, which the driver does under `-g` alone: without it the
  preprocessor reads as before. The driver's `SourceChecksums` hashes the bytes with `CvSha256` into the
  `CvModule`'s sources (`CvSource`) under the name the line tables use (`debugPath` in common/PathUtil, the rule
  `IrGen::setFiles` had inline), and `CvFiles` asks the module. A name is matched whatever the case of its letters
  (a file is read once, under its first spelling). The object so stays a function of the listing and the
  `CvModule` (4.4 b), and `<name>-cv6.txt` lists the sources with their checksums. By (c) a name that only `#line`
  gives gets none, which falls out of this: the unit read no file by it. (4) Section 9 had the SHA-256 in
  `CvFiles`. It is a class of its own, `CvSha256`: FIPS 180-4 as written there, a word an `unsigned long` held to
  32 bits (so a 64-bit long gives the same), the length counted in bytes in 64 bits; a message of 2^61 bytes or
  more, whose length in bits the padding cannot say, has no digest (refused, not wrong). `ofFile` reads through
  C's `FILE`, whose error indicator tells a failed read from the file's end; a stream gives both as a short count,
  and the review showed a read that fails in the middle of a file (a locked range) taken for the end. (5)
  `CvCheck` holds the file table as well (`files`: every entry whole, its name a string of the string table, no
  checksum or a SHA-256, zeros behind it; every file a line table's own bytes name is an entry's start), which
  5.5/4 does not list. (6) `--sha256 <file>...`, an option the design does not have: the driver prints the
  checksum of each file. The test needs it (the standard's examples are no sources of a unit), and it is what a
  user holds a file against. (7) The exit test asks dbghelp for every file of the table, not for the unit and one
  header alone, and reads the object's table two more ways (the test's own reader of the bytes, llvm-readobj).
  "No longer equal after the source is edited" is shown on the programs as they are (the PDB's checksum against
  the edited file) and then on the rebuilt ones. (8) An object rebuilt from its listing (`--compile6 --obj
  name.asm`) has no `CvModule`, so no checksums and the table of the files with code: section 2's limit, unchanged.
  (9) Not measured: what Visual Studio's debugger does with the checksums (its option "require source files to
  exactly match the original version"); the status lists it for the sessions by hand. (10) No request to GRIDE3
  comes of this stage: GRIDE's link carries the checksums as 3.3 said it would. One thing seen on the way is
  section 10's L5.
  The review of the code (an independent read with probes of its own: the class against Windows' BCrypt on every
  length from 0 to 300 bytes in one piece, in every split in two and in steps of 1 to 129, 49,366 checks, the
  same with `long` made 64 bits wide, and compiled by cppparse itself at `-O0` to `-O3`; 2^61 - 1 bytes taken and
  2^61 refused; files of 512 MB and of 4,294,967,299 bytes; the entries against cl's byte for byte; a header under
  five spellings; test 22 run against the compiler of the commit before, which fails it in the hash, the table,
  the comparison with cl and both linkers' PDBs, and against a compiler that swaps two files' checksums, which
  fails it in eight checks) found the class, the layout, invariant 1 and the determinism sound, and these, all
  taken: the second read of (3), with the `#line` names of (c) that it hashed where cl has none; the read error of
  (4); a line of the test that said "kind SHA256" of whatever llvm-readobj read (it now requires kind 3); `add`
  never entered with a block begun, by the compiler or by the test (`ofFile` reads pieces of 65,521 bytes, no
  multiple of a block, and the test has files around that length); `CvCheck` held the line tables' files by what
  the writer meant to write, not by the tables' bytes (`fileOf`); a note printed even under `-q` (gone with the
  second read). It also notes what stays: a path outside the ANSI code page cannot be opened, and names go into
  the string table as ANSI bytes, neither of this stage. That review read the first form; (2) and (3) as they
  stand were made after it. What holds them is test 22 (cl's table and cppparse's name the same files; a `#line`
  name has no checksum), a probe by hand (a header edited 1.5 s into its unit's compilation: the object has the
  checksum of the text that was compiled; a file with a locked range: `--sha256` says it cannot read it), and a
  second pass of the review over the code as it stands, by reading alone. It found no defect and three things,
  all taken: a header included a second time with its name in another case of letters had a second entry, where
  cl's table, measured for it, has one (`CvFiles::entry` now takes the spelling the file was first read by; test
  22 has the case); `CvCheck` did not hold the unit's sources to have their checksums, so an observer that is
  not set would have shown in the test alone (it holds the first entries, the sources, to kind 3); the driver
  left the observer's pointer set behind its input (reset). Measured for it as well: a `#line` that names a file
  the unit did read, the unit itself, shares that file's entry with its checksum, in cl's table as in
  cppparse's. Left as it is: a file reached under a relative and under an absolute spelling of one path is read
  twice by the preprocessor (its cache goes by the path as given) and keeps the first reading's checksum.

## Appendix: the probes

Scratch files, not in the repository. cl's dumps are `llvm-pdbutil dump -types -symbols -l` of the objects. The
review's own probes (p1: cppparse's parameters and frames at `-O0` and `-O2`; p2: cl /Z7 at /Od and /O2 /Ob0;
p3: a function whose catch resumes, at `-O2`) are quoted as "review probe".

| probe | what it ran | what it settled |
|---|---|---|
| t.cpp, a.ps1, hist.py | cl /Z7 /Od, /Z7 /O2, /Zi /Od and without, the record kinds counted | 3.1: the kinds, counts and bytes; /Zi's type server |
| b.ps1, b2.ps1 | those objects through Microsoft's link and GRIDE's, each program under gride-dbg (`bp`, `g`, `dv /t /V`, `?? ...`); /O2 /Ob0 for the range records | 3.2: both linkers carry cl's records alike; /Zi loses types in GRIDE's link; gride-dbg's own errors (D1 to D6) |
| c.ps1, c2.ps1 | t.cpp by cppparse `-O0` and `-O2`, linked by both, run, and under gride-dbg | section 1; GRIDE's link takes cppparse's objects |
| cvpatch.py, d.ps1 | the prototype: cppparse's `-O0` object plus `.debug$T` and the symbols of this design, seven variants | 3.2's table |
| e.ps1 | the DIA SDK's DIA2Dump (built with `NoRegCoCreate`) on the PDBs of cl's and the prototype's programs | DIA's view; parameters by position |
| f.ps1 | the prototype with main's locals and `DbgStart`: the caller's frame, a breakpoint on the function's line | rbp-relative variables in outer frames; D7 |
| n.cpp, n.ps1, n2.ps1 | cl's and cppparse's names for templates, operators, unnamed namespaces, local classes | 3.4 |
| q.cpp, q3.cpp | cl's records for by-value class parameters, results, `this`, floats, a variadic function, a handler | 3.1's details; 5.3's by-address rule |
| s.ps1, s2.ps1, coffsize.py | cl with and without /Z7 on four own sources, time and bytes by section kind; cppparse's objects of the same | 3.5, section 1 |
| locals.py | the variables of the four sources' `-O0` IR dumps by kind | 3.6's table |
| cvpatch2.py, r.ps1 | after the review: the prototype with optimised-out records, one name twice, ranges in pieces and with gaps, mixed and unmixed forms, an associative `.debug$S` behind `.drectve`; both linkers (also /OPT:REF,ICF), gride-dbg | 3.2's second table; 4.2's one form per procedure; 5.1's order; 5.3's scopes |

The prototype's records, as cvpatch.py writes them (all little-endian; a type record is {length, leaf, data}
padded to four bytes with 0xF3, 0xF2, 0xF1; a symbol record {length, kind, data} padded with zeros):

    LF_STRUCTURE 1505   count, properties (80h: forward reference; 200h: unique name follows), field list, derived 0,
                        vtable shape, size (a numeric leaf), name, [unique name]
    LF_UNION 1506       count, properties, field list, size (a numeric leaf), name, [unique name]
    LF_FIELDLIST 1203   LF_MEMBER 150D {access, type, offset, name}, LF_BCLASS 1400 {access, type, offset},
                        LF_VBCLASS 1401 / LF_IVBCLASS 1402 {access, base, vbptr type, vbptr offset, table index},
                        LF_ENUMERATE 1502 {access, value, name}, LF_STMEMBER 150E {access, type, name},
                        LF_VFUNCTAB 1409 {0, pointer to the shape}, LF_ONEMETHOD 1511 {attributes, type, [slot offset], name}
    LF_POINTER 1002     type, attributes: 1000Ch a pointer, 1002Ch a reference, 804Ch + {class, 1} a pointer to a data
                        member of a single-inheritance class (4 bytes), 1006Ch + {class, 5} to a member function (8 bytes)
    LF_MODIFIER 1001    type, 1 (const)             LF_ARRAY 1503      element, 23h, size in bytes, ""
    LF_BITFIELD 1205    type, width, position       LF_ARGLIST 1201    count, types
    LF_PROCEDURE 1008   result, 0, 0, parameter count, argument list
    LF_MFUNCTION 1009   result, class, this, 0, 0, parameter count, argument list, this adjustment
    S_REGREL32 1111     offset (signed, from the register), type, register (334: RBP), name
    S_FRAMEPROC 1012    frame bytes, 0, 0, saved-register bytes, 0, 0, flags (2 << 14 | 2 << 16: RBP for locals and parameters)
    S_GDATA32 110D / S_LDATA32 110C    type, offset (SECREL), section (SECTION), name
    S_CONSTANT 1107     type, value (a numeric leaf), name          S_UDT 1108    type, name
    S_LOCAL 113E        type, flags (1: a parameter; 100h: optimised out, no range follows), name
    S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE 1144    offset
    S_DEFRANGE_REGISTER_REL 1145       register, flags, offset, range {offset (SECREL), section (SECTION), length},
                                       then gaps {start from the range's offset, length}, two bytes each field
