// ehbench.cpp - the throw-and-catch path of the table model (row P1 stage 3, docs/design-selfhost.md 6: a cost, not a
// fault): a throw out of a recursion of five frames with destructors, caught and rethrown once, so many times; prints
// the nanoseconds per throw. Linked once with cl's object of the runtime and once with cppparse's (ehbench.ps1)
#include <stdio.h>
#include <windows.h>

struct Guard {
    int* count;
    explicit Guard(int* c) : count(c) {}
    ~Guard() { ++*count; }
};

static void deep(int k, int* destroyed)
{
    Guard g(destroyed);
    if (k == 0) throw k + 1;
    deep(k - 1, destroyed);
}

int main()
{
    const int rounds = 20000;
    int destroyed = 0, caught = 0;
    LARGE_INTEGER f, a, b;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&a);
    for (int i = 0; i < rounds; ++i) {
        try {
            try { deep(4, &destroyed); }
            catch (int) { throw; }
        }
        catch (int v) { caught += v; }
    }
    QueryPerformanceCounter(&b);
    const double ns = (double)(b.QuadPart - a.QuadPart) * 1e9 / (double)f.QuadPart / rounds;
    printf("%d throws, %d destructors, %.0f ns per throw and rethrow\n", caught, destroyed, ns);
    return caught == rounds && destroyed == 5 * rounds ? 0 : 1;
}
