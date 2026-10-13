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

int useStatics()
{
    WithStatic w;
    return counterA() + sharedInline() + cStatic() + w.m() + Holder2<Item>::get()->v;
}

}
