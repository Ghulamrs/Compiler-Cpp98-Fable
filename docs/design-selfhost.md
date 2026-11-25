# Design: the self-hosting boundary (row P1, 2026-11-19)

Status: the second text, with the independent review folded in (section 10 says what it found and what changed),
has the user's go-ahead of 2026-11-20 on the recommended answers to the ten questions of section 7; each is taken
as decided. Development goes by the stages of section 5, and what a stage measures against this text is a dated
note in section 10. Built: stage 0a (2026-11-20; the scripts, nothing in the compiler), with one of its exit tests
open on request G7 to GRIDE3; stage 1 (2026-11-24, thread-locals in the compiler); stage 2 (2026-11-24, intrinsics,
headers, the pragma, the model mark); stage 3 (2026-11-24, level 1: the self-host compiles its runtime). The order was the one of M7 (docs/design-cl-exceptions.md): the first text, an
independent review, this text, the user's review, then development.

In one paragraph: cppparse compiles itself, its library and its objects, but three things in every self-built
compiler are still Microsoft's: the object of its own runtime (`runtime/cppparse_rt.cpp`, compiled by cl), the linker,
and the C and C++ runtime underneath (libcmt, libucrt, libvcruntime). This design takes them away in three levels, each
with its own exit test: (1) cppparse compiles its runtime, which needs thread-local storage (`__declspec(thread)`, as
cl emits it), five intrinsics, one linker directive from the source and the unwind declarations in `platform/`; (2)
the self-host links with GRIDE's link, and generation two's exe is byte-identical to generation one's; (3) the
self-host runs on GRIDE's runtime, imports KERNEL32.dll alone and builds itself with no Visual Studio on the
machine, where a check set that needs no Microsoft tool passes. All three were reached by hand in the probes
(section 1), with bridges that are named there: thread-locals as globals, intrinsics from a cl stub, two links
forced. What stands between the probes and the levels is listed, with its owner: cppparse (sections 4 and 5) or
GRIDE3 (six requests in the reviewed text and a seventh from stage 0a, section 9). The test suite as a whole stays on the machine with Visual Studio: it is built on
cl, ml64 and dumpbin as its oracles (1.6), and moving it is a row of its own (question 7).

Measured with cl 19.52.36725 (x64, preview) and its link and dumpbin, cppparse 6.0 of main 5f463e2
(sandbox\cppparse.exe, cl-built), GRIDE's link and runtime as installed in GRIDE3 5220ffd (GRIDEd\x64\Release:
link.exe, lib\x86_64-windows\grt*.lib and kernel32.lib of 2026-11-05). x64 Windows only, as the rest of the back end.

## 1. The boundary today, measured

### 1.1 What cppparse's objects take from outside

