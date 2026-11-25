// main.cpp - the driver of the cross-compiler exception sample (see ehx.h), always compiled by cl /EHsc.
//   ehx cross <path>    one of the seven paths between the sides, with three thrown types:
//                       0 A throws, A catches               1 A throws, B catches       2 B throws, A catches
//                       3 A throws, through B's frame, A catches                        4 B throws, through A's, B catches
//                       5 A throws, B's catch (...) rethrows, A catches                 6 B throws, A's rethrows, B catches
//   ehx throw           every kind of thrown type of each side's throwers (throw.cpp) against cl's handlers here:
//                       which handler takes it, the value it sees, the copies and destructions
//   ehx unwind          main.cpp throws through each side's frames that own objects (unwind.cpp) and catches: what
//                       was destroyed, in which order
//   ehx catch           each side's handlers (catch.cpp) for the other side's throws, and their shapes: nested,
//                       in loops, left by return, goto and a new exception
//   ehx state           std::uncaught_exception in a destructor of each side while main.cpp's exception unwinds it;
//                       the standard exception classes of each side's library caught by the other, by reference
//                       (EHX_ONE_LIBRARY: both sides with cppparse's library, then by value too)
//   ehx terminate A|B   that side installs a terminate handler, the other side throws and nothing catches: the
//                       handler must run (it prints a line and exits with 7)
// One line per case. What a build must print depends on its models (ehx.ps1).
#include "ehx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_copies = 0, g_dtors = 0, g_guards = 0, g_log = 0;
Guard::~Guard() { g_guards += id; g_log = g_log * 10 + id % 10; }

int g_value = 0;
void note(int v) { g_log = g_log * 10 + v % 10; }
__declspec(noinline) double scale(double v) { return v * 1.5; }
static void reset() { g_copies = 0; g_dtors = 0; g_guards = 0; g_log = 0; g_value = 0; }

// ---- cross ---------------------------------------------------------------------------------------------------
void A_crossThrow(int); void A_crossMiddle(void (*)(int), int); int A_crossCatch(void (*)(int), int); int A_crossRethrow(void (*)(int), int);
void B_crossThrow(int); void B_crossMiddle(void (*)(int), int); int B_crossCatch(void (*)(int), int); int B_crossRethrow(void (*)(int), int);
static void viaB(int k) { B_crossMiddle(A_crossThrow, k); }
static void viaA(int k) { A_crossMiddle(B_crossThrow, k); }
static void rethrowB(int k) { B_crossRethrow(A_crossThrow, k); }
static void rethrowA(int k) { A_crossRethrow(B_crossThrow, k); }
static void groupCross(int path)
{
    for (int k = 0; k < 3; ++k) {
        int r = -99;
        reset();
        switch (path) {
        case 0: r = A_crossCatch(A_crossThrow, k); break;
        case 1: r = B_crossCatch(A_crossThrow, k); break;
        case 2: r = A_crossCatch(B_crossThrow, k); break;
        case 3: r = A_crossCatch(viaB, k); break;
        case 4: r = B_crossCatch(viaA, k); break;
        case 5: r = A_crossCatch(rethrowB, k); break;
        case 6: r = B_crossCatch(rethrowA, k); break;
        }
        printf("cross %d kind %d: r=%d guards=%d order=%d copies=%d dtors=%d\n", path, k, r, g_guards, g_log, g_copies, g_dtors);
    }
}

// ---- throw ---------------------------------------------------------------------------------------------------
int A_throwKinds(); void A_thrower(int kind); void A_rethrower();
int B_throwKinds(); void B_thrower(int kind); void B_rethrower();

