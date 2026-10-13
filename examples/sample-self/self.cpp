// self.cpp - constructs of cppparse's own sources that the self-built compiler got wrong, compiled by
// cppparse --compile6 with its own headers and driven by examples/sample-self-driver/driver-self.cpp
// (built with MSVC). Every function returns a number the driver compares with cl's result.
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

struct RegInfo { const char* name; int num; int size; };

const RegInfo kRegs[] = {
    {"rax",0,8},{"rcx",1,8},{"rdx",2,8},
    {"eax",0,4},{"ecx",1,4},
    {"al",0,1}
};

const char* const kWords[] = { "...", "<<=", "::", "->", "{", 0 };

}

int aggregates()                           // arrays of unknown bound, nested braces, elided braces, zero fill
{
    int r = static_cast<int>(sizeof(kRegs) / sizeof(kRegs[0])) * 1000 + static_cast<int>(sizeof(kWords) / sizeof(kWords[0])) * 100;
    for (int i = 0; kWords[i]; ++i) r += static_cast<int>(std::strlen(kWords[i]));
    for (size_t i = 0; i < sizeof(kRegs) / sizeof(kRegs[0]); ++i) r += kRegs[i].num * kRegs[i].size;
    static const char* const table[][2] = { {"add", "ADD"}, {"sub", "SUB"}, {0, 0} };
    for (int i = 0; table[i][0]; ++i) r += static_cast<int>(std::strlen(table[i][1])) * 10000;
    int flat[2][3] = { 1, 2, 3, 4 };       // braces elided: the rest is zero
    r += flat[1][0] * 100000 + flat[1][2] + flat[0][2] * 1000000;
    int zero[50] = { 7 };
    int sum = 0;
    for (int i = 0; i < 50; ++i) sum += zero[i];
    RegInfo one = { "x", 5 };
    return r + sum * 10000000 + one.size + one.num;
}

namespace {

int g_live = 0;

struct Live {
    int v;
    explicit Live(int x) : v(x) { ++g_live; }
    Live(const Live& o) : v(o.v) { ++g_live; }
    ~Live() { --g_live; }
};

struct Name { std::vector<std::string> parts; };

size_t lastLength(const Name& n)
{
    const std::string& last = n.parts.empty() ? std::string("") : n.parts.back();
    return last.size();
}

int pick(bool fresh, const Live& keep, int stop)
{
    for (int i = 0; i < 3; ++i) {
        const Live& r = fresh ? Live(7) : keep;   // a prvalue either way: Live(7) or a copy of keep, under its arm's flag
        if (i == stop) return r.v + g_live * 100;
        if (i == 1) continue;
    }
    return g_live * 1000;
}

}

int conditionalTemporaries()               // a ?: temporary bound to a reference is destroyed only if it was built
{
    Name n;
    int r = static_cast<int>(lastLength(n));
    n.parts.push_back("alpha");
    n.parts.push_back("beta");
    for (int i = 0; i < 3; ++i) r = r * 10 + static_cast<int>(lastLength(n));
    std::string s = n.parts.empty() ? std::string("none") : n.parts[0];
    s += "!";
    return r * 100 + static_cast<int>(n.parts[0].size() + n.parts[1].size() + s.size());
}

int conditionalReferences()                // the same through continue and return
{
    Live k(3);
    int p = pick(true, k, 1) + pick(false, k, 1) * 1000 + pick(false, k, 5) / 1000 * 1000000;
    return p + g_live * 10000000;
}

namespace {

struct Defaults {
    int base;
    int add(int a, size_t b = 40, int c = 2);
    int twice() { return add(1) + add(1, 2); }
};

int Defaults::add(int a, size_t b, int c) { return base + a + static_cast<int>(b) * c; }

}

int defaultArguments()                     // defaults of the declaration apply to calls of the out-of-line definition
{
    Defaults d;
    d.base = 100;
    return d.twice() + d.add(5) * 1000;
}

int pointerArithmetic()                    // 64-bit indices and pointer differences, negative ones too
{
    int a[8];
    for (int i = 0; i < 8; ++i) a[i] = i * i;
    int* end = a + 8;
    long long back = -3;
    std::ptrdiff_t d = end - a;
    size_t n = sizeof(a) / sizeof(a[0]);
    int r = end[back] + static_cast<int>(d) * 100 + static_cast<int>(n) * 1000;
    const int* p = end;
    p -= 2;
    r += *p * 10000;
    std::vector<int> v(a, a + 8);
    std::vector<int>::iterator it = v.end() + back;
    r += *it * 100000;
    unsigned long mask = 0xFFFFFFFFUL;
    size_t wide = mask;
    r += (wide + 5 > mask) ? 1 : 0;
    char c = '\xE9';
    r += c < 0 ? 2 : 0;
    return r;
}

