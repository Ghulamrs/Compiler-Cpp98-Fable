# Mending plan for cppparse after the cpp13 merge (written 2026-10-13, for the next session)

For Claude Opus 5.5. Read this whole file before touching code. It says where the work stands, the rules the
user has set, the defects to mend in order, and when the work counts as done.

## 1. Where things stand

| Tree | What it is | State |
|---|---|---|
| `Developer/Claude/cpp` | git, `origin` = `github.com/Ghulamrs/Compiler-Cpp98-Fable` | `main` at `8aa8d94` (pushed, the 2026-10-12 work). **Work branch `import-cpp13`** (local, not pushed): `85c6c68` cpp13 imported (line endings LF), then cpp's own 12-Oct commits reapplied: `7020bdc` Viable, `814b2fe` Chain (C++98), `58cfca3` and `f153f3a` (LP64 host fixes). |
| `Developer/Claude/cpp13` | git, **no remote** (made by the verification) | `f8d2a44` baseline as found, `eef0f98` + `b7eab18` C++98 ports, `8267108` **the verification report** `docs/verification-2026-10-13.md` with probes, scripts and results in `docs/verification-2026-10-13/`. Read-only for this work: it is the evidence, not the place to mend. |
| `Developer/Claude/CPPParadise` | git, `origin` = `github.com/Ghulamrs/CPPParadise`, `main` at `8a7c072` | The Visual Studio 2022 IDE (C++/CLI WinForms) with a pre-12-Oct cppparse. Its cppparse is replaced by the mended one only at the very end (section 6). |

Uncommitted in `cpp`'s working tree, belonging to the user: `sandbox/` (the Windows suite restored with vswhere
detection) and five files under `docs/` (`cpp11-compiling-cppparse-2026-10-13.md`, `cppparse-c98-lp64.patch`,
`runtests_linux.py`, two test logs). Do not delete them. `sandbox/` is the suite you will run (section 3); it was
first run on Windows on 2026-10-13 and works.

What the merge branch already passes on the Windows box (`C:\Users\GRA\work\cppmerge`):
- the self-host: 38 of 38 own units, the self-built compiler compiles all 38 identically, generation two equals
  generation one;
- the full suite `sandbox\build-cppparse.ps1`: Tests 1-5 pass, **Test 6 fails** (defect M1), Tests 7-16 have not
  run because the script stops at the first failing group.

## 2. Guidelines (the user's rules - follow them exactly)

1. **Windows is the platform of record.** cppparse is a Windows application; a fix qualifies on the Windows box.
   **Do not chase the Mac/Linux object difference** (the merge compiles 36 of 39 own units identically on macOS
   and Linux, `main` 39 of 39; three units - Sema, CxxParser, IrGen - differ). The user said explicitly not to
   pursue it. Record it as a known item; nothing more.
2. **cl is the oracle.** Every behaviour is checked against cl 19.44 (Visual Studio 2022, found with vswhere),
   link and dumpbin on the Windows box, with a small corner-case program. A claim without a program that shows
   it is not believed.
