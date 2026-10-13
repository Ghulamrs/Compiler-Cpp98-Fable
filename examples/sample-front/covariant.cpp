// covariant.cpp - must be rejected by --compile6: CD::self overrides SB::self with a covariant return type, and
// converting CD* to SB* moves the pointer (SB is at 16 in CD), so a call through SB* needs cl's return-adjusting
// thunk, which is not made yet. Error S131; an override whose conversion does not move the pointer is accepted.
struct SA { virtual ~SA() {} int a; };
struct SB { virtual ~SB() {} virtual SB* self() { return this; } int b; };
struct CD : SA, SB { CD* self() { return this; } };
struct PD : SB { PD* self() { return this; } };      // SB at 0: no adjustment, accepted

int main()
{
    CD c;
    SB* sb = &c;
    PD p;
    return sb->self() == sb && p.self() == &p ? 0 : 1;
}