namespace {

const char* const kNames[] = { "rbx", "rsi" };

std::string regName(int r, bool fl)
{
    return fl ? "xmm" + std::string(1, static_cast<char>('0' + r)) : kNames[r];
}

}

int conditionalConversions()               // a ?: whose operands convert to std::string in different ways
{
    std::string a = regName(1, false), b = regName(4, true);
    const std::string& c = a.empty() ? "none" : a;
    int r = static_cast<int>(a.size() * 100 + b.size() * 10 + c.size());
    if (b == "xmm4") r += 1000;
    if (c == "rsi") r += 10000;
    return r;
}

namespace {

struct QName {
    std::vector<std::string> parts;
    const std::string& last() const { static const std::string empty; return parts.empty() ? empty : parts.back(); }
};

}

int conditionalLvalues()                   // lvalues of the same class, one const: the result refers to the chosen one
{
    QName q;
    int r = static_cast<int>(q.last().size());
    q.parts.push_back("ident");
    const std::string& l = q.last();
    r = r * 10 + static_cast<int>(l.size());
    return r * 10 + (&l == &q.parts.back() ? 1 : 0);
}

int addressOfElements()                    // &v[i] through operator[] returning a reference is the element's address
{
    std::vector<unsigned char> b;
    b.push_back(7);
    b.push_back(9);
    std::string o;
    o.append(reinterpret_cast<const char*>(b.empty() ? 0 : &b[0]), b.size());
    const unsigned char* p = &b[1];
    std::string s("xyz");
    char* q = &s[2];
    *q = 'w';
    return static_cast<int>(o.size()) * 100 + o[1] * 10 + *p * 1000 + (s == "xyw" ? 1 : 0);
}

namespace {

struct Operand { enum Kind { None, Temp, Imm }; Kind kind; int id; long ival; Operand() : kind(None), id(-1), ival(0) {} };
struct Inst { enum Op { AddrField, AddrIndex, LoadMem, StoreMem }; Op op; int dst; long extra; std::vector<Operand> args; Inst() : op(AddrField), dst(-1), extra(0) {} };
struct Block { std::vector<Inst> insts; };
struct Func { std::vector<Block> blocks; };
struct Def { int block; int index; Def() : block(-1), index(-1) {} };

Operand temp(int id) { Operand o; o.kind = Operand::Temp; o.id = id; return o; }
Operand imm(long v) { Operand o; o.kind = Operand::Imm; o.ival = v; return o; }
Inst inst(Inst::Op op, int dst, long extra, const Operand& a) { Inst i; i.op = op; i.dst = dst; i.extra = extra; i.args.push_back(a); return i; }

}

int storeForwarding()                      // the memory forwarding of the optimizer: a store through another address kills
{
    Func f;
    f.blocks.resize(1);
    std::vector<Inst>& in = f.blocks[0].insts;
    in.push_back(inst(Inst::AddrField, 1, 8, temp(0)));
    in.push_back(inst(Inst::LoadMem, 2, 0, temp(1)));
    in.push_back(inst(Inst::AddrIndex, 3, 4, temp(5)));
    in.push_back(inst(Inst::StoreMem, -1, 0, temp(3)));
    in.back().args.push_back(imm(0));
    in.push_back(inst(Inst::AddrField, 6, 16, temp(0)));
    in.push_back(inst(Inst::StoreMem, -1, 0, temp(6)));
    in.back().args.push_back(temp(2));
    in.push_back(inst(Inst::LoadMem, 4, 0, temp(1)));
    in.push_back(inst(Inst::LoadMem, 7, 0, temp(6)));
    std::map<int, Def> defs;
    for (size_t b = 0; b < f.blocks.size(); ++b)
        for (size_t i = 0; i < f.blocks[b].insts.size(); ++i)
            if (f.blocks[b].insts[i].dst >= 0) { Def d; d.block = static_cast<int>(b); d.index = static_cast<int>(i); defs[f.blocks[b].insts[i].dst] = d; }
    int forwarded = 0, kept = 0;
    std::map<int, Operand> mem;
    for (size_t i = 0; i < in.size(); ++i) {
        Inst& x = in[i];
        if (x.op == Inst::StoreMem && x.args.size() == 2 && x.args[0].kind == Operand::Temp) {
            int addr = x.args[0].id;
            std::map<int, Def>::iterator da = defs.find(addr);
            const Inst* ad = da == defs.end() ? 0 : &f.blocks[static_cast<size_t>(da->second.block)].insts[static_cast<size_t>(da->second.index)];
            for (std::map<int, Operand>::iterator m = mem.begin(); m != mem.end();) {
                bool keep = false;
                if (m->first == addr) keep = true;
                else {
                    std::map<int, Def>::iterator db = defs.find(m->first);
                    const Inst* bd = db == defs.end() ? 0 : &f.blocks[static_cast<size_t>(db->second.block)].insts[static_cast<size_t>(db->second.index)];
                    if (ad && bd && ad->op == Inst::AddrField && bd->op == Inst::AddrField && ad->args.size() == 1 && bd->args.size() == 1 &&
                        ad->args[0].kind == Operand::Temp && bd->args[0].kind == Operand::Temp && ad->args[0].id == bd->args[0].id && ad->extra != bd->extra)
                        keep = true;
                }
                if (keep) { ++m; ++kept; } else mem.erase(m++);
            }
            if (x.args[1].kind == Operand::Temp || x.args[1].kind == Operand::Imm) mem[addr] = x.args[1]; else mem.erase(addr);
            continue;
        }
        if (x.op == Inst::LoadMem && x.args.size() == 1 && x.args[0].kind == Operand::Temp) {
            std::map<int, Operand>::iterator m = mem.find(x.args[0].id);
            if (m != mem.end()) ++forwarded;
            else if (x.dst >= 0) mem[x.args[0].id] = temp(x.dst);
        }
    }
    return forwarded * 100 + kept * 10 + static_cast<int>(mem.size());
}

