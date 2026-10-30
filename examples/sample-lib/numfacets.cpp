// numfacets.cpp - num_put and num_get (22.2.2): the streams write and read numbers through the locale's facets, so a
// facet the program installs is the one used; the facets called directly through the stream buffer iterators; the
// formatting of printf (no '+' on an unsigned value, no 0x on zero, internal padding) and the parsing of strtol and
// strtod (the base from the prefix, the range, the grouping). Built by cl with its own headers (the reference) and by
// cppparse --compile6 with include/, test 15 compares what the programs print at -O0 and -O2.
#include <cstdio>
#include <iomanip>
#include <ios>
#include <iterator>
#include <locale>
#include <sstream>
#include <string>

// a num_put that brackets the longs it writes (the other types are the library's)
class Bracketed : public std::num_put<char> {
public:
    explicit Bracketed(size_t refs = 0) : std::num_put<char>(refs) {}
protected:
    virtual iter_type do_put(iter_type s, std::ios_base& f, char_type fill, long v) const
    {
        *s++ = '[';
        s = std::num_put<char>::do_put(s, f, fill, v);
        *s++ = ']';
        return s;
    }
};

// a num_get that reads every long in hexadecimal
class HexLongs : public std::num_get<char> {
public:
    explicit HexLongs(size_t refs = 0) : std::num_get<char>(refs) {}
protected:
    virtual iter_type do_get(iter_type in, iter_type end, std::ios_base& f, std::ios_base::iostate& err, long& v) const
    {
        const std::ios_base::fmtflags saved = f.flags();
        f.setf(std::ios_base::hex, std::ios_base::basefield);
        in = std::num_get<char>::do_get(in, end, f, err, v);
        f.flags(saved);
        return in;
    }
};

// thousands in threes, with an apostrophe
class Thousands : public std::numpunct<char> {
protected:
    virtual char do_thousands_sep() const { return '\''; }
    virtual std::string do_grouping() const { return "\3"; }
};

static std::string text(const std::ostringstream& o) { return o.str(); }

template <class T> static void readOne(const char* what, const char* input, std::ios_base& (*base)(std::ios_base&))
{
    std::istringstream in(input);
    if (base) in >> base;
    T v = T();
    in >> v;
    std::ostringstream o;
    o << v;
    std::printf("%-22s [%s] -> %s fail %d eof %d\n", what, input, o.str().c_str(), in.fail() ? 1 : 0, in.eof() ? 1 : 0);
}

