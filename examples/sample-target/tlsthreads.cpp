// tlsthreads.cpp - thread-locals in threads (row P1 stage 1, docs/design-selfhost.md 4.1): eight threads at once,
// each counting in its own copies, one of which an address constant initialises; then 64 short-lived threads one
// after another, each of which must find the template's initial values. What it prints must equal what cl's build
// prints, under both linkers and on GRIDE's runtime. A C-like program: the C library and kernel32 only.
#include <stdio.h>
#include <windows.h>

static int triple(int x) { return 3 * x; }

__declspec(thread) int t_count;                          // zero-initialised
__declspec(thread) long t_sum = 100;                     // a constant
__declspec(thread) int (*t_fn)(int) = triple;            // an address constant (an ADDR64 in .tls$)
static __declspec(thread) unsigned char t_bytes[24] = { 1, 2, 3 };
struct Stat { int id; double scale; };
__declspec(thread) Stat t_stat = { -1, 0.5 };

static long g_results[8][5];
static long g_fresh;                                     // short-lived threads that found the initial values

static int fresh()
{
    return t_count == 0 && t_sum == 100 && t_fn == triple && t_bytes[0] == 1 && t_bytes[1] == 2 && t_bytes[2] == 3 &&
           t_bytes[23] == 0 && t_stat.id == -1 && t_stat.scale == 0.5;
}

static DWORD WINAPI counter(LPVOID arg)
{
    const int id = (int)(SIZE_T)arg;
    const int wasFresh = fresh();
    t_stat.id = id;
    t_stat.scale = id * 0.25;
    for (int i = 0; i < 200000 + 1000 * id; ++i) {
        ++t_count;
        t_sum += t_fn(i & 7);
        t_bytes[i % 24] = (unsigned char)(t_bytes[i % 24] + i + id);
        if ((i & 4095) == 0) Sleep(0);                   // let the others run between
    }
    unsigned long check = 0;
    for (int k = 0; k < 24; ++k) check = check * 31 + t_bytes[k];
    g_results[id][0] = t_count;
    g_results[id][1] = t_sum;
    g_results[id][2] = (long)(check & 0x7FFFFFFF);
    g_results[id][3] = t_stat.id + (long)(t_stat.scale * 4);
    g_results[id][4] = wasFresh;
    return 0;
}

static DWORD WINAPI shortLived(LPVOID arg)
{
    const int n = (int)(SIZE_T)arg;
    if (fresh()) ++g_fresh;                              // one at a time: main waits for each
    t_count = n;
    t_sum += n;
    t_fn = 0;
    t_bytes[0] = 99;
    return 0;
}

int main()
{
    HANDLE h[8];
    for (int i = 0; i < 8; ++i) h[i] = CreateThread(0, 0, counter, (LPVOID)(SIZE_T)i, 0, 0);
    WaitForMultipleObjects(8, h, TRUE, INFINITE);
    for (int i = 0; i < 8; ++i) CloseHandle(h[i]);
    for (int i = 0; i < 8; ++i)
        printf("thread %d: count %ld, sum %ld, bytes %ld, stat %ld, fresh %ld\n", i, g_results[i][0], g_results[i][1], g_results[i][2], g_results[i][3], g_results[i][4]);
    printf("main thread: fresh %d (count %d, sum %ld)\n", fresh(), t_count, t_sum);
    for (int i = 0; i < 64; ++i) {
        HANDLE s = CreateThread(0, 0, shortLived, (LPVOID)(SIZE_T)i, 0, 0);
        WaitForSingleObject(s, INFINITE);
        CloseHandle(s);
    }
    printf("short-lived threads: %ld of 64 found the initial values\n", g_fresh);
    printf("main thread after them: fresh %d\n", fresh());
    return 0;
}
