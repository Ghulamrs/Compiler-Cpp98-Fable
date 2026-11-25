// vector.cpp - <vector> copy-constructs, assigns and destroys its elements as 23.2.4 says (row P9); built by cl with
// its own headers (the reference) and by cppparse --compile6 with include/, test 15 compares what the programs print
// at -O0 and -O2.
// Self holds a pointer into itself, which only its own copy constructor keeps right; Cnt counts its constructions,
// copies, assignments and destructions and knows a living object from raw or destroyed storage. Each line shows the
// elements, the number of living objects, the number of broken ones (0) and, for Cnt, the counts of the step.
// The counts are printed where 23.2.4 fixes them or the two libraries agree on the usual way; a step marked "open"
// prints none, since the standard leaves them open and cl's library goes another way: growth from nothing (the
// growth factor), insert of a range in place (cl destroys and constructs where this library assigns), assign and
// operator= (cl assigns over the elements it has, C++98 23.2.4.1 says erase and insert).
#include <cstdio>
#include <vector>

struct Self {
    static int live, bad;
    int v;
    const int* p;
    Self() : v(0), p(&v) { ++live; }
    Self(const Self& o) : v(o.v), p(&v) { if (o.p != &o.v) ++bad; ++live; }
    Self& operator=(const Self& o) { if (p != &v || o.p != &o.v) ++bad; v = o.v; return *this; }
    ~Self() { if (p != &v) ++bad; p = 0; --live; }
};
int Self::live = 0;
int Self::bad = 0;

