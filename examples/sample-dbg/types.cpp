// types.cpp - sample-dbg: the kinds of type a debugger shows, and what the program itself knows of each (test 20).
// Built with -g, linked with /DEBUG and run, it prints the facts of every type below: its size, each member's
// offset (measured in a real object), a bit-field's bits (found by storing ones into it), each base's offset, an
// enumerator's value. sample-dbg-driver\dbgtypes.cpp reads the same facts from the PDB through dbghelp, and the
// test asks that every line printed here is one of the driver's: the program is the oracle, no expected number is
// kept in a script. The same source through cl /Z7 gives the driver's whole description of the types a second
// time, which must be the same text.
#include "types.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace geom {

enum Color { Red, Green = 5, Blue, Negative = -3, Big = 70000 };
struct Point { int x; int y; };
union Bits { int i; float f; unsigned char b[4]; };
struct Flags { unsigned a : 3; unsigned b : 5; int c : 8; unsigned : 4; unsigned d : 9; unsigned char e : 2; };

class Shape {
public:
    Shape(int id) : kind(Plain), m_id(id) { tag.c = 's'; tag.s = 1; ++count; }
    virtual ~Shape() {}
    virtual double area() const = 0;
    static int count;
    typedef int Id;
    struct Tag { char c; short s; };
    enum Kind { Plain, Fancy };
    Tag tag;
    Kind kind;
    static void report();
protected:
    int m_id;
};
int Shape::count = 0;

class Circle : public Shape {
public:
    Circle(int id, double r) : Shape(id), m_r(r) {}
    virtual double area() const { return 3.0 * m_r * m_r; }
    double scaled(double k) const { return m_r * k; }
    static void report();
private:
    double m_r;
};

struct VBase { int v; };
struct Left : virtual VBase { int l; };
struct Right : virtual VBase { int r; };
struct Both : Left, Right { int b; };

struct Second { virtual ~Second() {} virtual int second() { return 2; } int s; };
class Multi : public Circle, public Second {
public:
    Multi() : Circle(1, 1.0), extra(0) {}
    void touch() { ++extra; }
    int extra;
};

struct Empty {};
struct EmptyDerived : Empty { char c; };
struct Anon { int tag; union { int i; float f; }; struct { short lo; short hi; }; double d; };

struct Scalars {
    bool b; char c; signed char sc; unsigned char uc; wchar_t w; short s; unsigned short us; int i; unsigned u;
    long l; unsigned long ul; long long ll; unsigned long long ull; float f; double d; long double ld;
    void* p; const char* cs; int* const cp; int& r; int arr[3][4]; volatile int vi; const double cd;
    Scalars(int& x) : cp(0), r(x), cd(1.5) {}
};

struct Pointers {
    int (*fn)(int, int);
    double (Circle::*mf)(double) const;
    int Point::*md;
    int Both::*vmd;
    void (Multi::*mmf)();
    const volatile int* cv;
    Shape* shape;
    Point pts[2];
    Color col;
    Point** pp;
    const Point& (*ref)(const Point&);
};

void Shape::report()
{
    Circle c(1, 2.0);
    Shape& s = c;
    SIZE("geom::Shape", Shape); SIZE("geom::Shape::Tag", Tag);
    MEMBER("geom::Shape", s, tag); MEMBER("geom::Shape", s, kind); MEMBER("geom::Shape", s, m_id);
    MEMBER("geom::Shape::Tag", s.tag, c); MEMBER("geom::Shape::Tag", s.tag, s);
    factEnumerator("geom::Shape::Kind", "Plain", Plain); factEnumerator("geom::Shape::Kind", "Fancy", Fancy);
}

void Circle::report()
{
    Circle c(1, 2.0);
    SIZE("geom::Circle", Circle);
    MEMBER("geom::Circle", c, m_r);
    BASE("geom::Circle", c, Shape, "geom::Shape");
}

} // namespace geom

