// lib1.cpp - exercises cppparse's own string, vector, utility and algorithm headers; compiled
// by cppparse --compile6 -I include and checked by examples/sample-lib-driver/driver-lib.cpp.
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

int stringBasics()                         // 1264: "hello world" has 11 chars, 'w' at 6, compare and substr
{
    std::string s("hello");
    s += " ";
    s.append("world");
    int r = static_cast<int>(s.size()) * 100;
    r += static_cast<int>(s.find('w')) * 10;
    if (s.substr(6) == "world") r += 4;
    return r;
}

int stringOps()                            // 42: insert, erase, replace, rfind, find_first_of, comparisons
{
    std::string s = "abc";
    s.insert(1, "XY");                     // aXYbc
    s.erase(0, 1);                         // XYbc
    s.replace(2, 1, "BB");                 // XYBBc
    int r = 0;
    if (s == "XYBBc") r += 10;
    if (s.rfind('B') == 3) r += 20;
    if (s.find_first_of("cB") == 2) r += 5;
    if (std::string("a") < std::string("b")) r += 7;
    return r;
}

int stringThrows()                         // 4843: at, substr, erase, insert, replace, compare, the (str, pos) constructor and copy throw out_of_range (C1)
{
    std::string s("abc");
    int r = 0;
    try { s.at(1) = 'x'; r += (s == "axc") ? 10 : -1; s.at(7) = 'y'; r -= 1000; } catch (std::out_of_range&) { r += 20; }
    const std::string cs("q");
    try { r += cs.at(0) == 'q' ? 1 : 0; cs.at(3); r -= 1000; } catch (std::out_of_range&) { r += 100; }
    try { s.substr(9); r -= 1000; } catch (std::out_of_range&) { r += 200; }
    try { s.erase(9); r -= 1000; } catch (std::out_of_range&) { r += 300; }
    try { s.insert(9, "x"); r -= 1000; } catch (std::out_of_range&) { r += 400; }
    try { s.replace(9, 1, "x"); r -= 1000; } catch (std::out_of_range&) { r += 500; }
    try { s.compare(9, 1, "x"); r -= 1000; } catch (std::out_of_range&) { r += 600; }
    try { std::string t(s, 9); r -= 1000; } catch (std::out_of_range&) { r += 700; }
    char buf[4] = { 0, 0, 0, 0 };
    r += static_cast<int>(s.copy(buf, 2, 1)) * 1000;      // "xc"
    r += buf[0] == 'x' && buf[1] == 'c' ? 3 : -3;
    try { s.copy(buf, 1, 9); r -= 1000; } catch (std::out_of_range& e) { r += std::strlen(e.what()) > 0 ? 4 : -4; }
    r += s.substr(3).empty() ? 5 : -5;                     // a position equal to the size is allowed
    return r;
}

static bool descending(int a, int b) { return a > b; }

int vectorBasics()                         // 3752: sum 37 of 5 elements, capacity growth, sorted ends
{
    std::vector<int> v;
    for (int i = 1; i <= 5; ++i) v.push_back(i * i);            // 1 4 9 16 25 = 55
    v.pop_back();                                                // 1 4 9 16 = 30
    v.insert(v.begin() + 1, 7);                                  // 1 7 4 9 16 = 37
    int sum = 0;
    for (size_t i = 0; i < v.size(); ++i) sum += v[i];
    std::sort(v.begin(), v.end(), descending);                   // 16 9 7 4 1
    int r = sum * 100 + static_cast<int>(v.size()) * 10;
    if (v.front() == 16 && v.back() == 1) r += 2;
    return r;
}

int vectorOfStrings()                      // 12: three strings sorted, the first is "apple", total length 12
{
    std::vector<std::string> names;
    names.push_back("pear");
    names.push_back("apple");
    names.push_back("fig");
    std::sort(names.begin(), names.end());
    size_t total = 0;
    for (size_t i = 0; i < names.size(); ++i) total += names[i].size();
    std::vector<std::string> copy(names);
    copy.erase(copy.begin());
    if (names[0] != "apple" || copy.size() != 2 || copy[0] != "fig") return -1;
    return static_cast<int>(total);
}

int pairsAndSwap()                         // 305: pair of (3, "x"), swap of ints, min/max
{
    std::pair<int, std::string> p = std::make_pair(3, std::string("x"));
    int a = 1, b = 5;
    std::swap(a, b);
    return p.first * 100 + std::max(a, b) + static_cast<int>(p.second.size()) * 0 + (a == 5 ? 0 : 1000);
}

