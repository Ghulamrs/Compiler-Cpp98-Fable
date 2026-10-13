// vbase.cpp - must be rejected by --compile6: a class with a virtual base (here a diamond) is not laid out as cl
// lays it out yet (no vbptr, no shared subobject), so reading v through VL* would read the wrong bytes. Error S130
// for every class that has one, directly or through a base; --sema6 still analyses the unit without errors.
#include <stdio.h>

struct V  { int v; V() : v(7) {} };
struct VL : virtual V { int l; };
struct VR : virtual V { int r; };
struct VD : VL, VR { int d; };

int readV(VL* p) { return p->v; }

int main()
{
    VD d;
    printf("%d\n", readV(&d));
    return 0;
}