// The member functions a class's type lists (stage 4 of docs/design-codeview.md): each kind once. The driver
// sample-dbg-driver\diatypes.cpp reads them from the PDB through DIA, and the lines must be the ones of cl /Z7's build
// of this source, what cl generates itself apart (implicit constructors and operator=, __vecDelDtor, the thunk of a
// covariant override).
namespace mem {

struct Value { int v; };

class Ops {
public:
    Ops() : m_n(0) {}
    explicit Ops(int n) : m_n(n) {}
    Ops(const Ops& o) : m_n(o.m_n) {}
    ~Ops() {}
    Ops& operator=(const Ops& o) { m_n = o.m_n; return *this; }
    Ops& operator+=(int d) { m_n += d; return *this; }
    Ops operator+(const Ops& o) const { return Ops(m_n + o.m_n); }
    bool operator==(const Ops& o) const { return m_n == o.m_n; }
    int operator()(int a, int b) const { return a + b + m_n; }
    int operator[](int i) const { return m_n + i; }
    Ops& operator++() { ++m_n; return *this; }
    Ops operator++(int) { Ops old(*this); ++m_n; return old; }
    Ops* operator->() { return this; }
    void* operator new(size_t bytes) { return std::malloc(bytes); }
    void operator delete(void* p) { std::free(p); }
    // conversion functions: their names hold the type as a declaration reads
    operator int() const { return m_n; }
    operator const char*() const { return "ops"; }
    operator Value() const { Value x; x.v = m_n; return x; }
    operator const Value*() const { return 0; }
    operator geom::Color() const { return geom::Red; }
    // an overloaded name: one entry, its functions in a list
    void set(int n) { m_n = n; }
    void set(int a, int b) { m_n = a + b; }
    void set(double d) { m_n = static_cast<int>(d); }
    int get() const { return m_n; }
    Value value() const { Value x; x.v = m_n; return x; }      // a class result of a member: the C++ way
    int sum(int first, ...) const { return first + m_n; }
    static Ops make(int n) { return Ops(n); }
    static Value plain() { Value x; x.v = 1; return x; }       // of a static member: a plain aggregate, in rax
    static int count() { return 3; }
    static int count(int more) { return 3 + more; }
protected:
    void guarded() { ++m_n; }
private:
    void hidden() { --m_n; }
    int m_n;
};

struct Iface {
    virtual ~Iface() {}
    virtual int first() = 0;
    virtual int first(int scale) = 0;
    virtual int second() const { return 2; }
    int i;
};
struct Other {
    virtual int other() { return 1; }
    virtual void pure() const = 0;
    int o;
};
// other and pure are first held by Other's table, the second: they are called with this at Other
struct Impl : Iface, Other {
    Impl() : z(0) {}
    virtual int first() { return 1; }
    virtual int first(int scale) { return scale; }
    virtual int other() { return 5; }
    virtual void pure() const {}
    virtual int fresh() { return 6; }
    int z;
};
struct Abstract : Other {
    virtual void pure() const = 0;      // an override that is pure
    virtual int more() = 0;             // pure, with a slot of its own
};
struct Concrete : Abstract {
    virtual void pure() const {}
    virtual int more() { return 7; }
};

struct VRoot { virtual int root() { return 1; } int vr; };
struct VLeft : virtual VRoot { virtual int root() { return 2; } virtual int left() { return 2; } int vl; };
struct VRight : virtual VRoot { virtual int right() { return 3; } int vrt; };
struct VJoin : VLeft, VRight {
    VJoin() : vj(0) {}
    explicit VJoin(int v) : vj(v) {}
    virtual int root() { return 4; }
    virtual int right() { return 5; }
    virtual int join() { return 6; }
    int vj;
};

// a class that names a virtual base before the polymorphic base whose function it overrides (next-targets, row G45):
// run is first held by the table at 0, RunP's, and is called with this at the object (adjustment 0); stay is held by
// the table inside RunV alone
struct RunV { virtual int run() { return 1; } virtual int stay() { return 2; } int rv; };
struct RunP { virtual int run() { return 3; } int rp; };
struct RunJ : virtual RunV, RunP {
    virtual int run() { return 4; }
    virtual int stay() { return 5; }
    int rj;
};

struct Self { virtual Self* self() { return this; } int s; };
struct SelfDerived : Other, Self {
    virtual SelfDerived* self() { return this; }    // covariant, Self not at 0
    virtual void pure() const {}
    int sd;
};

template <class T> struct Holder {
    T v;
    Holder() : v() {}
    explicit Holder(const T& x) : v(x) {}
    T get() const { return v; }
    void put(const T& x) { v = x; }
    static T zero() { return T(); }
    virtual T vget() { return v; }
    operator T() const { return v; }
    struct Slot { T held; T take() const { return held; } };
};

union Either {
    int i; float f;
    Either() : i(0) {}
    int asInt() const { return i; }
    void setFloat(float x) { f = x; }
};

struct Nest {
    struct In { int inner(int a) { return a + n; } static int count() { return 1; } int n; };
    int outer(In* p) { return p ? p->n : m; }
    int m;
};

} // namespace mem

