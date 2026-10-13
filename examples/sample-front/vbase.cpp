// vbase.cpp - must be rejected by --compile6: a polymorphic virtual base (here in a diamond) keeps its vfptr
// inside the virtual base subobject, and an override from the derived classes may need cl's vtordisp, which
// --compile6 does not generate yet. Error S130 for every class that has one, directly or through a base;
// --sema6 still analyses the unit without errors. (Non-polymorphic virtual bases are compiled: sample-layout.)
#include <stdio.h>

struct V  { int v; V() : v(7) {} virtual int f() const { return v; } };
struct VL : virtual V { int l; };
struct VR : virtual V { int r; };
struct VD : VL, VR { int d; };

int readV(VL* p) { return p->v + p->f(); }

int main()
{
    VD d;
    printf("%d\n", readV(&d));
    return 0;
}
