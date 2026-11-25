// facets.cpp - <locale> (C3): the facets of the classic locale that 22.2 requires beyond ctype, numpunct, num_put,
// num_get and collate: codecvt (22.2.1.5), time_get and time_put (22.2.5), moneypunct, money_put and money_get
// (22.2.6), messages (22.2.7), and their _byname forms for "C". Built by cl with its own headers (the reference) and
// by cppparse --compile6 with include/; test 15 compares what the programs print at -O0 and -O2.
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <ctime>
#include <locale>
#include <sstream>
#include <string>
#include <iterator>

static void codecvtChecks()
{
    std::locale c = std::locale::classic();
    std::printf("has codecvt %d %d\n", std::has_facet<std::codecvt<char, char, std::mbstate_t> >(c) ? 1 : 0, std::has_facet<std::codecvt<wchar_t, char, std::mbstate_t> >(c) ? 1 : 0);
    // char to char: nothing to convert
    const std::codecvt<char, char, std::mbstate_t>& cc = std::use_facet<std::codecvt<char, char, std::mbstate_t> >(c);
    std::mbstate_t st = std::mbstate_t();
    const char src[] = "plain";
    char dst[8] = "-------";
    const char* from_next = 0; char* to_next = 0;
    std::codecvt_base::result r = cc.out(st, src, src + 5, from_next, dst, dst + 7, to_next);
    std::printf("cc out %d %d %d\n", static_cast<int>(r), static_cast<int>(from_next - src), static_cast<int>(to_next - dst));
    r = cc.in(st, src, src + 5, from_next, dst, dst + 7, to_next);
    std::printf("cc in %d %d %d\n", static_cast<int>(r), static_cast<int>(from_next - src), static_cast<int>(to_next - dst));
    r = cc.unshift(st, dst, dst + 7, to_next);
    std::printf("cc unshift %d %d\n", static_cast<int>(r), static_cast<int>(to_next - dst));
    std::printf("cc encoding %d noconv %d length %d max %d\n", cc.encoding(), cc.always_noconv() ? 1 : 0, cc.length(st, src, src + 5, 3), cc.max_length());
    // wchar_t to char and back
    const std::codecvt<wchar_t, char, std::mbstate_t>& wc = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t> >(c);
    const wchar_t wsrc[] = L"wide!";
    char out[16]; std::memset(out, '-', 15); out[15] = 0;
    const wchar_t* wfrom = 0;
    st = std::mbstate_t();
    r = wc.out(st, wsrc, wsrc + 5, wfrom, out, out + 15, to_next);
    std::printf("wc out %d %d %d [%.*s]\n", static_cast<int>(r), static_cast<int>(wfrom - wsrc), static_cast<int>(to_next - out), static_cast<int>(to_next - out), out);
    st = std::mbstate_t();
    r = wc.out(st, wsrc, wsrc + 5, wfrom, out, out + 3, to_next);            // too little room
    std::printf("wc out partial %d %d %d\n", static_cast<int>(r), static_cast<int>(wfrom - wsrc), static_cast<int>(to_next - out));
    wchar_t win[8];
    wchar_t* wto = 0;
    st = std::mbstate_t();
    r = wc.in(st, src, src + 5, from_next, win, win + 7, wto);
    std::printf("wc in %d %d %d %d %d\n", static_cast<int>(r), static_cast<int>(from_next - src), static_cast<int>(wto - win), static_cast<int>(win[0]), static_cast<int>(win[4]));
    st = std::mbstate_t();
    r = wc.in(st, src, src + 5, from_next, win, win + 2, wto);               // too little room
    std::printf("wc in partial %d %d %d\n", static_cast<int>(r), static_cast<int>(from_next - src), static_cast<int>(wto - win));
    st = std::mbstate_t();
    r = wc.unshift(st, out, out + 15, to_next);
    std::printf("wc unshift %d %d\n", static_cast<int>(r), static_cast<int>(to_next - out));
    st = std::mbstate_t();
    std::printf("wc noconv %d length %d\n", wc.always_noconv() ? 1 : 0, wc.length(st, src, src + 5, 3));
    const wchar_t euro[] = { L'a', static_cast<wchar_t>(0x20AC), L'b' };   // no such character in the "C" locale
    st = std::mbstate_t();
    r = wc.out(st, euro, euro + 3, wfrom, out, out + 15, to_next);
    std::printf("wc out error %d %d %d\n", static_cast<int>(r), static_cast<int>(wfrom - euro), static_cast<int>(to_next - out));
    const char high[] = { 'x', static_cast<char>(0xE9), 0 };
    st = std::mbstate_t();
    r = wc.in(st, high, high + 2, from_next, win, win + 7, wto);
    std::printf("wc in high %d %d %d\n", static_cast<int>(r), static_cast<int>(wto - win), static_cast<int>(win[1]));
    std::locale named(c, new std::codecvt_byname<wchar_t, char, std::mbstate_t>("C"));
    std::printf("byname noconv %d\n", std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t> >(named).always_noconv() ? 1 : 0);
}

