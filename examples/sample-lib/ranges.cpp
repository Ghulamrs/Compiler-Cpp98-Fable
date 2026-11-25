// ranges.cpp - the range members of the containers (row G31): the constructor, assign and insert of vector, deque and
// list, those and append and replace of basic_string, and the constructor and insert of set, multiset, map and multimap
// are member templates over any input iterator (23.1.1, 23.1.2, 21.3); two arguments of one integral type are a count and a
// value (23.1.1/9). Built by cl with its own headers (the reference) and by cppparse --compile6 with include/; test 15
// compares what the programs print at -O0 and -O2.
// Each line shows a container after one member took one kind of range: pointers, the iterators of a list, a set, a
// deque and a vector, reverse iterators, and stream iterators and Gen, which are input iterators: their range can be
// read once only and not be measured first.
// Cnt counts its constructions as vector.cpp's does, and the program's operator new the allocations: a vector takes a
// forward range of N elements with N copy constructions and one allocation (23.2.4.1/1), a list and a deque with N
// copies; what an input range costs is open ("order N" copies), and where the two libraries go different ways (cl
// destroys and constructs where this library assigns) the line says "open" and shows the balance only. A copy that
// throws loses nothing in a constructor, leaves a list as it was (23.2.2.3) and a vector that has to reallocate too.
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <new>
#include <vector>
#include <deque>
#include <list>
#include <set>
#include <map>
#include <string>
#include <iterator>
#include <sstream>
#include <utility>
#include <queue>

static bool g_on = false;
static int  g_news = 0;

void* operator new(size_t n) throw(std::bad_alloc)
{
    if (g_on) ++g_news;
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void* p) throw() { std::free(p); }

struct Cnt {
    static int live, bad, made, copied, assigned, destroyed, fuse;
    int v;
    int state;
    Cnt() : v(0), state(0xC0DE) { ++made; ++live; }
    Cnt(int x) : v(x), state(0xC0DE) { ++made; ++live; }
    Cnt(const Cnt& o) : v(o.v), state(0xC0DE)
    {
        if (o.state != 0xC0DE) ++bad;
        if (fuse > 0 && --fuse == 0) throw 7;
        ++copied; ++live;
    }
    Cnt& operator=(const Cnt& o) { if (state != 0xC0DE || o.state != 0xC0DE) ++bad; v = o.v; ++assigned; return *this; }
    ~Cnt() { if (state != 0xC0DE) ++bad; state = 0xDEAD; ++destroyed; --live; }
};
int Cnt::live = 0;
int Cnt::bad = 0;
int Cnt::made = 0;
int Cnt::copied = 0;
int Cnt::assigned = 0;
int Cnt::destroyed = 0;
int Cnt::fuse = 0;

// an input iterator of the program's own: Gen(a) to Gen(b) is the range of the pool's elements a, a + 1, ... up to
// (not including) b, whose values are their indices. Like a stream iterator it can only be read in passing; its
// increment throws 9 when the fuse runs out
struct Gen : public std::iterator<std::input_iterator_tag, Cnt, std::ptrdiff_t, const Cnt*, const Cnt&> {
    static int fuse;
    static const Cnt* pool;
    int at;
    Gen(int from) : at(from) {}
    const Cnt& operator*() const { return pool[at]; }
    const Cnt* operator->() const { return pool + at; }
    Gen& operator++() { if (fuse > 0 && --fuse == 0) throw 9; ++at; return *this; }
    Gen operator++(int) { Gen t(*this); ++*this; return t; }
    bool operator==(const Gen& o) const { return at == o.at; }
    bool operator!=(const Gen& o) const { return at != o.at; }
};
int Gen::fuse = 0;
const Cnt* Gen::pool = 0;

static int val(int x) { return x; }
static int val(const Cnt& x) { return x.v; }

template <class C>
static void show(const char* what, const C& c)
{
    std::printf("  %-30s", what);
    for (typename C::const_iterator i = c.begin(); i != c.end(); ++i) std::printf(" %d", val(*i));
    std::printf(" (%d)\n", static_cast<int>(c.size()));
}