static int catchExact(void (*f)(int), int k)
{
    try { f(k); }
    catch (int v) { g_value = v; return 1; }
    catch (double v) { g_value = static_cast<int>(v * 10); return 2; }
    catch (Colour v) { g_value = v; return 3; }
    catch (unsigned long v) { g_value = static_cast<int>(v); return 4; }
    catch (char* p) { g_value = p[0]; return 5; }
    catch (const char* p) { g_value = p[1]; return 6; }
    catch (int* p) { g_value = *p; return 7; }
    catch (const int* p) { g_value = *p + 1; return 8; }
    catch (Plain& p) { g_value = p.a + p.b; return 9; }
    catch (DD& d) { g_value = d.code * 100 + d.d; return 10; }
    catch (VD& v) { g_value = v.code * 100 + v.v; return 11; }
    catch (Derived& d) { g_value = d.code * 100 + d.o; return 12; }
    catch (Single& x) { g_value = x.code * 100 + x.s; return 22; }
    catch (Single* p) { g_value = p->code + p->s; return 23; }
    catch (Err& e) { g_value = e.code * 10 + e.kind(); return 13; }
    catch (DD* p) { g_value = p->code + p->d; return 14; }
    catch (VD* p) { g_value = p->code + p->v + p->kind(); return 15; }
    catch (Derived* p) { g_value = p->code + p->other(); return 16; }
    catch (const Derived* p) { g_value = p->code + 1; return 17; }
    catch (Hidden&) { return 18; }
    catch (Twice& t) { g_value = t.t; return 19; }
    catch (int (*fp)(int)) { g_value = fp(21); return 20; }
    catch (...) { return 99; }
    return 0;
}
static int catchBase(void (*f)(int), int k)
{
    try { f(k); }
    catch (const Err& e) { g_value = e.code * 10 + e.kind(); return 1; }
    catch (const Other& o) { g_value = o.other(); return 2; }
    catch (const Err* p) { g_value = p->code * 10 + p->kind(); return 3; }
    catch (const Other* p) { g_value = p->other(); return 4; }
    catch (const void* p) { g_value = p != 0; return 5; }
    catch (const volatile int& v) { g_value = v; return 6; }
    catch (...) { return 99; }
    return 0;
}
static int catchValue(void (*f)(int), int k)
{
    try { f(k); }
    catch (Other o) { g_value = o.o; return 1; }
    catch (Err e) { g_value = e.code * 10 + e.kind(); return 2; }
    catch (Plain p) { g_value = p.a; return 3; }
    catch (void* p) { g_value = p != 0; return 4; }
    catch (const volatile char* p) { g_value = p[2]; return 5; }
    catch (const volatile void* p) { g_value = p != 0; return 6; }
    catch (...) { return 99; }
    return 0;
}
// a handler rethrows through the side's "throw;"; the outer handler must get the same object
static int catchRethrown(void (*f)(int), void (*again)(), int k)
{
    const Err* first = 0;
    try {
        try { f(k); }
        catch (Err& e) { first = &e; again(); }
        catch (int) { again(); }
    }
    catch (Err& e) { g_value = e.code; return &e == first ? 1 : 2; }
    catch (int v) { g_value = v; return 3; }
    catch (...) { return 99; }
    return 0;
}
static void groupThrow(const char* side, int kinds, void (*f)(int), void (*again)())
{
    for (int k = 0; k < kinds; ++k) {
        reset(); int a = catchExact(f, k); int av = g_value, ac = g_copies, ad = g_dtors;
        reset(); int b = catchBase(f, k); int bv = g_value, bc = g_copies, bd = g_dtors;
        reset(); int c = catchValue(f, k); int cv = g_value, cc = g_copies, cd = g_dtors;
        reset(); int d = catchRethrown(f, again, k);
        printf("throw %s %2d: exact %d/%d c%d d%d  base %d/%d c%d d%d  value %d/%d c%d d%d  rethrown %d/%d c%d d%d\n",
               side, k, a, av, ac, ad, b, bv, bc, bd, c, cv, cc, cd, d, g_value, g_copies, g_dtors);
    }
}

// ---- unwind --------------------------------------------------------------------------------------------------
int A_unwindPlain(void (*)(int), int); int A_unwindTemps(void (*)(int), int); int A_unwindArrays(void (*)(int), int); int A_unwindCtors(void (*)(int), int);
int A_unwindFreed(); int A_unwindStatic(void (*)(int)); double A_unwindDoubles(void (*)(int), double); int A_unwindLoop(void (*)(int), Guard, int);
int B_unwindPlain(void (*)(int), int); int B_unwindTemps(void (*)(int), int); int B_unwindArrays(void (*)(int), int); int B_unwindCtors(void (*)(int), int);
int B_unwindFreed(); int B_unwindStatic(void (*)(int)); double B_unwindDoubles(void (*)(int), double); int B_unwindLoop(void (*)(int), Guard, int);

