// locale.cpp - <locale> (C3): the classic locale, use_facet and has_facet, ctype, numpunct and collate, a facet the
// program derives and installs in a stream; built by cl with its own headers (the reference) and by cppparse
// --compile6 with include/, test 15 compares what the programs print at -O0 and -O2.
#include <cstdio>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

// thousands grouped by threes with '.', a ',' for the point, "yes" / "no"
class Punct : public std::numpunct<char> {
public:
    explicit Punct(size_t refs = 0) : std::numpunct<char>(refs) {}
protected:
    virtual char do_decimal_point() const { return ','; }
    virtual char do_thousands_sep() const { return '.'; }
    virtual std::string do_grouping() const { return "\3"; }
    virtual std::string do_truename() const { return "yes"; }
    virtual std::string do_falsename() const { return "no"; }
};

int main()
{
    std::setvbuf(stdout, 0, _IONBF, 0);
    std::locale c = std::locale::classic();
    std::locale g;
    std::printf("classic name %s, global == classic %d, C == classic %d\n", c.name().c_str(), g == c ? 1 : 0, std::locale("C") == c ? 1 : 0);
    int bad = 0;
    try { std::locale nope("no-such-locale"); } catch (std::runtime_error&) { bad = 1; }
    std::printf("bad name throws %d\n", bad);

    const std::ctype<char>& ct = std::use_facet<std::ctype<char> >(c);
    std::printf("ctype is %d%d%d%d%d%d upper %c lower %c widen %c narrow %c\n", ct.is(std::ctype_base::alpha, 'a') ? 1 : 0, ct.is(std::ctype_base::digit, '7') ? 1 : 0,
                ct.is(std::ctype_base::space, '\t') ? 1 : 0, ct.is(std::ctype_base::punct, '!') ? 1 : 0, ct.is(std::ctype_base::upper, 'a') ? 1 : 0,
                ct.is(std::ctype_base::xdigit, 'F') ? 1 : 0, ct.toupper('q'), ct.tolower('Q'), ct.widen('w'), ct.narrow('n', '?'));
    char word[] = "MiXeD 42";
    ct.tolower(word, word + 8);
    std::printf("tolower range [%s]", word);
    ct.toupper(word, word + 8);
    std::printf(" toupper range [%s]", word);
    const char* text = "ab1 c";
    std::printf(" scan_is digit at %d scan_not alpha at %d\n", static_cast<int>(ct.scan_is(std::ctype_base::digit, text, text + 5) - text), static_cast<int>(ct.scan_not(std::ctype_base::alpha, text, text + 5) - text));
    const std::ctype<wchar_t>& wct = std::use_facet<std::ctype<wchar_t> >(c);
    std::printf("wide ctype alpha %d upper %c widen %d narrow %c %c\n", wct.is(std::ctype_base::alpha, L'z') ? 1 : 0, static_cast<char>(wct.toupper(L'k')), static_cast<int>(wct.widen('A')), wct.narrow(L'b', '?'), wct.narrow(static_cast<wchar_t>(0x263a), '?'));
    std::printf("convenience %d %d %d %d %d %c %c\n", std::isalpha('x', c) ? 1 : 0, std::isdigit('x', c) ? 1 : 0, std::isspace(' ', c) ? 1 : 0, std::isupper(L'Q', c) ? 1 : 0,
                std::isalnum('_', c) ? 1 : 0, std::toupper('m', c), std::tolower('M', c));

    const std::numpunct<char>& np = std::use_facet<std::numpunct<char> >(c);
    std::printf("numpunct point %c sep %c grouping %d true %s false %s\n", np.decimal_point(), np.thousands_sep(), static_cast<int>(np.grouping().size()), np.truename().c_str(), np.falsename().c_str());
    const std::collate<char>& col = std::use_facet<std::collate<char> >(c);
    const char* x = "apple"; const char* y = "apricot";
    std::printf("collate %d %d %d hash equal %d\n", col.compare(x, x + 5, y, y + 7), col.compare(y, y + 7, x, x + 5), col.compare(x, x + 5, x, x + 5),
                col.hash(x, x + 5) == col.hash(x, x + 5) ? 1 : 0);
    std::printf("locale() compares %d %d\n", c(std::string("a"), std::string("b")) ? 1 : 0, c(std::string("b"), std::string("a")) ? 1 : 0);
    std::printf("has_facet ctype %d numpunct %d\n", std::has_facet<std::ctype<char> >(c) ? 1 : 0, std::has_facet<std::numpunct<char> >(c) ? 1 : 0);

    // a stream imbued with a locale holding the program's numpunct
    std::locale mine(c, new Punct);
    std::printf("installed: has %d, name %s, == classic %d\n", std::has_facet<Punct>(mine) ? 1 : 0, mine.name().c_str(), mine == c ? 1 : 0);
    std::ostringstream o;
    o.imbue(mine);
    o << 1234567 << ' ' << 3.5 << ' ' << -1000 << ' ' << 999 << ' ' << std::boolalpha << true << ' ' << false;
    std::printf("imbued [%s]\n", o.str().c_str());
    std::istringstream in("1.234.567 2,25");
    in.imbue(mine);
    long n = 0; double d = 0;
    in >> n >> d;
    std::printf("read back %ld %.2f\n", n, d);
    std::locale back = o.getloc();
    std::printf("getloc has Punct %d\n", std::has_facet<Punct>(back) ? 1 : 0);
    std::locale mixed(c, mine, std::locale::numeric);
    std::printf("numeric from mine: point %c\n", std::use_facet<std::numpunct<char> >(mixed).decimal_point());
    std::locale combined = c.combine<std::numpunct<char> >(mine);
    std::printf("combine: sep %c\n", std::use_facet<std::numpunct<char> >(combined).thousands_sep());
    std::locale old = std::locale::global(mine);
    std::ostringstream o2;
    o2 << 12345;
    std::printf("global locale streams [%s]\n", o2.str().c_str());
    std::locale::global(old);
    std::ostringstream o3;
    o3 << 12345;
    std::printf("restored [%s]\n", o3.str().c_str());
    return 0;
}
