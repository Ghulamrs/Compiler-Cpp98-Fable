// unrollcontinue.cpp - full unrolling of loops with a continue (Optimizer::unrollLoops, -O3).
// A continue adds a back edge: every one must lead to the next copy, and the trip count holds only when
// the step runs once per iteration.

int continueToHeader()                     // the continue jumps to the header: 1 + 3 + 4 = 8
{
    int s = 0;
    int i = 0;
    while (i < 4) {
        i = i + 1;
        if (i == 2) continue;
        s += i;
    }
    return s;
}

int continueToStep()                       // the continue jumps to ++i: 0 + 1 + 3 = 4
{
    int s = 0;
    for (int i = 0; i < 4; ++i) {
        if (i == 2) continue;
        s += i;
    }
    return s;
}

int continueSkipsStep()                    // every other pass skips i = i + 1: 6 passes, not 3
{
    int i = 0, n = 0;
    while (i < 3) {
        ++n;
        if (n % 2) continue;
        i = i + 1;
    }
    return n;
}

int stepInInnerLoop()                      // the inner loop (too long to unroll) raises i to 20 in the first pass: 1, not 2
{
    int s = 0;
    int i = 0;
    while (i < 2) {
        for (int j = 0; j < 20; ++j) i = i + 1;
        s = s + 1;
    }
    return s;
}