struct Cnt {
    static int live, bad, made, copied, assigned, destroyed, fuse;
    int v;
    int state;
    Cnt() : v(0), state(0xC0DE) { ++made; ++live; }
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

static int pool[64];

static void set(Self& x, int i) { x.v = i; }
static void set(Cnt& x, int i) { x.v = i; }
static void set(int& x, int i) { x = i; }
static void set(double& x, int i) { x = i + 0.5; }
static void set(int*& x, int i) { x = pool + i; }
static int val(const Self& x) { return x.v; }
static int val(const Cnt& x) { return x.v; }
static int val(int x) { return x; }
static int val(double x) { return static_cast<int>(x * 2); }
static int val(int* x) { return x ? static_cast<int>(x - pool) : 0; }

// the tail of a line: the living objects, the broken ones, the counts since the line before
static void tally(const Self*, bool) { std::printf(" | live %d bad %d", Self::live, Self::bad); }
static void tally(const Cnt*, bool exact)
{
    std::printf(" | live %d bad %d", Cnt::live, Cnt::bad);
    if (exact) std::printf(" | made %d copied %d assigned %d destroyed %d", Cnt::made, Cnt::copied, Cnt::assigned, Cnt::destroyed);
    else std::printf(" | open");
    Cnt::made = Cnt::copied = Cnt::assigned = Cnt::destroyed = 0;
}
static void tally(const int*, bool) {}
static void tally(const double*, bool) {}
static void tally(int* const*, bool) {}

// (vector<T> through the using-declaration: deduction against the qualified std::vector<T> is row G8)
using std::vector;
template <class T>
static void show(const char* what, const vector<T>& v, bool exact)
{
    std::printf("  %-26s", what);
    for (size_t i = 0; i < v.size(); ++i) std::printf(" %d", val(v[i]));
    std::printf(" (%d)", static_cast<int>(v.size()));
    tally(static_cast<const T*>(0), exact);
    std::printf("\n");
}

template <class T>
static void scenario(const char* name)
{
    std::printf("%s\n", name);
    T src[12];
    for (int i = 0; i < 12; ++i) set(src[i], i + 1);
    {
        std::vector<T> v;
        show("empty", v, true);
        for (int i = 0; i < 10; ++i) v.push_back(src[i]);
        show("push_back, growing", v, false);
        v.reserve(40);
        show("reserve", v, true);
        v.reserve(8);
        show("reserve, smaller", v, true);
        v.push_back(src[10]);
        v.push_back(v[0]);
        show("push_back, room", v, true);

        std::vector<T> w;
        w.reserve(5);
        for (int i = 0; i < 5; ++i) w.push_back(src[i]);
        show("w: five in five", w, true);
        w.push_back(w[1]);
        show("w: push_back own, full", w, true);

        v.insert(v.begin() + 2, src[11]);
        show("insert one", v, true);
        v.insert(v.begin() + 1, v[5]);
        show("insert one, own", v, true);
        v.insert(v.end(), src[0]);
        show("insert one, end", v, true);
        v.insert(v.begin() + 3, 2, src[6]);
        show("insert 2, long tail", v, true);
        v.insert(v.end() - 2, 4, v[0]);
        show("insert 4 own, short tail", v, true);
        v.insert(v.end(), 3, src[7]);
        show("insert 3, end", v, true);
        v.insert(v.begin(), 0, src[7]);
        show("insert 0", v, true);
        v.insert(v.begin() + 4, src + 2, src + 5);
        show("insert range, long tail", v, false);
        v.insert(v.end() - 1, src + 8, src + 11);
        show("insert range, short tail", v, false);
        v.insert(v.end(), src, src + 2);
        show("insert range, end", v, true);
        v.insert(v.begin() + 1, src, src);
        show("insert range, empty", v, true);

        std::vector<T> x;
        x.reserve(4);
        for (int i = 0; i < 4; ++i) x.push_back(src[i]);
        x.insert(x.begin() + 1, x[3]);
        show("x: insert one own, full", x, true);
        std::vector<T> y(x);
        show("y: copy of x", y, true);
        y.reserve(6);
        y.push_back(src[9]);
        y.insert(y.begin() + 2, 3, y[0]);
        show("y: insert 3 own, full", y, true);
        std::vector<T> z(src, src + 3);
        show("z: from a range", z, true);
        z.insert(z.begin() + 1, src + 5, src + 9);
        show("z: insert range, full", z, true);

        v.erase(v.begin() + 3);
        show("erase one", v, true);
        v.erase(v.begin());
        show("erase first", v, true);
        v.erase(v.end() - 1);
        show("erase last", v, true);
        v.erase(v.begin() + 2, v.begin() + 7);
        show("erase range", v, true);
        v.erase(v.begin() + 4, v.begin() + 4);
        show("erase range, empty", v, true);
        v.erase(v.end() - 3, v.end());
        show("erase range, to the end", v, true);
        v.pop_back();
        show("pop_back", v, true);

        v.resize(v.size() + 3);
        show("resize, more", v, true);
        v.resize(v.size() + 2, v[1]);
        show("resize, more of own", v, true);
        v.resize(6);
        show("resize, fewer", v, true);
        z.resize(z.size() + 30, z[2]);
        z.erase(z.begin() + 9, z.end());
        show("z: resize own, full", z, true);
        z.resize(100);
        z.erase(z.begin() + 10, z.end() - 1);
        show("z: resize, full", z, true);

        v.swap(w);
        show("swap", v, true);
        show("w", w, true);
        w.assign(3, src[4]);
        show("assign 3", w, false);
        w.assign(src + 1, src + 10);
        show("assign range", w, false);
        w.assign(60, src[5]);
        w.erase(w.begin() + 2, w.end());
        show("assign 60", w, false);
        w = x;
        show("w = x", w, false);
        x = x;
        show("x = x", x, false);
        y.clear();
        show("clear", y, true);
        y.push_back(src[3]);
        show("push_back after clear", y, true);
        std::vector<T> none(y);
        none.clear();
        std::vector<T> copyOfNone(none);
        y = none;
        show("y = empty", y, false);
        show("v at the end", v, true);
    }
    std::vector<T> gone;
    show("all destroyed", gone, true);
}

// a copy that throws: push_back and reserve have no effect (23.1/10, 23.2.4.2); elsewhere the vector keeps valid
// elements and nothing is lost or destroyed twice (their number and values are open, so only the balance is shown)
static void throwing()
{
    std::printf("a copy throws\n");
    Cnt src[4];
    for (int i = 0; i < 4; ++i) src[i].v = i + 1;
    int base = Cnt::live;
    {
        std::vector<Cnt> v;
        v.reserve(4);
        for (int i = 0; i < 4; ++i) v.push_back(src[i]);
        const Cnt* at = &v[0];
        Cnt::fuse = 3;
        try { v.push_back(v[2]); std::printf("  no exception\n"); } catch (int e) { std::printf("  push_back: caught %d,", e); }
        std::printf(" same elements %d, size %d, lost %d bad %d\n", static_cast<int>(at == &v[0] && v.capacity() == 4), static_cast<int>(v.size()), Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        Cnt::fuse = 4;
        try { v.reserve(9); std::printf("  no exception\n"); } catch (int e) { std::printf("  reserve: caught %d,", e); }
        std::printf(" same elements %d, size %d, lost %d bad %d\n", static_cast<int>(at == &v[0] && v.capacity() == 4), static_cast<int>(v.size()), Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        Cnt::fuse = 2;
        try { v.insert(v.begin() + 1, src[3]); std::printf("  no exception\n"); } catch (int e) { std::printf("  insert, full: caught %d,", e); }
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        v.reserve(20);
        Cnt::fuse = 2;
        try { v.insert(v.begin() + 1, 3, src[3]); std::printf("  no exception\n"); } catch (int e) { std::printf("  insert 3: caught %d,", e); }
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        Cnt::fuse = 3;
        try { v.insert(v.begin() + 1, src, src + 4); std::printf("  no exception\n"); } catch (int e) { std::printf("  insert range: caught %d,", e); }
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        Cnt::fuse = 2;
        try { v.resize(v.size() + 30, src[0]); std::printf("  no exception\n"); } catch (int e) { std::printf("  resize, full: caught %d,", e); }
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        Cnt::fuse = 2;
        try { v.assign(v.size() + 30, src[1]); std::printf("  no exception\n"); } catch (int e) { std::printf("  assign: caught %d,", e); }
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        v.assign(src, src + 4);
        Cnt::fuse = 2;
        try { std::vector<Cnt> w(v); std::printf("  no exception\n"); } catch (int e) { std::printf("  copy: caught %d,", e); }
        std::printf(" lost %d bad %d\n", Cnt::live - base - static_cast<int>(v.size()), Cnt::bad);
        Cnt::fuse = 0;
    }
    std::printf("  after: lost %d bad %d\n", Cnt::live - base, Cnt::bad);
}

int main()
{
    for (int i = 0; i < 64; ++i) pool[i] = i;
    scenario<Self>("Self, a pointer into itself");
    scenario<Cnt>("Cnt, counted");
    scenario<int>("int");
    scenario<double>("double");
    scenario<int*>("int*");
    throwing();
    std::printf("Self: live %d bad %d; Cnt: live %d bad %d\n", Self::live, Self::bad, Cnt::live, Cnt::bad);
    return 0;
}