template <class T> struct Box { T v; struct Inner { T w; char tail; }; Inner in; static T shared; };
template <class T> T Box<T>::shared;

namespace { struct Node { int a; char b; }; }   // types2.cpp has a Node of another layout in its own unnamed namespace

typedef geom::Point PointAlias;
typedef unsigned long long Size64;
typedef int (*BinOp)(int, int);
typedef shared::Arr<double, 3> ArrOfDouble;

int g_counter = 7;
static int s_hidden = 3;
const int kLimit = 40;
geom::Color g_color = geom::Green;
geom::Point g_point = { 1, 2 };
geom::Bits g_bits;
geom::Flags g_flags;
geom::Both g_both;
geom::Multi g_multi;
geom::Empty g_empty;
geom::EmptyDerived g_emptyDerived;
geom::Anon g_anon;
geom::Scalars g_scalars(g_counter);
geom::Pointers g_pointers;
Box<int> g_box;
Box<geom::Point> g_boxPoint;
Node g_node1;
int g_table[3][4];
const char* g_name = "types";
PointAlias g_alias = { 3, 4 };
Size64 g_size64 = 5;
BinOp g_op = 0;
shared::Pair g_pair1 = { 1, 2.0 };
shared::Arr<double, 3> g_arr1;
shared::Counter g_counter1;
shared::Mode g_mode1 = shared::On;
mem::Value g_value = { 9 };
mem::Ops g_ops;
mem::Impl g_impl;
mem::Concrete g_concrete;
mem::VJoin g_join;
mem::SelfDerived g_selfDerived;
mem::RunJ g_runJ;
mem::Holder<int> g_holderInt;
mem::Holder<geom::Point> g_holderPoint;
mem::Holder<int>::Slot g_slot;
mem::Either g_either;
mem::Nest g_nest;
mem::Nest::In g_nestIn;

void factSize(const char* type, unsigned long long bytes) { std::printf("size %s %d\n", type, (int)bytes); }
void factMember(const char* type, const char* member, long long offset) { std::printf("member %s %s %d\n", type, member, (int)offset); }
void factBase(const char* type, const char* base, long long offset) { std::printf("base %s %s %d\n", type, base, (int)offset); }
void factEnumerator(const char* type, const char* name, long long value) { std::printf("enumerator %s %s %d\n", type, name, (int)value); }
void factGlobal(const char* name, unsigned long long bytes) { std::printf("global %s %d\n", name, (int)bytes); }
void factLocal(const char* function, const char* variable, unsigned long long bytes) { std::printf("local %s %s %d\n", function, variable, (int)bytes); }

// the object holds zeros but for the bit-field, which holds ones: its first bit and its width
void factBits(const char* type, const char* member, const void* object, unsigned long long bytes)
{
    const unsigned char* p = static_cast<const unsigned char*>(object);
    int first = -1, width = 0;
    for (int bit = 0; bit < (int)bytes * 8; ++bit)
        if (p[bit / 8] & (1 << (bit % 8))) { if (first < 0) first = bit; ++width; }
    std::printf("bits %s %s %d %d\n", type, member, first, width);
}

#define BITS(name, T, m) { T object; std::memset(&object, 0, sizeof object); object.m -= 1; factBits(name, #m, &object, sizeof object); }