static void throwInt(int v) { throw v + 1; }
static void noThrow(int) {}
static int through(int (*fn)(void (*)(int), int), void (*f)(int), int k)
{
    int r = -1;
    reset();
    try { r = fn(f, k); } catch (int v) { r = 100 + v; }
    return r;
}
static void groupUnwind(const char* side, int (*plain)(void (*)(int), int), int (*temps)(void (*)(int), int), int (*arrays)(void (*)(int), int),
                        int (*ctors)(void (*)(int), int), int (*freed)(), int (*statics)(void (*)(int)), double (*doubles)(void (*)(int), double),
                        int (*loop)(void (*)(int), Guard, int))
{
    int r = through(plain, throwInt, 0);
    printf("unwind %s plain: r=%d guards=%d order=%d", side, r, g_guards, g_log);
    r = through(plain, noThrow, 0);
    printf(" | no throw r=%d guards=%d order=%d\n", r, g_guards, g_log);
    for (int k = 0; k < 4; ++k) { r = through(temps, throwInt, k); printf("unwind %s temps %d: r=%d order=%d", side, k, r, g_log); r = through(temps, noThrow, k); printf(" | no throw r=%d order=%d\n", r, g_log); }
    for (int k = 0; k < 2; ++k) { r = through(arrays, throwInt, k); printf("unwind %s arrays %d: r=%d order=%d\n", side, k, r, g_log); }
    for (int k = 0; k < 2; ++k) { r = through(ctors, throwInt, k); printf("unwind %s ctors %d: r=%d guards=%d order=%d freed=%d\n", side, k, r, g_guards, g_log, freed()); }
    {
        int first = -1, second = -1, third = -1;
        reset();
        try { first = statics(throwInt); } catch (int v) { first = 100 + v; }
        const int guardsFirst = g_guards;
        try { second = statics(noThrow); third = statics(throwInt); } catch (int v) { third = 100 + v; }
        printf("unwind %s static: %d %d %d guards=%d then %d\n", side, first, second, third, guardsFirst, g_guards);
    }
    {
        // doubles of this frame around the call: the registers they live in must come back from the unwinding
        double keep = scale(3.0), other = scale(5.0), got = -1.0;
        reset();
        try { got = doubles(throwInt, 2.0); } catch (int v) { got = 100.0 + v; }
        const int thrown = g_log;
        reset();
        const double quiet = doubles(noThrow, 2.0);
        printf("unwind %s doubles: %.2f order=%d | no throw %.2f order=%d | kept %.2f %.2f\n", side, got, thrown, quiet, g_log, keep, other);
    }
    {
        int got = -1;
        reset();
        try { got = loop(throwInt, Guard(9), 3); } catch (int v) { got = 100 + v; }
        printf("unwind %s loop: r=%d guards=%d order=%d", side, got, g_guards, g_log);
        reset();
        got = loop(noThrow, Guard(9), 3);
        printf(" | no throw r=%d guards=%d order=%d\n", got, g_guards, g_log);
    }
}

// ---- catch ---------------------------------------------------------------------------------------------------
int A_catchExact(void (*)(int), int); int A_catchBase(void (*)(int), int); int A_catchValue(void (*)(int), int); int A_catchUnnamed(void (*)(int), int);
int A_catchRethrown(void (*)(int), void (*)(), int); int A_catchLeaving(void (*)(int)); int A_catchInHandler(void (*)(int)); int A_catchTwoLive(int);
int A_catchLoop(void (*)(int), int); int A_catchReturn(void (*)(int), int); Plain A_catchResult(void (*)(int)); int A_catchFunctionTry(void (*)(int)); void A_throwBomb(int);
int B_catchExact(void (*)(int), int); int B_catchBase(void (*)(int), int); int B_catchValue(void (*)(int), int); int B_catchUnnamed(void (*)(int), int);
int B_catchRethrown(void (*)(int), void (*)(), int); int B_catchLeaving(void (*)(int)); int B_catchInHandler(void (*)(int)); int B_catchTwoLive(int);
int B_catchLoop(void (*)(int), int); int B_catchReturn(void (*)(int), int); Plain B_catchResult(void (*)(int)); int B_catchFunctionTry(void (*)(int)); void B_throwBomb(int);

