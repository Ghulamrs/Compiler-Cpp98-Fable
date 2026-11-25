// intrin.h - the compiler intrinsics cppparse's own runtime uses (row P1 stage 2, docs/design-selfhost.md 4.2), with
// cl's names and meanings. A call of one of these declarations by name is emitted inline by cppparse
// (cppparse/target TargetIntrinsics): no function of these names exists to link, and taking the address of one is an
// error (S165). cl's <intrin.h> declares the same functions.
#ifndef CPPPARSE_INTRIN_H
#define CPPPARSE_INTRIN_H

extern "C" {
    long  _InterlockedIncrement(long volatile* addend);                                   // lock xadd: the new value
    long  _InterlockedDecrement(long volatile* addend);                                   // lock xadd: the new value
    long  _InterlockedExchange(long volatile* target, long value);                        // xchg: the old value
    long  _InterlockedCompareExchange(long volatile* destination, long exchange, long comparand);   // lock cmpxchg: the old value
    void* _AddressOfReturnAddress(void);   // where the return address of the function that contains the call lies;
                                           // that function keeps its frame and is never inlined
}

#endif