int main()
{
    // printf's rules
    std::ostringstream a;
    a << std::showpos << 5u << ' ' << 5 << ' ' << 0 << ' ' << 2.5 << std::noshowpos;
    a << ' ' << std::showbase << std::hex << 0 << ' ' << 255 << ' ' << std::oct << 0 << ' ' << 8 << std::dec << std::noshowbase;
    std::printf("printf [%s]\n", text(a).c_str());
    std::ostringstream b;
    b << std::internal << std::setw(8) << -42 << '|' << std::setw(10) << std::showbase << std::hex << 255 << '|' << std::dec;
    b << std::left << std::setw(6) << 7 << '|' << std::right << std::setfill('.') << std::setw(6) << 3.25 << '|';
    b << std::internal << std::setw(7) << std::showpos << 12 << std::noshowpos << '|';
    std::printf("padding [%s]\n", text(b).c_str());
    std::ostringstream c;
    c << std::hex << -1 << ' ' << static_cast<short>(-1) << ' ' << -1L << ' ' << -1LL << std::dec;
    std::printf("negative hex [%s]\n", text(c).c_str());

    // a num_put of the program's, through the stream and directly
    std::ostringstream d;
    d.imbue(std::locale(std::locale::classic(), new Bracketed));
    d << 42 << ' ' << 7L << ' ' << 3.5 << ' ' << 9u << ' ' << static_cast<short>(-3);
    std::printf("bracketed [%s]\n", text(d).c_str());
    std::ostringstream e;
    e.width(9);
    const std::num_put<char>& np = std::use_facet<std::num_put<char> >(e.getloc());
    np.put(std::ostreambuf_iterator<char>(e), e, '*', 1234L);
    np.put(std::ostreambuf_iterator<char>(e), e, '*', 0.125);
    std::printf("direct put [%s] width after %d\n", text(e).c_str(), static_cast<int>(e.width()));
    std::printf("has_facet %d %d\n", std::has_facet<std::num_put<char> >(std::locale::classic()) ? 1 : 0, std::has_facet<std::num_get<wchar_t> >(std::locale()) ? 1 : 0);

    // grouping, written and read
    std::locale grouped(std::locale::classic(), new Thousands);
    std::ostringstream g;
    g.imbue(grouped);
    g << 1234567 << ' ' << -1000 << ' ' << 999 << ' ' << 1234567.5;
    std::printf("grouped [%s]\n", text(g).c_str());
    std::istringstream gi("1'234'567 12'34 5");
    gi.imbue(grouped);
    long g1 = 0, g2 = 0;
    gi >> g1;
    std::printf("grouped read %ld fail %d\n", g1, gi.fail() ? 1 : 0);
    gi >> g2;
    std::printf("bad grouping %ld fail %d\n", g2, gi.fail() ? 1 : 0);

    // num_get: bases, ranges, signs, floating values, bool
    readOne<int>("int", "  -123x", 0);
    readOne<int>("int hex", "0x1F", std::hex);
    readOne<int>("int hex bare", "ff", std::hex);
    readOne<int>("int oct", "017", std::oct);
    readOne<int>("int overflow", "99999999999", 0);
    readOne<int>("int underflow", "-99999999999", 0);
    readOne<short>("short overflow", "40000", 0);
    readOne<short>("short", "-32768", 0);
    readOne<unsigned>("unsigned minus", "-1", 0);
    readOne<unsigned short>("ushort", "65535", 0);
    readOne<unsigned short>("ushort overflow", "65536", 0);
    readOne<long long>("long long", "-9223372036854775808", 0);
    readOne<unsigned long long>("ull", "18446744073709551615", 0);
    readOne<int>("no digits", "abc", 0);
    readOne<int>("sign only", "-", 0);
    readOne<double>("double", "1.5e3x", 0);
    readOne<double>("double point", "-.5", 0);
    readOne<double>("double exp", "2E-2", 0);
    readOne<double>("double none", "e5", 0);
    readOne<float>("float", "3.4028235e38", 0);
    readOne<bool>("bool 1", "1", 0);
    readOne<bool>("bool 0", "0", 0);
    readOne<bool>("bool 2", "2", 0);
    {
        std::istringstream in("0x1F 017 17");
        in.unsetf(std::ios_base::basefield);
        int x = 0, y = 0, z = 0;
        in >> x >> y >> z;
        std::printf("base from prefix %d %d %d\n", x, y, z);
    }
    {
        std::istringstream in("true false tru");
        in >> std::boolalpha;
        bool t = false, f = true, u = false;
        in >> t >> f;
        std::printf("boolalpha %d %d fail %d\n", t ? 1 : 0, f ? 1 : 0, in.fail() ? 1 : 0);
        in >> u;
        std::printf("boolalpha bad fail %d\n", in.fail() ? 1 : 0);
    }
    // a num_get of the program's, through the stream and directly
    {
        std::istringstream in("ff 10");
        in.imbue(std::locale(std::locale::classic(), new HexLongs));
        long h = 0;
        int i = 0;
        in >> h >> i;
        std::printf("hex longs %ld %d\n", h, i);
        std::istringstream raw("77 88");
        std::ios_base::iostate err = std::ios_base::goodbit;
        long r = 0;
        const std::num_get<char>& ng = std::use_facet<std::num_get<char> >(raw.getloc());
        std::istreambuf_iterator<char> it(raw), end;
        it = ng.get(it, end, raw, err, r);
        std::printf("direct get %ld err %d next [%c]\n", r, static_cast<int>(err), *it);
        ++it;
        it = ng.get(it, end, raw, err, r);
        std::printf("direct get2 %ld err %d next [%c]\n", r, static_cast<int>(err), *it);
    }
    // wide streams through the wide facets
    std::wostringstream w;
    w << 1234567 << L' ' << std::hex << std::showbase << 255;
    std::printf("wide %d chars\n", static_cast<int>(w.str().size()));
    std::wistringstream wi(L"  4096 2.5");
    int wv = 0;
    double wd = 0;
    wi >> wv >> wd;
    std::printf("wide read %d %.2f\n", wv, wd);
    return 0;
}
