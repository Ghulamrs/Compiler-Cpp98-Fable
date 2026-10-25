// storeload.cpp - store-to-load forwarding through a pointer (Optimizer::copyPropagation).
// "*p = 1" is remembered for the next "*p"; the store to x, which p points to, must forget it.

int storeThenLocal()                       // returns 2, not the 1 stored through p
{
    int x = 0;
    int* p = &x;
    *p = 1;
    x = 2;
    return *p;
}
