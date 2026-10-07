// life.cpp - version 6 object lifetimes, compiled by cppparse --compile6 and driven by
// sample-life-driver/driver-life.cpp (built with MSVC): destructors at scope exit and on
// break/continue/return, copy construction and assignment through the class's own members,
// temporaries, by-value parameters, arrays, new[]/delete[], and destructors that run while
// an exception unwinds. No library headers: the counters are plain globals.

int g_ctor = 0;
int g_dtor = 0;
int g_copy = 0;
int g_assign = 0;
int g_allocs = 0;

int liveObjects() { return g_ctor - g_dtor; }
int liveAllocs()  { return g_allocs; }

struct Tracker {
    int id;
    Tracker() : id(0) { ++g_ctor; }
    Tracker(int i) : id(i) { ++g_ctor; }
    Tracker(const Tracker& o) : id(o.id) { ++g_ctor; ++g_copy; }
    ~Tracker() { ++g_dtor; }
    Tracker& operator=(const Tracker& o) { id = o.id; ++g_assign; return *this; }
};

int scopes()                               // 210: two live inside, one in the middle, none after
{
    int order = 0;
    {
        Tracker a(1);
        {
            Tracker b(2);
            order = order * 10 + liveObjects();
        }
        order = order * 10 + liveObjects();
    }
    return order * 10 + liveObjects();
}

int loops(int n)                           // continue and break destroy the body's object: 200 for n = 5
{
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        Tracker t(i);
        if (i == 1) continue;
        if (i == 3) break;
        sum += t.id;
    }
    return sum * 100 + liveObjects();
}

int earlyReturn(int v)                     // 8 for v = 4, with both objects destroyed on the way out
{
    Tracker a(v);
    if (v > 0) {
        Tracker b(v);
        return b.id + a.id;
    }
    return -1;
}

int copies()                               // 217: two copy constructions, one assignment, id 7
{
    int c0 = g_copy, a0 = g_assign;
    Tracker a(7);
    Tracker b = a;
    Tracker c(a);
    b = c;
    return (g_copy - c0) * 100 + (g_assign - a0) * 10 + b.id;
}

static int idOf(Tracker t)                 // by value: the callee destroys its copy
{
    return t.id;
}

static Tracker make(int i)                 // copied into the hidden result object, t destroyed
{
    Tracker t(i);
    return t;
}

int byValue()                              // 2208: two live objects, two copies, ids 3 + 5
{
    int c0 = g_copy;
    Tracker a(3);
    int s = idOf(a);
    Tracker m = make(5);
    s += m.id;
    return liveObjects() * 1000 + (g_copy - c0) * 100 + s;
}

int temporaries()                          // 36: three destructions by the end of the statement, 4 + 2
{
    int d0 = g_dtor;
    int v = make(4).id + Tracker(2).id;
    return (g_dtor - d0) * 10 + v;
}

struct Text {
    char* data;
    int   len;
    Text(const char* s)
    {
        len = 0;
        while (s[len]) ++len;
        data = new char[len + 1];
        for (int i = 0; i <= len; ++i) data[i] = s[i];
        ++g_allocs;
    }
    Text(const Text& o)
    {
        len = o.len;
        data = new char[len + 1];
        for (int i = 0; i <= len; ++i) data[i] = o.data[i];
        ++g_allocs;
    }
    ~Text()
    {
        delete[] data;
        --g_allocs;
    }
    Text& operator=(const Text& o)
    {
        if (this != &o) {
            delete[] data;
            len = o.len;
            data = new char[len + 1];
            for (int i = 0; i <= len; ++i) data[i] = o.data[i];
        }
        return *this;
    }
    int sum() const
    {
        int s = 0;
        for (int i = 0; i < len; ++i) s += data[i];
        return s;
    }
};

struct Pair {                              // implicit copy constructor, assignment and destructor: memberwise
    Text first;
    Text second;
    Pair(const char* a, const char* b) : first(a), second(b) {}
};

int deepCopies()                           // 3946: 'a'+'b'+'c'+'d' = 394 in r, six live buffers
{
    Pair p("ab", "cd");
    Pair q = p;
    Pair r("x", "y");
    r = q;
    int s = r.first.sum() + r.second.sum();
    return s * 10 + g_allocs;
}

int arrays()                               // 563: five constructed, five destroyed, 1 + 3 + 9
{
    int d0 = g_dtor, c0 = g_ctor;
    int s = 0;
    {
        Tracker arr[3] = { Tracker(1), Tracker(2), Tracker(3) };
        Tracker* heap = new Tracker[2];
        heap[1].id = 9;
        s = arr[0].id + arr[2].id + heap[1].id;
        delete[] heap;
    }
    return (g_ctor - c0) * 100 + (g_dtor - d0) * 10 + s;
}

static void thrower(int v)                 // t is destroyed while the exception unwinds
{
    Tracker t(v);
    if (v > 0) throw v;
}

int unwinding()                            // 372: three destructions, caught 2 and 7
{
    int d0 = g_dtor;
    int caught = 0;
    try {
        Tracker a(1);
        thrower(2);
        caught = -1;
    } catch (int e) {
        caught = e;
    }
    try {
        Text t("abc");
        throw Tracker(7);
    } catch (const Tracker& t) {
        caught += t.id * 10;
    }
    return (g_dtor - d0) * 100 + caught;
}

struct Fragile {                           // a constructor body that throws: the built members are destroyed
    Text    name;
    Tracker tag;
    Fragile(int v) : name("frag"), tag(v) { if (v < 0) throw v; }
};

int ctorThrows()                           // 99: one destruction, no buffer left, caught -1
{
    int d0 = g_dtor, a0 = g_allocs;
    int r = 0;
    try {
        Fragile f(-1);
        r = 1;
    } catch (int e) {
        r = e;
    }
    return (g_dtor - d0) * 100 + (g_allocs - a0) * 10 + r;
}
