// ehthreads.cpp - eight threads throwing and catching at once (row P1 stage 2, docs/design-selfhost.md 5): the
// runtime's state of an exception (the frames, the exception in flight, the caught ones, the unwinding) is per thread.
// Compiled by cppparse in the three models with either object of the runtime, it must print what cl's build prints.
#include <stdio.h>
#include <windows.h>

struct Guard {
    int* count;
    explicit Guard(int* c) : count(c) {}
    ~Guard() { ++*count; }
};

struct Oops {
    int code;
    explicit Oops(int c) : code(c) {}
};

static void deep(int k, int* destroyed)
{
    Guard g(destroyed);
    if (k == 0) throw Oops(7);
    deep(k - 1, destroyed);
}

static long g_results[8][4];

static DWORD WINAPI worker(LPVOID arg)
{
    const int id = (int)(SIZE_T)arg;
    int destroyed = 0, caught = 0, rethrown = 0;
    long sum = 0;
    for (int i = 0; i < 20000 + 500 * id; ++i) {
        try { deep(i % 5, &destroyed); }
        catch (const Oops& o) { ++caught; sum += o.code; }
        try {
            try { throw i; }
            catch (int x) { if (x % 3 == 0) throw; sum += 1; }
        }
        catch (int y) { ++rethrown; sum += y % 7; }
        if ((i & 1023) == 0) Sleep(0);
    }
    g_results[id][0] = destroyed;
    g_results[id][1] = caught;
    g_results[id][2] = rethrown;
    g_results[id][3] = sum;
    return 0;
}

int main()
{
    HANDLE h[8];
    for (int i = 0; i < 8; ++i) h[i] = CreateThread(0, 0, worker, (LPVOID)(SIZE_T)i, 0, 0);
    WaitForMultipleObjects(8, h, TRUE, INFINITE);
    for (int i = 0; i < 8; ++i) {
        CloseHandle(h[i]);
        printf("thread %d: destroyed %ld, caught %ld, rethrown %ld, sum %ld\n", i, g_results[i][0], g_results[i][1], g_results[i][2], g_results[i][3]);
    }
    return 0;
}
