// mangle.cpp - declarations whose MSVC decorated names test 15 compares between cl and cppparse
// --compile6 --msvc-mangling: classes and enums returned by value, enum parameters, repeated parameter
// types, maps and sets of strings, nested template arguments, function pointers and globals.
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mangle {

struct Item {
    int v;
    enum Kind { Small, Large };
};

enum Color { Red, Green };

template <class T> struct Holder { T* p; };

class Box {
public:
    std::string text() const;
    Item item();
    const Item constItem();
    const int constInt();
    Item::Kind kind(Item::Kind k, Color c) const;
    Color color(Color c, Item::Kind k);
    bool flags(bool a, bool b, long long c, long long d);
    void strings(const std::string& a, const std::string& b, std::string c, std::string d);
    void byValue(const std::string a, std::string b);
    void maps(std::map<std::string, Item*>& m, const std::set<std::string>& s, std::map<std::string, std::string>& mm);
    void nested(std::vector<std::pair<std::string, int> >& v, std::map<std::string, std::vector<Item*> >& m);
    void pointers(char* const p, const char* q, char* r, int* a, int* b);
    void callbacks(Item (*f)(Item, const std::string&), Item g, const std::string& h);
    std::pair<std::string, int> pairOf(std::pair<std::string, int> p);
    Holder<const std::string>* constArg(Holder<const std::string>* h);
    static Box make(const Box& b);
    static int count;
    static const char* names[3];
    static std::string label;
private:
    int m_value;
};

std::string Box::text() const { return "box"; }
Item Box::item() { Item i; i.v = m_value; return i; }
const Item Box::constItem() { Item i; i.v = 2; return i; }
const int Box::constInt() { return 3; }
Item::Kind Box::kind(Item::Kind k, Color) const { return k; }
Color Box::color(Color c, Item::Kind) { return c; }
bool Box::flags(bool a, bool, long long, long long) { return a; }
void Box::strings(const std::string&, const std::string&, std::string, std::string) {}
void Box::byValue(const std::string, std::string) {}
void Box::maps(std::map<std::string, Item*>& m, const std::set<std::string>& s, std::map<std::string, std::string>& mm) { m.clear(); m_value = static_cast<int>(s.size()); mm.clear(); }
void Box::nested(std::vector<std::pair<std::string, int> >& v, std::map<std::string, std::vector<Item*> >& m) { v.clear(); m.clear(); }
void Box::pointers(char* const, const char*, char*, int*, int*) {}
void Box::callbacks(Item (*)(Item, const std::string&), Item, const std::string&) {}
std::pair<std::string, int> Box::pairOf(std::pair<std::string, int> p) { return p; }
Holder<const std::string>* Box::constArg(Holder<const std::string>* h) { return h; }
Box Box::make(const Box& b) { return b; }

int Box::count = 0;
const char* Box::names[3] = { "a", "b", "c" };
std::string Box::label;
int g_grid[2][3];
const char* g_name = "mangle";
Item* g_items[4];
int& g_ref = Box::count;

int useAll()
{
    Box b;
    std::map<std::string, Item*> m;
    std::set<std::string> s;
    s.insert("a");
    m["a"] = 0;
    std::map<std::string, std::string> mm;
    mm["x"] = "y";
    std::vector<std::pair<std::string, int> > v;
    v.push_back(std::make_pair(std::string("a"), 1));
    std::map<std::string, std::vector<Item*> > mv;
    mv["k"].push_back(0);
    b.maps(m, s, mm);
    b.nested(v, mv);
    return static_cast<int>(b.text().size()) + Box::count;
}

// static locals: cl's names, the function's body being block 2; internal unless the function is inline
int counterA() { static int n = 0; return ++n; }
inline int sharedInline() { static int shared = 5; static const char* const s = "x"; static Item it; return ++shared + *s + it.v; }
extern "C" int cStatic() { static long cn = 1; return static_cast<int>(cn++); }
struct WithStatic { int m() { static Item::Kind k = Item::Large; return k; } };
template <class T> struct Holder2 { static T* get() { static T t; return &t; } };

// a local class: its members are scoped by the function (?f@L@?1??localStatics@@YAHH@Z@SAHH@Z) and internal, as are
// their static locals (?s1@?1??f@L@?1??localStatics@@YAHH@Z@SAHH@Z@4HA)
int localStatics(int a)
{
    struct L { static int f(int b) { static int s1 = b; return s1; } int g(int b) { static int cnt = 0; return cnt += b; } };
    L l;
    return L::f(a) + l.g(a);
}

int useStatics()
{
    WithStatic w;
    return counterA() + sharedInline() + cStatic() + w.m() + Holder2<Item>::get()->v + localStatics(3);
}

// 64-bit template arguments (B7): the value in the parameter's type as a signed 64-bit number ($0BAAAAAAAAAA@,
// ~0ULL as $0?0, an unsigned int's 0xFFFFFFFF as $0PPPPPPPP@)
template <long long N> struct Wide { static long long get() { return N; } };
template <unsigned long long N> struct UWide { static unsigned long long get() { return N; } };
template <unsigned N> struct U32 { static unsigned get() { return N; } };
template <long long N> long long wideTwice() { return N * 2; }
// pointers to members as template arguments (B10): a data member's offset ($0BA@), a member function's decorated
// name ($1?h@MP@mangle@@QEAAHXZ), a virtual one's vcall thunk, in a class of multiple inheritance $H with the this
// adjustment
struct MP { int a; int b; long long c; int d; int h() { return a; } int k() const { return b; } virtual int v() { return d; } };
struct MA { virtual int fa() { return 1; } int x; };
struct MB { virtual int fb() { return 2; } int y; };
struct MM : MA, MB { int fb() { return 3; } int own() { return x; } int z; };
template <int MP::*D> int readMP(MP& p) { return p.*D; }
template <int (MP::*F)()> int callMP(MP& p) { return (p.*F)(); }
template <int (MP::*F)() const> struct ConstMP { static int run(const MP& p) { return (p.*F)(); } };
template <int (MM::*F)(), int MM::*D> int bothMM(MM& m) { return (m.*F)() + m.*D; }
int useMemberArgs(MP& p, MM& m)
{
    return readMP<&MP::a>(p) + readMP<&MP::d>(p) + callMP<&MP::h>(p) + callMP<&MP::v>(p) + ConstMP<&MP::k>::run(p) + bothMM<&MM::fb, &MM::z>(m) + bothMM<&MM::own, &MM::z>(m);
}

// cl's vtordispex thunk, an override held by another virtual base (?f@VXL@mangle@@$R4BI@7PPPPPPPM@BI@EBAHXZ), and the
// vtordisp thunk of an override in a non-virtual base of another virtual base, which moves this up (B11, S130)
struct VXV { int v; VXV() : v(1) {} virtual int f() const { return v; } };
struct VXL : virtual VXV { int l; VXL() : l(2) {} int f() const { return l; } };
struct VXW : virtual VXL { int w; VXW() : w(3) {} };
struct VXP { long long p; VXP() : p(4) {} virtual int pad() { return 1; } };
struct VXQ : VXP, VXL { int q; };
struct VXR : virtual VXQ { int r; VXR() : r(5) {} };
int useVtordisp() { VXW w; VXR r; const VXV* a = &w; const VXV* b = &r; return a->f() + b->f(); }

// covariant returns (B11, S131): a return thunk that reads the vbtable (?get@MCD@mangle@@QEAAPEAUMV@2@XZ), and a slot
// that also moves this, an adjustor to the return thunk (?g@MD@mangle@@WBA@EAAPEAUMX@2@XZ)
struct MV { int v; };
struct MVD : virtual MV { int d; };
struct MSB { virtual ~MSB() {} virtual MV* get() { return 0; } int b; };
struct MCD : MSB { MVD vd; MVD* get() { return &vd; } };
struct MSA { virtual ~MSA() {} int a; };
struct MX { int x; };
struct MY : MSA, MX { int y; };
struct CvA { virtual MY* g() { return 0; } int pa; };
struct CvB { virtual MX* g() { return 0; } int pb; };
struct MD : CvA, CvB { MY yy; MY* g() { return &yy; } };
int useCovariant() { MCD c; MD d; MSB* s = &c; CvB* b = &d; return (s->get() != 0) + (b->g() != 0); }

// a pointer to member converted into a class that does not share the base's vbptr, its vbtable indexing the virtual
// bases otherwise: cl's virtual displacement map ??_KDW@mangle@@$CDVW@1@ (B11, S132)
struct DS { int s; };
struct DS2 { int s2; };
struct DV : virtual DS { int v; };
struct DW : virtual DS2, virtual DS { int w; };
struct DVW : DV, DW { int vw; };
int DVW::* remapMember(int DW::* m) { return m; }

// pointers to members of classes with virtual bases as template arguments (B10): the virtual model's $F (offset and
// vbtable offset) and $I (function, adjustment, vbtable offset; a vcall thunk of a slot inside a virtual base with its
// entry; an adjustment from a shared vbptr's subobject, -8), and the unspecified model's $G and $J
struct VMV { int v; VMV() : v(7) {} virtual int vf() const { return v; } };
struct VMD : virtual VMV { int a; VMD() : a(1) {} int f() const { return a; } int vf() const { return 9; } };
struct VMA { int a0; VMA() : a0(1) {} };
struct VMB : virtual VMV { int b1; VMB() : b1(2) {} };
struct VMX : VMA, VMB { int x; VMX() : x(3) {} int fx() const { return x; } };
struct VMU;
int vmuSize = sizeof(int VMU::*);
struct VMU : virtual VMV { int u; VMU() : u(4) {} int fu() const { return u; } int vf() const { return u; } };
template <int VMD::*P> int readVMD(VMD& d) { return d.*P; }
template <int (VMD::*P)() const> int callVMD(const VMD& d) { return (d.*P)(); }
template <int (VMX::*P)() const> int callVMX(const VMX& x) { return (x.*P)(); }
template <int VMU::*P> int readVMU(VMU& u) { return u.*P; }
template <int (VMU::*P)() const> int callVMU(const VMU& u) { return (u.*P)(); }
int useMemberArgsVb(VMD& d, VMX& x, VMU& u)
{
    return readVMD<&VMD::a>(d) + callVMD<&VMD::f>(d) + callVMD<&VMD::vf>(d) + callVMX<&VMX::fx>(x)
         + readVMU<&VMU::u>(u) + callVMU<&VMU::fu>(u) + callVMU<&VMU::vf>(u);
}

// a covariant override whose slot is in a vftable inside a virtual base (B11, S131): the return thunk, cl's vtordisp
// thunk to it (?get@VCV@mangle@@$4PPPPPPPM@A@EAAPEAUVCX@2@XZ), the slot the override appends to the base's table, its
// adjustor in a bigger class and the vtordisp thunk of a further override that takes it
struct VCA { virtual ~VCA() {} int a; };
struct VCX { int x; };
struct VCY : VCA, VCX { int y; };
struct VCB { virtual VCX* get() { return 0; } int vb; };
struct VCV : virtual VCB { VCY yy; VCV() {} VCY* get() { return &yy; } };
struct VCV2 : VCV { VCY y2; VCV2() {} VCY* get() { return &y2; } };
struct VCPad { int p[5]; virtual ~VCPad() {} };
struct VCD : VCPad, VCV { int d; VCD() : d(1) {} };
int useCovariantVb() { VCV c; VCV2 c2; VCD d; VCB* a = &c; VCB* b = &c2; VCB* e = &d; return (a->get() != 0) + (b->get() != 0) + (e->get() != 0) + (d.get() != 0); }

// vftables named for the base they were inherited through where two would be named alike (B11): ??_7VND@mangle@@6BVNM@1@@,
// ??_7VNF@mangle@@6BVNW@1@@
struct VNA { virtual int f() { return 1; } int a; };
struct VNB { virtual int g() { return 2; } int b; };
struct VNM : VNB { int m; };
struct VND : VNA, VNM { int d; };
struct VNV { virtual int v() { return 4; } int vv; };
struct VNP { int p; };
struct VNW : VNP, virtual VNV { };
struct VNF : VNA, VNW { };
int vftableNames() { VND d; VNF f; return d.f() + d.g() + f.v(); }

// an explicit specialisation declared and never defined keeps its class key: a union is T (?takesUBox@mangle@@YAHPEAT?$UBox@H@1@@Z)
template <class T> union UBox { T v; int i; };
template <> union UBox<int>;
int takesUBox(UBox<int>* p) { return p ? 1 : 0; }

long long useWide()
{
    return Wide<(1LL << 40)>::get() + Wide<-1>::get() + Wide<-0x7FFFFFFFFFFFFFFFLL - 1>::get() + static_cast<long long>(UWide<~0ULL>::get())
         + static_cast<long long>(UWide<0x8000000000000000ULL>::get()) + U32<0xFFFFFFFFu>::get() + U32<5>::get() + wideTwice<0x123456789LL>();
}

}
