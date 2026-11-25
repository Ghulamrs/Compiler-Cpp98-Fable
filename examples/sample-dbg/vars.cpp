// vars.cpp - the value harness of row M10 (docs/design-codeview.md section 6): functions of every shape the design's
// rules name call checkpoint() with what their variables hold, as text. sample-dbg-driver\dbgvars.cpp runs the
// program under the Win32 debug API and compares, at every checkpoint, the variables the PDB gives a place for
// (dbghelp) with that text: in the checkpoint's caller, in the frames above it (each announced with ckPush), and in
// the caller again at the first address of the next line. checkpoint, ckPush and ckPop are in varsck.cpp (cl).
//
// A text: "fn=<function>;P=<the declared parameters in order>;<name>=<value>;..." where a value is a number, "?"
// (listed with a place, not compared) or a name is "-<name>" (listed without a place). "<name>.<member>" and
// "<name>-><member>" read a member. Every variable in scope is named. The line after a checkpoint changes nothing the
// text names.
#include <stdio.h>
#include <stdarg.h>

extern "C" void checkpoint(const char* text);
extern "C" void ckPush(const char* text);
extern "C" void ckPop();

int g_after = 0;

struct Point { int x; int y; };
struct Big { int a[4]; double d; };        // passed by address
struct Small { int s; short t; };          // passed in a register

// many parameters of many kinds
long long many(int a, double b, char c, bool d, float e, long long f, short g, unsigned char h)
{
    long long sum = a + f + g + h + static_cast<long long>(b * 2) + c + d + static_cast<long long>(e * 4);
    checkpoint("fn=many;P=a,b,c,d,e,f,g,h;a=1;b=2.5;c=99;d=1;e=0.25;f=-7;g=300;h=200;sum=600");
    g_after++;
    return sum;
}

// references, and class objects by value (one passed by address, one in a register)
int refs(int& r, const Point& p, Big big, Small sm)
{
    int total = r + p.x + big.a[2] + sm.s;
    checkpoint("fn=refs;P=r,p,big,sm;r=5;p.x=3;p.y=4;big.d=1.5;sm.s=7;sm.t=8;total=45");
    g_after++;
    return total;
}

// a parameter never read (no place at -O0), an unnamed one, one that is assigned
int params(int used, int unused, int, int changed)
{
    changed = used * 2;
    checkpoint("fn=params;P=used,unused,__formal,changed;used=6;-unused;-__formal;changed=12");
    g_after++;
    return changed;
}

// variadic: no record for "..."
int sum(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    int s = 0;
    for (int i = 0; i < count; ++i) s += va_arg(ap, int);
    va_end(ap);
    checkpoint("fn=sum;P=count;count=3;ap=?;s=60");
    g_after++;
    return s;
}

// this, and an override reached through a second polymorphic base (entered with an adjusted this)
struct Base1 { virtual ~Base1() {} int b1; };
struct Base2 { virtual int get(int k) { return k; } virtual ~Base2() {} int b2; };
struct Both : Base1, Base2 {
    int d;
    Both() { b1 = 1; b2 = 2; d = 9; }
    int get(int k);
    ~Both();
};

int Both::get(int k)
{
    int r = d + k;
    checkpoint("fn=Both::get;P=this,k;this=?;this->d=9;this->b1=1;this->b2=2;k=4;r=13");
    g_after++;
    return r;
}

Both::~Both()
{
    int gone = d - 1;
    checkpoint("fn=Both::~Both;P=this;this=?;this->d=9;gone=8");
    g_after++;
}

struct Counter {
    int n;
    int add(int k)
    {
        n += k;
        checkpoint("fn=Counter::add;P=this,k;this=?;this->n=12;k=2");
        g_after++;
        return n;
    }
};

// blocks: two siblings that declare one name, the first holding a try; a loop variable; nested blocks
int blocks(int n)
{
    int total = 0;
    {
        int x = n * 2;
        try {
            if (n < 0) throw n;
            total += x;
        } catch (int e) {
            total = e;
        }
        checkpoint("fn=blocks;P=n;n=3;total=6;x=6");
        g_after++;
    }
    {
        int x = n * 10;
        total += x;
        checkpoint("fn=blocks;P=n;n=3;total=36;x=30");
        g_after++;
    }
    for (int i = 0; i < 2; ++i) {
        int sq = i * i + 5;
        if (i == 1) {
            int inner = sq + 100;
            checkpoint("fn=blocks;P=n;n=3;total=36;i=1;sq=6;inner=106");
            g_after++;
        }
    }
    return total;
}