static void messagesChecks()
{
    std::locale c = std::locale::classic();
    std::printf("has messages %d %d\n", std::has_facet<std::messages<char> >(c) ? 1 : 0, std::has_facet<std::messages<wchar_t> >(c) ? 1 : 0);
    const std::messages<char>& m = std::use_facet<std::messages<char> >(c);
    std::messages_base::catalog cat = m.open("no-such-catalog", c);
    std::string got = m.get(cat, 1, 2, "the default");
    std::printf("messages open %d get [%s]\n", cat < 0 ? -1 : 1, got.c_str());
    m.close(cat);
    const std::messages<wchar_t>& wm = std::use_facet<std::messages<wchar_t> >(c);
    std::wstring wgot = wm.get(wm.open("x", c), 0, 0, L"wide default");
    std::printf("wmessages get %d\n", static_cast<int>(wgot.size()));
    std::locale named(c, new std::messages_byname<char>("C"));
    std::printf("messages byname [%s]\n", std::use_facet<std::messages<char> >(named).get(-1, 0, 0, "dflt").c_str());
}

static std::string putTime(const std::tm& t, const char* pattern)
{
    std::ostringstream os;
    const std::time_put<char>& tp = std::use_facet<std::time_put<char> >(os.getloc());
    tp.put(std::ostreambuf_iterator<char>(os), os, '*', &t, pattern, pattern + std::strlen(pattern));
    return os.str();
}

