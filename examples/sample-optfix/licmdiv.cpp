// licmdiv.cpp - loop-invariant code motion of a division (Optimizer::licm).
// 100 / d is loop-invariant, but it traps when d == 0: it must stay where the program runs it.

int guardedDivide(int n, int d)            // d == 0 is safe: the division is under the if
{
    int s = 0;
    for (int i = 0; i < n; ++i) {
        if (d != 0) s += 100 / d;
    }
    return s;
}

int zeroTripDivide(int n, int d)           // n == 0, d == 0 is safe: the body never runs
{
    int s = 0;
    for (int i = 0; i < n; ++i) {
        s += 100 / d;
    }
    return s;
}

int constantDivide(int n, int k)           // k / 4 cannot trap: hoisted as before
{
    int s = 0;
    for (int i = 0; i < n; ++i) {
        s += k / 4;
    }
    return s;
}