Probe m1 compiled the 47 own units and `libcpp\iostream.cpp` with `--compile6 -O0 --obj --msvc-mangling` (48
objects), cl compiled the runtime, Microsoft's link linked them with `/MAP` as selfhost.ps1 does. Probe m2 read every
object's symbol table (dumpbin /symbols) and the map. The 48 objects define 7,173 external names and leave 103
undefined among themselves (libcpp's own `operator new`, `operator delete` and their array and nothrow forms are
replaceable definitions in iostream6.obj and count as cppparse's):

| Provider | Names | Which |
|---|---|---|
| `cppparse_rt.obj` (cl-built today) | 15 | `cppparse_catch_begin`, `_catch_end`, `_catch_unwind`, `_eh_handler`, `_match`, `_rethrow`, `_terminate`, `_throw_owned`, `_uncaught_exception`, `_unwind_destroy`, `_unwind_enter`, `_unwind_leave`, `_interlocked_increment`, `_interlocked_decrement`, `_interlocked_compare_exchange` |
| kernel32 (imports) | 10 | `CreateThread`, `WaitForSingleObject`, `WaitForMultipleObjects`, `CloseHandle`, `GetSystemInfo`, `GetTickCount`, the four `...CriticalSection` |
| libcmt | 8 | `_Init_thread_header`, `_Init_thread_footer`, `_Init_thread_abort` (thread-safe statics), `??_7type_info@@6B@` (type_info's vftable, named by every type descriptor), `??3@YAXPEAX_K@Z` and `??_V@YAXPEAX_K@Z` (sized delete; they forward to libcpp's plain ones), `__chkstk`, `atexit` |
| libvcruntime | 7 | `_purecall`, `memcpy`, `memmove`, `memset`, `memcmp`, `memchr`, `strchr` |
| libucrt | 63 | stdio (`__acrt_iob_func`, `fopen`, `fclose`, `fread`, `fwrite`, `fgetc`, `fputc`, `fputs`, `ungetc`, `fflush`, `_fseeki64`, `_ftelli64`, `remove`), `malloc`, `free`, the ctype and wctype tests and case functions (23), conversions (`atoi`, `atol`, `atof`, `strtol`, `strtoul`, `strtod`, `strtof`), math (`sqrt`, `floor`, `ceil`, `fabs`, `fmod`, `frexp`, `ldexp`), time (`_time64`, `_localtime64`, `strftime`), files (`_findfirst64i32`, `_findnext64i32`, `_findclose`, `_stat64i32`, `_getcwd`), `getenv`, `system`, `strlen` |

Beside these the image has the entry (`mainCRTStartup`, libcmt) and whatever those names pull. The map counts, by
provider, 2,062 public names of libucrt, 552 of libvcruntime, 158 of libcmt and 116 imports of KERNEL32.dll in a
3,071,488-byte exe.

The compiler's own sources use neither `dynamic_cast` nor `typeid` nor `--eh=cl`. Other programs cppparse compiles
may also name (the back end's literals): `__RTtypeid`, `__RTDynamicCast`, `__RTCastToVoid`, `_setjmp` and
`cppparse_push_frame`/`_pop_frame` (`--eh=sjlj`), `_CxxThrowException` and `__CxxFrameHandler3` (`--eh=cl`), and any
C function the library headers declare.

### 1.2 The runtime

`runtime/cppparse_rt.cpp` (650 lines) defines 26 `cppparse_` names (measured on 5f463e2, as all of this section;
row G30 of 2026-11-20 added a 27th, `cppparse_unwind_destroy_next`, which the compiled objects call, and left the
runtime reading its return address in `cppparse_unwind_enter` alone). cl's object of it takes 33 names from outside:

| Provider | Names |
|---|---|
| kernel32 | `RtlCaptureContext`, `RtlLookupFunctionEntry`, `RtlVirtualUnwind`, `RtlRestoreContext`, `GetCurrentThreadId`, `SwitchToThread` |
| libcmt | `_tls_index` (thread-locals); `__security_cookie`, `__security_check_cookie`, `__GSHandlerCheck`, `__report_rangecheckfailure` (cl's /GS, not the source's); `__GSHandlerCheck_EH`, `__GSHandlerCheck_EH4` (addresses the source compares a frame's handler with) |
| libvcruntime | `__CxxFrameHandler3`, `__CxxFrameHandler4` (the same comparison), `__uncaught_exception`, `longjmp`, `memcpy`, `strchr`, `strstr` |
| libucrt | `terminate`, `set_terminate` (one terminate state with cl's runtime, M7 stage 4), `abort`, `malloc`, `realloc`, `free`, `fflush`, `__acrt_iob_func`, `__stdio_common_vfprintf` (cl's inline `fprintf`), `strcmp`, `strncmp`, `strlen`, `strtoul` |

What stops cppparse from compiling it (probes m3, m4):

- `#include <eh.h>` and `<intrin.h>`: not found; the compile ends there. From them the source uses `terminate`,
  `set_terminate`, `__uncaught_exception` (functions with C linkage) and five intrinsics: `_InterlockedIncrement`,
  `_InterlockedDecrement`, `_InterlockedCompareExchange`, `_InterlockedExchange`, `_AddressOfReturnAddress`.
  KERNEL32.dll exports none of the Interlocked functions on x64 (they are inline in the SDK's headers).
- `<windows.h>` resolves to `platform/windows.h`, which lacks `CONTEXT`, `M128A`, `EXCEPTION_RECORD`,
  `RUNTIME_FUNCTION`, `DISPATCHER_CONTEXT`, `EXCEPTION_DISPOSITION`, `PEXCEPTION_ROUTINE`, `DWORD64`, the
  `UNW_FLAG_` values, the four `Rtl` functions and `SwitchToThread`.
- `__declspec(thread)` on 13 variables (the frame stack, the exception in flight, the caught ones, the match
  results, the unwinding depth and its frames): the parser drops it without a word, so they would be one per process.

With those two includes replaced by a draft header of 50 lines (rt2\unwind_draft.h: the structures above written
flat, without the SDK's anonymous unions, so strict C++98 but for `__declspec(align(16))`; the intrinsics declared
as plain functions) **cppparse compiles the runtime without an error at -O0 and -O2** (49 functions; objects of
36,078 and 34,963 bytes against cl's 27,783). The draft's layout equals the SDK's: `sizeof(CONTEXT)` 1232, `Rip` at
248, `Xmm6` at 512, `VectorRegister` at 768, `sizeof(DISPATCHER_CONTEXT)` 80, `HandlerData` at 56,
`sizeof(EXCEPTION_RECORD)` 152, printed by cl from the SDK and by cppparse from the draft (the review printed the
SDK's again, r9). So nothing else in the language is missing: no syntax the parser refuses, no construct Sema
refuses.

Linked into the self-built compiler in place of cl's object (probe m5, part 5), that compiler compiles `sample-opt`
and `Sema.cpp` to the objects the cl-built compiler writes. What that object was, and so what the probes did not
test:

- **The thread-locals were process-wide globals** (the parser dropped the attribute), so the object is right for
  one thread only. No probe ran a `.tls$` section of cppparse's making, and none ran the runtime's thread-locals on
  GRIDE's runtime in more than one thread.
- **The five intrinsics came from a cl stub** (`lnk\stub.c`), each a function of its own. For four of them that is
  the same meaning with a call around it. For `_AddressOfReturnAddress` it is not: the stub returns the slot of its
  own return address, 64 bytes below the value `cppparse_unwind_enter` needs (its frame: `push rbp`, `push rbx`,
  `sub rsp, 40`). The runtime uses the value to recognise an exception that leaves a destructor during unwinding
  (`g_unwindFrame`, compared in `dispatch`), and no probe had such a destructor. So the intrinsic's meaning is
  untested (contract 4.2, stage 2).

The throw paths of probe m8 work through that object (a program with nested destructors, a rethrow, a throw from a
handler, `vector::at`, `dynamic_cast`, `typeid`, a static local; `--eh=table` and `--eh=sjlj`, -O0 and -O2): the 17
lines and the exit code equal those with cl's object of the runtime, on Microsoft's libraries and on GRIDE's
runtime (linked as in 1.4); the review ran it again with the same result (r8). That covers m8's paths and no
others. `--eh=cl` was not measured: the link refused it, rightly, because the probe's libcpp object was of the
other model family, and for a second reason that is a finding: **cppparse's object of the runtime carries the
model mark** (`cppparse_eh` = `cpp`, the mismatch directive of M7 stage 4, which X86.cpp writes into every object
whatever it contains), so it cannot be linked with `--eh=cl` objects, where cl's object of the runtime, which has
no mark, can (3.7, contract 4.5).

The header comment "Strict C++98 (C subset)" is not so today: the file needs `__declspec(thread)`, intrinsics and
one `unsigned long long`. Nothing checks the claim either: strict98.ps1 runs clang over the compiler's own units,
not over the runtime.

### 1.3 The linkers

The same 49 objects, linked twice two seconds apart (probe m5; the `/DEBUG` rows also from the review's r1 to r3):

| Linker, libraries | Options | Two links | The exe |
|---|---|---|---|
| Microsoft's, Microsoft's | (none) | 2 bytes differ (the time stamp, twice in the image) | 3,071,488 bytes |
| Microsoft's, Microsoft's | `/Brepro` | identical | |
| GRIDE's, Microsoft's | (none) | 2 bytes differ | 3,115,008 bytes, in 1.0 s; runs, compiles sample-opt identically |
| GRIDE's, Microsoft's | `/timestamp:0` | **identical**, also from two directories and under two output names (r2, part 3) | |
| GRIDE's, Microsoft's | `/DEBUG /timestamp:0`, the same output path twice | **identical**, exe and PDB (r2, part 1) | 3,899,904 bytes; PDB 13,672,448 |
| GRIDE's, Microsoft's | `/DEBUG /timestamp:0`, two output names in one directory (m5) | 17 bytes differ in the exe: one byte of the PDB's path in the RSDS record and the 16 of its GUID; 16 in the PDB (its GUID) | |
| GRIDE's, Microsoft's | `/DEBUG /timestamp:0`, the objects copied to a second directory, absolute paths (r2, part 2) | 17 bytes in the exe, 114 in the PDB, which holds 98 absolute paths of its directory | |
| GRIDE's, Microsoft's | `/DEBUG /timestamp:0`, two directories, objects by relative paths (r3) | 17 bytes in the exe, 16 in the PDB (the GUID); no absolute path in the PDB | |

So GRIDE's link takes every object of the self-host and Microsoft's libraries today, and its output is a function
of its input once the time stamp is given, with and without `/DEBUG`. The first text read the 17 bytes as a GUID
"drawn anew each time"; that was wrong. The GUID is a hash of the image as written (LINK\src\image.cpp,
`image_guid`), and the image holds the PDB's absolute path in its RSDS record; a different path gives a different
hash, and the PDB records the objects' paths as they were given. **The paths are the only input that differs.**

What else GRIDE's link does with options that matter here (LINK\src\main.cpp, layout.cpp; probe n1):

- `/PDBALTPATH:`, `/Brepro` and `/INCREMENTAL` are accepted and ignored.
- **`/FAILIFMISMATCH` is read and left alone** (layout.cpp: "read and left alone"). M7 stage 4's guard, that link
  refuses objects of the two exception-model families in one program, is that directive (ehx.ps1 expects
  Microsoft's LNK2038). Under GRIDE's link such a program links, and only the runtime's crossing stop is left,
  which sees the frames it walks and no others.
- `/ALTERNATENAME` and weak externals are honoured. Probe n1: an object that names `__CxxFrameHandler4`,
  `__GSHandlerCheck_EH` and `__GSHandlerCheck_EH4` with `/ALTERNATENAME:<name>=<a function of its own>` in its
  directives gets the libraries' definitions on Microsoft's libraries and its own default on GRIDE's runtime,
  under either linker (four links, four runs).
- **A replaceable definition does not stand against a library under it.** libcpp's `operator new` and its seven
  siblings are `__declspec(replaceable)`: the name is a weak external whose default is libcpp's code, written
  with the search kind ALIAS. Probe n3: when a library on the line also defines the name, Microsoft's link keeps
  the object's default and GRIDE's link takes the library's (it searches the archives for every weak external
  that is not NOLIBRARY). So a program linked by GRIDE's link gets the C runtime's `operator new` in place of
  libcpp's: libcmt's on Microsoft's libraries, grtcxx's on GRIDE's runtime, either of which reports a failure
  through cl's exception model, which no frame of the table or sjlj model catches (the reason libcpp has its
  own). m5's GRIDE-linked compiler ran with libcmt's allocator without anyone seeing it. Probe n4: with the kind
  patched to NOLIBRARY both linkers keep the object's default against a library and both give way to another
  object's definition, which is what replaceable means. Neither the first text nor the review had this; it is
  cppparse's to fix (4.4, stage 0a). **Not so, measured at stage 0a** (section 10, note of 2026-11-20): n4 had
  the default and its caller in one object. With the caller in another object, which is how every unit names
  libcpp's `operator new`, Microsoft's link does not honour NOLIBRARY: it takes the library's definition or
  reports the name unresolved, by the objects' order. No search kind serves both linkers; ALIAS stays, and the
  difference is GRIDE's link's to close (request G7).
- It has no `/FORCE`; the probes' bridged links used Microsoft's.
- Its map file is its own format; maps.ps1 and mapcompare.ps1 parse Microsoft's. (Stage 0a: the lines of the
  public names have link.exe's form, address, name, address in the image, `f`, module with `library:member` for a
  library's; selfhost.ps1 and replace.ps1 read those from either linker's map. The rest was not compared.)

### 1.4 GRIDE's runtime under cppparse's objects

Probe m6 looked every name of 1.1 and 1.2, and the other names the back end and the library headers can emit, up in
the six libraries (grtstart, grtcompat, grtcxx, grtc, grtwin, kernel32; 1,398 names). **All 63 libucrt names, the 7 of
libvcruntime and 6 of libcmt's 8 are there**, as are `__RTtypeid`, `__RTDynamicCast`, `__RTCastToVoid`,
`_CxxThrowException`, `__CxxFrameHandler3`, `_setjmp`, `longjmp`, `terminate`, `set_terminate`,
`__uncaught_exception`, `_tls_index`, `_tls_used`, `mainCRTStartup`. What is not there, or stands in the way:

| # | Finding | Seen as | Whose |
|---|---|---|---|
| a | `std::uncaught_exception` is defined in grtcxx's `exceptions.obj`, the object that also holds `terminate`, `set_terminate`, `__uncaught_exception`, `__CxxFrameHandler3`; cppparse's `<exception>` defines it inline in every object that includes it | GRIDE's link: "already defined in Ast6.obj; also in grtcxx.lib(exceptions.obj)", no exe. Microsoft's: LNK2005 | GRIDE3 (request G1) |
| b | sized `operator delete` (`??3@YAXPEAX_K@Z`, `??_V@YAXPEAX_K@Z`) is in none of the six libraries | unresolved in 19 and more objects | cppparse: libcpp defines the plain ones and should define these beside them |
| c | `RtlCaptureContext`, `RtlVirtualUnwind`, `RtlRestoreContext` are not in GRIDE's kernel32.lib (`RtlLookupFunctionEntry`, `RtlUnwindEx`, `RtlPcToFileHeader`, `RaiseException` are; KERNEL32.dll exports all of them) | unresolved in `dispatch` | GRIDE3 (request G2) |
| d | `__CxxFrameHandler4`, `__GSHandlerCheck_EH`, `__GSHandlerCheck_EH4`: handlers of cl's objects that the runtime only compares addresses with | unresolved in `clFrameHasState` | cppparse: `/ALTERNATENAME` with a default of the runtime's own (3.6, probe n1) |
| e | `__stdio_common_vfprintf`: only cl's object of the runtime names it (cl's inline `fprintf`) | unresolved in cl's object | nobody: cppparse's object calls `fprintf`, which grtc has |
| f | **grtcxx's `rtti.obj` pulls cpp11's `std::exception` family into every program with `dynamic_cast` or `typeid`.** `rtti.obj` (`__RTDynamicCast`, `__RTtypeid`, `__RTCastToVoid`) names `__grt_throw_bad_cast` and `__grt_throw_bad_typeid`; those are in `badcast.obj`, which defines 40 names of namespace `std` as COMDATs: vftables, RTTI, throw information and member functions of `std::exception`, `bad_cast`, `bad_typeid`, in Microsoft's layout (24 bytes; `what()` reads the word after the vfptr). cppparse's `std::exception` is 8 bytes under the same decorated names. `BadAlloc.obj` (37 names) is the same case when grtcxx's `operator new` is pulled (review, r6) | no message: the external names are COMDATs on both sides, the linker keeps the first, and objects come before libraries. Probe n2 (m8's program on GRIDE's runtime, with a map): `what`, the destructor and the type descriptor of `std::exception` come from cppparse's objects; the vftables, which are static symbols on both sides, are in the image twice, cppparse's and badcast.obj's; grtcxx's members in the image are badcast, ehcall, exceptions, rtti, support, typeinfo. So grtcxx's two throwers would build 24-byte objects behind their own vftables whose slots are cppparse's 8-byte functions. They are dead only as long as cppparse's code never passes a failed reference cast or a null `typeid` operand down, which it does not (4.4) | GRIDE3 (request G1, extended); until then a contract with a test (4.4) |
| g | grtcompat's `compat\gs.asm` defines `__security_cookie`, `__security_check_cookie`, `__GSHandlerCheck`, `__report_rangecheckfailure` | cl's object of the runtime finds its /GS names there | nobody; grtcompat serves cl's objects, and the link line of 4.6 leaves it out |

With (a) forced, (b) from a two-line stub and (c) from Microsoft's kernel32.lib (probe m7, Microsoft's link with
`/FORCE`, because GRIDE's has no such switch), **the self-built compiler runs on GRIDE's runtime**: 48 objects
compiled by cppparse, the runtime compiled by cppparse (1.2, with its two bridges), GRIDE's start-up, C++ support,
C library and Windows layer. It imports KERNEL32.dll alone, is 3,007,488 bytes (64,000 fewer than on Microsoft's
libraries), and compiles `sample-opt` and `Sema.cpp` at `-O2` to the same objects as the cl-built compiler; the
review confirmed the imports, sample-opt and `--health -j 4` (r7). `Sema.cpp` took 25.1 s against 24.2 s for the
same objects on Microsoft's libraries: one run each, the machine not idle, so the figure says "about equal" and no
more (the compiler's objects are `-O0` here, the cl-built compiler takes 11.4 s). Stage 5a measures it properly.

### 1.5 cl's thread-local storage

Probe m3, part b: five functions over `__declspec(thread)` variables, /Od and /O2; the review's r4 confirmed it and
r5 wrote the forms for ml64.

- The variables live in a section `.tls$` (flags 0xC0500040: initialised data, read, write, 16-byte alignment), with
  their initial values; a static one is a static symbol there, an external one an external symbol.
- An access is five instructions (cl /O2, `int get() { return g_count; }`):

      mov  ecx, dword ptr [_tls_index]     ; 8B 0D rel32        REL32 to _tls_index
      mov  rax, qword ptr gs:[58h]         ; 65 48 8B 04 25 58 00 00 00   the thread's array of blocks
      mov  edx, offset g_count             ; BA imm32           SECREL to the variable
      mov  rax, qword ptr [rax+rcx*8]      ; this module's block
      mov  eax, dword ptr [rdx+rax]

  /Od does the same with other registers. The offset is a 32-bit section-relative relocation (`SECREL`), added to
  the block's address; an address taken (`&g_pair.b`) is the same sum.
- ml64 assembles the forms to cl's bytes when they are spelled `mov rax, qword ptr gs:[58h]` and `mov edx,
  SECTIONREL g_count`, in a segment `_TLS SEGMENT ALIGN(16) READ WRITE ALIAS(".tls$") 'DATA'` (r5). `OFFSET` gives
  an ADDR32 relocation without a word, and a segment named `_TLS` without the alias is not merged into `.tls`. cl's
  own /FA listing does not assemble.
- The object defines no directory. `_tls_index` and `_tls_used` (the image's TLS directory, which the linker enters
  as data directory 9 when the name exists) come from the C runtime: libcmt's `tlssup.obj`, grtwin's `tls.obj`
  (`win\tls.asm`; grtcxx's `tlsdata.obj` is another thing, cpp11's `thread_local` table). The reference to
  `_tls_index` pulls that object in. Both linkers merge `.tls$` into `.tls` between the runtime's markers.
- A thread-local declared `extern` and defined elsewhere is reached through `__tls_guard` and
  `__dyn_tls_on_demand_init` (it may have a dynamic initialiser, a C++11 matter). The runtime's 13 are all static with
  constant initialisers.

### 1.6 The scripts, and the first compiler

Two facts the first text passed over; the review found both.

**The suite is built on Microsoft's tools as oracles.** `sandbox\build-cppparse.ps1` (1,517 lines, 19 tests) names
cl on 41 lines, 31 of them `cl ... /Fe`, which compiles a driver and links through Microsoft's link; dumpbin on 32;
ml64 in tests 5, 6 and 8. Test 1 builds cppparse with cl, test 10 compares `-E` with `cl /EP`, tests 13 to 18 link
drivers that cl compiles (driver-native, -abi, -align, -layout, -lib, -life, -self, -statics). Only tests 2, 3, 4,
7, 11 and 12 name no Microsoft tool, and they run no compiled program. The other scripts are the same: eh.ps1,
ehx.ps1, mix.ps1, matrix.ps1, lib.ps1, life.ps1 compile their drivers or references with cl, mapcompare.ps1 reads
dumpbin. toolset.ps1 fails outright where `$VsRoot\VC\Tools\MSVC` does not exist. cl's driver objects cannot stand
on GRIDE's runtime either: they need `__CxxFrameHandler4`, `__GSHandlerCheck_EH4` and `__std_terminate`, which
GRIDE's Runtime_Design 13.4 only plans. So "the whole suite on a machine without Visual Studio" is not a thing
that can be asked of this row; what can be asked is in 3.8.

**The first compiler without Visual Studio.** Where there is no cl there is nothing but cppparse to compile
`runtime/cppparse_rt.cpp`, so the first compiler on such a machine must itself be at level 1. GRIDE3's example
(`examples\projects\cppparse`: cppparse.pro, the vcxproj, `cppparse\{analyzer, backend, common, driver, frontend,
middle}`) has no `eh\` folder, so it is older than M7 stage 0, and it holds none of `runtime\`, `include\`,
`libcpp\`, `platform\`. Nothing has measured that a cpp11-built cppparse writes the objects a cl-built one writes;
the record so far is that its `--health` output is equal.

## 2. Goals, levels and limits

**Goal.** No Microsoft tool and no Microsoft library in the path from cppparse's sources to a running cppparse:
the compiler compiles every object, GRIDE's link links them, GRIDE's runtime is underneath. The goal is about
building and running the compiler; the test suite's oracles stay Microsoft's (1.6, 3.8).

| Level | What becomes true | Exit test |
|---|---|---|
| 1 Own runtime | every object of cppparse-self.exe is compiled by cppparse; cl still compiles the same runtime source for programs that mix cl's objects | selfhost.ps1 compiles `runtime/cppparse_rt.cpp` with the compiler under test in both generations and calls cl nowhere; the runtime's object and listing are identical across the generations at -O0 and -O2; eh.ps1 (with its terminate programs), ehx.ps1, mix.ps1, unwind.ps1, failnew.ps1 pass with cppparse's object of the runtime and with cl's; the thread tests of stages 1 and 2 pass, the three models' throw test among them |
| 2 GRIDE's link | the self-host is linked by GRIDE's link; an exe is a function of its objects | cppparse-self.exe and cppparse-self2.exe are byte-identical. 2a: without `/DEBUG`, with `/timestamp:0`, in reach today (r2), and the suite passes under the GRIDE-linked compiler. 2b: with `/DEBUG`, exe and PDB, both generations linked under one file name from relative paths, after request G3 |
| 3 GRIDE's runtime | cppparse-self.exe links against grtstart, grtcxx, grtc, grtwin and GRIDE's kernel32.lib, and builds itself where there is no Visual Studio and no Windows SDK | 3a, on the machine with Visual Studio: the exe imports KERNEL32.dll alone; generations identical on GRIDE's link and runtime; the whole suite passes with that exe as the compiler under test (its test programs on Microsoft's libraries wherever cl compiles a driver, as today); the thread tests pass on GRIDE's runtime. 3b, with no Microsoft tool: the check set of 3.8 (`novs.ps1`) passes from a given first compiler, in a mode that sets no MSVC path, INCLUDE or LIB; the same on a machine without Visual Studio (the user's check); and the compiler from a cpp11-built first compiler equals the one from the cl-built one, byte for byte (3.9) |

**In reach, and the order.** 2a first (selfhost.ps1 only: measured in 1.3), then 1 (compiler work, all of it inside
this repository), then 3 (needs requests G1 and G2 of GRIDE3 and level 1), 2b whenever G3 arrives. Level 3 depends
on level 1 because a machine without Visual Studio has no cl to compile the runtime; cl's object of it would also
need `__stdio_common_vfprintf` and three handler names GRIDE's runtime does not have (1.4 d, e; its /GS names are
in grtcompat, 1.4 g).

**Limits.**
- Microsoft's link and libraries stay supported and stay the harness's default until the user decides otherwise
  (question 1): every level adds a configuration, none removes one. Programs that mix cl's objects (mix.ps1,
  `--eh=cl` across cl's frames, every test with a cl-compiled driver) keep Microsoft's vcruntime: that is where
  `__CxxFrameHandler4` and cl's STL live.
- The suite's oracles (cl, `cl /EP`, ml64, dumpbin, dbghelp) are not replaced here. On a machine without Visual
  Studio the check set of 3.8 runs, not the suite.
- No new runtime of cppparse's own for what GRIDE's has: start-up, the C library, RTTI's helpers, the statics'
  guards, `_purecall` and FH3 come from GRIDE's runtime at level 3 (4.4). cppparse's runtime stays what it is: its
  exception models and the three counters.
- Thread-locals: namespace scope, static storage with a constant initialiser, which is what the runtime has. No
  dynamic initialisers, no destructors (`thread_local` is C++11), no block-scope or member thread-locals (4.1).
- Under GRIDE's link the link-time guard against mixed exception-model families does not exist until request G5
  is in (1.3); the runtime's crossing stop remains.
- The first compiler on a machine without Visual Studio is a cppparse.exe built elsewhere (3.9, question 2); this
  design does not make cppparse appear from nothing. It does measure that two differently built first compilers
  give the same compiler.
- The runtime's terminate and unexpected handlers stay process-wide (M7's limit). With thread-locals they could
  be per thread, as the UCRT's and grtcxx's are; that is a later step, not this row's.
- Not here: the default pipeline (P19), DLLs, other targets.

## 3. The options

### 3.1 Thread-local storage

| | (a) `__declspec(thread)` in the compiler, as cl | (b) slots through the platform header (`TlsAlloc` or `FlsAlloc`, `TlsGetValue`) |
|---|---|---|
| The runtime's source | the 13 declarations stay, spelled through one macro (4.5); cl and cppparse compile the same text | rewritten: one structure per thread behind a slot, every use through a function that fetches it; cl compiles that too |
| An access | five instructions inline (1.5), no call | a call through the import table per fetch (the sjlj model fetches on every `try` entry and exit) |
| First use in a thread | nothing: the loader makes the block | allocate the structure (`malloc` can fail inside a `throw`), and make the slot once per process, which needs a compare-and-exchange or a lock of its own |
| A thread's end | nothing: the loader frees the block | the structure leaks, or `FlsAlloc`'s callback frees it |
| Strict C++98 | no: one more `__declspec`, beside `align`, which the sources have already | yes |
| Compiler work | parser, Sema, one IR operation, the back end's sequence, `.tls$` and its relocation in the object | none |
| Use beyond the runtime | cl's programs whose thread-locals are of the kind of 4.1 (namespace scope, constant initialiser) compile; those with a block-scope or member thread-local are refused with a message | none |
| Needs of the C runtime | `_tls_index`, `_tls_used`: libcmt has them, grtwin has them (1.5) | `TlsAlloc` and its family from kernel32 (GRIDE's kernel32.lib would have to list them) |

**Recommendation: (a).** The runtime is on the path of every `throw` and, in the sjlj model, of every `try`; (b)
puts a call and a failure path there and makes the runtime own an allocation per thread. (a) costs compiler work
once and leaves the runtime's logic alone, so cl's object and cppparse's stay the same program, which the mixed
tests need. The price is honesty in the header comment, "C++98 with three documented extensions: thread-local
storage, alignment, a 64-bit integer type", and a check behind the comment: the runtime and the new headers join
strict98.ps1 (4.3). (b) remains the fallback if the user refuses the extension (question 4); nothing else in this
design depends on the choice. The review agreed with (a).

Kept small: static storage duration, constant initialiser, no constructor or destructor; anything else is an error
with a number (as cl's C2482 for a dynamic initialiser before C++11). An `extern` thread-local of another object
is reached by the same five instructions, without `__tls_guard` (there is nothing to initialise on demand; cl's own
`/Zc:tlsGuards-` does the same).

### 3.2 The five intrinsics

| | (a) intrinsics in the compiler | (b) an assembly object (GRIDE's masm) | (c) no atomics: critical sections; the return address passed by the generated code |
|---|---|---|---|
| Level 1's "every object is cppparse's" | yes | no: one object from another tool, which the harness would need | yes |
| Cost at run time | `lock xadd`, `lock cmpxchg`, `xchg`, `lea` inline | a call each | a critical section per counter operation; every landing pad passes one more argument (all objects change) |
| Compiler work | five names known to the back end, three encodings in the object writer's assembler | none | the pad's contract in three models |
| Later use | libcpp's reference counts inline instead of a call into the runtime | none | none |

**Recommendation: (a)**, by name and signature, declared in a `platform/intrin.h` of ours: a call to
`_InterlockedIncrement`, `_InterlockedDecrement`, `_InterlockedExchange`, `_InterlockedCompareExchange` (on `long
volatile*`) or `_AddressOfReturnAddress` that resolves to that C-linkage declaration is emitted inline. The
runtime keeps `cppparse_interlocked_increment` and its two siblings as functions, so no object of today changes its
calls; inlining them in the library is a later, separate step.

`_AddressOfReturnAddress` is the one with a trap, which the review found: cppparse gives a function with nothing
in memory and no call no frame at all (X86.cpp `decideFrame`), and `cppparse_unwind_enter`, whose only call today
is the stub, becomes such a function once the intrinsic and the thread-local accesses are inline. `rbp` is then
the caller's, and `[rbp+8]` names the pad function's own return address: the unwinding frame is noted one frame
too high, and an exception that leaves a destructor terminates a frame late or is caught where it must not be.
Two ways out: the frameless form (`lea rax, [rsp]` at entry), or the function keeps its frame. The second is
taken: one rule, no second encoding, and the function with the intrinsic is not a hot one (4.2).

### 3.3 The declarations

The runtime includes `<windows.h>`, `<eh.h>`, `<intrin.h>`. Under cl those are the SDK's and vcruntime's; under
cppparse `-I platform` comes first.

- (a) Add what is missing to `platform/`: the unwind structures and functions in `windows.h` (flat, as the draft of
  1.2), `intrin.h` with the five declarations, `eh.h` with `terminate`, `set_terminate`, `__uncaught_exception`.
- (b) Write the declarations into the runtime under `#ifdef __cppparse__`. One file, but two texts for one thing, and
  cl never checks ours against its own.

**Recommendation: (a)**, with a check that keeps (a) honest: a test prints the sizes and offsets of 1.2 from the
SDK's headers through cl and from `platform/` through cppparse, and they must be equal.

### 3.4 Who supplies C++ support under GRIDE's runtime

cppparse's objects use Microsoft's ABI for RTTI, the statics' guards and pure calls (1.1), and cl's model when
`--eh=cl` is asked.

| | (a) grtcxx, as for cpp11's objects | (b) cppparse's runtime grows its own (`__RTDynamicCast`, `__RTtypeid`, `__RTCastToVoid`, `_Init_thread_*`, `_purecall`, type_info's vftable, FH3 and `_CxxThrowException`) |
|---|---|---|
| Work in cppparse | none for the helpers; the library's names must not collide (1.4 a, f) | about a thousand lines that exist in grtcxx already, FH3 the largest; tests for each |
| Work in GRIDE3 | request G1: one function moved, the two throwers of `rtti.obj` cut loose from cpp11's `std` classes, and the rule behind both | none |
| One implementation of an ABI on the machine | yes | two, to be kept equal |
| Link line | grtstart, grtcxx, grtc, grtwin, kernel32 | grtstart, grtc, grtwin, kernel32 |
| Mixing cpp11's objects and cppparse's later | one RTTI, one set of guards, one terminate state | two of each |

**Recommendation: (a).** GRIDE's runtime was built to Microsoft's ABI so that a compiler emitting it links against
it; cppparse is such a compiler. What belongs to cppparse stays cppparse's: its exception models (the table and sjlj
personalities, `cppparse_*`), its C++ library (`include/`, libcpp, with `operator new` and `delete`, to which the
sized forms are added), and nothing else. The division by name is contract 4.4. The review agreed, and showed that
the division is not yet clean on grtcxx's side (1.4 f): until G1 is in, the link rests on COMDAT order, which 4.4
states as a contract and tests.

### 3.5 The linker and determinism

- Which linker: GRIDE's link is added beside Microsoft's in the scripts (a switch, section 4.6); nothing in the
  compiler knows a linker. Both stay tested as long as Microsoft's libraries are a configuration.
- Determinism without `/DEBUG`: `/timestamp:0` exists and suffices (1.3). The self-host passes it.
- With `/DEBUG` the image is a function of its objects and of two sets of paths (1.3): the PDB's own path, which
  the RSDS record holds and the GUID hashes, and the objects' paths, which the PDB holds as given. The second set
  is in selfhost.ps1's hands: it links each generation inside that generation's directory, the objects by relative
  paths, both exes under one file name. The first needs `/PDBALTPATH:%_PDB%` (the record names the PDB's file
  alone), which GRIDE's link accepts and ignores today: request G3, re-scoped from the first text's `/Brepro`,
  which asked for a hash that is already there. Comparing with the 17 bytes masked remains refused: a mask hides
  exactly the kind of difference the test is for.
- Until G3, level 2b is reachable by a detour, if it is wanted before (question 10): link both generations at one
  absolute path, one after the other, the objects moved into one directory for each link (r2, part 1: identical).
  The plan does not take the detour.
- Time stamp 0 is enough for identity. A symbol server keys an exe by its time stamp and image size, so two builds
  of equal size collide at 0; a stamp derived from the image's hash, which is what Microsoft's `/Brepro` writes,
  avoids that. Optional in G3; nothing here needs it.

### 3.6 The handlers of cl's objects (finding 1.4 d)

`clFrameHasState` compares a frame's handler with `__CxxFrameHandler3`, `__CxxFrameHandler4`,
`__GSHandlerCheck_EH` and `__GSHandlerCheck_EH4` to stop an exception that reaches a frame of cl's model. Under
GRIDE's runtime only the first exists.

- (a) `/ALTERNATENAME:<name>=<default>` from the runtime's source: where a library defines the name, that
  definition is taken; where none does, the name is the runtime's own `cppparse_no_handler`, a function no frame
  has as its handler. **Measured (probe n1) on both linkers and both runtimes**: the libraries' definitions on
  Microsoft's libraries, the default on GRIDE's. cl writes the directive from `#pragma comment(linker, ...)`;
  cppparse has the directive's path already (`;@drectve`, for the model mark) and needs the pragma.
- (b) A macro: the runtime built for GRIDE's runtime (`-D CPPPARSE_RT_FH3_ONLY`) names `__CxxFrameHandler3` only.
  Two objects of one source, chosen by the link line.
- (c) GRIDE3 adds the three names to grtcompat (its Runtime_Design 13.4 plans `__CxxFrameHandler4` there).

**Recommendation: (a)**, changed from (b) by the review's finding 12 and the probe it asked for. One object of the
runtime per compiler: half the self-host comparisons, no `-D` in the link function, and the crossing stop stays in
force under GRIDE's runtime for the frames that exist there (cpp11's and `--eh=cl`'s, both FH3). The cost is one
pragma in the preprocessor, limited to `/ALTERNATENAME` (4.5). (b) is the fallback; (c) would make (a)'s default
unused one day and needs nothing changed here.

### 3.7 The model mark and the runtime's object

Every object of cppparse carries `/FAILIFMISMATCH:"cppparse_eh=cpp"` or `=cl` (M7 stage 4), the runtime's too when
cppparse compiles it, so that object could not be linked into an `--eh=cl` program (1.2).

- (a) A new option `--eh=none`: no model, no mark, `try` and `throw` are errors. It leaves open what happens to
  automatic objects with destructors, which an exception would pass without a cleanup, and it is one more option
  to document.
- (b) The mark is written only into an object that has a construct for which the back end emits code or data of
  one model: a try region, a throw or rethrow, a cleanup, a checked exception specification, a funclet. An object
  without any is the same in the three models; the runtime is such an object, and so is any C-like unit.

**Recommendation: (b)**, the review's alternative (finding 11): no option, and the mark says what it means. The
runtime is compiled with an explicit `--eh=table` whatever the harness's `-Eh` says, so its object is one and the
same in every mode. Objects of today that have no such construct lose their directive; the self-host's identity
is between generations and is not touched.

### 3.8 What "no Visual Studio" checks

The suite cannot run there (1.6). Three ways to give level 3 an exit test that can be met:

- (a) Two parts. On the machine with Visual Studio, the compiler under test is the one built on GRIDE's link and
  runtime, and the whole suite runs with it as it runs today with the self-built compiler (`suite-self`): the test
  programs stay on Microsoft's libraries wherever cl compiles a driver. Without Visual Studio, a check set that
  names no Microsoft tool (`novs.ps1`): the self-host from a given first compiler, generations two and three
  identical; the suite's tests that only run the compiler (2, 3, 4, 7, 11, 12); and programs that cppparse compiles
  entirely, linked by GRIDE's link on GRIDE's runtime, each against an output stored in the repository. The stored
  outputs are made on the machine with Visual Studio from the same programs on Microsoft's libraries, and the
  harness there runs the set in both configurations, so a stored output cannot go stale unseen.
- (b) Restructure the suite: every driver compiled by cppparse in the GRIDE configuration, every oracle (cl's
  output, `cl /EP`, ml64's bytes, dumpbin's tables) stored. About 150 calls in 12 scripts, and the oracles lose
  their freshness: today a new cl is a new measurement.
- (c) (a) now, (b) as a row of its own if the user wants the suite itself free of Visual Studio.

**Recommendation: (c)**, put to the user as question 7. (a) tests what the goal names, a compiler that builds
itself and runs programs with no Microsoft tool; the suite's hundreds of comparisons against cl are a test of
the compiler's correctness, and that is established on the machine that has cl, for a compiler that is
byte-identical to the one the other machine builds (3.9).

### 3.9 The first compiler

On a machine without Visual Studio the chain is:

1. A first compiler at level 1 or later: GRIDE's own build of cppparse (cpp11 compiles the example project), once
   the example is refreshed to such a commit (request G6), or a released cppparse.exe pinned by its hash and its
   commit.
2. `novs.ps1 -Root <that exe>` first compiles the thread-local test of stage 1 with it and reads `.tls$` in the
   object, and refuses a first compiler that drops the attribute (an older one would build a runtime that is
   wrong in threads, without a message). Then it compiles this repository's sources and runtime: generation one.
3. Generation one compiles generation two, generation two generation three; two and three are compared, objects,
   listings and exes.

**The diverse double-compiling check** answers "a binary someone must trust" with a measurement, on the machine
that has both: generation two from the cpp11-built first compiler equals generation two from the cl-built one,
byte for byte, objects and exe, at -O0 and -O2 (harness check `ddc`, section 5). Two compilers that share no code
would have to carry the same fault for it to pass wrongly.

### Recommendation, together

Level 2a now, in selfhost.ps1; the scripts' other links follow as a stage of their own with a written list of
what differs under GRIDE's link. Level 1 by `__declspec(thread)`, five intrinsics and one pragma in the compiler
and three headers in `platform/`, the runtime's logic unchanged: a macro for the attribute, three directives, the
comment. One object of the runtime, without a model mark. Level 3 on grtcxx, grtc and grtwin, with requests G1
and G2 to GRIDE3 and the sized `delete` in libcpp; its test is the suite under the GRIDE-built compiler where
Visual Studio is, and a check set of its own where it is not, with the two first compilers proven equal. Level
2b by paths, after G3. Microsoft's linker and libraries remain a tested configuration throughout, and the default
until G5 is in and the user says otherwise.

## 4. Contracts

### 4.1 `__declspec(thread)`

- **Where.** On a variable of namespace scope, defined (internal or external linkage) or declared `extern`. On a
  block-scope static, a class's static member, a function, a type or a parameter: an error. A redeclaration that
  adds the attribute to a variable declared without it, or drops it, is an error.
- **What.** Any object type without a constructor or destructor to run, with no initialiser or a constant one (what
  Sema already places in `.data` without a dynamic initialiser). Otherwise an error. An initialiser may hold an
  address constant (the address of a function or of an ordinary static; an ADDR64 relocation inside `.tls$`, as cl
  writes it: the loader relocates the template before it copies it); stage 1 tests one against cl. The address of
  a thread-local is not an address constant: it cannot initialise a static, nor another thread-local.
- **Alignment.** Up to 16. A thread-local whose type asks for more is an error: neither the loader's block nor the
  template's start (grtwin's `.tls`, 16) promises more.
- **The object.** One section `.tls$`, characteristics 0xC0500040 (initialised data, read, write, 16-byte
  alignment), holding the initial values, zeros written out as cl writes them. A static thread-local is a static
  symbol in it, an external one an external symbol under its usual decorated name. The object names `_tls_index`
  as an undefined external once it has an access. It defines neither `_tls_index` nor `_tls_used` nor a directory.
- **The code.** A new IR operation yields a thread-local's address. Its value is the whole sum, the module's block
  plus the variable's offset, and is never split into parts that outlive the instruction sequence: the array at
  `gs:[58h]` can be moved by the loader when a module with thread-locals is loaded, a module's block cannot. The
  sum reads no memory the program can write and may be reused within a function, across calls too (a function
  runs in one thread). It lowers to cl's steps:

      mov  ecx, dword ptr [_tls_index]     ; REL32
      mov  rax, qword ptr gs:[58h]
      mov  rax, qword ptr [rax+rcx*8]
      mov  edx, SECTIONREL <variable>      ; SECREL, 32 bits, zero-extended
      add  rax, rdx

- **The listing.** Spelled so that ml64 assembles it to the same bytes (1.5): `qword ptr gs:[58h]`, `SECTIONREL`,
  the variables in `_TLS SEGMENT ALIGN(16) READ WRITE ALIAS(".tls$") 'DATA'`. The object writer's assembler encodes
  the two new forms and writes `IMAGE_REL_AMD64_SECREL` (it writes that relocation for CodeView already). ml64 is
  the second oracle beside cl: stage 1 assembles the test's listing with it and compares code bytes and
  relocations with `--obj`'s, as test 6 does for version 4.
- **The link.** `_tls_index` and `_tls_used` come from the C runtime underneath (libcmt's tlssup.obj, grtwin's
  tls.obj); the linker merges `.tls$` and enters the directory. A program linked with neither has an unresolved
  `_tls_index`, which is the right message.
- **Equal to cl's.** An object of cl that defines a thread-local and one of cppparse that declares it `extern`,
  and the reverse, share the variable.

### 4.2 The intrinsics

A function declared with C linkage, not defined, with one of these names and exactly these parameters is an
intrinsic when called by name; taking its address is an error.

| Declaration | Emitted | Result |
|---|---|---|
| `long _InterlockedIncrement(long volatile* p)` | `mov eax, 1` / `lock xadd [p], eax` / `inc eax` | the new value |
| `long _InterlockedDecrement(long volatile* p)` | `mov eax, -1` / `lock xadd [p], eax` / `dec eax` | the new value |
| `long _InterlockedExchange(long volatile* p, long v)` | `xchg [p], eax` (eax = v) | the old value |
| `long _InterlockedCompareExchange(long volatile* p, long x, long c)` | `mov eax, c` / `lock cmpxchg [p], x` | the old value |
| `void* _AddressOfReturnAddress(void)` | `lea rax, [rbp+8]` | where the return address of the function that contains the call lies |

To the optimiser the first four read and write `*p` and are barriers as a call is; the fifth is a value of the
frame. The declarations are `platform/intrin.h`; cl's `<intrin.h>` declares the same names with the same meaning.

`_AddressOfReturnAddress` binds its function in two ways, both by a flag on the IR function that IrGen sets where
it emits the operation:
- **The function keeps its frame**: `decideFrame` never makes it frameless, so `rbp` is its own and `[rbp+8]` is
  its return address at -O0 and at -O2.
- **The function is not inlined**, and the operation is never moved into another function: the value is about
  the function the source wrote it in (cl gives the enclosing function's slot after inlining, which is the reason
  the runtime's use needs the rule).

Its test does not rest on a cl stub: stage 2 calls it in a function that would be frameless without it and in
one with a frame, at -O0 and -O2, compiled by cppparse; the word at the returned address must be an address inside
the calling function (`RtlLookupFunctionEntry` gives the caller's entry) and the returned address plus 8 must be
the caller's stack pointer at the call, which the caller passes in; cl's build of the same source is the
reference. And the runtime's use is tested by what it is for: eh.ps1's `terminate.cpp` (an exception leaves a
destructor run by the unwinding) and a new sibling (the destructor throws and catches inside, and the program goes
on), in the table and sjlj models, with cppparse's object of the runtime.

### 4.3 The headers in `platform/`

- `windows.h` gains `DWORD64`, `ULONG_PTR`, `PVOID`, `M128A`, `CONTEXT` (`_CONTEXT`), `EXCEPTION_RECORD`
  (`_EXCEPTION_RECORD`), `RUNTIME_FUNCTION`, `DISPATCHER_CONTEXT`, `EXCEPTION_DISPOSITION`, `PEXCEPTION_ROUTINE`,
  `UNW_FLAG_NHANDLER`/`EHANDLER`/`UHANDLER`, `RtlCaptureContext`, `RtlLookupFunctionEntry`, `RtlVirtualUnwind`,
  `RtlRestoreContext`, `SwitchToThread`. The structures are written flat (the SDK's anonymous unions are not C++98):
  the same member names the runtime uses, the same offsets.
- `intrin.h`: the five declarations of 4.2. `eh.h`: `terminate`, `set_terminate`, `__uncaught_exception`, C linkage.
- The rule of `platform/` holds: only what cppparse's own sources and runtime use; strict C++98 but for
  `__cppparse_align` and the 64-bit typedefs.
- A test (`layout`) prints 12 sizes and offsets (those of 1.2 and the non-volatile registers' offsets) through cl
  with the SDK and through cppparse with `platform/`; the two outputs must be equal.
- **Checked, not only said.** strict98.ps1 gains the runtime as a unit (clang's C++98 front end with `-pedantic`,
  against `include\` and `platform\` as for the compiler's units), which also reads the three headers. The
  allowed diagnostics are listed there as today's are: `long long` at `platform\windows.h`'s 64-bit typedefs, and
  nothing for the thread-locals, which the runtime spells through a macro that is `__thread` for clang (4.5).

### 4.4 Who defines which name (level 3)

| Owner | Names | Note |
|---|---|---|
| cppparse's objects, `include/`, libcpp | everything of namespace `std` and of the library's headers (`std::exception`, `bad_alloc`, `bad_cast`, `bad_typeid`, `uncaught_exception`, `set_terminate`, `set_new_handler`, their RTTI), type_info's member functions, `operator new` and `delete` in every form including the sized ones (new), all replaceable | cppparse's `std` is not cpp11's: the two libraries' objects are not mixed in one program |
| `cppparse_rt` | the 26 `cppparse_` names: the table and sjlj models, the crossing stop, the three counters, the statics' lock of the old model, the walk for tests; `cppparse_no_handler` (3.6) | compiled by the compiler that compiled the program, or by cl |
| grtcxx | `__RTtypeid`, `__RTDynamicCast`, `__RTCastToVoid`, `_Init_thread_header`, `_Init_thread_footer`, `_Init_thread_abort`, `_purecall`, `??_7type_info@@6B@`, `terminate`, `set_terminate`, `__uncaught_exception`; for `--eh=cl`: `_CxxThrowException`, `__CxxFrameHandler3` | the rule of request G1: no member that defines one of these, and no member reached from one, defines a name of namespace `std` |
| grtstart, grtc, grtwin | `mainCRTStartup`, `__chkstk`, `_fltused`; the C library with `atexit`, `_setjmp`, `longjmp`; files, the allocator, `_tls_index`, `_tls_used` (tls.obj), `CreateThread` (grtwin's wrapper, which cppparse's worker threads then go through) | grtcompat is not on the line: it serves cl's objects (1.4 g) |
| kernel32.lib | the imports of 1.1 and 1.2 | request G2 for three of them |

ABI points that both sides must keep, each with the test that holds it:
- **Statics.** cppparse calls `_Init_thread_header(&guard)` and constructs when the guard then reads -1,
  `_Init_thread_footer` after, `_Init_thread_abort` when the constructor throws. vcruntime and grtcxx both take
  that protocol (Runtime_Design 8.2). Test: the check set's static-initialisation program, and eight threads racing
  for one static, on both runtimes.
- **RTTI.** Microsoft's x64 layout (`??_R0` to `??_R4`), which cppparse emits and grtcxx reads (8.3 there).
  cppparse's code tests a failed reference cast and a null `typeid` operand itself and throws its own library's
  `bad_cast` and `bad_typeid` (`__cppparse_throw_bad_cast`, `__cppparse_throw_bad_typeid`), so the runtime
  underneath never throws its own classes into cppparse's handlers: `__RTDynamicCast` is never called with
  `isReference` set, `__RTtypeid` never with a null object. Test: the check set's RTTI program on GRIDE's runtime
  (a failed reference cast and a null `typeid` operand caught as cppparse's classes); m8 passed its one
  `dynamic_cast` and `typeid`.
- **The `std` names both sides define (1.4 f), until G1 is in.** cppparse's definitions win because both are
  COMDATs and objects come before libraries. That is a reliance on the linker's order, so it is tested: the map of
  the RTTI program on GRIDE's runtime must show `??_7exception@std@@6B@`, `?what@exception@std@@UEBAPEBDXZ` and
  every other external name of `std` that grtcxx's badcast.obj also defines as taken from cppparse's objects
  (probe n2 is that test by hand; the vftables are static on both sides and are there twice, which the test
  allows and lists). After G1 the test is stronger and simpler: the map shows no member of grtcxx that defines a
  name of `std`.
- **Replaceable definitions.** A replaceable function's default stands unless another object of the program
  defines the name; no library is asked. The object writer spells that as a weak external of the search kind
  ALIAS, as since row G17, and the contract is the linker's: Microsoft's link keeps it, GRIDE's link must (request
  G7; today it searches the libraries for the name). Then `operator new` and `delete` in every form are libcpp's
  on every linker and runtime, and grtcxx's `operator_new.obj` and its `BadAlloc.obj` are not pulled. Test
  (`scripts\replace.ps1`, harness check `replace`): the default and its caller in one object and in two, in
  either order, against a library and against an object that define the name again, give one answer under both
  linkers; a program that allocates and sample-lib's newhandler, replacenew, replacepart and replacearray print
  what cl's builds print, and their maps give none of the eight names to a member of a library; failnew.ps1 under
  GRIDE's link (stage 0b). Until G7 the cases GRIDE's link does not hold are printed and counted (4.6), and
  `replace.ps1 -Strict` is G7's exit test. (The second text had the object writer change the kind to NOLIBRARY.
  Stage 0a measured that Microsoft's link honours NOLIBRARY only while no other object names the function:
  section 10, note of 2026-11-20.)
- **setjmp.** `_setjmp(buf, 0)` fills at most 256 bytes at 16-byte alignment, and `longjmp` on a buffer whose
  frame word is null restores the registers without unwinding. Test: the check set's sjlj programs; m8 passed.
- **terminate.** One state: the one of the C++ runtime underneath (`terminate`, `set_terminate`), as M7 stage 4
  set it for vcruntime. Test: the terminate programs' output and exit code on both runtimes.
- **Thread-locals.** 4.1. Test: the thread tests of stages 1 and 2 on both runtimes (stage 1's needs neither G1
  nor G2: a C-like program).

### 4.5 The runtime's source and its objects

- The text changes in four places, none of them logic:
  1. the header comment: what it is built with, "C++98 with three documented extensions";
  2. the attribute through one macro at the head of the file, `CPPPARSE_THREAD`: `__thread` for a compiler that
     defines `__GNUC__` (clang, for strict98.ps1), `__declspec(thread)` otherwise (cl and cppparse); the 13
     declarations use it. It lives in the runtime, not in `include\__config`: cl compiles the runtime against its
     own headers, and no header of the library has a thread-local;
  3. `DWORD64` for the `unsigned long long` of `cppparse_walk_frames` (the same type);
  4. `cppparse_no_handler` and three lines `#pragma comment(linker, "/alternatename:<name>=cppparse_no_handler")`
     for `__CxxFrameHandler4`, `__GSHandlerCheck_EH`, `__GSHandlerCheck_EH4` (3.6).
- **The pragma.** cppparse's preprocessor passes `#pragma comment(linker, "<text>")` on when the text is one
  `/alternatename:<a>=<b>` (case as cl takes it); the back end writes it into `.drectve`. Any other text is a
  warning and is dropped: what a source may tell the linker stays a short list.
- Built by cl as today (`cl /c /EHsc`), or by cppparse: `--compile6 -O2 --obj --msvc-mangling --eh=table -I include
  -I platform`. The object carries **no model mark**: the mark is written only into an object with a construct of
  one model (3.7), and the runtime has none. So one object of the runtime links with the cpp family and with
  `--eh=cl`, as cl's does today. Its functions have unwind data as every function of cppparse has.
- **One object per compiler**, for Microsoft's libraries and for GRIDE's runtime alike.

### 4.6 The scripts

- toolset.ps1 gains two settings, each from a parameter or an environment variable: the linker (`ms`, `gride`)
  and the runtime underneath (`ms`, `grt`), and finds GRIDE's installation (question 3). `grt` implies `gride`
  unless said otherwise (Microsoft's link may link GRIDE's runtime, as in the probes, for diagnosis). A third
  setting, `-NoVs`, sets no MSVC path, INCLUDE or LIB, defines neither `$cl` nor `$link`, and takes the compiler
  under test from `-Root`; a script that reaches for cl in that mode stops with "needs Microsoft's tools".
- One function links a program (`Link-Program`: objects, output, options) and one builds the runtime's object
  (`Build-Runtime`); the scripts call them instead of naming `$link` and `$cl`. Every `cl ... /Fe` becomes `cl /c`
  and `Link-Program` (31 in build-cppparse.ps1, the others in eh, ehx, mix, matrix, lib, life, failnew, cprog). The
  link lines:

  | Linker / runtime | Line |
  |---|---|
  | ms / ms | as today |
  | gride / ms | the same libraries, `/timestamp:0` |
  | gride / grt | the objects, libcpp's, the runtime's, grtstart, grtcxx, grtc, grtwin, GRIDE's kernel32.lib, `/nodefaultlib`, `/timestamp:0` |

- **What differs under GRIDE's link**, written down so that a difference is a decision and not a surprise; each is
  printed by the script as "not under GRIDE's link: <reason>" and counted in the summary:

  | Where | Under `-Linker gride` | Until |
  |---|---|---|
  | ehx.ps1, the mixed-family link that must fail with LNK2038 | runs with Microsoft's link only | G5 |
  | ehx.ps1, the `/INCREMENTAL` flavour (calls through thunks) | runs with Microsoft's link only: GRIDE's ignores the option, the case would test nothing | for good |
  | maps.ps1, mapcompare.ps1 | stay on Microsoft's link: they parse its map format and compare with cl's layout | for good |
  | test 18, CodeView lines read back through dbghelp | run on GRIDE's PDB too and reported; a difference goes to row M10, whose design owns the PDB | M10 |
  | programs with a cl-compiled driver on GRIDE's runtime | not linked: cl's objects need what GRIDE's runtime does not have (1.6) | GRIDE's Runtime_Design 13.4 |
  | libcpp's replaceable allocation functions (selfhost.ps1's map, replace.ps1's cases; stage 0a) | the C runtime's library supplies the ones a program does not define: GRIDE's link searches the libraries for a weak external of the alias kind (4.4). Printed case by case and counted; the programs that need libcpp's own (a `bad_alloc` through cppparse's models: newhandler, replacenew) do not run right | G7 |

  **As built and measured at stage 0b** (2026-11-24; GRIDE's link of GIT Test\GRIDE 4d4b834, SHA-256 bb43ca57...,
  in the GRIDE3 checkout at 5220ffd; section 10, note of 2026-11-24): four cases are printed (eh.ps1's two in each of
  its runs), all of them rows that stay on Microsoft's link: ehx.ps1's mixed-family link (G5: GRIDE's `layout.cpp` still reads `/FAILIFMISMATCH` and
  leaves it alone) and its `/DEBUG /INCREMENTAL` flavour, maps.ps1 and mapcompare.ps1. The other rows print
  nothing. Test 18 on GRIDE's PDB gives the lines and the record offsets Microsoft's gives, at -O0 and -O2: nothing
  for M10. The replaceable definitions hold under GRIDE's link since G7: replace.ps1's cases, sample-lib's
  newhandler, replacenew, replacepart and replacearray in test 15, and failnew.ps1 (60 runs in each of the table
  and cl models, every one "out of memory" and exit code 4, as under Microsoft's link). The row of a cl-compiled
  driver on GRIDE's runtime has nothing to print before the runtime setting exists (stage 4).

- **Where GRIDE is found** (toolset.ps1, question 3; built at stage 0a): the folder that holds `link.exe` and
  `lib\x86_64-windows`. First the environment variable `GRIDE_ROOT`, which names that folder (an installed GRIDE,
  or `GRIDEd\x64\Release` of a checkout) or the checkout above it; when it is set nothing else is looked at, so
  a wrong value is "not found" and never another GRIDE than the one named. Else the development tree, `My
  Documents\GRIDE3`. Else the location GRIDE's Setup wrote: `InstallDir` under `Software\G. R. Akhtar\GRIDE
  <version>\<outlook>` in the registry, the user's before the machine's, the highest version first.

- **novs.ps1** (3.8): the check set. `-Root <cppparse.exe>`, GRIDE's link and runtime, no Microsoft tool:
  1. the first compiler passes the thread-local probe (3.9);
  2. the self-host: generations one, two, three; two and three identical (objects, listings, the runtime's object,
     exes), at -O0 and -O2;
  3. the suite's compiler-only tests (2, 3, 4, 7, 11, 12 of build-cppparse.ps1, through a switch that runs those
     and names the rest "not run: needs Microsoft's tools");
  4. programs cppparse compiles entirely, output and exit code against `examples\novs\expected\`: m8's program
     (table and sjlj, -O0 and -O2); sample-eh's four terminate programs; the RTTI and static-initialisation
     programs of 4.4; the thread tests of stages 1 and 2; the unwinding-destructor pair of 4.2; a program over
     `<iostream>`, `<fstream>`, `<sstream>`, `<map>`, `<algorithm>` and number formatting. The list is fixed in
     novs.ps1 at stage 5b; a program enters it with its stored output.
  `novs.ps1 -Record`, on the machine with Visual Studio only, writes the expected outputs from the same programs
  linked ms / ms. The harness there runs the set ms / ms against the stored outputs, then gride / grt.
- **harness.ps1.**
  - New checks: `selfhost-gride`, `target`, `selfhost-grt`, `suite-self-grt`, `novs`, `ddc` (section 5). As built
    at stage 0a, `selfhost-gride` is `selfhost-gride-O0` and `selfhost-gride-O2`, each after the default self-host
    of its level (the objects of a level are gone once the next is compiled), with `suite-self-gride` and `replace`
    beside them (section 10, note of 2026-11-20).
  - A check that needs GRIDE prints "skipped: GRIDE not found" where it is not on the machine; the summary names
    every skipped check, and a skipped check is not a pass. The harness then exits 0 only without `-RequireGride`;
    the gate of this row's stages, and the lead's, runs with it, where a skip is a failure.
  - Every check that uses GRIDE logs what it used: the SHA-256 of link.exe and of each grt*.lib and kernel32.lib,
    and GRIDE3's commit where the installation is a checkout. Generations compared on GRIDE's runtime are equal
    only under the same binaries; the stored outputs of the check set do not depend on them.
  - `harness.ps1 -Eh cl` stays a gate at every stage, as M7 made it, for the checks of today, `target` and
    `selfhost-gride` (a linker's matter; Microsoft's libraries). The checks on GRIDE's runtime are printed "not
    run under -Eh cl" there and counted apart from the skips: the self-host in `--eh=cl` on grtcxx's FH3 is
    question 5, a row after this one.

### 4.7 What changes where

| Where | What |
|---|---|
| frontend (Pp, CxxParser, Sema) | `#pragma comment(linker, "/alternatename:...")` passed on (4.5); `__declspec(thread)` kept and checked (4.1); the intrinsics' declarations recognised (4.2) |
| middle (IrGen, Optimizer) | the thread-local address operation; the five intrinsic operations and their effects; the keeps-its-frame and not-inlined flag |
| backend (X86, Lower, Coff) | (not the replaceable definitions' search kind, which stays ALIAS: 4.4); the sequences of 4.1 and 4.2; `.tls$`; the `gs:` operand, the section-relative immediate, `lock xadd`, `lock cmpxchg`, `xchg` in the assembler; the directive of 4.5; the model mark only where a construct needs it (3.7) |
| driver | nothing (no new option) |
| `platform/` | windows.h grown, intrin.h and eh.h new |
| `runtime/` | the four places of 4.5 |
| libcpp, `include/` | sized `operator delete` and `delete[]`, replaceable as the others |
| sandbox/scripts | toolset.ps1's settings and two functions, selfhost.ps1, harness.ps1, strict98.ps1, target.ps1 and novs.ps1 new, replace.ps1 new (stage 0a); build-cppparse.ps1 and the test scripts through `Link-Program` |
| examples | `examples\novs\` (the check set's programs and stored outputs); `examples\sample-replace\` (replace.ps1's units, stage 0a) |
| README.md, docs | the extensions, the configurations, what differs under GRIDE's link, the bootstrap, gap-analysis item 8, row P1 |

## 5. Plan

Each stage ends with harness.ps1 and `harness.ps1 -Eh cl` passing in the default configuration, and with its own
exit test.

| Stage | Work | Exit test |
|---|---|---|
| 0a Level 2a: the self-host on GRIDE's link | the replaceable definitions' search kind (4.4), which changes every object that has one and so comes first; toolset.ps1's linker setting and `Link-Program`, used by selfhost.ps1 only; `selfhost.ps1 -Linker gride`; the harness's skip rules and the log of GRIDE's binaries | n3's five links give one answer under both linkers, and the GRIDE-linked compiler's map shows libcpp's `operator new`; cppparse-self.exe and cppparse-self2.exe byte-identical at -O0 and -O2 (check `selfhost-gride`); the whole suite passes with the GRIDE-linked generation one as the compiler under test, which catches a linker's fault in the compiler's own binary; with `ms` every result as before |
| 0b The scripts on `Link-Program` | every link of build-cppparse.ps1 and the test scripts through the function (`cl /c` plus the link); the list of 4.6 in the scripts | `build-cppparse.ps1 -Linker gride` and the test scripts give the results they give under Microsoft's link, but for the rows of 4.6's list, each printed and counted; with `ms` every result as before, byte for byte where a script compares bytes |
| 1 Thread-locals | 4.1 through the compiler; errors for what is refused | the listing of a test of five accesses assembles with ml64 to `--obj`'s code bytes and relocations, and its `.tls$`, symbols and relocations are cl's (dumpbin); eight threads, each counting in thread-locals (one with an address-constant initialiser), and 64 short-lived threads one after another print what cl's build prints, under both linkers **and on GRIDE's runtime**; a cl object and a cppparse object sharing a thread-local both ways; each refusal of 4.1 has a test; self-host identical |
| 2 Intrinsics, headers, the pragma, the mark | 4.2, 4.3, 4.5's pragma, 3.7; strict98.ps1 over the runtime | the layout test; eight threads incrementing one counter two million times each reach the sum; the `_AddressOfReturnAddress` test of 4.2, frameless candidate and framed, -O0 and -O2; the runtime compiles by cppparse at -O0 and -O2 without a message and its object has no model mark; n1's four links with cppparse's object in place of cl's; eh.ps1 (the terminate programs and the new sibling), ehx.ps1, mix.ps1, unwind.ps1, failnew.ps1 pass with cppparse's object of the runtime in place of cl's, `--eh=cl` among the models now that the mark allows it; eight threads throwing and catching at once in the three models pass with either object; ehx.ps1's mixed-family link still fails with LNK2038; strict98 passes with the runtime in it |
| 3 Level 1: the self-host owns its runtime | selfhost.ps1 compiles the runtime with the compiler under test in each generation and compares it; the comment; README, gap-analysis item 8 | selfhost.ps1 runs cl nowhere; generation two identical including the runtime's object and listing, -O0 and -O2, under both linkers; suite-self passes; a throw-and-catch benchmark with cppparse's object against cl's is reported (not a gate) |
| 4 Level 3, the link | sized delete in libcpp; the `grt` setting and link line | (with G1 and G2 delivered) cppparse-self.exe on GRIDE's runtime imports KERNEL32.dll alone, generations byte-identical, `Sema.cpp` and sample-opt compiled as by the cl-built compiler (check `selfhost-grt`); the thread-local test, the eight-thread throw test in the table and sjlj models and `--health -j` of that compiler pass on GRIDE's runtime; the map test of 4.4 |
| 5a Level 3 where Visual Studio is | `suite-self-grt`; the timing | the whole suite passes with the compiler built on GRIDE's link and runtime as the compiler under test; reported, not a gate: the median of five runs on an idle machine of the whole self-host compile, the same objects on Microsoft's libraries and on GRIDE's runtime; a slowdown goes to GRIDE3 as a finding with the numbers |
| 5b Level 3 with no Microsoft tool | toolset.ps1's `-NoVs`; novs.ps1, its programs and stored outputs; the `ddc` check; every difference between the two runtimes in the set's outputs becomes a fix here or a request to GRIDE3 with its exit test | `novs.ps1 -Root <the cl-built compiler>` passes in a shell without the MSVC paths (the mode defines neither `$cl` nor `$link`, so a script that reached for one would have stopped); the set passes ms / ms and gride / grt in the harness; (with G6 delivered) `novs.ps1 -Root <GRIDE's example cppparse.exe>` passes and `ddc` holds: generation two from that first compiler is byte-identical to generation two from the cl-built one; the same `novs.ps1` on a machine without Visual Studio (the user's check) |
| 6 Level 2b | selfhost.ps1 links each generation in its own directory, objects by relative paths, both under one file name, `/DEBUG /PDBALTPATH:%_PDB%` (after G3) | exe and PDB of the two generations byte-identical; gride-dbg loads the PDB of the exe |

Stages 0a and 1 to 3 are independent of GRIDE3's work, and 0b of all the others (it can run beside 1 to 3, in
another lane, since it touches scripts only). Stage 4 can be developed against the probes' bridges (Microsoft's
link with `/FORCE`, Microsoft's kernel32.lib) but passes only with G1 (its first part) and G2. Stage 5b's first
three exit tests need nothing more; its G6 part waits for GRIDE3. Stage 6 needs G3 and nothing else, so it may
come at any point after 0a.

**Where the stages stand.**

- **0a: done on 2026-11-20; its last exit test passed on 2026-11-24 with request G7** (GRIDE's link resolves the
  weak externals by their search kind as link.exe does, GIT Test\GRIDE 4d4b834: `replace.ps1 -Strict` holds 29 of
  29 cases in both models, and the GRIDE-linked compiler's `operator new` is libcpp's, iostream6.obj).
  The stage as measured on 2026-11-20: Measured on the stage's commit
  with cl 19.52.36725, GRIDE's link of 2026-11-05 (GRIDE3 5220ffd; the harness's logs hold its SHA-256), in the
  table model and, where the harness runs it, in cl's:
  - *The two exes.* `selfhost.ps1 -Linker gride`, the whole self-host under GRIDE's link (the GRIDE-linked
    generation one compiles generation two): at -O0 and at -O2 the 47 objects and listings and libcpp's are
    identical, and cppparse-self.exe and cppparse-self2.exe are byte-identical, 3,140,096 bytes at -O0 and
    9,135,616 at -O2 (Microsoft's link makes 3,096,576 and 9,071,104 of the same objects). The harness's checks
    `selfhost-gride-O0` and `selfhost-gride-O2` link the objects the default self-host left (`-Relink`, five
    seconds); at -O0 their exe was compared with the whole run's and is the same file, byte for byte: three links
    from three folders, one image.
  - *The suite under the GRIDE-linked generation one* (-O2): every test group passes (`suite-self-gride`).
  - *With `ms` every result as before.* The default self-host prints what it printed; the stage changes the
    compiler's output for no input (libcpp's object from the compiler before the stage and from the one after is
    one file); of the self-host's own objects one differs from the day before, Coff6.obj, by the line numbers
    below a comment three lines longer.
  - *The harness's skip rules.* With `GRIDE_ROOT` naming a folder that holds no GRIDE the four checks are
    `SKIPPED` by name, the harness says "every check that ran passed" and exits 0, and exits 1 with
    `-RequireGride`. Each check that uses GRIDE logs the folder, twelve SHA-256 and the commit.
  - **Open: "n3's five links give one answer under both linkers, and the GRIDE-linked compiler's map shows
    libcpp's `operator new`".** Not met, and not by a change in cppparse: the search kind the plan named does
    not hold under Microsoft's link (section 10, note of 2026-11-20). `replace.ps1`: Microsoft's link holds 27
    of 27 cases; GRIDE's link 9 of 29 in the table model and 11 of 28 in cl's, each of the others printed; the
    GRIDE-linked compiler's `operator new` is libcmt's `new_scalar.obj`, which the check names. It closes with
    G7, whose exit test is `replace.ps1 -Strict`.
- **1: done on 2026-11-24** (thread-locals, 4.1; `cppparse/target/TargetTls`, the IR operation `addrtls`,
  `scripts\target.ps1` and the harness check `target`, `examples\sample-target\`). Measured with cl 19.52.36725, ml64
  and dumpbin of the same toolset, GRIDE's link and runtime of GRIDE3 (the harness's log holds their SHA-256):
  - *The listing against ml64* (tls5.cpp: six functions, five of which reach five thread-locals defined there and
    one declared `extern`): at -O0 86 instructions, at -O2 80, each disassembling the same from ml64's object of
    the listing and from `--obj`'s, with the same 19 relocations at the same places (9 SECREL, 9 REL32 to
    `_tls_index`, one REL32 to a constant); the 27 instructions of the new forms (`gs:`, the load of `_tls_index`,
    `SECTIONREL`) are the same 180 bytes. The other instructions are compared as test 6 compares them, by their
    disassembly: the encoder writes `add r8, rdx` as `49 01 D0` where ml64 writes `4C 03 C2`, as it always has.
  - *`.tls$` against cl's object* (dumpbin's fields, read from both objects): the five variables of tls5.cpp are
    there with cl's storage classes (three external, two static) and cl's initial bytes, the one relocation is
    cl's (ADDR64 to `helper` at `g_fn`), `_tls_index` is undefined, neither `_tls_index` nor `_tls_used` is
    defined. Not equal, and so not compared: the order and the section's alignment. cl lays the five out as
    g_count, s_scale, g_fn, g_pair, s_hidden in 36 bytes with flags C0400040 (the contents' 8-byte alignment;
    C0300040 for one `int`, C0100040 for `char`s, which it also orders backwards); cppparse keeps the declaration
    order, as it does in `.data`, in 32 bytes with 4.1's C0500040.
  - *Threads* (tlsthreads.cpp): the 11 lines of cl's build (eight counters, the main thread's untouched copies, 64
    of 64 short-lived threads finding the template's values) at -O0 and -O2, under Microsoft's link, GRIDE's link
    on Microsoft's libraries and GRIDE's link on GRIDE's runtime (grtstart, grtcxx, grtc, grtwin, kernel32): six
    of six.
  - *Shared with cl's objects* (tlsdef.cpp, tlsuse.cpp): cl's definitions with cppparse's uses and the reverse
    print what the two cl objects print, under both linkers (cl's uses go through `__tls_guard`, as 1.5 says).
  - *The refusals*: 16 cases of tlsrefuse.cpp, each the error S164 with its own words and no object (4.1's list,
    and a typedef, a non-static member and an automatic local beside it).
  - *Under -Eh cl* target.ps1 passes as well (tls5.cpp is compiled with `--eh=table` there: ml64 stops at a
    listing of cl's model).
  - Two changes beside 4.1: a listing no longer names a variable it declares `EXTERN` `PUBLIC` as well (ml64
    refused the pair, A2005; the objects are the same), and the parser takes `__declspec` at the start of a
    parameter (so that S164 can say what it is).
  - *Self-host identical*: no source of the compiler has a thread-local, and its objects and listings stay what
    they were but for the units the stage changed; the gate of the stage's commit (`harness.ps1 -RequireGride` and
    `-Eh cl -RequireGride`, 22 checks each with `target`) has generation two identical at -O0, -O2 and with `-g`,
    under both linkers.
- **2: done on 2026-11-24** (4.2, 4.3, 4.5, 3.7: `cppparse/target/TargetIntrinsics` and the IR operation
  `intrinsic`, `platform/windows.h` grown, `platform/intrin.h` and `eh.h`, the pragma in `Pp`, the model mark,
  the runtime's four places, strict98 over the runtime; `target.ps1` parts 6 to 9, `Build-Runtime` in toolset.ps1
  with `CPPPARSE_RT=cppparse`, the harness check `eh-rtself`, `examples\sample-eh\dtor-catch.cpp`). Measured:
  - *The layout test* (layout.cpp): 26 sizes and offsets, cl with the SDK and cppparse with `platform\`, equal
    (`CONTEXT` 1232 with `Rip` at 248, `Xmm6` at 512, `VectorRegister` at 768, the eight non-volatile registers;
    `DISPATCHER_CONTEXT` 80 with `HandlerData` at 56; `EXCEPTION_RECORD` 152; `RUNTIME_FUNCTION` 12).
  - *Eight threads, two million increments each* (atomics.cpp): 16,000,000 at -O0 and -O2, under Microsoft's link
    and on GRIDE's runtime, with the compare-exchange lock never entered twice and the four single values cl's
    build prints; no call of an intrinsic in the listing. ml64 assembles the listing to the same disassembly, and
    its 12 `lock xadd`, `lock cmpxchg` and `xchg` instructions to the same bytes.
  - *`_AddressOfReturnAddress`* (retaddr.cpp): in the frameless candidate and in the framed function, at -O0 and
    -O2, the word at the address lies in the caller (`RtlLookupFunctionEntry` gives the caller's entry) and the
    address plus 8 is the stack pointer the caller captured, as in cl's build; the candidate keeps a frame (`push
    rbp`, `lea rax, [rbp+8]`). Not run on GRIDE's runtime: its kernel32.lib lacks `RtlCaptureContext` (G2).
  - *The runtime compiled by cppparse* at -O0 and -O2: no message, and its `.drectve` holds the three
    `/alternatename` directives and no model mark. Probe n1 with that object (alias.cpp): `__CxxFrameHandler4`,
    `__GSHandlerCheck_EH` and `__GSHandlerCheck_EH4` are the library's on Microsoft's libraries and
    `cppparse_no_handler` on GRIDE's runtime, under either linker (four links; on GRIDE's runtime Microsoft's
    kernel32.lib is added for the three `Rtl` functions, G2's bridge).
  - *eh.ps1 with cppparse's object of the runtime* (`CPPPARSE_RT=cppparse`, harness check `eh-rtself`): every
    check passes, 136 of them: the probes in the five model pairings at -O0 to -O3, the four terminate programs
    and the new sibling dtor-catch.cpp (a destructor run by the unwinding throws and catches inside; the program
    goes on) in the table, sjlj and cl models at -O0 and -O2, and ehx.ps1 with its `--eh=cl` crossings, where the
    mixed-family link still fails with LNK2038. The same with cl's object.
  - *mix.ps1 with cppparse's object* (`CPPPARSE_RT=cppparse`): the all-cppparse build and each of the 60 units as
    cppparse's object among cl's reference objects link and run (62 builds, exit 0). *failnew.ps1 with it*: in the
    table and cl models, 60 runs each with one of the run's 51,314 and 51,306 allocations failing end with `out of
    memory` and exit code 4. Neither is in the harness (mix.ps1 runs there with cl's object; failnew.ps1 takes ten
    minutes).
  - *Eight threads throwing and catching at once* (ehthreads.cpp): the table, sjlj and cl models at -O0 and -O2,
    each with cl's object and with cppparse's of the runtime: 12 of 12 print what cl's build prints.
  - *Refusals*: the address of an intrinsic is S165; a linker directive other than `/alternatename` is a warning and
    is dropped; a function of an intrinsic's family with other parameters (`_InterlockedOr`) is an ordinary call.
  - *strict98*: 61 units, the runtime among them, no diagnostic but `long long` at `include\__config`. The runtime
    compared a handler with a function's address through a cast of a function pointer to `const void*`, which
    `-pedantic` names; it now goes through an integer (`codeBehind` takes the handler's type).
  - *The model mark*: an object carries it when its code names a model entry of the runtime it does not define,
    or has a PROC with a handler (eh/EhModel.hpp `ehModelSymbol`); `unwind.ps1` is the library of test 6, which
    builds its runtime object itself (build-cppparse.ps1, the 0b lane's file) and was left alone.
- **0b: done on 2026-11-24.** Every program build-cppparse.ps1 and the test scripts link goes through
  `Link-Program`, or `Build-Program` (`cl /c`, then `Link-Program`) where a `cl ... /Fe` was: the compiler's own build,
  the drivers and cl's reference programs of tests 6 to 19, clang's references (matrix.ps1, test 17), eh.ps1,
  ehx.ps1, mix.ps1, matrix.ps1, lib.ps1, life.ps1, failnew.ps1, cprog.ps1, maps.ps1, mapcompare.ps1, replace.ps1's
  references, target.ps1's references and its part 9, build.ps1. Each takes `-Linker ms|gride` (build-cppparse.ps1
  now includes toolset.ps1). Tests 20 and 21, replace.ps1's cases and target.ps1's parts 3, 4, 6 and 8 keep their
  links by name: they link each program with both linkers, and on GRIDE's runtime, already. Measured on stage 2's
  tree (f13501b) with GRIDE's link of 4d4b834 (section 10, note of 2026-11-24):
  - `build-cppparse.ps1 -Linker gride`: the 426 lines of results are the ones Microsoft's link gives, word for word
    (cppparse.exe itself, every driver and every reference GRIDE-linked); no line "not under GRIDE's link".
    In cl's model (`CPPPARSE_EH=cl`, as `harness.ps1 -Eh cl` runs it) the same: 426 lines, none printed.
  - The test scripts with `-Linker gride` give the results they give under Microsoft's link: mix, cprog (A=B=C at
    both levels), matrix, lib and life at -O0 and -O2, eh with ehx, eh with cppparse's object of the runtime
    (`CPPPARSE_RT=cppparse`, the harness's eh-rtself), target, replace, failnew. Known differences, each printed and
    counted, all on Microsoft's link (4.6): ehx.ps1's mismatch link and its `/INCREMENTAL` flavour (two in each run
    of eh.ps1), maps.ps1 (1), mapcompare.ps1 (1). In cl's model the same results and the same differences.
  - With `ms` every result as before: the suite's 426 lines and every script's log are those of the tree before the
    stage, but for what the folder's name puts in them (anonymous namespaces' names in maps.ps1, the report's size).
- **3: done on 2026-11-24, on top of 0b; level 1 is reached** (selfhost.ps1 compiles the runtime with the compiler
  of each generation; test 6 of the suite builds its object of the runtime through `Build-Runtime`;
  `scripts\ehbench.ps1`; README, gap-analysis item 8). Measured:
  - *selfhost.ps1 runs cl nowhere*: the script names neither `` nor `cl.exe`; generation one is compiled by the
    cl-built compiler, its runtime object too, and every link goes through `Link-Program` (Microsoft's link, or
    GRIDE's with `-Linker gride`).
  - *Generation two identical, the runtime included*: the 60 units, libcpp's iostream and `runtime\cppparse_rt`
    (object and listing, compiled by each generation) are identical at -O0, -O2 and with `-g`, and the relinks
    under GRIDE's link give byte-identical exes (3,311,616 bytes at -O0); `suite-self` and `suite-self-gride`
    pass under compilers whose every object is cppparse's. That is the gate of the stage's commit
    (`harness.ps1 -RequireGride` and `-Eh cl -RequireGride`, 23 checks each).
  - *The suite with cppparse's object of the runtime* (`CPPPARSE_RT=cppparse`, test 6's object through
    `Build-Runtime`; 0b left it to this stage): every test group passes, as with cl's object. Not a harness check:
    the self-host's suites already run on compilers whose runtime is cppparse's.
  - *The throw path* (ehbench.ps1, reported, not a gate; an idle machine): a throw out of five frames with
    destructors, caught, rethrown and caught again, the median of five runs of 20,000: in the table model 3,820 ns
    with cl's object of the runtime and 3,922 ns with cppparse's (+3%); in the sjlj model 3,069 and 3,526 ns
    (+15%); under GRIDE's link 3,776 and 3,902, 3,077 and 3,460. The risk row's cost, measured; cl's object stays
    available.- **4 to 6: not begun.** Stage 4 needs requests G1 and G2 of GRIDE3 (section 9) to pass; it may be developed
  against the probes' bridges (Microsoft's link with `/FORCE`, Microsoft's kernel32.lib), and stage 2's retaddr.cpp
  is the first program that then runs on GRIDE's runtime.

## 6. Risks and estimate

| Risk | Weight | Held by |
|---|---|---|
| The new encodings (`gs:`, SECREL immediate, `lock` forms) are wrong in a case | medium | stage 1 compares with ml64's bytes and cl's tables, stage 2 with cl's disassembly; the thread tests |
| A thread-local's address reused where it must not be | low: a function runs in one thread; the sum is never split (4.1); the runtime has no coroutines or fibres | the operation's contract; the eight-thread tests at -O2 |
| `_AddressOfReturnAddress` names the wrong frame (frameless, inlined) | was open in the first text; closed by 4.2's two rules | stage 2's test in both kinds of function, and the unwinding-destructor programs |
| cppparse's object of the runtime is slower than cl's /O2 one (it is 26% larger) on the throw path | medium, a cost not a fault | stage 3's benchmark, reported; cl's object stays available |
| The walk through the runtime's own frames (`dispatch` holds two CONTEXTs, 2.5 KB of frame) depends on cppparse's unwind data for the runtime itself | low: m8 passed in both models at both levels | stage 2's suites |
| The self-host's identity breaks while the compiler learns the new forms | low | every stage ends with the harness |
| Under GRIDE's link nothing refuses a program of mixed model families | medium until G5: a wrong link is silent, the crossing stop sees only the frames it walks | the mismatch test stays on Microsoft's link (4.6); the default linker does not change before G5 (question 1); ehx.ps1's test under GRIDE's link is G5's exit test |
| A replaceable definition loses to a library under GRIDE's link (1.3): the C runtime's `operator new` in place of libcpp's | was unseen, then thought closed by a search kind, which Microsoft's link does not honour (4.4); open until G7, and no longer silent | selfhost.ps1 names the supplier of the GRIDE-linked compiler's `operator new` from the map, replace.ps1 every case, the harness counts them; the default linker does not change before G7 either (question 1) |
| The `std` names of grtcxx's badcast.obj under cppparse's (1.4 f): a change of link order or of COMDAT selection would put cpp11's 24-byte `std::exception` under cppparse's code | medium until G1's second part | the map test of 4.4 at stage 4 and in the check set; cppparse never reaches grtcxx's throwers (4.4) |
| GRIDE's runtime differs from Microsoft's in something a program prints (number formatting, wide characters, locale: GRIDE's has the "C" locale only; the exit code of `abort`) | medium to high | stage 5b exists for this; each difference is listed with its owner; a program that cannot pass is named in novs.ps1 with the reason, not left out silently |
| A stored output of the check set goes stale | low | the harness runs the set ms / ms against the stored outputs on every run where Visual Studio is (4.6) |
| The first compiler is older than level 1 and builds a runtime that is wrong in threads | low | novs.ps1's first step refuses it (3.9) |
| cpp11 cannot compile the sources of the day, so G6 or `ddc` waits | medium: cpp11 has built cppparse at an older commit; the new `target\` code is plain C++98 | `ddc` is a named skip until then; a released binary, pinned, is the other first compiler |
| G1, G2, G3, G5, G6 wait on GRIDE3's sessions | medium | stages 0a to 3 and 0b need none; stage 4 develops on the bridges; each request has its own exit test |
| grtwin's `CreateThread` wrapper and cppparse's worker threads (`--health -j`) | low: GRIDE's own exit test ran cppparse with workers 30 of 30; r7 ran `-j 4` | stage 4 runs it |
| Harness time: more configurations | medium | `selfhost-gride` only links (2 s); `selfhost-grt`, `suite-self-grt`, `novs` and `ddc` run in the full harness, not in `-Quick` |
| The first compiler on a clean machine is a binary someone must trust | accepted, and measured: `ddc` | question 2 |

Not measured in this design, and said so: thread-locals of cppparse's making, anywhere; the runtime's
thread-locals in more than one thread on GRIDE's runtime; `_AddressOfReturnAddress` as an intrinsic; `--eh=cl`
through cppparse's object of the runtime and on GRIDE's runtime; any program of the suite on GRIDE's runtime but
m8's; timing beyond one run of `Sema.cpp`; cpp11 on today's sources; the pragma's directive written by cppparse
(n1 used cl's object); the search kind NOLIBRARY written by cppparse (n4 patched the object; measured at stage
0a, and found wanting: section 10).

**Estimate.** The unit first. M7's design gave working days; its seven stage commits landed within four hours of
one afternoon (the M10 design's section 7 has the times), so those days were weights that ranked the stages, not
days of the calendar. This design counts in lanes from the start: one lane is one agent's session of the size of
an M7 stage, about an hour when it goes well, with the lead's review between lanes and the user's answers setting
the pace.

| Stage | First text | Now | What moved it |
|---|---|---|---|
| 0a | half (with 0b) | a quarter to a half | selfhost.ps1 only; suite-self under the GRIDE-linked compiler is a harness line; the replaceable definitions' search kind and its test |
| 0b | | one | about 50 links in a dozen scripts through one function (31 in build-cppparse.ps1), the list of differences, test 18 on GRIDE's PDB |
| 1 | one | one to one and a quarter | the ml64 oracle, the run on GRIDE's runtime, the 64 threads, the refusals |
| 2 | one | one to one and a half | the frame rule and its test, the pragma, the mark, strict98 over the runtime, `--eh=cl` in the runtime's suites |
| 3 | half | half | |
| 4 | half | half to three quarters | one object instead of two; the thread tests and the map test on GRIDE's runtime |
| 5a | (stage 5: one to two) | half | suite-self-grt and the timing |
| 5b | | one to one and a half | `-NoVs`, novs.ps1, its programs and outputs, `ddc`; the unknowns of the two runtimes' outputs are here |
| 6 | a quarter | a quarter | selfhost.ps1's paths; G3 is smaller |

Together **six to eight lanes** in cppparse (the first text: four and a half to six; the review, with the
suite restructured, six and a half to eight and a half). The suite's restructure of 3.8 (b) is not in it: as a row of
its own it would be two to three lanes more (question 7). In GRIDE3: G1 (two parts) and G2 one session; G3 a small
one (an option honoured and a test); G5 a small one; G6 one session, after stage 3 here, and it is the one that
may meet something in cpp11.

## 7. Questions for the user

1. **The default configuration.** Recommended: Microsoft's linker and libraries stay the harness's default until
   stage 5b has passed and G5 is in (GRIDE's link refuses mixed model families), with GRIDE's link and runtime as
   added checks; the default is reconsidered then. Or: switch the linker at stage 0b and the runtime at stage 4,
   accepting the open guard.
2. **The first compiler where there is no Visual Studio.** Recommended: GRIDE's own build of cppparse (cpp11
   compiles the example project, refreshed by request G6), which then compiles this repository's sources (3.9),
   proven equal to the cl-built line by the `ddc` check. Beside it, or where cpp11 cannot compile the sources of
   the day, a released cppparse.exe pinned by its SHA-256 and its commit in README.md: kept where?
3. **Where the scripts find GRIDE.** Recommended: an environment variable (`GRIDE_ROOT`), else the development
   tree `My Documents\GRIDE3\GRIDEd\x64\Release`, else the installed location Setup writes. Which should count?
4. **`__declspec(thread)` as a documented extension** (3.1), against slots and a runtime in strict C++98.
   Recommended: the extension, named in README.md beside `__declspec(align)` and the 64-bit integers, spelled
   through a macro in the runtime and checked by strict98.ps1.
5. **`--eh=cl` on GRIDE's runtime** (grtcxx's FH3 reading cppparse's tables): inside level 3, or a row after it?
   Recommended: a row after stage 5b; level 3's exit tests cover the table and sjlj models, and the harness names
   the GRIDE checks "not run under -Eh cl".
6. **cl's object of the runtime.** Recommended: kept and tested as long as programs mix cl's objects (mix.ps1,
   `--eh=cl` across cl's frames); it is the same source.
7. **What "no Visual Studio" must check** (3.8). Recommended: the check set (`novs.ps1`: the self-host through
   three generations, the compiler-only tests, a dozen programs against stored outputs) and the `ddc` check; the
   suite itself stays where cl is. Or: the suite restructured so that it runs without Microsoft's tools, its
   oracles stored: a row of its own, two to three lanes, and the oracles stop being measured afresh.
8. **Which GRIDE the checks accept.** Recommended: whichever `GRIDE_ROOT` names, with the hashes of link.exe and
   the libraries and GRIDE3's commit in the harness's log; no pinned version, since GRIDE moves and the generations
   are compared under one set of binaries. Or: a pinned set, the harness failing on another.
9. **The model mark** (3.7). Recommended: written only into objects that have a construct of one model, so the
   runtime's object and C-like units carry none; no `--eh=none`. Or: `--eh=none` as an option the help text and
   README name, with automatic objects that have destructors refused under it.
10. **Level 2b** (the self-host identical with `/DEBUG`, exe and PDB). It is cheap now: a quarter of a lane after
    G3, a small request. Recommended: keep it, since row M10 will make the self-host's PDB worth comparing; wait
    for G3 rather than take the one-path detour of 3.5. Or: drop it, or take the detour now.

## 8. Source layout

The rule: a subsystem's classes in a folder of their own, as `cppparse/eh/`.

    cppparse/target/               new: what the Windows x64 target adds to the language
        TargetTls.hpp, .cpp        thread-locals: the checks of 4.1, the address operation's lowering, the .tls$ records
        TargetIntrinsics.hpp, .cpp the table of 4.2: recognition, effects for the optimiser, emission
    platform/windows.h             grown (4.3)
    platform/intrin.h, eh.h        new
    runtime/cppparse_rt.cpp        the four places of 4.5; still one source, one object
    libcpp/                        sized operator delete beside the plain ones
    examples/novs/                 new: the check set's programs; expected\ holds their stored outputs
    sandbox/scripts/toolset.ps1    the linker and runtime settings, -NoVs, Link-Program, Build-Runtime
    sandbox/scripts/target.ps1     new: stages 1 and 2's tests (thread-locals, atomics, layout, the frame, threads)
    sandbox/scripts/novs.ps1       new: the check set (4.6), -Root, -Record
    sandbox/scripts/selfhost.ps1   the runtime by the compiler under test; -Linker, -Runtime, three generations for novs
    sandbox/scripts/strict98.ps1   the runtime as a unit
    sandbox/scripts/harness.ps1    checks selfhost-gride, target, selfhost-grt, suite-self-grt, novs, ddc; -RequireGride

The frontend, middle and backend call into `target/` at the points of 4.7 and keep no table of their own. The
new folder must be named in every list of source folders, and there are more than one: `$SourceDirs` in
toolset.ps1 **and** the separate `$SourceDirs` of build-cppparse.ps1 (line 38; cl's build of cppparse would miss
`target\*.cpp`), cppparse.vcxproj and its filters, strict98.ps1 through toolset's list, and outside this
repository GRIDE3's example (cppparse.pro and its vcxproj: request G6). Stage 1's exit includes cl's build and the
Visual Studio build of the solution, which is where a missed list shows. The tests' sources go where the suite's
are; no source moves.

**Effect on P19** (the default pipeline): none on its decision, and none of this needs it; the self-host uses
`--compile6` throughout. What this design adds for a later driver is the knowledge of a complete link line
without Microsoft (4.6): if a plain `cppparse x.cpp` is one day to write an exe, the driver takes that line from
the scripts, and that row then has a linker and a runtime it can count on being installed with GRIDE.

## 9. Requests to GRIDE3

GRIDE3 is another repository with its own sessions; nothing here was changed there, and nothing will be from
this row. Each request has the test that shows it done. Stages 0b and 1 to 3 need none of them; stage 0a's
scripts need none, and one of its exit tests waits for G7 (added 2026-11-20).

| # | Where | Request | Exit test | Needed by |
|---|---|---|---|---|
| G1 | RT, grtcxx (`cxx\exceptions.cpp`, `cxx\rtti.cpp`, `cxx\badcast.cpp`) | The rule: **no member of grtcxx that defines a C-level helper of the ABI (the list of 4.4), and no member reached from one, defines a name of namespace `std`**, so that another C++ library can stand on those helpers. Two places break it today. (1) `std::uncaught_exception` is in the object that holds `terminate`, `set_terminate`, `__uncaught_exception`, `__CxxFrameHandler3` and `_CxxThrowException`: it moves to an object of its own (or becomes a COMDAT any definition may replace). (2) `rtti.obj` names `__grt_throw_bad_cast` and `__grt_throw_bad_typeid`, which pulls `badcast.obj` and its 40 names of `std` in Microsoft's 24-byte layout: `rtti.obj` reaches the two throwers weakly (for example `/ALTERNATENAME` to a default that calls `terminate`; GRIDE's link honours it), and the throwers with their classes are pulled by cpp11's own objects or live in cpp11's library. `BadAlloc.obj` behind grtcxx's `operator new` is the same case and is covered by the rule; cppparse keeps it out of its programs by its own means (4.4, replaceable definitions) | A matrix, not one name: for each helper of 4.4, a program that names it, linked by GRIDE's link with an object that defines cppparse's `std` names (`std::exception` of 8 bytes, `bad_cast`, `bad_typeid`, `uncaught_exception`, under their decorated names): no "already defined", and the map shows no member of grtcxx that defines a name of `std`. cppparse's link of probe m5, part 3, gets past its message. cpp11's own programs still catch `std::bad_cast` from a failed reference cast and `std::bad_typeid` from a null operand | stage 4 (part 1); part 2 replaces the map test of 4.4 by the stronger one |
| G2 | RT, `kernel32.def` | add `RtlCaptureContext`, `RtlVirtualUnwind`, `RtlRestoreContext` (KERNEL32.dll exports them; `RtlLookupFunctionEntry` is listed already; Runtime_Design 8.4 already says the three belong there, so this brings the file in line with the design) | a C program `main` calls f1 calls f2 calls f3; f3 captures its context and walks three frames with the four functions; for each frame the entry's `BeginAddress` plus the image base equals the address of f3, f2, f1 in that order. It links against GRIDE's kernel32.lib and against Microsoft's and prints "f3 f2 f1" with both | stage 4 |
| G3 | LINK | `/PDBALTPATH:` honoured, at least `%_PDB%`: the RSDS record names the PDB as the option says (its file name alone), and the GUID, a hash of the image already, then no longer depends on the output directory. Optional beside it: `/Brepro` as a time stamp derived from the same hash, so that two builds do not share the stamp 0 on a symbol server | the same objects, given by relative paths, linked in two directories under one output name with `/DEBUG /timestamp:0 /PDBALTPATH:%_PDB%`: byte-identical exes and PDBs; gride-dbg loads the PDB lying beside the exe | stage 6 |
| G4 | RT, tests (no change to the runtime) | once stage 4 passes here: "cppparse built by cppparse" as an exit test of the runtime beside the cpp11-built one | that build's `--health .` output equals the one on Microsoft's CRT, as the cpp11-built one's does | after stage 5a |
| G5 | LINK | `/FAILIFMISMATCH` enforced: objects whose directives give different values for one key are refused, the message naming the key, both values and both objects (Microsoft's LNK2038 says the same) | ehx.ps1's link of an `--eh=cl` object beside an `--eh=table` one under `-Linker gride` fails with that message and writes no exe; the same set in one model links; an object without the directive links with either | the default linker (question 1); 4.6's first row |
| G6 | examples, `examples\projects\cppparse` | the example refreshed to a commit of this repository at level 1 or later (after stage 3 here): the sources with the folders `cppparse\eh\` and `cppparse\target\` in cppparse.pro and the vcxproj, and a line that records the commit. So that the example's compiler can build a program by itself, the four folders it then needs beside it, and no more: `include\`, `libcpp\`, `runtime\`, `platform\` | the example builds with cpp11 in GRIDE; `novs.ps1 -Root <that cppparse.exe>` passes in a checkout of this repository at the recorded commit; the `ddc` check holds: generation two from that compiler is byte-identical to generation two from the cl-built one | stage 5b's last two exit tests |

| G7 | LINK (`layout.cpp`, `take_module`) | A weak external of the search kind ALIAS (3) is resolved as link.exe resolves it: the libraries are not searched for its name; another object's definition takes it, and otherwise the default the record names stands. Today only NOLIBRARY (1) keeps the name out of the archive search (`nosearch`), so a library on the line that defines the name wins over the default, where link.exe keeps the default with the caller in the same object or in another, in any order of the inputs (stage 0a's measurement, section 10). cppparse writes `__declspec(replaceable)` that way, libcpp's eight allocation functions (row G17), and cannot write NOLIBRARY instead: link.exe honours that kind only while no other object names the function. To be checked on GRIDE's side: the C runtime's own alias members (a weak external whose default is another name, pulled through the archive's index) must still resolve | `sandbox\scripts\replace.ps1 -Strict` of this repository passes: 7 orders of a default, its caller, a library and an object that define the name again give under GRIDE's link what they give under Microsoft's, also on GRIDE's runtime; five programs over libcpp's allocation functions at -O0 and -O2 print what cl's builds print, and their maps give none of the eight names to a member of a library (today 20 of GRIDE's 29 cases do not hold). `selfhost.ps1 -Linker gride` then names libcpp's `operator new` for the GRIDE-linked compiler. GRIDE's own link tests pass as before | stage 0a's first exit test; stage 4 (grtcxx's `operator_new.obj` and `BadAlloc.obj` stay out); failnew.ps1 under GRIDE's link (0b); the default linker (question 1) |

Findings handed over without a request:
- Runtime_Design 8.1 names sized `delete` among grtcxx's allocation functions; the installed grtcxx.lib
  (2026-11-05) defines neither `??3@YAXPEAX_K@Z` nor `??_V@YAXPEAX_K@Z` (dumpbin /linkermember; the link of 1.4).
  cppparse will define its own (4.4), so it needs nothing; cl's objects with sized deallocation would.
- GRIDE's link has no `/FORCE`; the probes used Microsoft's link for the bridged links. Not needed once G1 is in.
- GRIDE's link accepts `/INCREMENTAL` and ignores it; a test that depends on incremental thunks tests nothing
  under it (4.6).
- GRIDE's link is deterministic on 49 objects and Microsoft's libraries with `/timestamp:0`, with and without
  `/DEBUG`, as long as the paths are the same (1.3).
- `/ALTERNATENAME` from an object's directives behaves as Microsoft's under GRIDE's link on both runtimes (n1).
- A weak external of search kind ALIAS (3): GRIDE's link searches the archives for its name and takes a
  library's definition; Microsoft's does not and keeps the default (n3: five links, the two linkers differ in
  the two with a library). The second text handed this over without a request, because cppparse would write
  NOLIBRARY, on which the two linkers agree in n4's one-object case. They do not agree in general (stage 0a),
  so it is request G7 above.

## 10. Review and changes

The independent review of commit 545b8a1 (2026-11-19; its text and its probes r1 to r9 are scratch files, see
the appendix) confirmed the direction and most of section 1 by measuring again: thread-locals in the compiler,
intrinsics in the compiler, flat declarations checked by a layout test, grtcxx rather than a second runtime;
level 2a; the self-built compiler on GRIDE's runtime. It found the design not ready for the user: one blocker,
six major findings, eleven minor ones and five notes. Every one was checked against the code or the probe it
cites before it was taken: GRIDE's `image.cpp`, `main.cpp`, `layout.cpp`, `rtti.cpp` and `kernel32.def`,
cppparse's `decideFrame` and its model mark, the runtime's use of the intrinsic, build-cppparse.ps1's calls,
GRIDE3's example folder, r1 to r3 and r6. All 23 hold. Twenty are taken as they stand or sharpened; three are
adapted (10, 12, 19), none is rejected. For finding 12 the author ran the probe the review asked for (n1,
appendix); its result changed 3.6's recommendation. While checking finding 5 the author linked m8's program
again with a map (n2), which corrected the test the review proposed (the vftables are static and are in the
image twice) and led to a fact neither text had: under GRIDE's link a library's definition takes the place of a
replaceable one, so libcpp's `operator new` would give way to the C runtime's (n3). The fix is cppparse's and
small (n4: the search kind NOLIBRARY), and it is the first work of stage 0a.

| # | Finding | Decision | Done |
|---|---|---|---|
| 1 blocker | level 3's exit test cannot be met: build-cppparse.ps1 is built on cl, ml64, dumpbin and link; toolset.ps1 fails without Visual Studio; cl's driver objects cannot stand on GRIDE's runtime | accepted, the review's two parts; the restructure put to the user | 1.6 states the facts; the goal says what it covers; level 3 is 3a (the suite under the GRIDE-built compiler, where Visual Studio is) and 3b (the check set `novs.ps1`, no Microsoft tool); options 3.8; contract 4.6 names the set's four parts and how its stored outputs are kept fresh; stage 5 split into 5a and 5b; question 7; the limits; the estimate |
| 2 major | the `/DEBUG` difference is the PDB's path, not a GUID drawn anew; G3 asks for a hash that exists | accepted | 1.3's table and text corrected, with r1 to r3; 3.5 rewritten; G3 is `/PDBALTPATH` (optional `/Brepro` stamp); level 2b and stage 6: one file name, own directories, relative object paths; the detour through one path named and not taken; question 10 |
| 3 major | GRIDE's link ignores `/FAILIFMISMATCH`: M7's model-family guard is lost under it | accepted | 1.3; a limit in section 2; request G5 with ehx.ps1's link as its exit test; 4.6's list keeps the mismatch test on Microsoft's link until then; question 1's switch waits for G5; a risk row; stage 2 asserts the guard still holds under Microsoft's link after the mark's change |
| 4 major | `_AddressOfReturnAddress` as `lea rax,[rbp+8]` is wrong in a frameless function; the probes' stub did not test the meaning | accepted, and extended | 1.2 says what the stub was; 3.2 and 4.2: the function keeps its frame, and (the author's addition) is not inlined; the test runs cppparse's own code in a frameless candidate and a framed function against cl's build, and the unwinding-destructor programs with cppparse's object of the runtime (stage 2, level 1's exit test); a risk row |
| 5 major | grtcxx's `rtti.obj` pulls `badcast.obj` and cpp11's `std::exception` family; the link works by COMDAT order; G1's rule did not catch it | accepted, and extended by probes n2 to n4 | 1.4 f rewritten as the answered case, with n2's map; `BadAlloc.obj` is kept out by the replaceable definitions' search kind (1.3, 4.4, stage 0a); 3.4; 4.4: the rule covers what a helper reaches, the COMDAT reliance is a contract with a map test, the two calls cppparse never makes are named; G1 has two parts and a matrix as its exit test; a risk row; stage 4's exit test |
| 6 major | the bootstrap is not closed: the first compiler must be at level 1; GRIDE3's example predates M7 and lacks four folders; nothing measures cpp11-built against cl-built | accepted | 1.6; 3.9: the chain in three steps, a first compiler that drops the attribute refused, the diverse double-compiling check; request G6; stage 5b's exit tests and the harness check `ddc`; question 2; two risk rows |
| 7 major | stage 0 is under-scoped: 31 links are `cl /Fe`; `/INCREMENTAL`, LNK2038, the map formats and dbghelp differ under GRIDE's link | accepted | stage 0a (selfhost.ps1, and the suite under the GRIDE-linked compiler) and 0b (every link through `Link-Program`); 4.6's table of what differs, each printed and counted; the estimate: a quarter and one lane |
| 8 minor | thread-locals on GRIDE's runtime were never run; "tlsdata.obj" is grtcxx's, the directory is grtwin's tls.obj | accepted | 1.2 and section 6 say it; stage 1 runs its thread tests on GRIDE's runtime too, with 64 short-lived threads; stage 4 adds the throw test and `--health -j`; the names corrected in 1.5, 4.1, 4.4 |
| 9 minor | the listing's spelling is not given; ml64 is a free second oracle | accepted | 1.5 (r5's forms and its two traps); 4.1 "The listing"; stage 1 compares ml64's bytes and relocations with `--obj`'s |
| 10 minor | the extension is documented but not checked; spell it `__cppparse_thread` in `__config`; `DWORD64`; strict98 over the runtime | adapted: the macro lives in the runtime | 4.3 and 4.5: strict98.ps1 takes the runtime as a unit, the macro is `__thread` for clang, `DWORD64` replaces the `unsigned long long`. Not in `__config`: cl compiles the runtime against its own headers without `-I include` (eh.ps1, selfhost.ps1), and no library header has a thread-local, so the macro stands at the head of the one file that uses it. Question 4 |
| 11 minor | `--eh=none` is underspecified; write the mark only where a construct needs it | accepted, the alternative | 3.7 (new); 4.5: no option, the runtime compiled with an explicit `--eh=table` and unmarked; 4.7's driver row; question 9. Checked in X86.cpp: a function without states is plain in the three models (`m_cl` and `m_tableEh` are cleared for it) |
| 12 minor | 3.6 chose two objects without measuring a weak reference | adapted: `/ALTERNATENAME` measured instead of a NOLIBRARY weak external | probe n1: four links, the libraries' handlers on Microsoft's libraries, the default on GRIDE's runtime, both linkers. 3.6's recommendation is now (a), one object; 4.5's pragma, limited to that directive; 1.4 d; `CPPPARSE_RT_FH3_ONLY` is gone from 4.5, 4.6, the plan. `/ALTERNATENAME` rather than a weak external because cl writes it from the same source line, so the runtime stays one text |
| 13 minor | gaps in the thread-local contract | accepted, all four | 4.1: alignment above 16 refused; address constants in an initialiser allowed and tested against cl; a redeclaration that adds or drops the attribute is an error; the operation's value is the whole sum, never split |
| 14 minor | G2's exit test is not objective | accepted | G2: frames identified by function through `BeginAddress`, on both kernel32.libs; the note on Runtime_Design 8.4 |
| 15 minor | level 3's speed measured once on a busy machine | accepted | 1.4 says what the figure is worth; stage 5a reports the median of five idle runs, a slowdown goes to GRIDE3; not a gate |
| 16 minor | the reason for "level 3 after level 1" is half wrong; grtcompat dropped without a word | accepted | section 2's sentence; 1.4 g; 4.4 and 4.6 say why grtcompat is off the line |
| 17 minor | skips and `-Eh cl` in the harness | accepted | 4.6: a skip is named, is not a pass, and fails under `-RequireGride`, which the stages' gate uses; `-Eh cl` is a gate at every stage (section 5's first line); the GRIDE-runtime checks are "not run under -Eh cl" |
| 18 minor | the new folder needs more lists | accepted | section 8: both `$SourceDirs`, the vcxproj and filters, GRIDE3's example (G6); stage 1's exit includes cl's and Visual Studio's build |
| 19 note | probe outputs not kept; the scratchpad can be cleared | adapted | the probes are copied to the lane's durable scratch folder and the appendix points there; n1 keeps its output; m4 to m8 still print to the console, and the appendix says which of them the review ran again. They stay outside the repository, as M7's and M10's probes do |
| 20 note | small inaccuracies | accepted, all four | 1.5 "five instructions"; 1.3; the tls.obj name; 3.1's row on cl's programs |
| 21 note | terminate state per thread | accepted as a note | a limit in section 2: later, not this row |
| 22 note | time stamp 0 and symbol servers | accepted | 3.5's last point; optional in G3 |
| 23 note | four questions missing | accepted, all four | questions 7 (what "no Visual Studio" checks), 8 (which GRIDE), 9 (the mark), 10 (level 2b) |

What changed in the recommendation: one object of the runtime instead of two (`/ALTERNATENAME`, not a macro); no
`--eh=none`, the model mark only where a construct needs it; level 3's test is the suite under the GRIDE-built
compiler plus a check set without Microsoft's tools, not "the whole suite without Visual Studio"; level 2b by
paths and `/PDBALTPATH`, not by a new hash; the first compiler is a chain with a measurement (`ddc`) instead of
an open question; stage 0 is two stages, and begins with the replaceable definitions' search kind, so that
libcpp's `operator new` stays libcpp's under GRIDE's link; six requests to GRIDE3 instead of four, G1 wider and
G3 smaller.

What did not change: thread-locals as cl emits them, the intrinsics in the compiler, the declarations in
`platform/` with the layout test, grtcxx under cppparse's objects, the order of the levels, Microsoft's linker
and libraries as a tested configuration and the default.

### Notes from the stages

Where a stage measures something other than this text says, the text is corrected in its place with a pointer
here, and the note says what was measured.

**2026-11-20, stage 0a.**

1. **The replaceable definitions' search kind: NOLIBRARY is not a fix (1.3, 4.4, the first work of stage 0a).**
   Probe n4 patched the object of n3, in which the default and its only caller are one object, and both linkers
   then kept the default. Stage 0a had cppparse write the kind (the one constant of Coff.cpp) and measured what
   n4 left out, libcpp's own case: the caller in another object (probe w1, appendix; the cases are now
   `scripts\replace.ps1`). Whose definition is linked:

   | Inputs, in their order | ALIAS, Microsoft's | ALIAS, GRIDE's | NOLIBRARY, Microsoft's | NOLIBRARY, GRIDE's |
   |---|---|---|---|---|
   | one object (default and caller), a library that defines the name | default | **library** | default | default |
   | one object, a second definition as an object | object | object | object | object |
   | caller, default, library | default | **library** | **library** | default |
   | default, caller, library | default | **library** | **LNK2001, no exe** | default |
   | library, caller, default | default | **library** | **library** | default |
   | caller, default, a second definition as an object (either order) | object | object | object | object |
   | libcpp: a program that only allocates; newhandler | links, libcpp's `operator new` | links, **libcmt's**; newhandler ends in the runtime's crossing stop (0xC0000409) | **LNK2019**: `operator delete` unresolved from libcpp's own defaults and from libcmt's sized forms | libcpp's, runs |
   | libcpp: replacenew (the program defines two of the eight) | runs | those it leaves are **libcmt's**; ends 0xC0000409 | links, ends **0xC0000409** | runs |
   | libcpp: replacearray | libcpp's `operator new` | **libcmt's** | **libcmt's** | libcpp's |

   Microsoft's link honours NOLIBRARY only while no other object names the function: a plain reference from
   another object sends it to the libraries after all, or leaves the name unresolved, by the objects' order.
   Every unit that says `new` is such an object. So one kind is right under each linker and neither under both:
   ALIAS under Microsoft's, NOLIBRARY under GRIDE's. The record has one field; a second weak record for the same
   name, or a kind chosen by a compiler option that names the linker, would be workarounds (3.5: nothing in the
   compiler knows a linker). The format's own words do not settle it (NOLIBRARY: "no library search for the
   name"; ALIAS: "the name is an alias for" the default); what settles it is that link.exe is the linker GRIDE's
   link follows elsewhere and the one cppparse's objects must keep linking under, and link.exe resolves ALIAS to
   "the default unless an object defines it" in every case measured. So the difference is GRIDE's link's to
   close: **request G7** (section 9), with `replace.ps1 -Strict` as its exit test.
   The constant stays 3, the object writer's comment says why, and no object of cppparse changes its bytes for
   this stage's sake. Until G7: a program linked by GRIDE's link has the C runtime's allocation functions
   wherever it defines none; the GRIDE-linked compiler runs and passes the suite that way (m5 ran so, unseen),
   and what it loses is the `bad_alloc` through its own frames ("cppparse: out of memory", failnew.ps1), since
   libcmt's `operator new` throws in cl's model. Stage 0a's first exit test stays open on it, in sight: the
   checks print and count each case.
   One more thing the cases showed, in cl's model only (`harness.ps1 -Eh cl`): GRIDE's link refuses newhandler
   at -O2 with "symbol in a section that was left out: ??_7bad_alloc@std@@6B@" (libcmt's `operator new` brings
   the C runtime's `bad_alloc` beside libcpp's). It has the same root and is to be looked at again once G7 is in.
2. **GRIDE's map** (1.3 said "its own format"). The lines of the public names have link.exe's form, so one
   pattern reads either linker's map for "which object supplies this name"; that is all the scripts use.
3. **The harness's checks.** 4.6 named one check, `selfhost-gride`, that "only links". Built: two,
   `selfhost-gride-O0` and `selfhost-gride-O2`, each right after the default self-host of its level, because the
   objects of a level are overwritten by the next; each is `selfhost.ps1 -Linker gride -Relink`, which takes
   the default run's objects of both generations (their level is checked by a stamp the compile leaves,
   `selfhost-level.txt`, and their identity again), links each set and compares the exes. The whole self-host
   under GRIDE's link (`selfhost.ps1 -Linker gride`: the GRIDE-linked compiler compiles generation two, in
   folders of its own) is the stage's exit test and runs by hand; in the harness it would cost what the default
   self-host costs, a second time. The linker's fault in the compiler's own binary, which the whole run would
   also catch, is `suite-self-gride`'s to catch. That check runs before `suite-self`, so that the checks after
   the two find in `out\` what `suite-self` leaves, as before. `replace` is the fourth, for note 1. Together
   they add about five minutes to the harness.
4. **Skips.** A script that needs GRIDE and does not find it prints "skipped: GRIDE not found" and exits with
   77; the harness reports `SKIPPED`, also for a check that builds on a skipped one (`suite-self-gride`), names
   them in the summary and says "every check that ran passed", never "all checks passed". `-RequireGride`
   (harness.ps1, passed on by gate.ps1) makes a skip a failure. When `GRIDE_ROOT` is set and wrong, GRIDE is not
   found: the variable is never a hint that falls back to another GRIDE (question 8: whichever it names).
5. **What of 4.6 is not built yet**, as the plan has it: the runtime setting (`grt`), `Build-Runtime`, `-NoVs`
   and the "not run under -Eh cl" rule belong to stages 4 and 5; the scripts other than selfhost.ps1 name
   `$link` until 0b. Under `-Eh cl` the four new checks run as the others do (Microsoft's libraries).

**2026-11-24, stage 0b.**

1. **What toolset.ps1 gives the scripts.** `Link-Program` as 4.6 has it, with two optional parameters the plan
   did not name: `-Libraries`, for the links whose objects name their C runtime themselves (cl `/MD` in ehx.ps1's
   MD flavour, `/MTd` in the Debug build: an empty list, so that no `libcmt` stands beside `msvcrt` or `libcmtd`),
   and `-With ms`, which keeps a link of 4.6's list on Microsoft's link whatever `-Linker` says. `Build-Program`
   is what a `cl ... /Fe` was: `cl /c` with the same options, then `Link-Program` of the objects `/Fo` names (an
   object's name where the folder has a space, as test 20 already did). `Assert-Linker` prints the GRIDE used, or
   "skipped: GRIDE not found" and exit 77; `Write-NotUnderGride` prints a known difference and counts it, and a
   script's last line carries the count. build-cppparse.ps1 includes toolset.ps1 after its own set-up and keeps the
   `-Exe` it was given; it prints the count above "build and all tests succeeded". harness.ps1 is unchanged: it runs
   every script with the default, Microsoft's link, and the counts on its lines stay at zero there.
2. **Where the text departs.** The links of tests 20 and 21 are not moved: each of their programs is linked by
   Microsoft's link and by GRIDE's by name and the two compared (M10), which `-Linker` would only duplicate. The
   compiler's own build (test 1's set-up) is linked by `Link-Program` too, so `-Linker gride` tests a GRIDE-linked
   cl-built compiler, which stage 0a's checks did not have. clang's references (matrix.ps1, covariant-vbase.cpp in
   test 17) are compiled with `-c` and linked by `Link-Program` as well, and build.ps1 (VectorTest, MatrixTest),
   which the text did not list. A link with Microsoft's static C runtime now names its libraries
   (`libucrt`, `libvcruntime`, `libcmt`, `kernel32`, `legacy_stdio_definitions`) and `/SUBSYSTEM:CONSOLE`, where
   `cl /Fe` and eh.ps1's and ehx.ps1's links took them from the objects' directives; the programs' outputs are the
   same (the ms runs above). That is also what cppparse's object of the runtime needs, which has no `/DEFAULTLIB`
   (stage 2). target.ps1 (stages 1 and 2, written beside this stage) links its programs by Microsoft's link,
   GRIDE's and GRIDE's runtime by name; only its cl references and part 9 (eight threads throwing, Microsoft's link
   until now) follow `-Linker`. Left as it was: test 6 of build-cppparse.ps1 compiles its object of the runtime with
   cl itself, not with `Build-Runtime`, so `CPPPARSE_RT=cppparse` does not reach the suite (stage 2 left the file to
   this stage; it is stage 3's to decide, which owns the runtime's object in the self-host).
3. **GRIDE's link on cl's objects.** Every reference program cl compiles (sample-lib's 21, front, front2, copies,
   alttok, ucn, MatrixTest, matrix, the terminate programs, ehx's three cl flavours, cprog's C and C++ builds, mix's
   reference of the compiler's own sources, dbgcheck, dbgtypes, dbgvars and diavars with the DIA SDK's libraries
   and `/LIBPATH`), the `/MD` flavour through `/DEFAULTLIB` alone, and the Debug build of cppparse (`/MTd`,
   `/DEBUG`, its PDB; built, and `--version` run, not the suite) link under GRIDE's link and run as under
   Microsoft's. No new request to GRIDE3. Request G8 (newhandler at -O2 in cl's model) did not show: since G7 no
   program here pulls libcmt's `throw_bad_alloc.obj`, so test 15's newhandler and replace.ps1's pass in cl's model
   under GRIDE's link; the request stays open for the case it names.

**2026-11-24, stages 1 to 3.** Where the stages depart from the text (the measurements are in "Where the stages
stand"):

1. **`.tls$` against cl's.** 4.1's characteristics (C0500040) and the declaration order are kept; cl aligns the
   section by its contents and orders the variables its own way, so stage 1 compares `.tls$` symbol by symbol
   (storage class, initial bytes, relocations), not byte for byte. A static data member with the attribute is
   refused, as 4.1 says, though cl takes it.
2. **The ml64 oracle compares disassembly, the new forms byte for byte.** The encoder has always written some
   register forms with the other opcode (`01 /r` where ml64 writes `03 /r`); test 6 compares disassembly for that
   reason, and stages 1 and 2 do the same, with the bytes of every `gs:`, `SECTIONREL`, `lock` and `xchg`
   instruction compared exactly.
3. **The model mark's criterion** (3.7): an object is marked when its code names a model entry of the runtime that
   it does not define, or has a PROC with a handler. A listing no longer declares an `EXTERN` variable `PUBLIC` too
   (ml64 refused the pair).
4. **retaddr.cpp is not run on GRIDE's runtime** until G2 (its kernel32.lib lacks `RtlCaptureContext`); probe n1
   with cppparse's object adds Microsoft's kernel32.lib there, the probes' bridge.
5. **`unwind.ps1`** of 4.2's list is the library test 6 reads unwind tables with, not a check of its own; the
   runtime's unwinding is checked by eh.ps1's unwind walk with either object.
6. **Test 6's object of the runtime** goes through `Build-Runtime` (0b left the choice to stage 3): the default stays
   cl's, as question 6 recommends for programs that mix cl's objects, and `CPPPARSE_RT=cppparse` now reaches the
   suite. "Runs cl nowhere" is selfhost.ps1's claim; the suite keeps cl as its oracle (1.6).
7. **The harness** gains `target` (stage 1) and `eh-rtself` (stage 2): 23 checks. mix.ps1 and failnew.ps1 with
   cppparse's object, and ehbench.ps1, are run by hand and recorded, not in the harness.

## Appendix: the probes

Scratch files, not in the repository. The design's own are in
`C:\Users\GRA\.claude\projects\D--GRA-My-Documents-GIT-Test-cpp\lane-scratch\2026-11-19\p1-probes` (copied there
from the session's scratchpad, which may be cleared), the review's in `...\2026-11-19\p1review` beside its text
(review.md). Each script runs plainly from PowerShell; all take the sources from the design's worktree and the
cl-built compiler from `sandbox\cppparse.exe`. m2, m3 and n1 to n4 keep their output beside them; m4 to m8 print to the
console, and of those the review ran m8 again (r8.txt) and checked m7's result (r7.txt).

| Script | What it does | Feeds |
|---|---|---|
| m1.ps1 | compiles the 47 own units and libcpp at -O0 into `obj0\`, cl compiles the runtime, Microsoft's link with `/MAP`; dumpbin /symbols of every object | 1.1, 1.2 |
| m2.ps1 (output m2.txt) | the undefined externals of cppparse's objects and of cl's runtime object, each with its provider from the map; the map's publics by library | 1.1, 1.2 |
| m3.ps1 (output m3.txt) | cppparse on `runtime\cppparse_rt.cpp` as it is; `tls\t.cpp` (five functions over `__declspec(thread)` variables) through cl /Od and /O2 with disassembly, sections, relocations | 1.2, 1.5 |
| m4.ps1, `rt2\unwind_draft.h` | the runtime with `<eh.h>` and `<intrin.h>` replaced by the draft header, compiled by cppparse at -O0 and -O2; the layout printed by cl from the SDK and by cppparse from the draft | 1.2, 4.3 |
| m5.ps1 | the 49 objects linked twice by Microsoft's link (plain, `/Brepro`) and by GRIDE's (plain, `/timestamp:0`, `/DEBUG /timestamp:0`) with byte comparison; GRIDE's runtime under both linkers; the runtime compiled by cppparse (`lnk\rt36.obj`, intrinsics from `lnk\stub.c`) in the self-built compiler | 1.2, 1.3, 1.4 |
| m6.ps1 | every name of 1.1 and 1.2 and the back end's other names looked up in GRIDE's six libraries; the names both sides define | 1.4 |
| m7.ps1 | the bridged link on GRIDE's runtime (sized delete from `lnk\sized.cpp`, `/FORCE`, Microsoft's kernel32.lib for three imports); the result compiles sample-opt and `Sema.cpp`, compared with the cl-built compiler's objects, timed | 1.4 |
| m8.ps1, `eh\t.cpp` | a throwing program in the three models at -O0 and -O2, linked with cl's object of the runtime, with cppparse's, and with cppparse's on GRIDE's runtime; outputs compared | 1.2, 4.4, 6 |
| n1.ps1 (output n1.txt), `weak\w.c` | after the review: an object that names the three handlers GRIDE's runtime lacks with `/ALTERNATENAME` to a function of its own, linked by both linkers against Microsoft's libraries and against GRIDE's runtime, and run | 1.3, 3.6, 4.5 |

| n2.ps1 (output n2.txt) | m8's table program on GRIDE's runtime linked again with a map: which object supplies each name both sides define, and which members of grtcxx are in the image | 1.4 f, 4.4 |
| n3.ps1 (output n3.txt), `repl\` | a `__declspec(replaceable)` function in a cppparse object and a second definition in a library, then in an object: which each linker takes | 1.3, 4.4 |
| n4.ps1 (output n4.txt) | n3's object with the weak external's search kind patched from ALIAS to NOLIBRARY, the same five links | 1.3, 4.4 |
| w1.ps1 (stage 0a, in `...\2026-11-20\p1-0a`; outputs w1-before.txt, w1-after.txt) | n3's links and the same with the default and its caller in two objects, in three orders; libcpp's allocation functions under five programs with the maps; once with the compiler as it was (ALIAS) and once with the kind written as NOLIBRARY. Its cases are `sandbox\scripts\replace.ps1` now | section 10's note of 2026-11-20, G7 |

The review's probes, quoted in the text as r1 to r9: r1 (where m5's two `/DEBUG` exes differ, with the RSDS record
in text), r2 (GRIDE's link with `/DEBUG` on one path and in two directories; without `/DEBUG` under two names), r3
(the same with relative object paths), r4 (cl's thread-local forms again), r5 (ml64 on those forms), r6 (every
member of grtcxx.lib: the `std` names it defines, its COMDATs, its undefined names), r7 (m7's exe: imports,
sample-opt, `--health -j 4`), r8 (m8 again), r9 (the SDK's sizes and offsets by cl).

*2026-11-24, request G7 done:* GRIDE's link (commit 7741ce0, merged as 4d4b834 into GIT Test\GRIDE main) resolves
a weak external of every search kind as link.exe does, measured on 45 cases written byte by byte (16 differed
before, none after): ALIAS is never searched in the libraries and serves plain references too, NOLIBRARY is searched
only when a plain reference came first, the default of NOLIBRARY and LIBRARY serves only references through the weak
symbol, and a real definition that comes after a taken default replaces it; GRIDE's librarian indexes ALIAS members
as lib.exe does. GRIDE's link tests (12 of 12, the runtime's entry test 36 of 36) and 126 links of cl's objects of
sample-lib against libcmt and libcpmt are unchanged. In cppparse: `replace.ps1 -Strict` 29 of 29 in both models
(9 and 11 before), `selfhost.ps1 -Linker gride` names libcpp's `operator new`. Found on the way, request G8 (open):
GRIDE's link refuses newhandler at -O2 in cl's model when libcmt's throw_bad_alloc.obj is pulled in ("symbol in a
section that was left out: ??_7bad_alloc@std@@6B@"): cppparse's vftable is a static in a COMDAT that /OPT:ICF folds,
and fix_up looks the name up among the resolved externals (libcmt's, dropped by /OPT:REF) before the kept sections.
