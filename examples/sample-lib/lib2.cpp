// lib2.cpp - exercises cppparse's own map and set headers; compiled by cppparse --compile6 and
// checked by examples/sample-lib-driver/driver-lib.cpp.
#include <map>
#include <set>
#include <string>
#include <vector>

int mapBasics()                            // 2613: 26 = 2+5+8+11, 1 erased, 3 left
{
    std::map<int, int> m;
    for (int i = 0; i < 4; ++i) m[i] = 3 * i + 2;                // 2 5 8 11
    int sum = 0;
    for (std::map<int, int>::iterator it = m.begin(); it != m.end(); ++it) sum += it->second;
    size_t erased = m.erase(2);
    int r = sum * 100 + static_cast<int>(erased) * 10 + static_cast<int>(m.size());
    if (m.find(2) != m.end() || m.find(3) == m.end() || m.count(0) != 1) return -1;
    return r;
}

int mapValueType()                         // 4321: the value type is pair<const K, V> (C2): the insert forms, references to the elements, a vector of them
{
    typedef std::map<std::string, int> M;
    M m;
    m.insert(M::value_type("a", 1));
    m.insert(std::pair<const std::string, int>("b", 2));
    m.insert(std::make_pair(std::string("c"), 3));
    m["d"] = 4;
    M::iterator it = m.begin();
    std::pair<const std::string, int>& r = *it;
    const M::value_type& vr = *m.find("b");
    std::vector<M::value_type> v(m.begin(), m.end());
    int r2 = 0;
    r2 += r.first == "a" && r.second == 1 ? 1000 : 0;
    r2 += vr.first == "b" && vr.second == 2 ? 300 : 0;
    r2 += m.size() == 4 && v[3].first == "d" && v[3].second == 4 ? 20 : 0;
    int sum = 0;
    for (M::const_iterator c = m.begin(); c != m.end(); ++c) sum += c->second;
    r2 += sum == 10 ? 1 : 0;
    std::map<int, std::string> mi;
    mi[2] = "two";
    mi.insert(std::make_pair(1, std::string("one")));
    r2 += mi.begin()->second == "one" && mi.begin()->first == 1 ? 3000 : 0;
    return r2;
}

int mapOfStrings()                         // 731: "cat" -> 7, 3 keys in order, "ant" first
{
    std::map<std::string, int> ages;
    ages["dog"] = 4;
    ages["cat"] = 7;
    ages["ant"] = 1;
    ages["cat"] += 0;
    std::string first = ages.begin()->first;
    std::map<std::string, int>::iterator last = ages.end();
    --last;
    int r = ages["cat"] * 100 + static_cast<int>(ages.size()) * 10;
    if (first == "ant" && last->first == "dog") r += 1;
    std::map<std::string, int> copy(ages);
    copy.erase("dog");
    if (copy.size() != 2 || ages.size() != 3) return -1;
    return r;
}

int setBasics()                            // 4253: 4 distinct of 6 inserted, 2 = count of 5 + count of 9, lower_bound(4) = 5, 3 left after erase
{
    std::set<int> s;
    int values[6] = { 5, 1, 9, 5, 3, 1 };
    for (int i = 0; i < 6; ++i) s.insert(values[i]);
    int r = static_cast<int>(s.size()) * 1000;
    r += (static_cast<int>(s.count(5)) + static_cast<int>(s.count(9)) + static_cast<int>(s.count(7))) * 100;
    r += *s.lower_bound(4) * 10;
    s.erase(s.find(9));
    r += static_cast<int>(s.size());
    int prev = -1;
    for (std::set<int>::iterator it = s.begin(); it != s.end(); ++it) { if (*it <= prev) return -1; prev = *it; }
    return r;
}

