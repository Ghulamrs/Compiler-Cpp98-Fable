// volatile.cpp - volatile objects through the optimiser (IrGen marks the accesses, IrInst::isVolatile).
// Every read and write of a volatile object is a side effect: none is removed, merged, forwarded or moved.

volatile int flag;

int globalTwice()                          // two stores, two loads
{
    flag = 1;
    flag = 2;
    return flag + flag;
}

int localStores()                          // the local's stores stay, although nothing reads it
{
    volatile int spin = 0;
    for (int i = 0; i < 3; ++i) spin = i;
    return 0;
}

int localRead()                            // the discarded read stays, and x is read again
{
    volatile int x = 5;
    x;
    return x;
}

int throughPointer(volatile int* p)        // *p is read back, not taken from the store
{
    *p = 1;
    return *p;
}

int spinWait(volatile int& ready)          // ready is read in every iteration
{
    int n = 0;
    while (ready == 0) ++n;
    return n;
}

struct Device {
    int          mode;
    volatile int status;
    volatile unsigned control : 3;
    int  wait();
    void command();
};

int Device::wait()                         // status is read in every iteration, not once before the loop
{
    int n = 0;
    while (status == 0) ++n;
    return n;
}

void Device::command()                     // each assignment reads the unit once and writes it once: no read-back
{
    control = 1;
    control = 2;
}