static void timeChecks()
{
    std::locale c = std::locale::classic();
    std::printf("has time %d %d %d %d\n", std::has_facet<std::time_put<char> >(c) ? 1 : 0, std::has_facet<std::time_get<char> >(c) ? 1 : 0,
                std::has_facet<std::time_put<wchar_t> >(c) ? 1 : 0, std::has_facet<std::time_get<wchar_t> >(c) ? 1 : 0);
    std::tm t = std::tm();
    t.tm_year = 98; t.tm_mon = 8; t.tm_mday = 1; t.tm_hour = 14; t.tm_min = 5; t.tm_sec = 9; t.tm_wday = 2; t.tm_yday = 243;
    std::printf("put [%s]\n", putTime(t, "%Y-%m-%d %H:%M:%S").c_str());
    std::printf("put [%s]\n", putTime(t, "%a %A %b %B %j %y %I %p %%").c_str());
    std::printf("put [%s]\n", putTime(t, "%x | %X | %c").c_str());
    std::printf("put [%s]\n", putTime(t, "day %d, month %m: %U %w %W").c_str());
    {
        std::ostringstream os;
        const std::time_put<char>& tp = std::use_facet<std::time_put<char> >(os.getloc());
        tp.put(std::ostreambuf_iterator<char>(os), os, ' ', &t, 'H');
        os << '/';
        tp.put(std::ostreambuf_iterator<char>(os), os, ' ', &t, 'M', 0);
        std::printf("put one [%s]\n", os.str().c_str());
    }
    {
        std::wostringstream os;
        const std::time_put<wchar_t>& tp = std::use_facet<std::time_put<wchar_t> >(os.getloc());
        const wchar_t pat[] = L"%H:%M %B";
        tp.put(std::ostreambuf_iterator<wchar_t>(os), os, L' ', &t, pat, pat + 8);
        std::wstring w = os.str();
        std::printf("wput %d %d %d\n", static_cast<int>(w.size()), static_cast<int>(w[0]), static_cast<int>(w[w.size() - 1]));
    }
    const std::time_get<char>& tg = std::use_facet<std::time_get<char> >(c);
    std::printf("date_order %d\n", static_cast<int>(tg.date_order()));
    {
        std::istringstream is("14:05:09 rest");
        std::tm g = std::tm(); std::ios_base::iostate err = std::ios_base::goodbit;
        std::istreambuf_iterator<char> end = tg.get_time(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), is, err, &g);
        std::printf("get_time %d:%d:%d err %d next %c\n", g.tm_hour, g.tm_min, g.tm_sec, static_cast<int>(err), end == std::istreambuf_iterator<char>() ? '$' : *end);
    }
    {
        std::istringstream is("09/01/98 rest");
        std::tm g = std::tm(); std::ios_base::iostate err = std::ios_base::goodbit;
        std::istreambuf_iterator<char> end = tg.get_date(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), is, err, &g);
        std::printf("get_date %d %d %d err %d next %c\n", g.tm_mon, g.tm_mday, g.tm_year, static_cast<int>(err), end == std::istreambuf_iterator<char>() ? '$' : *end);
    }
    {
        std::istringstream is("Tuesday Tue Sat xyz");
        std::tm g = std::tm(); std::ios_base::iostate err = std::ios_base::goodbit;
        std::istreambuf_iterator<char> it(is), end;
        it = tg.get_weekday(it, end, is, err, &g);
        std::printf("get_weekday %d err %d", g.tm_wday, static_cast<int>(err));
        ++it; err = std::ios_base::goodbit;
        it = tg.get_weekday(it, end, is, err, &g);
        std::printf(" %d err %d", g.tm_wday, static_cast<int>(err));
        ++it; err = std::ios_base::goodbit;
        it = tg.get_weekday(it, end, is, err, &g);
        std::printf(" %d err %d", g.tm_wday, static_cast<int>(err));
        ++it; err = std::ios_base::goodbit; g.tm_wday = -5;
        it = tg.get_weekday(it, end, is, err, &g);
        std::printf(" bad %d fail %d\n", g.tm_wday, (err & std::ios_base::failbit) ? 1 : 0);
    }
    {
        std::istringstream is("September Sep dec 1998 98 x");
        std::tm g = std::tm(); std::ios_base::iostate err = std::ios_base::goodbit;
        std::istreambuf_iterator<char> it(is), end;
        it = tg.get_monthname(it, end, is, err, &g);
        std::printf("get_monthname %d err %d", g.tm_mon, static_cast<int>(err));
        ++it; err = std::ios_base::goodbit;
        it = tg.get_monthname(it, end, is, err, &g);
        std::printf(" %d err %d", g.tm_mon, static_cast<int>(err));
        ++it; err = std::ios_base::goodbit; g.tm_mon = -5;
        it = tg.get_monthname(it, end, is, err, &g);
        std::printf(" %d fail %d", g.tm_mon, (err & std::ios_base::failbit) ? 1 : 0);
        ++it; err = std::ios_base::goodbit;
        it = tg.get_year(it, end, is, err, &g);
        std::printf(" year %d err %d", g.tm_year, static_cast<int>(err));
        ++it; err = std::ios_base::goodbit;
        it = tg.get_year(it, end, is, err, &g);
        std::printf(" %d err %d\n", g.tm_year, static_cast<int>(err));
    }
    {
        const char* years[] = { "05", "68", "69", "2005", "1969", "x" };
        std::printf("years");
        for (int i = 0; i < 6; ++i) {
            std::istringstream is(years[i]);
            std::tm g = std::tm(); g.tm_year = -1; std::ios_base::iostate err = std::ios_base::goodbit;
            tg.get_year(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), is, err, &g);
            std::printf(" %d/%d%d", g.tm_year, (err & std::ios_base::failbit) ? 1 : 0, (err & std::ios_base::eofbit) ? 1 : 0);
        }
        std::printf("\n");
    }
    {
        const char* times[] = { "23:59:59", "24:00:00", "7:5:3", "12:60:00", "12-30-00", "12:30" };
        std::printf("times");
        for (int i = 0; i < 6; ++i) {
            std::istringstream is(times[i]);
            std::tm g = std::tm(); g.tm_hour = -1; std::ios_base::iostate err = std::ios_base::goodbit;
            tg.get_time(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), is, err, &g);
            if (err & std::ios_base::failbit) std::printf(" fail"); else std::printf(" %d:%d:%d", g.tm_hour, g.tm_min, g.tm_sec);   // (a failed get leaves unspecified values)
        }
        std::printf("\n");
        const char* dates[] = { "12/31/1999", "1/2/3", "13/01/98", "02/30/98", "09-01-98", "09/01" };
        std::printf("dates");
        for (int i = 0; i < 6; ++i) {
            std::istringstream is(dates[i]);
            std::tm g = std::tm(); g.tm_mon = -1; std::ios_base::iostate err = std::ios_base::goodbit;
            tg.get_date(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), is, err, &g);
            if (err & std::ios_base::failbit) std::printf(" fail"); else std::printf(" %d/%d/%d", g.tm_mon, g.tm_mday, g.tm_year);
        }
        std::printf("\n");
    }
    {
        const std::time_get<wchar_t>& wtg = std::use_facet<std::time_get<wchar_t> >(c);
        std::wistringstream is(L"Friday");
        std::tm g = std::tm(); std::ios_base::iostate err = std::ios_base::goodbit;
        wtg.get_weekday(std::istreambuf_iterator<wchar_t>(is), std::istreambuf_iterator<wchar_t>(), is, err, &g);
        std::printf("wide weekday %d fail %d\n", g.tm_wday, (err & std::ios_base::failbit) ? 1 : 0);
    }
}