namespace {

struct Counter {
    int v;
    Counter() : v(0) {}
    Counter& operator++() { v += 10; return *this; }
    Counter operator++(int) { Counter c(*this); v += 1; return c; }
};

}

int postIncrement()                        // it++ on a class calls operator++(int), not the prefix form
{
    std::map<int, int> a;
    a[5] = 1;
    a[6] = 2;
    std::map<int, int>::iterator it = a.begin();
    std::map<int, int>::iterator old = it++;
    Counter c;
    Counter d = c++;
    ++c;
    return old->first * 1000 + it->first * 100 + d.v * 10 + c.v;
}

namespace {

struct VBase { int b; VBase() : b(0) {} virtual ~VBase() {} virtual int id() const { return 1; } };
struct Other { int pad[3]; };
struct VDer : VBase, Other { int id() const { return 2; } };

int& counterRef() { static int value = 5; static int& r = value; return r; }
float scale() { static float f = 2.5f; f += 1.0f; return f; }

}

int castsAndStatics()                      // base adjustments keep null, slicing keeps the vptr, static references and floats
{
    VDer d;
    VDer* nd = 0;
    Other* no = nd;
    Other* po = &d;
    VDer* back = static_cast<VDer*>(po);
    VBase copy = d;
    VBase assigned;
    assigned = d;
    int r = (no == 0 ? 1 : 0) + (back == &d ? 10 : 0) + copy.id() * 100 + assigned.id() * 1000;
    r += reinterpret_cast<char*>(po) != reinterpret_cast<char*>(&d) ? 10000 : 0;
    ++counterRef();
    r += counterRef() * 100000;
    r += static_cast<int>(scale() * 2.0f) * 10000000;
    return r;
}

namespace {

unsigned g_u = 0xFFFFFFFFu;
unsigned g_div = 7u;
double g_half = 0.5;

}

int narrowArithmetic()                     // 32-bit and narrower results wrap in their own width; a op= b uses the common type
{
    unsigned a = g_u + 1u;
    unsigned b = (g_u + 1u) / 2u;
    unsigned c = (g_u * 3u) / 5u;
    unsigned d = (g_u + 2u) % 7u;
    int e = 3;
    e *= g_half;
    int f = -9;
    f /= g_div;
    double x = static_cast<double>(g_u + 1u);
    signed char sc = 127;
    sc += 1;
    unsigned short us = 65535;
    us += 1;
    int r = static_cast<int>(c % 100000) + static_cast<int>(d) * 100000 + e * 200000 + (f % 1000) * 1000000;
    if (a == 0 && b == 0 && x == 0.0 && sc == -128 && us == 0) r += 400000;
    return r;
}

int bigFrame()                             // a frame of several pages is probed before use
{
    char buf[60000];
    for (int i = 0; i < 60000; i += 1000) buf[i] = static_cast<char>(i / 1000);
    buf[59999] = 7;
    int s = 0;
    for (int i = 0; i < 60000; i += 1000) s += buf[i];
    return s + buf[59999];
}

int stringEscapes()                        // octal and hexadecimal escapes in string literals
{
    const char* s = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\101\x42\0end";
    return static_cast<int>(std::strlen(s)) * 1000 + static_cast<unsigned char>(s[10]) + s[14] + s[15];
}

