// replacenew.cpp - a program's own allocation functions (18.4.1.1/2): it defines operator new and operator delete,
// which count their calls, and the library's give way to them when the program is linked. The forms the program
// leaves to the library reach its two: operator new[] and the nothrow forms call operator new (18.4.1.2,
// 18.4.1.1/7), the deallocation functions operator delete, and a failure the program's operator new reports is the
// new-expression's. What the library's strings and containers allocate comes from the program's operator new as
// well (20.4.1.1/5; row G20), and its failure is theirs. Test 15 compares the output with cl's build with its own
// library, at -O0 and -O2 (row G17).
#include <new>
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <map>
#include <set>
#include <valarray>
#include <cstdlib>
#include <cstddef>

static bool   g_on = false;      // counts only between the marks below: what the two libraries allocate differs
static int    g_news = 0, g_deletes = 0;
static size_t g_bytes = 0;
static bool   g_fail = false;

void* operator new(size_t n) throw(std::bad_alloc)
{
    if (g_fail) throw std::bad_alloc();
    if (g_on) { ++g_news; g_bytes += n; }
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) throw()
{
    if (g_on && p) ++g_deletes;
    std::free(p);
}

struct Node {
    int value;
    static int live;
    Node() : value(7) { ++live; }
    ~Node() { --live; }
};
int Node::live = 0;

// the library's allocations: how many and how large differs between the two libraries, that there are some does not
static void library(const char* what, size_t atLeast)
{
    g_on = false;
    std::cout << what << ": " << (g_news > 0 ? "allocated" : "not allocated") << ", " << (g_bytes >= atLeast ? "enough" : "too little")
              << ", " << (g_deletes > 0 ? "freed" : "not freed") << std::endl;
    g_news = 0; g_deletes = 0; g_bytes = 0;
    g_on = true;
}

static void report(const char* what)
{
    g_on = false;
    std::cout << what << ": " << g_news << " new, " << g_deletes << " delete, " << g_bytes << " bytes" << std::endl;
    g_news = 0; g_deletes = 0; g_bytes = 0;
    g_on = true;
}

int main()
{
    std::cout << "replacenew" << std::endl;
    g_on = true;

    int* a = new int(1);
    report("new int");
    delete a;
    report("delete");

    int* b = new int[5];
    report("new int[5]");
    delete[] b;
    report("delete[]");

    Node* c = new Node[3];              // an array with a count before it
    int live = Node::live;
    report("new Node[3]");
    delete[] c;
    report("delete[] Node");
    g_on = false; std::cout << "nodes " << live << " " << Node::live << std::endl; g_on = true;

    int* d = new (std::nothrow) int(2);
    report("new (nothrow) int");
    delete d;
    report("delete");

    char* e = new (std::nothrow) char[100];
    report("new (nothrow) char[100]");
    delete[] e;
    report("delete[]");

    void* f = ::operator new(24);
    report("operator new(24)");
    ::operator delete(f);
    report("operator delete");

    void* g = ::operator new[](40, std::nothrow);
    report("operator new[](40, nothrow)");
    ::operator delete[](g, std::nothrow);
    report("operator delete[](nothrow)");

    { std::string s(200, 'x'); s += s; }
    library("string", 400);
    { std::vector<int> v(100, 1); v.push_back(2); }
    library("vector", 404);
    { std::list<int> l; for (int i = 0; i < 10; ++i) l.push_back(i); }
    library("list", 40);
    { std::deque<double> q; for (int i = 0; i < 100; ++i) q.push_back(i); }
    library("deque", 800);
    { std::map<int, int> m; for (int i = 0; i < 10; ++i) m[i] = i; }
    library("map", 80);
    { std::set<long> t; for (long i = 0; i < 10; ++i) t.insert(i); }
    library("set", 40);
    { std::valarray<double> va(1.5, 50); }
    library("valarray", 400);
    g_on = false;

    // a failure of the program's operator new: bad_alloc from the plain forms, null from the nothrow forms, and
    // bad_alloc from a container that grows
    g_fail = true;
    int caught = 0;
    try { std::string big(300, 'y'); } catch (std::bad_alloc&) { ++caught; }
    try { std::list<int> l; l.push_back(1); } catch (std::bad_alloc&) { ++caught; }
    try { std::map<int, int> m; m[1] = 2; } catch (std::bad_alloc&) { ++caught; }
    try { int* p = new int(3); delete p; } catch (std::bad_alloc&) { ++caught; }
    try { int* p = new int[3]; delete[] p; } catch (std::bad_alloc&) { ++caught; }
    int* n1 = new (std::nothrow) int(4);
    int* n2 = new (std::nothrow) int[4];
    g_fail = false;
    std::cout << "failing: " << caught << " caught, " << (n1 == 0) << " " << (n2 == 0) << std::endl;
    return 0;
}