static char partName(char part)
{
    return part == std::money_base::none ? 'n' : part == std::money_base::space ? '_' : part == std::money_base::symbol ? '$'
         : part == std::money_base::sign ? '+' : part == std::money_base::value ? 'v' : '?';
}

template <bool Intl> static void punctChecks(const char* what)
{
    const std::moneypunct<char, Intl>& mp = std::use_facet<std::moneypunct<char, Intl> >(std::locale::classic());
    std::money_base::pattern p = mp.pos_format(), n = mp.neg_format();
    std::printf("%s point %d sep %d grouping %d symbol [%s] positive [%s] negative [%s] frac %d pos %c%c%c%c neg %c%c%c%c intl %d\n", what,
                static_cast<int>(mp.decimal_point()), static_cast<int>(mp.thousands_sep()), static_cast<int>(mp.grouping().size()), mp.curr_symbol().c_str(),
                mp.positive_sign().c_str(), mp.negative_sign().c_str(), mp.frac_digits(), partName(p.field[0]), partName(p.field[1]), partName(p.field[2]), partName(p.field[3]),
                partName(n.field[0]), partName(n.field[1]), partName(n.field[2]), partName(n.field[3]), std::moneypunct<char, Intl>::intl ? 1 : 0);
}

// two decimals, grouped by threes, a currency symbol and signs
class Euro : public std::moneypunct<char, false> {
public:
    explicit Euro(size_t refs = 0) : std::moneypunct<char, false>(refs) {}
protected:
    virtual char do_decimal_point() const { return ','; }
    virtual char do_thousands_sep() const { return '.'; }
    virtual std::string do_grouping() const { return "\3"; }
    virtual std::string do_curr_symbol() const { return "EUR"; }
    virtual std::string do_positive_sign() const { return "+"; }
    virtual std::string do_negative_sign() const { return "()"; }
    virtual int do_frac_digits() const { return 2; }
    virtual pattern do_pos_format() const { pattern p = { { symbol, space, sign, value } }; return p; }
    virtual pattern do_neg_format() const { pattern p = { { sign, value, space, symbol } }; return p; }
};

