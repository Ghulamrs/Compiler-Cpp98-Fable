// throw.cpp - side SIDE's throwers: every kind of thrown type, from frames that own no object (see ehx.h).
#include "ehx.h"

static int g_int = 5;
static char g_text[] = "side";
static int twice(int v) { return v * 2; }

int FN(throwKinds)() { return 27; }

void FN(thrower)(int kind)
{
    static Derived derived(21);
    static DD dd;
    static Single single;
    switch (kind) {
    case 0: throw 42;
    case 1: throw 2.5;
    case 2: throw Green;
    case 3: throw "literal";
    case 4: { const char* p = g_text; throw p; }
    case 5: { char* p = g_text; throw p; }
    case 6: throw &g_int;
    case 7: { const int* p = &g_int; throw p; }
    case 8: throw Plain();
    case 9: throw Err(11);
    case 10: throw Derived(12);
    case 11: throw VD();
    case 12: throw DD();
    case 13: throw &derived;
    case 14: { const Derived* p = &derived; throw p; }
    case 15: { VD* p = &dd; throw p; }                  // the object is a DD: its Err lies where DD's vbtable says
    case 16: throw &dd;
    case 17: throw Hidden();
    case 18: throw Twice();
    case 19: throw &twice;
    case 20: { static const Err e(13); throw e; }
    case 21: { static Err e(14); Err& r = e; throw r; }
    case 22: throw static_cast<unsigned long>(77);
    case 23: { volatile int v = 9; throw v; }
    case 24: { volatile char* p = g_text; throw p; }  // beside 4 and 5: one descriptor (char*), three ThrowInfos
    case 25: throw Single();                          // Err at offset 0 here, at 16 in Derived: two CatchableTypes of Err
    case 26: throw &single;
    default: break;
    }
}

// "throw;" with the exception of the caller's handler
void FN(rethrower)() { throw; }