template <class C>
static typename C::iterator at(C& c, int k)
{
    typename C::iterator i = c.begin();
    for (; k > 0; --k) ++i;
    return i;
}

// vector<int>, deque<int> and list<int>: every member with every kind of range
template <class C>
static void sequence(const char* name)
{
    std::printf("%s\n", name);
    const int arr[6] = { 5, 3, 8, 1, 3, 2 };
    int marr[3] = { 7, 7, 4 };
    std::list<int> L; std::set<int> S; std::deque<int> D; std::vector<int> V;
    for (int i = 0; i < 6; ++i) { L.push_back(arr[i] + 10); S.insert(arr[i] + 20); D.push_back(arr[i] + 30); V.push_back(arr[i] + 40); }
    const std::list<int>& CL = L;
    const std::vector<int>& CV = V;
    { C c(arr, arr + 6); show("from pointers", c); }
    { C c(marr, marr + 3); show("from pointers to non-const", c); }
    { C c(L.begin(), L.end()); show("from a list", c); }
    { C c(CL.begin(), CL.end()); show("from a const list", c); }
    { C c(S.begin(), S.end()); show("from a set", c); }
    { C c(D.begin(), D.end()); show("from a deque", c); }
    { C c(V.begin(), V.end()); show("from a vector", c); }
    { C c(CV.begin(), CV.end()); show("from a const vector", c); }
    { C c(V.rbegin(), V.rend()); show("from a vector, reversed", c); }
    { C c(L.rbegin(), L.rend()); show("from a list, reversed", c); }
    { C c(S.rbegin(), S.rend()); show("from a set, reversed", c); }
    { std::istringstream in("61 62 63 64 65 66 67 68 69 70 71"); std::istream_iterator<int> first(in), last; C c(first, last); show("from a stream", c); }
    { C c(arr, arr); show("from no pointers", c); }
    { C c(L.end(), L.end()); show("from an empty list range", c); }
    { C c(5, 7); show("5 and 7", c); }
    { short n = 2, v = 9; C c(n, v); show("short 2 and 9", c); }
    { C c(3u, 4); show("3u and 4", c); }
    { C c(0, 0); show("0 and 0", c); }

    C c;
    c.assign(arr, arr + 6);               show("assign pointers", c);
    c.assign(marr, marr + 3);             show("assign pointers to non-const", c);
    c.assign(L.begin(), L.end());         show("assign a list", c);
    c.assign(S.begin(), S.end());         show("assign a set", c);
    c.assign(D.begin(), D.begin() + 2);   show("assign a deque's first two", c);
    c.assign(V.rbegin(), V.rend());       show("assign a vector, reversed", c);
    c.assign(CL.rbegin(), CL.rend());     show("assign a const list, reversed", c);
    { std::istringstream in("81 82 83"); std::istream_iterator<int> first(in), last; c.assign(first, last); show("assign a stream", c); }
    c.assign(L.begin(), L.begin());       show("assign an empty range", c);
    c.assign(3, 4);                       show("assign 3 and 4", c);
    { unsigned char n = 2, v = 6; c.assign(n, v); show("assign unsigned char 2 and 6", c); }
    c.assign(2u, 5);                      show("assign 2u and 5", c);

    c.insert(at(c, 1), arr, arr + 3);                 show("insert pointers", c);
    c.insert(c.end(), marr, marr + 2);                show("insert pointers to non-const, end", c);
    c.insert(c.begin(), L.begin(), L.end());          show("insert a list, begin", c);
    c.insert(at(c, 4), S.begin(), S.end());           show("insert a set", c);
    c.insert(at(c, 2), D.begin(), D.end());           show("insert a deque", c);
    c.insert(c.end(), V.rbegin(), V.rend());          show("insert a vector reversed, end", c);
    c.insert(at(c, static_cast<int>(c.size()) - 1), L.rbegin(), L.rend());  show("insert a list reversed, last", c);
    { std::istringstream in("91 92 93 94"); std::istream_iterator<int> first(in), last; c.insert(at(c, 3), first, last); show("insert a stream", c); }
    { std::istringstream in("95 96"); std::istream_iterator<int> first(in), last; c.insert(c.end(), first, last); show("insert a stream, end", c); }
    c.insert(at(c, 2), S.end(), S.end());             show("insert an empty range", c);
    c.assign(arr, arr + 2);
    c.insert(c.begin(), 2, 9);                        show("insert 2 and 9", c);
    { long n = 3, v = 1; c.insert(c.end(), n, v);     show("insert long 3 and 1", c); }
    c.insert(at(c, 1), 2u, 6);                        show("insert 2u and 6", c);
    c.insert(at(c, 1), 0, 6);                         show("insert 0 and 6", c);
}