int stableSorting()                        // 4: stable_sort keeps the input order of equal keys
{
    std::vector<std::pair<int, int> > items;
    for (int i = 0; i < 40; ++i) items.push_back(std::make_pair(i % 3, i));
    std::stable_sort(items.begin(), items.end());
    int ok = 0;
    for (size_t i = 1; i < items.size(); ++i)
        if (items[i - 1].first == items[i].first && items[i - 1].second > items[i].second) return -1;
    if (items[0].first == 0 && items[0].second == 0) ok += 1;
    if (items[39].first == 2 && items[39].second == 38) ok += 3;
    return ok;
}

// linkage across the units of sample-lib (C++98 3.5/3, 14.7.1): a namespace-scope const object is internal to its
// unit (both units define kLinkShared with different values), the static data member of a class template is one
// object for the program (a COMDAT), the static local of an inline function too
const int kLinkShared = 1;
static int s_linkHidden = 1;
inline int linkInline(int x) { static int calls = 0; return x + ++calls; }
template <class T> struct LinkBox { T v; LinkBox(T x) : v(x) {} T get() const { return v; } static int made; };
template <class T> int LinkBox<T>::made = 0;
// a static data member of a class template with a dynamic initialiser is built once for the program, by the one
// unit's initialiser the linker keeps (cl's ??__E dynamic initializer in a COMDAT with its .CRT$XCU entry)
int g_linkDynBuilt = 0;
struct LinkLog { int v; LinkLog(int x) : v(x) { ++g_linkDynBuilt; } };
template <class T> struct LinkDyn { static LinkLog name; static std::string text; static int viaCall; };
template <class T> LinkLog LinkDyn<T>::name(7);
template <class T> std::string LinkDyn<T>::text = "shared";
template <class T> int LinkDyn<T>::viaCall = linkInline(100);

// a static data member declared in both units' copy of the class and defined in this one (9.4.2/2); the static const
// member with an in-class initializer is folded in both units and defined here for the pointer lib2 takes
struct LinkStatic { static int n; static const int k = 7; int arr[k]; };
int LinkStatic::n = 5;
const int LinkStatic::k;

// explicit instantiation definitions (14.7.2): the templates are defined in this unit only and instantiated here for
// lib2.cpp, which declares them. The class form instantiates every member (the nested class's too), the member form
// two members of LinkInst<double>, the function form one specialisation each, the argument written or deduced; each
// is a COMDAT with cl's name, so lib2.cpp (and a cl-built unit) links against them
template <class T> struct LinkInst {
    T v;
    LinkInst(T x);
    T get() const;
    static int made;
    struct In { int twice(int n) const { return 2 * n; } };
};
template <class T> LinkInst<T>::LinkInst(T x) : v(x) { ++made; }
template <class T> T LinkInst<T>::get() const { return v; }
template <class T> int LinkInst<T>::made = 0;
template <class T> T linkTwice(T x) { return x + x; }
template struct LinkInst<int>;
template LinkInst<double>::LinkInst(double);
template double LinkInst<double>::get() const;
template int linkTwice<int>(int);
template long linkTwice(long);

int explicitInstA()                        // 3105: LinkInst<int>(3).get() 3, made 1 after it (lib2's objects come later), linkTwice(2) 4, plus 1
{
    LinkInst<int> b(3);
    return b.get() * 1000 + LinkInst<int>::made * 100 + linkTwice(2) + 1;
}

int linkageA()                             // 1122: kLinkShared 1, s_linkHidden 1, linkInline(21) on its second call 23 (viaCall's was the first), made 1 after one object, minus 1 for the dynamic statics
{
    LinkBox<int> b(3);
    LinkBox<int>::made += 1;
    const int dyn = LinkDyn<int>::name.v == 7 && g_linkDynBuilt == 1 && LinkDyn<int>::text.size() == 6 && LinkDyn<int>::viaCall == 101 ? -1 : -50;
    LinkStatic ls;
    const int st = LinkStatic::n == 5 && sizeof(ls.arr) == 7 * sizeof(int) ? 0 : -500;
    return kLinkShared * 1000 + s_linkHidden * 100 + linkInline(21) + LinkBox<int>::made * 0 + (LinkBox<int>::made == 1 ? 0 : -1) + b.get() * 0 + dyn + st;
}