static int add(int a, int b) { return a + b; }

namespace mem {

const int kMembers = 138;

// the classes above, by the facts the driver compares, and their functions called once
static int report()
{
    SIZE("mem::Value", Value); MEMBER("mem::Value", g_value, v);
    SIZE("mem::Ops", Ops);
    SIZE("mem::Iface", Iface); MEMBER("mem::Iface", static_cast<Iface&>(g_impl), i);
    SIZE("mem::Other", Other); MEMBER("mem::Other", static_cast<Other&>(g_impl), o);
    SIZE("mem::Impl", Impl); MEMBER("mem::Impl", g_impl, z);
    BASE("mem::Impl", g_impl, Iface, "mem::Iface"); BASE("mem::Impl", g_impl, Other, "mem::Other");
    SIZE("mem::Abstract", Abstract); SIZE("mem::Concrete", Concrete);
    SIZE("mem::VRoot", VRoot); SIZE("mem::VLeft", VLeft); SIZE("mem::VRight", VRight); SIZE("mem::VJoin", VJoin);
    MEMBER("mem::VJoin", g_join, vj); BASE("mem::VJoin", g_join, VLeft, "mem::VLeft"); BASE("mem::VJoin", g_join, VRight, "mem::VRight");
    SIZE("mem::RunV", RunV); SIZE("mem::RunP", RunP); SIZE("mem::RunJ", RunJ); MEMBER("mem::RunJ", g_runJ, rj);
    BASE("mem::RunJ", g_runJ, RunP, "mem::RunP");
    SIZE("mem::Self", Self); SIZE("mem::SelfDerived", SelfDerived); MEMBER("mem::SelfDerived", g_selfDerived, sd);
    BASE("mem::SelfDerived", g_selfDerived, Other, "mem::Other"); BASE("mem::SelfDerived", g_selfDerived, Self, "mem::Self");
    SIZE("mem::Holder<int>", Holder<int>); MEMBER("mem::Holder<int>", g_holderInt, v);
    SIZE("mem::Holder<geom::Point>", Holder<geom::Point>); MEMBER("mem::Holder<geom::Point>", g_holderPoint, v);
    SIZE("mem::Holder<int>::Slot", Holder<int>::Slot); MEMBER("mem::Holder<int>::Slot", g_slot, held);
    SIZE("mem::Either", Either); MEMBER("mem::Either", g_either, i); MEMBER("mem::Either", g_either, f);
    SIZE("mem::Nest", Nest); MEMBER("mem::Nest", g_nest, m);
    SIZE("mem::Nest::In", Nest::In); MEMBER("mem::Nest::In", g_nestIn, n);
    GLOBAL(g_value); GLOBAL(g_ops); GLOBAL(g_impl); GLOBAL(g_concrete); GLOBAL(g_join); GLOBAL(g_selfDerived); GLOBAL(g_holderInt);
    GLOBAL(g_holderPoint); GLOBAL(g_slot); GLOBAL(g_either); GLOBAL(g_nest); GLOBAL(g_nestIn); GLOBAL(g_runJ);
    // what the functions give, through the tables where they are virtual: kMembers in all
    Ops a(2);
    a.set(1); a.set(1, 2); a.set(4.0); a += 1; ++a; a++;                         // 7
    Ops b = a + Ops::make(1);                                                     // 8
    int sum = a.get() + b.get() + a(1, 1) + a[1] + static_cast<int>(a) + a.value().v + Ops::plain().v + Ops::count() + Ops::count(1) + a.sum(1, 2) + a->get();
    Iface& iface = g_impl; Other& other = g_impl; Other& made = g_concrete;
    VRoot& root = g_join; VRight& right = g_join; Self& self = g_selfDerived;
    sum += iface.first() + iface.first(2) + iface.second() + other.other() + g_impl.fresh() + static_cast<Abstract&>(g_concrete).more();
    other.pure(); made.pure();
    sum += root.root() + right.right() + g_join.join() + g_join.left() + (self.self() == &g_selfDerived ? 1 : 0);
    RunP& runP = g_runJ; RunV& runV = g_runJ;
    sum += runP.run() + runV.run() + runV.stay();                                // 4 + 4 + 5
    g_holderInt.put(3); g_slot.held = 2; g_nestIn.n = 1; g_either.setFloat(0.0f);
    sum += g_holderInt.get() + g_holderInt.vget() + Holder<int>::zero() + static_cast<int>(g_holderInt) + g_slot.take() + g_holderPoint.get().x
         + g_either.asInt() + g_nest.outer(&g_nestIn) + g_nestIn.inner(1) + Nest::In::count();
    return (a == b ? 1 : 0) + sum;
}

} // namespace mem

