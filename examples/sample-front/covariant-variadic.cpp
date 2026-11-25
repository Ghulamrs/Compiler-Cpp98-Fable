// covariant-variadic.cpp - a variadic function overridden with a covariant return type whose conversion moves the
// pointer (rows 19, B11, G3): the slot of the overridden function cannot hold a thunk that calls the override (the
// unnamed arguments cannot be passed on), so it holds a second copy of the override's body that returns the
// overridden function's type. Its static locals are the function's own objects, shared by the two bodies (row G9):
// CD3::w is reached through SB3's slot (the second body) before it is called directly, and its Counted is built once
// and destroyed once, after main. Test 17 compares the output with the lines below: cl refuses the program (C2688
// "covariant returns with multiple or virtual inheritance not supported for varargs functions").
// Expected output:
//   covvar 2 60 21 1
//   covvar 2 11 9
//   covvar 2 115 109 1 5
//   covvar static 4101 4102 4103 1
//   covvar dropped 41 1
#include <cstdio>
#include <cstdarg>
struct SA { virtual ~SA() {} int a; SA() : a(1) {} };
struct X { int x; X() : x(2) {} };
struct Y : SA, X { int y; Y() : y(3) {} };
struct SB2 {
    virtual X* v(int n, ...) { (void)n; return 0; }
    virtual X& r(const char* f, ...) { static X sx; (void)f; return sx; }
    virtual ~SB2() {}
    int b;
};
struct CD2 : SB2 {
    Y yy;
    int sum;
    Y* v(int n, ...)
    {
        va_list ap; va_start(ap, n);
        sum = 0;
        for (int i = 0; i < n; ++i) sum += va_arg(ap, int);
        va_end(ap);
        if (n < 0) return 0;
        yy.y = sum;
        return &yy;
    }
    Y& r(const char* f, ...)
    {
        va_list ap; va_start(ap, f);
        double d = 0;
        for (; *f; ++f) { if (*f == 'd') d += va_arg(ap, double); else d += va_arg(ap, int); }
        va_end(ap);
        yy.y = static_cast<int>(d);
        return yy;
    }
};
struct V2 { virtual int other() { return 5; } int k; V2() : k(6) {} virtual ~V2() {} };
struct MD : V2, CD2 {
    Y* v(int n, ...)
    {
        va_list ap; va_start(ap, n);
        int s = 100;
        for (int i = 0; i < n; ++i) s += va_arg(ap, int);
        va_end(ap);
        yy.y = s;
        return &yy;
    }
};
static int g_built = 0;
struct Counted {
    int id;
    Counted(int i) : id(i) { ++g_built; }
    ~Counted() { std::printf("covvar dropped %d %d\n", id, g_built); }
};
static int nextId() { static int n = 40; return ++n; }
struct SB3 { virtual X* w(int n, ...) { (void)n; return 0; } virtual ~SB3() {} };
struct CD3 : SB3 {
    Y yy;
    Y* w(int n, ...)
    {
        static Counted once(nextId());   // a guarded initialisation and a destructor at exit
        static int calls = 0;            // static data
        (void)n;
        yy.y = once.id * 100 + ++calls;
        return &yy;
    }
};

int main()
{
    CD2 d; SB2& b = d;
    X* p = b.v(3, 10, 20, 30);
    const int viaBase = static_cast<Y*>(p)->y;
    Y* q = d.v(6, 1, 2, 3, 4, 5, 6);
    const bool none = b.v(-1) == 0;
    std::printf("covvar %d %d %d %d\n", p->x, viaBase, q->y, none);
    X& rx = b.r("idid", 1, 2.5, 3, 4.5);
    const int mixed = static_cast<Y&>(rx).y;
    Y& ry = d.r("iiiiii", 1, 1, 1, 2, 2, 2);
    std::printf("covvar %d %d %d\n", rx.x, mixed, ry.y);
    MD m; SB2& mb = m; CD2& mc = m;
    X* mp = mb.v(2, 7, 8);
    const int viaSb = static_cast<Y*>(mp)->y;
    Y* mq = mc.v(1, 9);
    const int viaCd = mq->y;
    const bool same = static_cast<X*>(mq) == mp;
    std::printf("covvar %d %d %d %d %d\n", mp->x, viaSb, viaCd, same, m.other());
    CD3 e; SB3& eb = e;
    const int s1 = static_cast<Y*>(eb.w(0))->y;   // the second body, through SB3's slot: it builds the statics
    const int s2 = e.w(0)->y;                     // the function itself: the same objects, built already
    const int s3 = static_cast<Y*>(eb.w(0))->y;
    std::printf("covvar static %d %d %d %d\n", s1, s2, s3, g_built);
    return 0;
}