// nested blocks in one piece each: the plain form
int nested(int a)
{
    int top = a;
    {
        int mid = top + 1;
        {
            int deep = mid * 2;
            checkpoint("fn=nested;P=a;a=4;top=4;mid=5;deep=10");
            g_after++;
            top += deep;
        }
        top += mid;
    }
    return top;
}

// a static local
int counted()
{
    static int calls = 0;
    int now = ++calls + 40;
    checkpoint("fn=counted;P=;calls=1;now=41");
    g_after++;
    return now;
}

// an object with a destructor in a block; the destructor checks in, its caller above it
struct Noisy {
    int id;
    explicit Noisy(int i) : id(i) {}
    ~Noisy()
    {
        checkpoint(id == 1 ? "fn=Noisy::~Noisy;P=this;this=?;this->id=1" : "fn=Noisy::~Noisy;P=this;this=?;this->id=2");
        g_after++;
    }
};

int scoped(int k)
{
    int before = k + 1;
    {
        Noisy keep(1);
        int inside = before * 3;
        ckPush("fn=scoped;P=k;k=20;before=21;keep.id=1;inside=63");
    }
    ckPop();
    checkpoint("fn=scoped;P=k;k=20;before=21");
    g_after++;
    return before;
}

// handlers: a variable of a handler; a destructor run while an exception passes (its frame above: the function)
struct Err { int code; };

int thrower(int v)
{
    Noisy guard(2);
    int local = v + 1;
    ckPush("fn=thrower;P=v;v=5;guard.id=2;local=6;~e");
    if (v > 0) { Err e; e.code = v * 7; throw e; }
    ckPop();
    return local;
}

int handler(int v)
{
    int result = -1;
    try {
        result = thrower(v);
    } catch (const Err& err) {
        ckPop();
        int seen = err.code + 1;
        checkpoint("fn=handler;P=v;v=5;result=-1;err.code=35;seen=36");
        g_after++;
        result = seen;
    }
    return result;
}

// a catch (...) that resumes: the scalars of the function keep their values (the review's p3)
int resumes(int v)
{
    int observed = v;
    int extra = 0;
    try {
        observed = v + 1;
        if (v > 1) throw v;
        extra = 1;
    } catch (...) {
        extra = 2;
    }
    checkpoint("fn=resumes;P=v;v=2;observed=3;extra=2");
    g_after++;
    return observed + extra;
}

// floats and doubles, a bool and a char
double floats(float f, double d)
{
    float half = f / 2;
    double twice = d * 2;
    bool flag = d > f;
    char letter = 'A';
    checkpoint("fn=floats;P=f,d;f=1.5;d=-3.25;half=0.75;twice=-6.5;flag=0;letter=65");
    g_after++;
    return half + twice + flag + letter;
}

// a caller whose frame is above another's checkpoint
int leaf(int q)
{
    int w = q + 1;
    checkpoint("fn=leaf;P=q;q=7;w=8");
    g_after++;
    return w;
}

int outer(int depth)
{
    int mine = depth * 100;
    Point p;
    p.x = 11; p.y = 12;
    ckPush("fn=outer;P=depth;depth=7;mine=700;p.x=11;p.y=12;got=?");
    int got = leaf(depth);
    ckPop();
    return mine + got;
}

int main()
{
    Big big;
    big.a[0] = 0; big.a[1] = 0; big.a[2] = 30; big.a[3] = 0; big.d = 1.5;
    Small sm;
    sm.s = 7; sm.t = 8;
    Point p;
    p.x = 3; p.y = 4;
    int five = 5;
    long long r = many(1, 2.5, 'c', true, 0.25f, -7, 300, 200);
    r += refs(five, p, big, sm);
    r += params(6, 99, 42, 0);
    r += sum(3, 10, 20, 30);
    Both* both = new Both;
    Base2* second = both;
    r += second->get(4);
    Counter c;
    c.n = 10;
    r += c.add(2);
    r += blocks(3);
    r += counted();
    r += nested(4);
    r += scoped(20);
    r += handler(5);
    r += resumes(2);
    r += static_cast<long long>(floats(1.5f, -3.25));
    r += outer(7);
    delete both;
    printf("vars: %lld %d\n", r, g_after);
    return 0;
}
