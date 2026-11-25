// tlsdef.cpp - defines the thread-locals tlsuse.cpp declares extern (row P1 stage 1, docs/design-selfhost.md 4.1): a
// cl object and a cppparse object share them, either way round
__declspec(thread) int shared_count = 7;
__declspec(thread) int shared_arr[4] = { 1, 2, 3, 4 };
static __declspec(thread) int own_calls;

int bumpShared(int by)
{
    ++own_calls;
    shared_count += by;
    shared_arr[by & 3] += own_calls;
    return shared_count;
}

int ownCalls() { return own_calls; }
