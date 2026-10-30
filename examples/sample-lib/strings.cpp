// strings.cpp - basic_string, char_traits, string and wstring (C3); built by cl with its own headers (the reference)
// and by cppparse --compile6 with include/, test 15 compares what the programs print at -O0 and -O2.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static int sign(int v) { return v < 0 ? -1 : v > 0 ? 1 : 0; }
static void show(const char* what, const std::string& s) { std::printf("%-14s [%s] %d\n", what, s.c_str(), static_cast<int>(s.size())); }
static void showW(const char* what, const std::wstring& s)
{
    std::printf("%-14s [", what);
    for (size_t i = 0; i < s.size(); ++i) { unsigned u = static_cast<unsigned>(s[i]); if (u < 128) std::printf("%c", static_cast<char>(u)); else std::printf("\\x%04x", u); }
    std::printf("] %d\n", static_cast<int>(s.size()));
}
// a rotating buffer: up to eight per printf, whichever order the compiler evaluates the arguments in
static const char* pos(size_t p) { static char buf[8][24]; static int k = 0; k = (k + 1) % 8; if (p == std::string::npos) return "npos"; std::sprintf(buf[k], "%d", static_cast<int>(p)); return buf[k]; }

static void construction()
{
    std::string a;
    std::string b("hello");
    std::string c("hello world", 5);
    std::string d(4, 'x');
    std::string e(b);
    std::string f(std::string("abcdef"), 2);
    std::string g(std::string("abcdef"), 1, 3);
    const char* p = "range";
    std::string h(p, p + 5);
    std::vector<char> v(3, 'v');
    std::string i(v.begin(), v.end());
    std::string j(5, 65);                       // two integers: five 'A' (21.3/6)
    std::string k(b.rbegin(), b.rend());
    std::basic_string<char, std::char_traits<char>, std::allocator<char> > l("spelled out", std::allocator<char>());
    show("default", a); show("c-string", b); show("prefix", c); show("count", d); show("copy", e); show("tail", f);
    show("middle", g); show("pointers", h); show("vector", i); show("ints", j); show("reversed", k); show("allocator", l);
    std::printf("empty %d %d, npos %d\n", a.empty() ? 1 : 0, b.empty() ? 1 : 0, std::string::npos == static_cast<size_t>(-1) ? 1 : 0);
}

static void modifiers()
{
    std::string s("abc");
    s += 'd'; s += "ef"; s += std::string("gh");
    show("+=", s);
    s.append("ijk", 2).append(2, '-').append(std::string("0123456"), 2, 3).append("!");
    show("append", s);
    const char* p = "PQRS";
    s.append(p + 1, p + 3);
    s.push_back('#');
    show("append range", s);
    s.assign("new"); show("assign", s);
    s.assign("abcdef", 4); show("assign n", s);
    s.assign(3, 'z'); show("assign c", s);
    s.assign(std::string("0123456789"), 3, 4); show("assign sub", s);
    s.assign(p, p + 4); show("assign range", s);
    s = "0123456789";
    s.insert(2, "ab"); show("insert", s);
    s.insert(0, std::string("XY")); show("insert str", s);
    s.insert(4, 2, '*'); show("insert n", s);
    s.insert(s.begin() + 1, '^'); show("insert it", s);
    s.insert(s.end(), 2, '$'); show("insert it n", s);
    s.insert(s.begin(), p, p + 2); show("insert range", s);
    s.erase(0, 3); show("erase", s);
    s.erase(s.begin()); show("erase it", s);
    s.erase(s.begin() + 2, s.begin() + 5); show("erase range", s);
    s.erase(10); show("erase tail", s);
    s = "the quick brown fox";
    s.replace(4, 5, "slow"); show("replace", s);
    s.replace(0, 3, std::string("a")); show("replace str", s);
    s.replace(s.begin(), s.begin() + 1, "one"); show("replace it", s);
    s.replace(4, 4, 3, '='); show("replace n", s);
    s.replace(s.begin(), s.begin() + 3, p, p + 4); show("replace range", s);
    s.resize(6); show("resize", s);
    s.resize(9, '.'); show("resize c", s);
    std::string t("other");
    s.swap(t); show("swap", s); show("swapped", t);
    std::swap(s, t); show("std::swap", s);
    s.clear(); show("clear", s);
    // the source inside the string itself
    std::string w("abcd");
    w += w; show("self +=", w);
    w.append(w.c_str() + 2, 3); show("self append", w);
    w.insert(1, w); show("self insert", w);
    w.replace(0, 4, w.c_str() + 6, 5); show("self replace", w);
    w.assign(w.c_str() + 3, 4); show("self assign", w);
}