3. **Every fix gets a regression test** that fails before the fix and passes after, added to the suite
   (`sandbox/build-cppparse.ps1`'s groups or an `examples/sample-*` set the suite runs). The verification probes in
   `cpp13/docs/verification-2026-10-13/probes/` are the starting point; copy what you need into `cpp`.
4. **Work on the branch `import-cpp13`**, one commit per defect, message in the repository's house style (what
   was wrong, the rule it now follows, how it was checked), ending with
   `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Line endings in `cpp` are LF.
5. **Never push without the user's explicit yes.** Merging `import-cpp13` into `main` and pushing to GitHub is
   the user's decision at the end; ask once, with the results.
6. **Use Opus 5.5, not Fable 5.1**, unless the user names Fable for a task.
7. **Access:** read and write only in the folders the session has been granted under `Developer/Claude`
   (cpp, cpp13, CPPParadise, Compilers, RIDE-4.7, TI, Windows); the shell too. ssh to the Windows box (`ssh windows`)
   and the Linux box is allowed. Never delete user data; on the boxes, only inside your own work folders.
8. **On the Windows box,** work in `C:\Users\GRA\work\cppmerge` (or a fresh `...\cppmend`); ship the tree with
   `COPYFILE_DISABLE=1 tar` (no AppleDouble files); the ssh shell is cmd - put multi-step work in a `.cmd` or
   `.ps1` file and run it by full path; inside a cmd `for` block `%ERRORLEVEL%` is stale (use delayed expansion).
9. **Shell gotchas on the Mac:** zsh does not word-split `$VAR` (use `${=VAR}` or arrays); `$B:c` is a zsh
   modifier (write `${B}:path` for scp); macOS has no `timeout` (use `perl -e 'alarm N; exec @ARGV'`).
10. **Say what is true.** A defect that turns out not to reproduce is a finding too. Report each result with the
    program and both compilers' output.

## 3. How to verify (the gate for every step)

On the Windows box, from a fresh copy of the branch's committed tree plus `sandbox/`:

    powershell -NoProfile -ExecutionPolicy Bypass -File <tree>\sandbox\build-cppparse.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File <tree>\sandbox\scripts\selfhost.ps1

`build-cppparse.ps1` builds cppparse with MSVC at `/W4 /permissive-` (warnings are errors) and runs the 16 test
groups; it prints a TOTAL line only when every group ran. `main` scored 226 of 226 on 2026-10-12. `selfhost.ps1`
must print "38 of 38" twice, "compiles identically ... 38 of 38" and "generation two is identical to generation
one". The 2026-10-13 run log is in `C:\Users\GRA\work\cppmerge\suite.log` and `selfhost.log`.

## 4. The defects, in the order to mend them

Severity first: a silent wrong answer or a crash before a missing feature. M1 comes first only because it blocks
the suite from reaching Tests 7-16, which may reveal more.

| # | Defect | Evidence | Where to look |
|---|---|---|---|
| **M1** | **Table-based exceptions crash in the MASM path.** A program cppparse writes as `.asm`, assembled by ml64 and linked with a cl driver, dies with 0xC0000409 (fail-fast) when it throws; with `--eh=sjlj` the same program prints ALL OK. The `.asm` does contain unwind directives. Suite Test 6 (`examples/sample-opt` + `sample-opt-driver`). | `C:\Users\GRA\work\t6.ps1` reproduces both modes; `Compilers/C++Optimize/tests/out-cppmerge/t6.ps1` is its source | X86.cpp's MASM emission of the unwind data (PROC FRAME / .pdata / .xdata, the handler and the range table) against what the direct-COFF writer (Coff.cpp `buildUnwindInfo`) emits - the COFF path works (verification claim 1). Compare `dumpbin /unwindinfo` of the ml64 object with cppparse's own `--obj` object for the same unit. |
| **M2** | **Varargs call side truncates 64-bit integers to 32 bits** (`printf("%lld", 123456789012LL)` prints -1097262572); floats are not promoted to double at -O0; the 5th and later float parameters are passed wrongly when the first four are floats. Silent wrong values. | verification section 6; probes `va1.cpp`, `va2.cpp`, `fp1.cpp` | X86.cpp call lowering for variadic callees and the stack-argument path for floats |
| **M3** | **`examples/sample-statics` hangs** at "flakyX cl constructs" (-O0 and -O2, table and sjlj): after a throwing static initialiser the thread-safe guard is left in progress, although next-targets says `_Init_thread_abort` is done. | verification section 4; `scripts/statics-sjlj.cmd` | IrGen's guarded static-local initialisation and its landing pad calling `_Init_thread_abort` |
| **M4** | **Covariant return through a secondary base is silently miscompiled** when the class also overrides the primary base's function (`vb3.cpp` prints 1, cl 2); S131 fires only for the single-override shape. Either implement the return-adjusting thunk or refuse every such shape with S131. Refusing is acceptable; miscompiling is not. | verification section 3; probes `vb3.cpp`, `covariant.cpp` | driver/main.cpp `needsReturnAdjustment` and `virtualBaseRefusal` (check every overridden function, not only the first) |
| **M5** | **A `long long` bit-field wider than 32 bits is read and written as 32 bits** (5000000000 becomes 705032704, cl -3589934592). | verification section 2; probe `layout1.cpp` | IrGen `loadBits` / `storeBits` / `bitField`: the mask and shift in 64 bits for a 64-bit unit |
| **M6** | **A static data member declared in a class is defined in every cppparse unit** that sees the class (link LNK2005). | verification section 4; probe in `results/sdm/` | IrGen / MangleSema: emit the definition only in the unit that defines it (out-of-class), or as a COMDAT where C++ allows several (const integral in-class initialisers) |
| **M7** | `delete[]` of a class with a trivial destructor (no cookie) bypasses the class's `operator delete[](void*)`. | verification section 4 | IrGen `freeArray` / `classOperatorDelete` for the no-cookie case |
| **M8** | A trailing empty base: `struct A7 : E1, E2 {}` puts E2 at offset 0, cl at 1; the project's own sample-layout test fails 1 of 376 for the same reason (`DE : D8, E`). | verification section 2 | Sema `layoutClass`: the padding byte between zero-sized subobjects at the end of a class |
| **M9** | vftables are `Static` symbols in writable `.data` COMDATs where cl emits External `.rdata`. Wrong in a mixed link, not in an all-cppparse one. | verification section 4 | IrGen `registerMetadata` / X86 and Coff section choice |

Housekeeping, after the defects (one commit):
- README and next-targets.md name `eh.ps1` and `mapcompare.ps1`, which exist in neither tree: point them at the
  real scripts or remove the references.
- README's `/p:PlatformToolset=v145` does not build on the box; v143 does. Say v143.
- next-targets.md: `takeV(mk())` now matches cl and `->*` is refused (S109) - move both out of the known limits.
- cppparse writes `-ast6.txt` / `-sym6.txt` even for a refused unit: decide whether that is intended and say so.
- Record the Mac/Linux three-unit difference (guideline 1) as a known item.

## 5. When the work is done

All of these, on the Windows box, from the branch's committed tree:
1. `build-cppparse.ps1`: every group runs, TOTAL shows 0 failed (expect more than 226 checks once the new
   regression tests are in).
2. `selfhost.ps1`: 38 of 38, identical, generation two identical.
3. Each mended item's verification probe now prints what cl's build prints (rerun the relevant
   `cpp13/docs/verification-2026-10-13/scripts`).
4. A short `docs/mending-2026-10-<day>.md` in `cpp`: per defect, the fix, the commit, the test, before/after.

Then ask the user, once: merge `import-cpp13` into `main` and push to `Compiler-Cpp98-Fable`? Push only on yes.

## 6. Afterwards (only when the user asks)

- **CPPParadise:** replace its `cppparse/`, `include/`, `runtime/`, `libcpp/`, `platform/` and `examples/` with the
  mended tree, build `CPPParadise.sln` on the Windows box (v143, Debug and Release x64), check the IDE starts and
  its Compile / Present commands run, commit as its own commit, push only on the user's yes.
- **cpp13:** it can then be retired or kept as the record of the verification; ask the user.

## 7. Amendments from Fable 5.1's review of this plan (2026-10-13) - these override section 4 where they differ

Who does what: Opus 5.5 can do M3-M8 and the housekeeping alone. For **M1, the stack-float half of M2 and M9**
(unwind/ABI mechanisms where a wrong choice is a silent miscompile the suite does not see), write the design and
the `dumpbin /unwindinfo` (or section) evidence down first and show it to the user before coding; the user may
ask for a Fable review of it.

Order: M1, then **M2**, then M4-M8, M3 only after its link-order check, **M9 last**.

Extra gate per commit: `cpp13/docs/verification-2026-10-13/scripts/recompile.cmd` - the 64 verification objects
must stay byte-identical except where the commit means to change them.

- **M1, the root cause is known.** `X86.cpp:1226` writes `PROC FRAME:cppparse_eh_handler`, but the range table
  exists only as `;@ehrange` comments (`X86.cpp:1357`); the direct COFF writer turns them into the `CPPX` table
  after the handler RVA (`Coff.cpp:546-592`). ml64 has no directive for handler data, so the ml64 object's
  UNWIND_INFO has the handler and **no table**: the runtime finds no region and fail-fasts. Recommended fix (a): a
  side table - each function's ranges as data in a named section (`.rdata$cppx`: function start + ranges), found
  by the runtime through `BeginAddress` from `RtlLookupFunctionEntry`; one mechanism for both paths (consider
  moving the COFF path to it too). Alternative (b): hand-written `.pdata`/`.xdata` in MASM, then cxx1's CLAUDE.md
  lessons apply (`OPTION DOTNAME`, the section is `.pdata` with the dot, `OPTION NOSCOPED`, `READONLY ALIGN(4)`,
  offsets as label differences, `.pdata` order). Test: Test 6 and `examples/sample-eh` built through ml64 in table
  and sjlj modes (today only `--obj` is tested).
- **M2 is three faults:** (i) a 64-bit integer varargs argument is moved as 32 bits (`sumll`, `printf %lld`);
  (ii) float->double promotion is missing at -O0 only; (iii) the caller stores 5th+ stack floats wrongly only when
  parameters 1-4 are floats (likely the spill goes through an xmm still holding argument 1, the mirror of the
  callee bug of row 19). cl's behaviour: 8-byte slots; a float vararg promoted to double and placed in both the
  xmm and the integer register; a stack float `movss` into the low 4 bytes of `[rsp+32+8*(i-4)]`. Keep `mix2.cpp`
  as a two-way mixed sample with a cl driver; the callee side is right today and must stay right.
- **M3: check the link order before touching the guard.** In the verification the cl driver object came first,
  so link may have kept **cl's** COMDAT copy of `flakyX()`, and the throw from cppparse's `FlakyP` constructor
  then crossed a cl frame: the documented cross-runtime limit, not a guard defect. Decide with the link map (owner
  of `?flakyX@@YAAEAUFlakyP@@XZ`) and by swapping the object order. Only if cppparse's copy was kept: check that
  the cleanup region covers the constructor call and that the pad calls `_Init_thread_abort` (IrGen ~2053) before
  `cppparse_resume`. It may end as a documentation item.
- **M4: refuse, do not implement thunks this round.** `driver/main.cpp:175 needsReturnAdjustment` stops at the
  first overridden function; check them all. Test: `vb3.cpp` refused with S131, `covariant.cpp` unchanged (CD
  refused, PD accepted).
- **M5:** mask and shift in the unit's width (64) and sign-extend from the field width (`c:33` = 5000000000 reads
  -3589934592 in cl); test signed and unsigned, values >= 2^32, a field crossing bit 32.
- **M6:** cl emits nothing for an in-class declaration (UNDEF where used). Trap: `static const int k = 3;` used by
  value needs no definition; if odr-used, C++98 requires the out-of-class one - do not invent a COMDAT. Test
  `sdm.cpp` both ways plus the `static const` case.
- **M7:** cl calls the class's `operator delete[](void*)` with the block; the sized form gets `count*size` (+8 with
  a cookie). Test both forms, with and without a cookie (`kl8.cpp` Q, `mix1.cpp` Cls).
- **M8, cl 19.44's measured rule:** an empty base after a non-empty subobject goes at the current end of data
  (`DE`: E at 8 = sizeof; `A7`: E2 at 1 after E1 at 0); a byte is inserted only between two zero-sized
  subobjects. The sample's comment "at 7" came from another cl; the driver compares against the running cl.
  cxx1's CLAUDE.md "A4" section has the fuller ends-with / leads-with rule.
- **M9, cl's shape:** `.rdata` COMDAT, flags 40401040, 8-byte aligned, External, selection 6 (pick largest), with
  `??_7` at +8 of a 16-byte section (slot 0 is the RTTI complete-object locator). Trap: making cppparse's table
  External lets the linker pick one copy for both compilers, and cppparse's has no locator slot, so if it wins,
  cl's `dynamic_cast`/`typeid` read garbage at -8. Either emit a (null) locator slot with the symbol at +8, or keep
  it Static and only move it to `.rdata`. Test with a polymorphic class defined in both objects of a mixed link.