int treeStress(int n)                      // n: n keys inserted in a scrambled order, half erased, all accounted for
{
    std::map<int, int> m;
    std::vector<int> keys;
    for (int i = 0; i < n; ++i) keys.push_back((i * 7919) % n);
    for (size_t i = 0; i < keys.size(); ++i) m[keys[i]] = keys[i] * 2;
    if (static_cast<int>(m.size()) != n) return -1;
    int removed = 0;
    for (std::map<int, int>::iterator it = m.begin(); it != m.end();) {
        if (it->first % 2 == 1) { std::map<int, int>::iterator victim = it; ++it; m.erase(victim); ++removed; }
        else ++it;
    }
    int expect = 0;
    for (std::map<int, int>::iterator it = m.begin(); it != m.end(); ++it) { if (it->first != expect || it->second != expect * 2) return -2; expect += 2; }
    return removed + static_cast<int>(m.size());
}

// linkage across the units of sample-lib (C++98 3.5/3, 14.7.1): a namespace-scope const object is internal to its
// unit (both units define kLinkShared with different values), the static data member of a class template is one
// object for the program (a COMDAT), the static local of an inline function too
const int kLinkShared = 2;
static int s_linkHidden = 2;
inline int linkInline(int x) { static int calls = 0; return x + ++calls; }
template <class T> struct LinkBox { T v; LinkBox(T x) : v(x) {} T get() const { return v; } static int made; };
template <class T> int LinkBox<T>::made = 0;
// the same dynamically initialised static members as lib1.cpp's: one object each, built once (g_linkDynBuilt counts)
extern int g_linkDynBuilt;
struct LinkLog { int v; LinkLog(int x) : v(x) { ++g_linkDynBuilt; } };
template <class T> struct LinkDyn { static LinkLog name; static std::string text; static int viaCall; };
template <class T> LinkLog LinkDyn<T>::name(7);
template <class T> std::string LinkDyn<T>::text = "shared";
template <class T> int LinkDyn<T>::viaCall = linkInline(100);

// the static data member lib1.cpp defines: this unit's class declares it only
struct LinkStatic { static int n; static const int k = 7; int arr[k]; };

// the templates lib1.cpp defines and explicitly instantiates: declared only here, so every use below is a reference to
// a COMDAT of lib1's object (LNK2019 when the explicit instantiation emitted nothing)
template <class T> struct LinkInst {
    T v;
    LinkInst(T x);
    T get() const;
    static int made;
    struct In { int twice(int n) const; };
};
template <class T> T linkTwice(T x);

int explicitInstB()                        // 4321: LinkInst<int>(4).get() 4, made 2 (lib1's object and this one) 300, In::twice(10) 20, LinkInst<double> 1, linkTwice 0
{
    LinkInst<int> b(4);
    LinkInst<int>::In in;
    LinkInst<double> d(2.5);
    return b.get() * 1000 + (LinkInst<int>::made == 2 ? 300 : -3000) + in.twice(10) + (d.get() == 2.5 ? 1 : -10) + (linkTwice(5) == 10 && linkTwice(6L) == 12L ? 0 : -100);
}

int linkageB()                             // 2225: kLinkShared 2, s_linkHidden 2, linkInline(22) on its third call 25, made 1 + 10 = 11 -> 2000 + 200 + 25 + 1, minus 1 for the dynamic statics
{
    LinkBox<int>::made += 10;
    LinkDyn<int>::text += "!";
    const int dyn = LinkDyn<int>::name.v == 7 && g_linkDynBuilt == 1 && LinkDyn<int>::text == "shared!" && LinkDyn<int>::viaCall == 101 ? -1 : -50;
    LinkStatic::n += 10;
    const int* pk = &LinkStatic::k;
    const int st = LinkStatic::n == 15 && *pk == 7 && LinkStatic::k == 7 ? 0 : -500;
    return kLinkShared * 1000 + s_linkHidden * 100 + linkInline(22) + (LinkBox<int>::made == 11 ? 1 : -100) + LinkBox<long>(5L).get() * 0 + dyn + st;
}