// the tail of a step: the living objects (the pool's 80 apart) and the broken ones, then the counts since the step
// before where they are fixed, and the allocations where those are too
enum Tally { Open, Counts, Allocs };
static void tally(Tally how)
{
    g_on = false;
    std::printf("    live %d bad %d", Cnt::live - 80, Cnt::bad);
    if (how != Open) std::printf(" | made %d copied %d assigned %d destroyed %d", Cnt::made, Cnt::copied, Cnt::assigned, Cnt::destroyed);
    if (how == Allocs) std::printf(" | allocations %d", g_news);
    std::printf(how == Open ? " | open\n" : "\n");
    Cnt::made = Cnt::copied = Cnt::assigned = Cnt::destroyed = 0;
    g_news = 0;
    g_on = true;
}

// what a range costs: the copies and the allocations of a vector, the copies of a list and a deque
static void counted()
{
    std::printf("Cnt, counted\n");
    Cnt src[8];
    for (int i = 0; i < 8; ++i) src[i].v = i + 1;
    std::list<Cnt> L(src, src + 8);
    tally(Open);
    {
        std::vector<Cnt> v(src, src + 8);                 show("vector from pointers", v); tally(Allocs);
        std::vector<Cnt> w(L.begin(), L.end());           show("vector from a list", w); tally(Allocs);
        std::vector<Cnt> r(L.rbegin(), L.rend());         show("vector from a list, reversed", r); tally(Allocs);
        std::vector<Cnt> g(Gen(1), Gen(9));               show("vector from Gen", g); tally(Open);
        v.reserve(40);                                    tally(Allocs);
        v.insert(v.begin() + 2, L.begin(), L.end());      show("insert a list, room, long tail", v); tally(Open);
        v.insert(v.end() - 3, L.begin(), L.end());        show("insert a list, room, short tail", v); tally(Open);
        v.insert(v.end(), L.rbegin(), L.rend());          show("insert a list reversed, end", v); tally(Allocs);
        v.insert(v.begin() + 1, Gen(50), Gen(53));        show("insert Gen, room", v); tally(Open);
        v.insert(v.end(), Gen(60), Gen(62));              show("insert Gen, end", v); tally(Open);
        w.insert(w.begin() + 3, L.begin(), L.end());      show("insert a list, full", w); tally(Allocs);
        w.insert(w.begin() + 1, src, src + 2);            show("insert pointers, full", w); tally(Allocs);
        r.insert(r.begin() + 1, Gen(70), Gen(73));        show("insert Gen, full", r); tally(Open);
        r.reserve(30);                                    tally(Allocs);
        r.assign(L.begin(), L.end());                     show("assign a list, room", r); tally(Open);
        g.assign(Gen(20), Gen(40));                       show("assign Gen", g); tally(Open);
        std::vector<Cnt> e;
        e.assign(L.begin(), L.end());                     show("assign a list to an empty one", e); tally(Allocs);
        std::vector<Cnt> n(3, 4);                         show("3 and 4", n); tally(Allocs);
    }
    tally(Counts);
    {
        std::list<Cnt> l(src, src + 8);                   show("list from pointers", l); tally(Counts);
        std::list<Cnt> m(L.rbegin(), L.rend());           show("list from a list, reversed", m); tally(Counts);
        std::list<Cnt> g(Gen(1), Gen(5));                 show("list from Gen", g); tally(Counts);
        l.insert(at(l, 2), L.begin(), L.end());           show("insert a list", l); tally(Counts);
        l.insert(l.end(), Gen(30), Gen(33));              show("insert Gen, end", l); tally(Counts);
        m.assign(src, src + 3);                           show("assign pointers", m); tally(Open);
        g.insert(g.begin(), 2, 5);                        show("insert 2 and 5", g); tally(Open);
    }
    tally(Counts);
    {
        std::deque<Cnt> d(L.begin(), L.end());            show("deque from a list", d); tally(Counts);
        std::deque<Cnt> p(src, src + 8);                  show("deque from pointers", p); tally(Counts);
        std::deque<Cnt> g(Gen(1), Gen(5));                show("deque from Gen", g);
        d.insert(d.begin(), L.rbegin(), L.rend());        show("insert a list reversed, begin", d);
        d.insert(d.end(), src, src + 2);                  show("insert pointers, end", d);
        d.insert(d.begin() + 3, L.begin(), L.end());      show("insert a list", d);
        d.insert(d.end() - 2, L.begin(), L.end());        show("insert a list, short tail", d);
        d.insert(d.begin() + 1, Gen(40), Gen(43));        show("insert Gen", d);
        p.assign(L.rbegin(), L.rend());                   show("assign a list, reversed", p);
        p.assign(Gen(7), Gen(10));                        show("assign Gen", p);
        g.assign(src, src + 2);                           show("assign pointers", g);
        g.insert(g.begin(), 2, 5);                        show("insert 2 and 5", g);
        tally(Open);
    }
    tally(Open);
    L.clear();
    tally(Open);
}