static void searching()
{
    const std::string s("one two three two one");
    std::printf("find %s %s %s %s %s\n", pos(s.find("two")), pos(s.find("two", 5)), pos(s.find('e')), pos(s.find("xyz")), pos(s.find("")));
    std::printf("find n %s %s %s\n", pos(s.find("threx", 0, 4)), pos(s.find(std::string("one"), 1)), pos(s.find('o', 100)));
    std::printf("rfind %s %s %s %s\n", pos(s.rfind("two")), pos(s.rfind("two", 10)), pos(s.rfind('o')), pos(s.rfind("one", 0)));
    std::printf("first_of %s %s %s\n", pos(s.find_first_of("wxyz")), pos(s.find_first_of(std::string("ht"), 5)), pos(s.find_first_of('t')));
    std::printf("last_of %s %s %s\n", pos(s.find_last_of("wt")), pos(s.find_last_of("o", 5)), pos(s.find_last_of('q')));
    std::printf("first_not_of %s %s\n", pos(s.find_first_not_of("onet ")), pos(s.find_first_not_of('o')));
    std::printf("last_not_of %s %s\n", pos(s.find_last_not_of("one ")), pos(s.find_last_not_of('e')));
    std::printf("empty: %s %s %s\n", pos(std::string().find('a')), pos(std::string().rfind("a")), pos(std::string().find_last_of("a")));
    show("substr", s.substr(4, 3));
    show("substr tail", s.substr(18));
    show("substr all", s.substr());
}

static void comparing()
{
    std::string a("apple"), b("apricot"), c("apple");
    std::printf("compare %d %d %d %d\n", sign(a.compare(b)), sign(b.compare(a)), sign(a.compare(c)), sign(a.compare("app")));
    std::printf("compare pos %d %d %d %d\n", sign(a.compare(0, 2, b, 0, 2)), sign(a.compare(1, 3, "ppl")), sign(b.compare(2, 3, "ric", 2)), sign(a.compare(0, 5, c)));
    std::printf("== %d %d %d != %d %d\n", a == c ? 1 : 0, a == "apple" ? 1 : 0, "apple" == a ? 1 : 0, a != b ? 1 : 0, "pear" != a ? 1 : 0);
    std::printf("< %d %d %d > %d %d %d\n", a < b ? 1 : 0, a < "b" ? 1 : 0, "a" < a ? 1 : 0, b > a ? 1 : 0, b > "apple" ? 1 : 0, "z" > b ? 1 : 0);
    std::printf("<= %d %d %d >= %d %d %d\n", a <= c ? 1 : 0, a <= "apple" ? 1 : 0, "apple" <= b ? 1 : 0, b >= a ? 1 : 0, b >= "b" ? 1 : 0, "b" >= b ? 1 : 0);
    std::string x = a + "-" + b + '!' + c;
    show("operator+", x);
    show("char+", '[' + a + ']');
    show("cstr+", "<" + a);
    std::vector<std::string> v;
    v.push_back("pear"); v.push_back("fig"); v.push_back("apple"); v.push_back("banana");
    std::sort(v.begin(), v.end());
    for (size_t i = 0; i < v.size(); ++i) std::printf("%s ", v[i].c_str());
    std::printf("\n");
}

static void access()
{
    std::string s("abc");
    s[0] = 'A';
    s.at(1) = 'B';
    const std::string& cs = s;
    std::printf("access %c %c %c %d\n", s[0], cs[1], cs.at(2), cs[3] == '\0' ? 1 : 0);
    int caught = 0;
    try { s.at(3); } catch (std::out_of_range& e) { caught += 1; std::printf("at: %s\n", e.what()); }
    try { cs.at(10); } catch (std::out_of_range&) { caught += 2; }
    try { s.substr(4); } catch (std::out_of_range&) { caught += 4; }
    try { s.insert(9, "x"); } catch (std::out_of_range&) { caught += 8; }
    try { s.erase(4); } catch (std::out_of_range&) { caught += 16; }
    try { s.replace(5, 1, "x"); } catch (std::out_of_range&) { caught += 32; }
    try { s.compare(4, 1, "x"); } catch (std::out_of_range&) { caught += 64; }
    try { std::string t(s, 5); } catch (std::out_of_range&) { caught += 128; }
    try { s.append(s, 9, 1); } catch (std::out_of_range&) { caught += 256; }
    try { char buf[2]; s.copy(buf, 1, 4); } catch (std::out_of_range&) { caught += 512; }
    std::printf("caught %d, substr(size) empty %d\n", caught, s.substr(3).empty() ? 1 : 0);
    char buf[8] = { 0 };
    size_t n = s.copy(buf, 5, 1);
    std::printf("copy %d [%s]\n", static_cast<int>(n), buf);
    std::printf("c_str %d data %d\n", std::strcmp(s.c_str(), "ABc") == 0 ? 1 : 0, std::memcmp(s.data(), "ABc", 3) == 0 ? 1 : 0);
}