namespace geom {

int reportAll()
{
    static int calls = 0;
    ++calls;
    SIZE("geom::Point", Point); MEMBER("geom::Point", g_point, x); MEMBER("geom::Point", g_point, y);
    SIZE("geom::Bits", Bits); MEMBER("geom::Bits", g_bits, i); MEMBER("geom::Bits", g_bits, f); MEMBER("geom::Bits", g_bits, b);
    SIZE("geom::Flags", Flags);
    BITS("geom::Flags", Flags, a) BITS("geom::Flags", Flags, b) BITS("geom::Flags", Flags, c) BITS("geom::Flags", Flags, d) BITS("geom::Flags", Flags, e)
    factEnumerator("geom::Color", "Red", Red); factEnumerator("geom::Color", "Green", Green); factEnumerator("geom::Color", "Blue", Blue);
    factEnumerator("geom::Color", "Negative", Negative); factEnumerator("geom::Color", "Big", Big);
    Shape::report();
    Circle::report();
    SIZE("geom::VBase", VBase); SIZE("geom::Left", Left); SIZE("geom::Right", Right); SIZE("geom::Both", Both);
    MEMBER("geom::Left", static_cast<Left&>(g_both), l); MEMBER("geom::Right", static_cast<Right&>(g_both), r); MEMBER("geom::Both", g_both, b);
    BASE("geom::Both", g_both, Left, "geom::Left"); BASE("geom::Both", g_both, Right, "geom::Right");
    SIZE("geom::Second", Second); MEMBER("geom::Second", static_cast<Second&>(g_multi), s);
    SIZE("geom::Multi", Multi); MEMBER("geom::Multi", g_multi, extra);
    BASE("geom::Multi", g_multi, Circle, "geom::Circle"); BASE("geom::Multi", g_multi, Second, "geom::Second");
    SIZE("geom::Empty", Empty); SIZE("geom::EmptyDerived", EmptyDerived); MEMBER("geom::EmptyDerived", g_emptyDerived, c);
    BASE("geom::EmptyDerived", g_emptyDerived, Empty, "geom::Empty");
    SIZE("geom::Anon", Anon); MEMBER("geom::Anon", g_anon, tag); MEMBER("geom::Anon", g_anon, i); MEMBER("geom::Anon", g_anon, f);
    MEMBER("geom::Anon", g_anon, lo); MEMBER("geom::Anon", g_anon, hi); MEMBER("geom::Anon", g_anon, d);
    SIZE("geom::Scalars", Scalars);
    MEMBER("geom::Scalars", g_scalars, b); MEMBER("geom::Scalars", g_scalars, c); MEMBER("geom::Scalars", g_scalars, sc); MEMBER("geom::Scalars", g_scalars, uc);
    MEMBER("geom::Scalars", g_scalars, w); MEMBER("geom::Scalars", g_scalars, s); MEMBER("geom::Scalars", g_scalars, us); MEMBER("geom::Scalars", g_scalars, i);
    MEMBER("geom::Scalars", g_scalars, u); MEMBER("geom::Scalars", g_scalars, l); MEMBER("geom::Scalars", g_scalars, ul); MEMBER("geom::Scalars", g_scalars, ll);
    MEMBER("geom::Scalars", g_scalars, ull); MEMBER("geom::Scalars", g_scalars, f); MEMBER("geom::Scalars", g_scalars, d); MEMBER("geom::Scalars", g_scalars, ld);
    MEMBER("geom::Scalars", g_scalars, p); MEMBER("geom::Scalars", g_scalars, cs); MEMBER("geom::Scalars", g_scalars, cp); MEMBER("geom::Scalars", g_scalars, arr);
    MEMBER("geom::Scalars", g_scalars, vi); MEMBER("geom::Scalars", g_scalars, cd);
    SIZE("geom::Pointers", Pointers);
    MEMBER("geom::Pointers", g_pointers, fn); MEMBER("geom::Pointers", g_pointers, mf); MEMBER("geom::Pointers", g_pointers, md); MEMBER("geom::Pointers", g_pointers, vmd);
    MEMBER("geom::Pointers", g_pointers, mmf); MEMBER("geom::Pointers", g_pointers, cv); MEMBER("geom::Pointers", g_pointers, shape); MEMBER("geom::Pointers", g_pointers, pts);
    MEMBER("geom::Pointers", g_pointers, col); MEMBER("geom::Pointers", g_pointers, pp); MEMBER("geom::Pointers", g_pointers, ref);
    SIZE("Box<int>", Box<int>); MEMBER("Box<int>", g_box, v); MEMBER("Box<int>", g_box, in); MEMBER("Box<int>::Inner", g_box.in, w); MEMBER("Box<int>::Inner", g_box.in, tail);
    SIZE("Box<geom::Point>", Box<Point>); MEMBER("Box<geom::Point>", g_boxPoint, v); MEMBER("Box<geom::Point>", g_boxPoint, in);
    SIZE("shared::Pair", shared::Pair); MEMBER("shared::Pair", g_pair1, first); MEMBER("shared::Pair", g_pair1, second);
    SIZE("shared::Arr<double,3>", ArrOfDouble); MEMBER("shared::Arr<double,3>", g_arr1, a); MEMBER("shared::Arr<double,3>", g_arr1, count);
    SIZE("shared::Counter", shared::Counter); MEMBER("shared::Counter", g_counter1, m_n);
    factEnumerator("shared::Mode", "Off", shared::Off); factEnumerator("shared::Mode", "On", shared::On); factEnumerator("shared::Mode", "Auto", shared::Auto);
    GLOBAL(g_counter); GLOBAL(s_hidden); GLOBAL(g_color); GLOBAL(g_point); GLOBAL(g_bits); GLOBAL(g_flags); GLOBAL(g_both); GLOBAL(g_multi);
    GLOBAL(g_anon); GLOBAL(g_scalars); GLOBAL(g_pointers); GLOBAL(g_box); GLOBAL(g_boxPoint); GLOBAL(g_node1); GLOBAL(g_table); GLOBAL(g_name);
    GLOBAL(g_alias); GLOBAL(g_size64); GLOBAL(g_op); GLOBAL(g_pair1); GLOBAL(g_arr1); GLOBAL(g_counter1); GLOBAL(g_mode1);
    factGlobal("geom::Shape::count", sizeof(Shape::count)); factGlobal("Box<int>::shared", sizeof(Box<int>::shared));
    // the members of this unit's Node, through its variable (the second unit has another Node)
    factMember("g_node1", "a", (const char*)&g_node1.a - (const char*)&g_node1); factMember("g_node1", "b", (const char*)&g_node1.b - (const char*)&g_node1);
    const int members = mem::report();
    reportSecondUnit();
    // every variable is used, so that none is left out of the object
    g_pointers.fn = add; g_pointers.md = &Point::y; g_op = add; g_multi.touch(); Box<int>::shared = g_box.v;
    const int used = g_counter + s_hidden + kLimit + g_point.x + g_alias.y + (int)g_size64 + g_op(1, 2) + g_pointers.fn(1, 1) + g_table[1][2] + calls + g_name[0]
                   + (int)g_multi.area() + g_counter1.next() + g_mode1 + Shape::count + g_color + (int)sizeof(g_empty);
    if (members != mem::kMembers) std::printf("mem::report gives %d, not %d\n", members, mem::kMembers);
    return used != 0 && members == mem::kMembers ? 0 : 1;
}

} // namespace geom

int main()
{
    return geom::reportAll();
}
