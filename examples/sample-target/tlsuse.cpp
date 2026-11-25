// tlsuse.cpp - reads and writes the thread-locals tlsdef.cpp defines, in four threads and in main (row P1 stage 1)
#include <stdio.h>
#include <windows.h>

extern __declspec(thread) int shared_count;
extern __declspec(thread) int shared_arr[4];
int bumpShared(int by);
int ownCalls();

static long g_out[4][3];

static DWORD WINAPI work(LPVOID arg)
{
    const int id = (int)(SIZE_T)arg;
    long seen = shared_count;                        // 7 in every new thread
    for (int i = 0; i < 1000 * (id + 1); ++i) {
        bumpShared(id + 1);
        shared_arr[i & 3] -= 1;
        if ((i & 255) == 0) Sleep(0);
    }
    g_out[id][0] = seen;
    g_out[id][1] = shared_count;
    g_out[id][2] = shared_arr[0] + 10 * shared_arr[1] + 100 * shared_arr[2] + 1000 * shared_arr[3] + 100000 * ownCalls();
    return 0;
}

int main()
{
    HANDLE h[4];
    for (int i = 0; i < 4; ++i) h[i] = CreateThread(0, 0, work, (LPVOID)(SIZE_T)i, 0, 0);
    WaitForMultipleObjects(4, h, TRUE, INFINITE);
    for (int i = 0; i < 4; ++i) { CloseHandle(h[i]); printf("thread %d: first %ld, count %ld, array %ld\n", i, g_out[i][0], g_out[i][1], g_out[i][2]); }
    printf("main: count %d, array %d %d %d %d, calls %d\n", shared_count, shared_arr[0], shared_arr[1], shared_arr[2], shared_arr[3], ownCalls());
    return 0;
}
