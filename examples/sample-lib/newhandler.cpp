// newhandler.cpp - std::set_new_handler (18.4.2.3) and the loop of the allocation functions (18.4.1.1/3 and /7): when
// the storage cannot be had, operator new calls the installed handler and tries again, for as long as there is one. A
// handler ends the loop by installing none (bad_alloc follows) or by throwing bad_alloc; the nothrow forms give null
// then. Test 15 compares the output with cl's build with its own library, at -O0 and -O2 (row G11).
#include <new>
#include <cstdio>
#include <cstddef>

size_t g_huge = static_cast<size_t>(-1) / 2;   // not a constant, and more than any allocation function can give
static int g_calls = 0;

static void retryThenStop() { if (++g_calls == 3) std::set_new_handler(0); }   // returns: the loop tries again
static void giveUp() { ++g_calls; throw std::bad_alloc(); }

int main()
{
    // a handler that returns twice and then uninstalls itself: three calls, then bad_alloc
    std::new_handler first = std::set_new_handler(retryThenStop);
    int caught = 0;
    try { char* p = new char[g_huge]; std::printf("unexpected %p\n", static_cast<void*>(p)); }
    catch (std::bad_alloc&) { caught = 1; }
    std::printf("newhandler %d %d %d\n", first == 0, g_calls, caught);

    // a handler that throws bad_alloc: the nothrow form gives null, the plain form lets the exception through
    std::new_handler second = std::set_new_handler(giveUp);
    char* q = new (std::nothrow) char[g_huge];
    std::printf("newhandler %d %d %d\n", second == 0, q == 0, g_calls);
    try { void* r = ::operator new(g_huge); std::printf("unexpected %p\n", r); }
    catch (std::bad_alloc&) { caught = 2; }

    // an allocation that succeeds calls no handler
    int* ok = new int(5);
    std::printf("newhandler %d %d %d\n", caught, g_calls, *ok);
    delete ok;

    // without a handler the nothrow form gives null at once
    std::new_handler third = std::set_new_handler(0);
    void* none = ::operator new(g_huge, std::nothrow);
    std::printf("newhandler %d %d %d\n", third == giveUp, none == 0, g_calls);
    return 0;
}