struct CatchSide {
    const char* name;
    int (*exact)(void (*)(int), int); int (*base)(void (*)(int), int); int (*value)(void (*)(int), int); int (*unnamed)(void (*)(int), int);
    int (*rethrown)(void (*)(int), void (*)(), int); int (*leaving)(void (*)(int)); int (*inHandler)(void (*)(int)); int (*twoLive)(int);
    int (*loop)(void (*)(int), int); int (*ret)(void (*)(int), int); Plain (*result)(void (*)(int)); int (*functionTry)(void (*)(int));
};
// 'side' catches what the other side's throwers (thrower, again, bomb) and this file's throw
static void groupCatch(const CatchSide& side, int kinds, void (*thrower)(int), void (*again)(), void (*bomb)(int))
{
    for (int k = 0; k < kinds; ++k) {
        reset(); int a = side.exact(thrower, k); int av = g_value, ac = g_copies, ad = g_dtors;
        reset(); int b = side.base(thrower, k); int bv = g_value, bc = g_copies, bd = g_dtors;
        reset(); int c = side.value(thrower, k); int cv = g_value, cc = g_copies, cd = g_dtors;
        reset(); int u = side.unnamed(thrower, k); int uc = g_copies, ud = g_dtors;
        reset(); int d = side.rethrown(thrower, again, k);
        printf("catch %s %2d: exact %d/%d c%d d%d  base %d/%d c%d d%d  value %d/%d c%d d%d  unnamed %d c%d d%d  rethrown %d/%d c%d d%d\n",
               side.name, k, a, av, ac, ad, b, bv, bc, bd, c, cv, cc, cd, u, uc, ud, d, g_value, g_copies, g_dtors);
    }
    int r;
    reset(); r = side.leaving(throwInt); printf("catch %s leaving: r=%d guards=%d order=%d\n", side.name, r, g_guards, g_log);
    reset(); r = side.inHandler(throwInt); printf("catch %s in handler: r=%d guards=%d order=%d\n", side.name, r, g_guards, g_log);
    reset(); r = side.twoLive(0); printf("catch %s two alive: r=%d order=%d\n", side.name, r, g_log);
    reset(); r = side.loop(throwInt, 9); printf("catch %s loop: r=%d guards=%d\n", side.name, r, g_guards);
    for (int k = 0; k < 3; ++k) { reset(); r = side.ret(k < 2 ? throwInt : noThrow, k); printf("catch %s return %d: r=%d guards=%d order=%d\n", side.name, k, r, g_guards, g_log); }
    for (int k = 0; k < 2; ++k) { reset(); r = side.ret(throwInt, 2 + k); printf("catch %s fall through %d: r=%d guards=%d order=%d\n", side.name, k, r, g_guards, g_log); }
    { reset(); Plain p = side.result(throwInt); Plain q = side.result(noThrow); printf("catch %s result: %d %d | %d %d\n", side.name, p.a, p.b, q.a, q.b); }
    reset(); r = side.functionTry(throwInt); printf("catch %s function try: r=%d order=%d", side.name, r, g_log);
    reset(); r = side.functionTry(noThrow); printf(" | no throw r=%d order=%d\n", r, g_log);
    reset(); r = side.exact(bomb, 0); printf("catch %s copy that throws: %d/%d\n", side.name, r, g_value);
}