static void iterators()
{
    std::string s("iterate");
    std::string up;
    for (std::string::iterator it = s.begin(); it != s.end(); ++it) up += static_cast<char>(*it - 32);
    show("upper", up);
    std::string r;
    const std::string& cs = s;
    for (std::string::const_reverse_iterator it = cs.rbegin(); it != cs.rend(); ++it) r.push_back(*it);
    for (std::string::reverse_iterator it = s.rbegin(); it != s.rend(); ++it) r.push_back(*it == 'e' ? 'E' : *it);
    show("reverse it", r);
    std::reverse(s.begin(), s.end()); show("std::reverse", s);
    std::sort(s.begin(), s.end()); show("std::sort", s);
    std::printf("count e %d, distance %d\n", static_cast<int>(std::count(s.begin(), s.end(), 'e')), static_cast<int>(std::distance(s.begin(), s.end())));
    std::string::size_type n = s.size();
    std::string::difference_type d = s.end() - s.begin();
    std::printf("size_type %d difference_type %d\n", static_cast<int>(n), static_cast<int>(d));
}

static void streams()
{
    std::ostringstream o;
    std::string s("text");
    o << s << '|' << std::string("more") << '|';
    show("<<", o.str());
    std::istringstream in("alpha beta\ngamma line\nlast");
    std::string w1, w2, line, rest;
    in >> w1 >> w2;
    std::getline(in, line);              // the rest of the first line: empty
    std::string l2;
    std::getline(in, l2);
    std::getline(in, rest, 't');
    show(">> 1", w1); show(">> 2", w2); show("getline", line); show("getline 2", l2); show("getline t", rest);
}

static void traits()
{
    typedef std::char_traits<char> T;
    char buf[8];
    T::copy(buf, "abcd", 5);
    T::move(buf + 1, buf, 3);
    std::printf("traits %s %d %d %d %d\n", buf, static_cast<int>(T::length("hello")), sign(T::compare("abc", "abd", 3)), T::eq('a', 'a') ? 1 : 0, T::lt('a', 'b') ? 1 : 0);
    const char* hello = "hello";
    std::printf("find %d eof %d %d not_eof %d int_type %d\n", T::find(hello, 5, 'l') - hello == 2 ? 1 : 0, T::eof(), T::eq_int_type(T::eof(), T::to_int_type('a')) ? 1 : 0, T::not_eof(T::eof()) != T::eof() ? 1 : 0, T::to_int_type(static_cast<char>(200)));
    T::assign(buf, 3, 'q'); buf[3] = 0;
    char c; T::assign(c, 'z');
    std::printf("assign %s %c %c\n", buf, c, T::to_char_type(66));
    typedef std::char_traits<wchar_t> W;
    wchar_t wb[8];
    W::copy(wb, L"wide", 5);
    std::printf("wtraits %d %d %d %d %d\n", static_cast<int>(W::length(wb)), sign(W::compare(L"ab", L"ac", 2)), W::find(wb, 4, L'e') == wb + 3 ? 1 : 0, static_cast<int>(W::eof()), W::eq_int_type(W::eof(), W::to_int_type(L'x')) ? 1 : 0);
}

static void wide()
{
    std::wstring a(L"wide");
    std::wstring b = a + L" string" + L'!';
    showW("wstring", b);
    std::wstring c(3, L'\x263a');
    showW("count", c);
    b.replace(0, 4, L"narrow");
    showW("replace", b);
    std::printf("wfind %s %s %s, compare %d %d\n", pos(b.find(L"string")), pos(b.rfind(L'r')), pos(b.find_first_of(L"!g")), sign(b.compare(L"narrow")), b == L"narrow string!" ? 1 : 0);
    std::wstring d(b.rbegin(), b.rend());
    showW("reversed", d);
    showW("substr", b.substr(7, 3));
    int caught = 0;
    try { b.at(99); } catch (std::out_of_range&) { caught = 1; }
    std::wstring e;
    for (const wchar_t* p = L"abc"; *p; ++p) e.insert(e.begin(), *p);
    std::basic_string<wchar_t> f(e);
    std::printf("at %d, insert [%d %d %d] %d, sizeof unit %d\n", caught, e[0], e[1], e[2], f == e ? 1 : 0, static_cast<int>(sizeof(std::wstring::value_type)));
    std::vector<std::wstring> v;
    v.push_back(L"b"); v.push_back(L"a"); v.push_back(L"c");
    std::sort(v.begin(), v.end());
    showW("sorted", v[0] + v[1] + v[2]);
}

int main()
{
    construction();
    modifiers();
    searching();
    comparing();
    access();
    iterators();
    streams();
    traits();
    wide();
    return 0;
}