// a copy that throws, or the iterator itself
static void throwing()
{
    std::printf("a copy throws\n");
    Cnt src[6];
    for (int i = 0; i < 6; ++i) src[i].v = i + 1;
    std::list<Cnt> L(src, src + 6);
    int base = Cnt::live;
    Cnt::fuse = 4;
    try { std::vector<Cnt> v(L.begin(), L.end()); std::printf("  no exception\n"); } catch (int e) { std::printf("  vector from a list: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
    std::printf(" lost %d bad %d\n", Cnt::live - base, Cnt::bad);
    Cnt::fuse = 4;
    try { std::vector<Cnt> v(Gen(1), Gen(9)); std::printf("  no exception\n"); } catch (int e) { std::printf("  vector from Gen: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
    std::printf(" lost %d bad %d\n", Cnt::live - base, Cnt::bad);
    Gen::fuse = 3;
    try { std::vector<Cnt> v(Gen(1), Gen(9)); std::printf("  no exception\n"); } catch (int e) { std::printf("  vector from Gen, Gen throws: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
    std::printf(" lost %d bad %d\n", Cnt::live - base, Cnt::bad);
    Cnt::fuse = 3;
    try { std::list<Cnt> l(L.rbegin(), L.rend()); std::printf("  no exception\n"); } catch (int e) { std::printf("  list from a list: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
    std::printf(" lost %d bad %d\n", Cnt::live - base, Cnt::bad);
    Cnt::fuse = 5;
    try { std::deque<Cnt> d(L.begin(), L.end()); std::printf("  no exception\n"); } catch (int e) { std::printf("  deque from a list: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
    std::printf(" lost %d bad %d\n", Cnt::live - base, Cnt::bad);
    {
        std::vector<Cnt> v(src, src + 4);
        const Cnt* was = &v[0];
        Cnt::fuse = 6;
        try { v.insert(v.begin() + 1, L.begin(), L.end()); std::printf("  no exception\n"); } catch (int e) { std::printf("  insert a list, full: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
        std::printf(" same elements %d, size %d, lost %d bad %d\n", static_cast<int>(was == &v[0] && v.capacity() == 4), static_cast<int>(v.size()), Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        v.reserve(30);
        Cnt::fuse = 3;
        try { v.insert(v.begin() + 1, L.begin(), L.end()); std::printf("  no exception\n"); } catch (int e) { std::printf("  insert a list, room: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        Gen::fuse = 2;
        try { v.insert(v.begin() + 1, Gen(1), Gen(9)); std::printf("  no exception\n"); } catch (int e) { std::printf("  insert Gen, Gen throws: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        v.clear();
        Cnt::fuse = 3;
        try { v.assign(L.begin(), L.end()); std::printf("  no exception\n"); } catch (int e) { std::printf("  assign a list: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
    }
    {
        std::list<Cnt> l(src, src + 3);
        Cnt::fuse = 4;
        try { l.insert(at(l, 1), L.begin(), L.end()); std::printf("  no exception\n"); } catch (int e) { std::printf("  list insert a list: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
        std::printf(" size %d, lost %d bad %d\n", static_cast<int>(l.size()), Cnt::live - base - static_cast<int>(l.size()), Cnt::bad);
        Gen::fuse = 2;
        try { l.insert(l.end(), Gen(1), Gen(9)); std::printf("  no exception\n"); } catch (int e) { std::printf("  list insert Gen, Gen throws: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
        std::printf(" size %d, lost %d bad %d\n", static_cast<int>(l.size()), Cnt::live - base - static_cast<int>(l.size()), Cnt::bad);
        Cnt::fuse = 2;
        try { l.insert(l.begin(), 5, src[0]); std::printf("  no exception\n"); } catch (int e) { std::printf("  list insert 5 copies: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
        std::printf(" size %d, lost %d bad %d\n", static_cast<int>(l.size()), Cnt::live - base - static_cast<int>(l.size()), Cnt::bad);
        std::deque<Cnt> d(src, src + 3);
        Cnt::fuse = 3;
        try { d.insert(d.begin(), L.begin(), L.end()); std::printf("  no exception\n"); } catch (int e) { std::printf("  deque insert a list, begin: caught %d,", e); } Cnt::fuse = 0; Gen::fuse = 0;
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(l.size()) - static_cast<int>(d.size()), Cnt::bad);
    }
    Cnt::fuse = 0;
    Gen::fuse = 0;
    std::printf("  after: lost %d bad %d\n", Cnt::live - base, Cnt::bad);
}

static void str(const char* what, const std::string& s) { std::printf("  %-30s %s (%d)\n", what, s.c_str(), static_cast<int>(s.size())); }

// basic_string: the constructor, assign, append, insert and replace
static void strings()
{
    std::printf("string\n");
    const char* text = "the quick brown fox jumps";
    const char* p = text;
    const char* q = text + 25;
    char marr[4] = { 'w', 'x', 'y', 'z' };
    std::list<char> L(p + 4, p + 9);
    std::set<char> S(p, q);
    std::deque<char> D(p + 10, p + 15);
    std::vector<char> V(p + 16, p + 19);
    const std::string other("lazy dog, over and under");
    { std::string s(p, q); str("from pointers", s); }
    { std::string s(p, p + 3); str("from pointers, short", s); }
    { std::string s(marr, marr + 4); str("from pointers to non-const", s); }
    { std::string s(L.begin(), L.end()); str("from a list", s); }
    { std::string s(S.begin(), S.end()); str("from a set", s); }
    { std::string s(D.begin(), D.end()); str("from a deque", s); }
    { std::string s(V.begin(), V.end()); str("from a vector", s); }
    { std::string s(other.begin(), other.end()); str("from a string", s); }
    { std::string s(other.rbegin(), other.rend()); str("from a string, reversed", s); }
    { std::string s(L.rbegin(), L.rend()); str("from a list, reversed", s); }
    { std::istringstream in("one two  three"); std::istreambuf_iterator<char> first(in), last; std::string s(first, last); str("from a stream buffer", s); }
    { std::istringstream in("a b c d e f g h i j k l m n o p q r s t"); std::istream_iterator<char> first(in), last; std::string s(first, last); str("from a stream", s); }
    { std::string s(p, p); str("from no pointers", s); }
    { std::string s(3, 65); str("3 and 65", s); }
    { std::string s(p, 0); str("a pointer and 0", s); }
    { std::string s(static_cast<size_t>(2), 'y'); str("2 and 'y'", s); }

    std::string s;
    s.assign(p + 4, p + 9);                        str("assign pointers", s);
    s.assign(L.rbegin(), L.rend());                str("assign a list, reversed", s);
    s.assign(S.begin(), S.end());                  str("assign a set", s);
    s.assign(s.rbegin(), s.rend());                str("assign its own, reversed", s);
    s.assign(s.begin() + 2, s.begin() + 6);        str("assign its own", s);
    { std::istringstream in("streamed"); std::istreambuf_iterator<char> first(in), last; s.assign(first, last); str("assign a stream buffer", s); }
    s.assign(2, 67);                               str("assign 2 and 67", s);

    s.append(p, p + 3);                            str("append pointers", s);
    s.append(marr, marr + 4);                      str("append pointers to non-const", s);
    s.append(L.begin(), L.end());                  str("append a list", s);
    s.append(D.rbegin(), D.rend());                str("append a deque, reversed", s);
    s.append(s.rbegin(), s.rend());                str("append its own, reversed", s);
    s.append(s.begin(), s.begin() + 2);            str("append its own", s);
    { std::istringstream in("!?"); std::istreambuf_iterator<char> first(in), last; s.append(first, last); str("append a stream buffer", s); }
    s.append(2, 66);                               str("append 2 and 66", s);
    s.assign(p, p + 3);
    s.append(S.begin(), S.end());                  str("append a set, growing", s);

    s.assign(p, p + 9);
    s.insert(s.begin() + 3, marr, marr + 2);       str("insert pointers", s);
    s.insert(s.end(), L.begin(), L.end());         str("insert a list, end", s);
    s.insert(s.begin(), V.rbegin(), V.rend());     str("insert a reversed vector", s);
    s.insert(s.begin() + 1, s.rbegin(), s.rend()); str("insert its own, reversed", s);
    { std::istringstream in("<>"); std::istreambuf_iterator<char> first(in), last; s.insert(s.begin() + 2, first, last); str("insert a stream buffer", s); }
    s.insert(s.begin(), 2, 68);                    str("insert 2 and 68", s);

    s.assign(p, p + 9);
    s.replace(s.begin() + 4, s.begin() + 9, p + 10, p + 15);     str("replace by pointers", s);
    s.replace(s.begin(), s.begin() + 3, L.begin(), L.end());     str("replace by a list", s);
    s.replace(s.begin() + 1, s.begin() + 2, D.rbegin(), D.rend()); str("replace by a deque, reversed", s);
    s.replace(s.begin(), s.begin() + 4, s.rbegin(), s.rend());   str("replace by its own, reversed", s);
    { std::istringstream in("[]"); std::istreambuf_iterator<char> first(in), last; s.replace(s.end() - 2, s.end(), first, last); str("replace by a stream buffer", s); }
    s.replace(s.begin(), s.begin() + 1, 3, 69);                  str("replace by 3 and 69", s);

    std::list<wchar_t> wl;
    wl.push_back(L'a'); wl.push_back(L'b');
    std::wstring w(wl.begin(), wl.end());
    const wchar_t* wp = L"xyz";
    w.append(wp, wp + 3);
    w.insert(w.begin(), wl.rbegin(), wl.rend());
    std::wstring wide(p, p + 3);                  // each char widened
    std::printf("  wstring                        %d %d %d %d %d\n", static_cast<int>(w.size()), static_cast<int>(w[0]), static_cast<int>(w[6]), static_cast<int>(wide.size()), static_cast<int>(wide[2]));
}

template <class M>
static void showMap(const char* what, const M& m)
{
    std::printf("  %-30s", what);
    for (typename M::const_iterator i = m.begin(); i != m.end(); ++i) std::printf(" %d:%d", i->first, i->second);
    std::printf(" (%d)\n", static_cast<int>(m.size()));
}

// set, multiset, map and multimap: the constructor and insert; the elements of other types convert. A multiset and a
// multimap keep equal keys in the order they came
static void associative()
{
    std::printf("set and map\n");
    const int arr[6] = { 5, 3, 8, 1, 3, 2 };
    std::list<int> L(arr, arr + 6);
    std::vector<int> V(arr, arr + 6);
    { std::set<int> s(arr, arr + 6); show("set from pointers", s); }
    { std::set<int> s(L.begin(), L.end()); show("set from a list", s); }
    { std::set<int> s(V.rbegin(), V.rend()); show("set from a vector, reversed", s); }
    { std::istringstream in("9 7 9 8"); std::istream_iterator<int> first(in), last; std::set<int> s(first, last); show("set from a stream", s); }
    std::set<int> s;
    s.insert(arr + 1, arr + 3);                    show("set insert pointers", s);
    s.insert(L.rbegin(), L.rend());                show("set insert a list, reversed", s);
    { std::istringstream in("4 6"); std::istream_iterator<int> first(in), last; s.insert(first, last); show("set insert a stream", s); }
    std::set<int> t(s.begin(), s.end());           show("set from a set", t);

    { std::multiset<int> ms(arr, arr + 6); show("multiset from pointers", ms); }
    { std::multiset<int> ms(L.rbegin(), L.rend()); show("multiset from a list, reversed", ms); }
    { std::istringstream in("9 7 9 8"); std::istream_iterator<int> first(in), last; std::multiset<int> ms(first, last); show("multiset from a stream", ms); }
    std::multiset<int> ms(V.begin(), V.end());
    ms.insert(arr, arr + 2);                       show("multiset insert pointers", ms);
    ms.insert(s.rbegin(), s.rend());               show("multiset insert a reversed set", ms);
    { std::istringstream in("3 9 3"); std::istream_iterator<int> first(in), last; ms.insert(first, last); show("multiset insert a stream", ms); }
    std::multiset<int> copy(ms);
    const int threes = static_cast<int>(ms.count(3)), sevens = static_cast<int>(ms.count(7)), erased = static_cast<int>(ms.erase(3));
    std::printf("  multiset count, erase          %d %d %d %d (%d) (%d)\n", threes, sevens, erased, static_cast<int>(ms.count(3)), static_cast<int>(ms.size()), static_cast<int>(copy.size()));
    std::vector<int> fromMs(ms.rbegin(), ms.rend()); show("vector from a multiset", fromMs);

    typedef std::pair<int, int> P;
    P pa[4] = { P(4, 40), P(2, 20), P(4, 44), P(1, 10) };
    std::list<P> pl(pa, pa + 4);
    std::vector<std::pair<const int, int> > exact(pa, pa + 2);
    { std::map<int, int> m(pa, pa + 4); showMap("map from pointers", m); }
    { std::map<int, int> m(pl.begin(), pl.end()); showMap("map from a list", m); }
    { std::map<int, int> m(pl.rbegin(), pl.rend()); showMap("map from a list, reversed", m); }
    { std::map<int, int> m(exact.begin(), exact.end()); showMap("map from its value type", m); }
    std::map<int, int> m;
    m.insert(pa + 1, pa + 3);                      showMap("map insert pointers", m);
    m.insert(pl.rbegin(), pl.rend());              showMap("map insert a list, reversed", m);
    std::map<int, int> n(m.begin(), m.end());      showMap("map from a map", n);
    n.insert(m.begin(), m.end());                  showMap("map insert a map", n);
    { std::multimap<int, int> mm(pa, pa + 4); showMap("multimap from pointers", mm); }
    { std::multimap<int, int> mm(pl.rbegin(), pl.rend()); showMap("multimap from a list, reversed", mm); }
    std::multimap<int, int> mm(m.begin(), m.end());
    mm.insert(pa, pa + 4);                         showMap("multimap insert pointers", mm);
    mm.insert(pl.begin(), pl.end());               showMap("multimap insert a list", mm);
    mm.insert(std::pair<const int, int>(2, 22));
    std::multimap<int, int> mcopy(mm);
    std::pair<std::multimap<int, int>::iterator, std::multimap<int, int>::iterator> four = mm.equal_range(4);
    int fours = 0;
    for (std::multimap<int, int>::iterator i = four.first; i != four.second; ++i) fours += i->second;
    const int counted = static_cast<int>(mm.count(4)), none = static_cast<int>(mm.count(3)), gone = static_cast<int>(mm.erase(4));
    std::printf("  multimap count, erase          %d %d %d %d (%d) (%d)\n", counted, none, fours, gone, static_cast<int>(mm.size()), static_cast<int>(mcopy.size()));
    showMap("multimap after erase", mm);
    std::vector<P> back(n.begin(), n.end());       // and a map's range into a vector: its iterators are bidirectional
    back.insert(back.begin(), m.rbegin(), m.rend());
    std::printf("  vector from a map              %d %d:%d %d:%d\n", static_cast<int>(back.size()), back[0].first, back[0].second, back[5].first, back[5].second);
}

// the elements convert; other containers built on these members
static void others()
{
    std::printf("others\n");
    const int arr[5] = { 4, 1, 3, 5, 2 };
    std::list<int> L(arr, arr + 5);
    std::vector<long> vl(L.begin(), L.end());
    vl.insert(vl.begin(), arr, arr + 2);
    vl.assign(arr + 1, arr + 4);
    std::printf("  vector<long> from ints         %d %d %d\n", static_cast<int>(vl.size()), static_cast<int>(vl[0]), static_cast<int>(vl[2]));
    std::vector<double> vd(5, 7);
    vd.assign(0u, 0);
    vd.insert(vd.begin(), 0u, 0);
    std::vector<double> none(0u, 0);
    vd.insert(vd.begin(), arr, arr + 5);
    std::printf("  vector<double>                 %d %g %d\n", static_cast<int>(vd.size()), vd[3], static_cast<int>(none.size()));
    const char* names[3] = { "one", "two", "three" };
    std::vector<std::string> vs(names, names + 3);
    vs.insert(vs.begin() + 1, names, names + 2);
    vs.assign(names + 1, names + 3);
    std::list<std::string> ls(names, names + 3);
    std::deque<std::string> ds(ls.rbegin(), ls.rend());
    std::printf("  strings from const char*       %d %s %s %s %s\n", static_cast<int>(vs.size()), vs[0].c_str(), vs[1].c_str(), ls.back().c_str(), ds.front().c_str());
    std::vector<std::vector<int> > vv(3, std::vector<int>(2, 1));
    std::vector<std::vector<int> > vw(vv.rbegin(), vv.rend());
    std::printf("  vector of vectors              %d %d %d\n", static_cast<int>(vv.size()), static_cast<int>(vv[2].size()), static_cast<int>(vw.size()));
    std::priority_queue<int> pq(L.begin(), L.end());
    std::printf("  priority_queue from a list     %d %d\n", static_cast<int>(pq.size()), pq.top());
    int* ptrs[3] = { 0, 0, 0 };
    std::vector<const int*> vp(ptrs, ptrs + 3);    // int* to const int*: not the vector's own pointers
    std::vector<int*> vq(3, static_cast<int*>(0));
    std::printf("  vectors of pointers            %d %d\n", static_cast<int>(vp.size()), static_cast<int>(vq.size()));
}

int main()
{
    Cnt pool[80];
    for (int i = 0; i < 80; ++i) pool[i].v = i;
    Gen::pool = pool;
    sequence<std::vector<int> >("vector<int>");
    sequence<std::deque<int> >("deque<int>");
    sequence<std::list<int> >("list<int>");
    g_on = true;
    counted();
    g_on = false;
    throwing();
    strings();
    associative();
    others();
    std::printf("Cnt: live %d bad %d\n", Cnt::live - 80, Cnt::bad);
    return 0;
}