// ---- state ---------------------------------------------------------------------------------------------------
int A_stateUncaught(void (*)(int)); void A_stateSetTerminate(void (*)()); void A_stateThrowStd(int); int A_stateCatchStd(void (*)(int), int, char*, int);
int B_stateUncaught(void (*)(int)); void B_stateSetTerminate(void (*)()); void B_stateThrowStd(int); int B_stateCatchStd(void (*)(int), int, char*, int);
#ifdef EHX_ONE_LIBRARY
int A_stateCatchStdValue(void (*)(int), int, char*, int);
int B_stateCatchStdValue(void (*)(int), int, char*, int);
#endif
static void stateStd(const char* name, int (*catcher)(void (*)(int), int, char*, int), void (*thrower)(int))
{
    for (int k = 0; k < 3; ++k) {
        char text[40];
        const int r = catcher(thrower, k, text, sizeof text);
        printf("state %s %d: handler %d '%s'\n", name, k, r, text);
    }
}
static void groupState()
{
    int during, quiet;
    reset(); try { A_stateUncaught(throwInt); } catch (int) {} during = g_log; reset(); A_stateUncaught(noThrow); quiet = g_log;
    printf("state uncaught A: %d while unwinding, %d without\n", during, quiet);
    reset(); try { B_stateUncaught(throwInt); } catch (int) {} during = g_log; reset(); B_stateUncaught(noThrow); quiet = g_log;
    printf("state uncaught B: %d while unwinding, %d without\n", during, quiet);
    stateStd("A catches A", A_stateCatchStd, A_stateThrowStd);
    stateStd("A catches B", A_stateCatchStd, B_stateThrowStd);
    stateStd("B catches A", B_stateCatchStd, A_stateThrowStd);
    stateStd("B catches B", B_stateCatchStd, B_stateThrowStd);
#ifdef EHX_ONE_LIBRARY
    stateStd("A catches B by value", A_stateCatchStdValue, B_stateThrowStd);
    stateStd("B catches A by value", B_stateCatchStdValue, A_stateThrowStd);
#endif
}
static void terminateHandler()
{
    printf("terminate handler\n");
    exit(7);
}

int main(int argc, char** argv)
{
    setvbuf(stdout, 0, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "cross") == 0) {
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') groupCross(atoi(argv[++i]));
            else for (int p = 0; p < 7; ++p) groupCross(p);
        }
        if (strcmp(argv[i], "throw") == 0) {
            groupThrow("A", A_throwKinds(), A_thrower, A_rethrower);
            groupThrow("B", B_throwKinds(), B_thrower, B_rethrower);
        }
        if (strcmp(argv[i], "state") == 0) groupState();
        if (strcmp(argv[i], "terminate") == 0 && i + 1 < argc) {
            // (an exception that leaves main: the C runtime calls terminate)
            if (argv[++i][0] == 'A') { A_stateSetTerminate(terminateHandler); B_stateThrowStd(0); }
            else { B_stateSetTerminate(terminateHandler); A_stateThrowStd(0); }
        }
        if (strcmp(argv[i], "unwind") == 0) {
            groupUnwind("A", A_unwindPlain, A_unwindTemps, A_unwindArrays, A_unwindCtors, A_unwindFreed, A_unwindStatic, A_unwindDoubles, A_unwindLoop);
            groupUnwind("B", B_unwindPlain, B_unwindTemps, B_unwindArrays, B_unwindCtors, B_unwindFreed, B_unwindStatic, B_unwindDoubles, B_unwindLoop);
        }
        if (strcmp(argv[i], "catch") == 0) {
            const CatchSide a = { "A", A_catchExact, A_catchBase, A_catchValue, A_catchUnnamed, A_catchRethrown, A_catchLeaving, A_catchInHandler, A_catchTwoLive, A_catchLoop, A_catchReturn, A_catchResult, A_catchFunctionTry };
            const CatchSide b = { "B", B_catchExact, B_catchBase, B_catchValue, B_catchUnnamed, B_catchRethrown, B_catchLeaving, B_catchInHandler, B_catchTwoLive, B_catchLoop, B_catchReturn, B_catchResult, B_catchFunctionTry };
            groupCatch(a, B_throwKinds(), B_thrower, B_rethrower, B_throwBomb);
            groupCatch(b, A_throwKinds(), A_thrower, A_rethrower, A_throwBomb);
        }
    }
    printf("done\n");
    return 0;
}