static std::string putMoney(const std::locale& loc, long double units, bool showbase, int width = 0, std::ios_base::fmtflags adjust = std::ios_base::right)
{
    std::ostringstream os;
    os.imbue(loc);
    if (showbase) os.setf(std::ios_base::showbase);
    os.setf(adjust, std::ios_base::adjustfield);
    os.width(width);
    const std::money_put<char>& mp = std::use_facet<std::money_put<char> >(loc);
    mp.put(std::ostreambuf_iterator<char>(os), false, os, '*', units);
    return os.str();
}
static std::string putMoneyDigits(const std::locale& loc, const char* digits, bool showbase)
{
    std::ostringstream os;
    os.imbue(loc);
    if (showbase) os.setf(std::ios_base::showbase);
    const std::money_put<char>& mp = std::use_facet<std::money_put<char> >(loc);
    mp.put(std::ostreambuf_iterator<char>(os), false, os, '*', std::string(digits));
    return os.str();
}
static void getMoney(const std::locale& loc, const char* text, bool showbase)
{
    std::istringstream is(text);
    is.imbue(loc);
    if (showbase) is.setf(std::ios_base::showbase);
    const std::money_get<char>& mg = std::use_facet<std::money_get<char> >(loc);
    std::ios_base::iostate err = std::ios_base::goodbit;
    long double units = -1;
    std::istreambuf_iterator<char> end;
    std::istreambuf_iterator<char> it = mg.get(std::istreambuf_iterator<char>(is), end, false, is, err, units);
    std::istringstream is2(text);
    is2.imbue(loc);
    if (showbase) is2.setf(std::ios_base::showbase);
    std::ios_base::iostate err2 = std::ios_base::goodbit;
    std::string digits = "?";
    mg.get(std::istreambuf_iterator<char>(is2), end, false, is2, err2, digits);
    std::printf("get [%s] units %.0f fail %d eof %d next %c digits [%s] fail %d\n", text, static_cast<double>(units), (err & std::ios_base::failbit) ? 1 : 0,
                (err & std::ios_base::eofbit) ? 1 : 0, it == end ? '$' : *it, digits.c_str(), (err2 & std::ios_base::failbit) ? 1 : 0);
}

static void moneyChecks()
{
    std::locale c = std::locale::classic();
    std::printf("has money %d %d %d %d\n", std::has_facet<std::moneypunct<char, false> >(c) ? 1 : 0, std::has_facet<std::moneypunct<char, true> >(c) ? 1 : 0,
                std::has_facet<std::money_put<char> >(c) ? 1 : 0, std::has_facet<std::money_get<char> >(c) ? 1 : 0);
    punctChecks<false>("moneypunct");
    punctChecks<true>("moneypunct intl");
    std::printf("classic put [%s] [%s] [%s] [%s] [%s]\n", putMoney(c, 123456, false).c_str(), putMoney(c, -42, false).c_str(), putMoney(c, 0, true).c_str(),
                putMoneyDigits(c, "-0012", false).c_str(), putMoney(c, 7, false, 6).c_str());
    getMoney(c, "1234 x", false);
    getMoney(c, "-56", false);
    getMoney(c, "abc", false);
    std::locale e(c, new Euro);
    std::printf("euro put [%s] [%s] [%s] [%s]\n", putMoney(e, 123456789, false).c_str(), putMoney(e, 123456789, true).c_str(), putMoney(e, -5, true).c_str(), putMoney(e, 5, false).c_str());
    std::printf("euro put [%s] [%s] [%s] [%s]\n", putMoney(e, -123456, true, 20).c_str(), putMoney(e, -123456, true, 20, std::ios_base::left).c_str(),
                putMoney(e, -123456, true, 20, std::ios_base::internal).c_str(), putMoneyDigits(e, "98765", true).c_str());
    getMoney(e, "EUR +1.234,56", true);
    getMoney(e, "EUR +1.234,56", false);
    getMoney(e, "+1.234,56", false);
    getMoney(e, "(12,34 EUR)", true);
    getMoney(e, "(12,34)", false);
    getMoney(e, "+12,3", false);
    getMoney(e, "+1.234,56 EUR", true);
    getMoney(e, "(1.234.567,89  EUR) tail", true);
    getMoney(e, "(0,05 EUR)", true);
    getMoney(e, "(007,00 EUR)", true);
    getMoney(e, "(12,34 USD)", true);
    getMoney(e, "(12,34 EUR", true);
    getMoney(e, "(1234,56 EUR)", true);
    getMoney(e, "+1.23,45", false);
    std::locale named(c, new std::moneypunct_byname<char, false>("C"));
    std::printf("byname frac %d\n", std::use_facet<std::moneypunct<char, false> >(named).frac_digits());
}

int main()
{
    std::setvbuf(stdout, 0, _IONBF, 0);
    codecvtChecks();
    messagesChecks();
    timeChecks();
    moneyChecks();
    return 0;
}