void selfSink(size_t n);                   // defined by the driver: a call the optimiser cannot see into

// the header of the second and third loop only passes n on (its copies sit in edge blocks after the loops):
// n keeps its register there
int carriedCounter(const int* left, int nl, const int* right, int nr, int* out)
{
    const int* a = left;
    const int* b = right;
    size_t n = 0;
    while (a < left + nl && b < right + nr) {
        if (*b < *a) { out[n] = *b; ++b; }
        else { out[n] = *a; ++a; }
        ++n;
    }
    while (a < left + nl) { selfSink(n); out[n] = *a; ++a; ++n; }
    while (b < right + nr) { selfSink(n); out[n] = *b; ++b; ++n; }
    return static_cast<int>(n);
}

namespace {

int g_spin = 0;
int g_slowBuilt = 0;

struct Slow {
    int value;
    Slow() : value(0) { for (int i = 0; i < 20000000; ++i) g_spin += i & 1; ++g_slowBuilt; value = 42; }
};

int innerStatic() { static std::string s("inner"); return static_cast<int>(s.size()); }

struct Outer {
    int n;
    Outer() : n(innerStatic() * 2) {}
};

}

int sharedStatic()                         // called by several threads at once: one construction, seen by all
{
    static Slow s;
    return s.value * 10 + g_slowBuilt;
}

int nestedStatics()                        // an initializer that reaches another static local
{
    static Outer o;
    return o.n * 100 + innerStatic();
}
int wideConstants()                        // constant expressions of 64-bit types are not folded in 32 bits
{
    const unsigned long long one = 1;
    unsigned long long a = one << 52;
    unsigned long long b = (one << 40) + 5;
    unsigned long long c = static_cast<unsigned long long>(-1) / 2;
    long long d = -(static_cast<long long>(1) << 33);
    unsigned long long e = 1;
    e <<= 45;
    unsigned long long mask = (one << 52) - 1;
    int r = static_cast<int>(a >> 50) * 100000 + static_cast<int>(b >> 40) * 10000 + static_cast<int>(b & 0xFF) * 1000;
    r += static_cast<int>(c >> 62) * 100 + ((d >> 33) == -1 && (d & 0xFFFFFFFF) == 0 ? 10 : 0) + static_cast<int>(e >> 45);
    if ((mask >> 51) == 1 && (mask & 1) == 1) r += 1000000;
    return r;
}

// floats after the fourth parameter arrive in 8-byte stack slots whose upper half the caller leaves as it
// was; each is homed in a 4-byte frame slot (the caller is the cl-built driver, which first passes doubles
// through the same slots)
double stackDoubles(double a, double b, double c, double d, double e, double f, double g)
{
    return a + b + c + d + e + f + g;
}

float stackFloats(float a, float b, float c, float d, float e, float f, float g)
{
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7;
}

namespace {

char g_exitOrder[8];
int g_exitCount = 0;

struct AtExit {
    char tag;
    explicit AtExit(char t) : tag(t) {}
    ~AtExit()
    {
        g_exitOrder[g_exitCount++] = tag;
        if (tag == 'a') std::printf("  destructors at exit: %s\n", g_exitOrder);   // the build script checks the order
    }
};

AtExit g_first('a');
AtExit g_second('b');

}

int exitOrder()                            // globals and static locals are destroyed after main, newest first
{
    static AtExit third('c');
    return g_exitCount * 100 + third.tag;
}

// a variadic function finds its unnamed arguments in the home slots its prologue fills from rcx, rdx, r8 and r9
// (a double travels in the integer register too) and on the caller's stack after them; also called by the driver
double sumMixed(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    double s = 0;
    for (int i = 0; i < n; ++i) {
        if (i % 2 == 0) s += va_arg(ap, double);
        else s += va_arg(ap, int) * 100;
    }
    va_end(ap);
    return s;
}

namespace {

double scaled(double k, int n, ...)        // a named double: homed from xmm0, the integers follow it
{
    va_list ap;
    va_start(ap, n);
    int s = 0;
    for (int i = 0; i < n; ++i) s += va_arg(ap, int);
    va_end(ap);
    return k * s;
}

int formatInto(char* buf, const char* fmt, ...)   // passes its list on to the C library
{
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

}

int varargs()
{
    double a = sumMixed(6, 1.5, 2, 2.25, 3, 4.0, 5);
    char buf[64];
    int n = formatInto(buf, "%d-%.2f-%s", 42, 2.5, "ok");
    int r = static_cast<int>(a * 100) + static_cast<int>(scaled(0.5, 3, 10, 20, 30)) + n * 1000000;
    return r + (std::strcmp(buf, "42-2.50-ok") == 0 ? 100000000 : 0);
}
