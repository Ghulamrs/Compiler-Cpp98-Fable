// probe.cpp - compiled by cppparse (sandbox\scripts\eh.ps1): functions with every frame shape the code generator
// writes unwind info for, called from an ml64 function that fills the callee-saved registers with patterns.
// At the bottom, the driver walks the stack back with RtlVirtualUnwind; the registers it restores in the ml64
// frame must be the patterns again.
//   probeDeep: recursion with integers and doubles live across the call (pushed registers, xmm6.. saves)
//   probeBig:  a frame of more than a page (allocated after __chkstk)
// Strict C++98.

extern "C" int probe_report(int depth);   // in the driver: walks the stack, returns 1000

double g_probeSink = 0.0;

int probeDeep(int n, double x)
{
    int a = n * 3, b = n + 7, c = n ^ 5, d = n * n, e = n - 2;
    double y = x * 1.5, z = x + 2.25, w = x * x;
    int r;
    if (n == 0) r = probe_report(0);
    else r = probeDeep(n - 1, x + 1.0);
    g_probeSink = g_probeSink + y + z + w;
    return r + a + b + c + d + e + static_cast<int>(y + z + w);
}

int probeBig(int n)
{
    char buf[6000];
    for (int i = 0; i < 6000; ++i) buf[i] = static_cast<char>(i & 63);
    int r = probeDeep(n, 0.5);
    return r + buf[n] + buf[5999];
}
