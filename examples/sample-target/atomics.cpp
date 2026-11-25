// atomics.cpp - the interlocked intrinsics (row P1 stage 2, docs/design-selfhost.md 4.2): eight threads increment one
// counter two million times each and must reach the sum; decrement, exchange and compare-exchange give cl's values.
// What it prints must equal what cl's build prints.
#include <stdio.h>
#include <windows.h>
#include <intrin.h>

static long volatile g_counter;
static long volatile g_down = 16000000;
static long volatile g_owner;
static long volatile g_inside;
static long volatile g_collisions;
static long g_last[8];

static DWORD WINAPI worker(LPVOID arg)
{
    const long id = (long)(SIZE_T)arg + 1;
    long last = 0;
    for (int i = 0; i < 2000000; ++i) {
        last = _InterlockedIncrement(&g_counter);
        _InterlockedDecrement(&g_down);
        if ((i & 1023) == 0) {
            // a lock by compare-exchange: one owner at a time
            while (_InterlockedCompareExchange(&g_owner, id, 0) != 0) Sleep(0);
            if (_InterlockedIncrement(&g_inside) != 1) _InterlockedIncrement(&g_collisions);
            _InterlockedDecrement(&g_inside);
            _InterlockedExchange(&g_owner, 0);
        }
    }
    g_last[id - 1] = last > 0 ? 1 : 0;
    return 0;
}

int main()
{
    HANDLE h[8];
    for (int i = 0; i < 8; ++i) h[i] = CreateThread(0, 0, worker, (LPVOID)(SIZE_T)i, 0, 0);
    WaitForMultipleObjects(8, h, TRUE, INFINITE);
    for (int i = 0; i < 8; ++i) CloseHandle(h[i]);
    long seen = 0;
    for (int i = 0; i < 8; ++i) seen += g_last[i];
    printf("counter %ld (want 16000000), down %ld (want 0), collisions %ld, owner %ld, threads %ld\n", g_counter, g_down, g_collisions, g_owner, seen);
    long volatile v = 5;
    long a = _InterlockedExchange(&v, 9);
    long b = _InterlockedCompareExchange(&v, 1, 7);     // not equal: unchanged
    long c = _InterlockedCompareExchange(&v, 1, 9);     // equal: replaced
    long d = _InterlockedDecrement(&v);
    long e = _InterlockedIncrement(&v);
    printf("exchange %ld, compare-exchange %ld then %ld, value %ld, decrement %ld, increment %ld\n", a, b, c, v, d, e);
    return g_counter == 16000000 && g_down == 0 && g_collisions == 0 ? 0 : 1;
}
