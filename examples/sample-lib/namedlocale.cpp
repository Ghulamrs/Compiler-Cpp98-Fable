// namedlocale.cpp - named locales (row G28): std::locale("de-DE") and the other names the C runtime knows, each facet
// of 22.2 read from the C runtime's locale of that name: ctype, numpunct, collate, the time and the monetary facets
// for char and wchar_t, the _byname facets, the constructors that combine categories, and locale::global, which sets
// the C locale. Built by cl with its own headers (the reference) and by cppparse --compile6 with include/; test 15
// compares what the programs print at -O0 and -O2. The four names are those of the row; one that the C runtime of
// the machine does not accept (_create_locale) is reported and left out, in both builds alike. With other names as
// arguments the program prints those locales' facets instead; with the argument own, what cppparse's library does on
// purpose where cl's does otherwise (the test checks those lines for cppparse's build alone).
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// a string with every byte outside printable ASCII as <XX>, a wide one with those characters as <U+XXXX>
static std::string shown(const std::string& s)
{
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 0x20 && c < 0x7F) r += s[i];
        else { char b[8]; std::sprintf(b, "<%02X>", c); r += b; }
    }
    return r;
}
static std::string shown(const std::wstring& s)
{
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned c = static_cast<unsigned>(s[i]) & 0xFFFFu;
        if (c >= 0x20 && c < 0x7F) r += static_cast<char>(c);
        else { char b[12]; std::sprintf(b, "<U+%04X>", c); r += b; }
    }
    return r;
}
static std::string shown(char c) { return shown(std::string(1, c)); }
static std::string shown(wchar_t c) { return shown(std::wstring(1, c)); }
static std::string sizes(const std::string& grouping)
{
    std::string r;
    for (size_t i = 0; i < grouping.size(); ++i) { char b[8]; std::sprintf(b, "%s%d", i ? " " : "", static_cast<int>(grouping[i])); r += b; }
    return r.empty() ? std::string("none") : r;
}
static const char* part(char p)
{
    switch (p) {
    case std::money_base::none: return "none";
    case std::money_base::space: return "space";
    case std::money_base::symbol: return "symbol";
    case std::money_base::sign: return "sign";
    case std::money_base::value: return "value";
    default: return "?";
    }
}
static std::string format(const std::money_base::pattern& p)
{
    std::string r;
    for (int i = 0; i < 4; ++i) { if (i) r += ' '; r += part(p.field[i]); }
    return r;
}
static int sign(int v) { return v < 0 ? -1 : v > 0 ? 1 : 0; }
static const char* order(std::time_base::dateorder o)
{
    switch (o) {
    case std::time_base::no_order: return "no_order";
    case std::time_base::dmy: return "dmy";
    case std::time_base::mdy: return "mdy";
    case std::time_base::ymd: return "ymd";
    case std::time_base::ydm: return "ydm";
    default: return "?";
    }
}
static bool available(const char* name)
{
    _locale_t h = _create_locale(LC_ALL, name);
    if (h) _free_locale(h);
    return h != 0;
}

// 22.2.3: the punctuation of numbers, and the streams that write and read with it
static void numbers(const std::locale& loc)
{
    const std::numpunct<char>& np = std::use_facet<std::numpunct<char> >(loc);
    std::printf("  numpunct point [%s] sep [%s] grouping %s true %s false %s\n", shown(np.decimal_point()).c_str(), shown(np.thousands_sep()).c_str(), sizes(np.grouping()).c_str(),
                np.truename().c_str(), np.falsename().c_str());
    const std::numpunct<wchar_t>& wnp = std::use_facet<std::numpunct<wchar_t> >(loc);
    std::printf("  wide numpunct point [%s] sep [%s] grouping %s true %s\n", shown(wnp.decimal_point()).c_str(), shown(wnp.thousands_sep()).c_str(), sizes(wnp.grouping()).c_str(), shown(wnp.truename()).c_str());
    std::ostringstream o;
    o.imbue(loc);
    o << 1234567 << ' ' << 1234567.25 << ' ' << -0.5 << ' ' << 999 << ' ' << std::boolalpha << true;
    std::printf("  stream writes [%s]\n", shown(o.str()).c_str());
    std::istringstream in(o.str());
    in.imbue(loc);
    long n = 0; double d = 0, e = 0;
    in >> n >> d >> e;
    std::printf("  stream reads %ld %.2f %.2f state %d\n", n, d, e, static_cast<int>(in.rdstate()));
    std::wostringstream wo;
    wo.imbue(loc);
    wo << 7654321 << L' ' << 2.5;
    std::printf("  wide stream writes [%s]\n", shown(wo.str()).c_str());
    std::wistringstream win(wo.str());
    win.imbue(loc);
    long wn = 0; double wd = 0;
    win >> wn >> wd;
    std::printf("  wide stream reads %ld %.2f\n", wn, wd);
}

// 22.2.1: the classification and the case of the characters of the locale's code page, and of wide characters
static void characters(const std::locale& loc)
{
    typedef std::ctype_base cb;
    const std::ctype<char>& ct = std::use_facet<std::ctype<char> >(loc);
    static const unsigned char probe[] = { 'a', 0xE4, 0xC4, 0xDF, 0xA0, 0x80, 0xB5, 0xD7, 0xFF };
    for (size_t i = 0; i < sizeof(probe); ++i) {
        const char c = static_cast<char>(probe[i]);
        std::printf("  %02X alpha %d lower %d upper %d space %d punct %d print %d cntrl %d upper %02X lower %02X\n", probe[i], ct.is(cb::alpha, c) ? 1 : 0, ct.is(cb::lower, c) ? 1 : 0, ct.is(cb::upper, c) ? 1 : 0,
                    ct.is(cb::space, c) ? 1 : 0, ct.is(cb::punct, c) ? 1 : 0, ct.is(cb::print, c) ? 1 : 0, ct.is(cb::cntrl, c) ? 1 : 0, static_cast<unsigned char>(ct.toupper(c)), static_cast<unsigned char>(ct.tolower(c)));
    }
    // every character of the code page: how many are in each class, and where the case conversions lead
    static const cb::mask masks[] = { cb::upper, cb::lower, cb::alpha, cb::digit, cb::punct, cb::xdigit, cb::space, cb::cntrl, cb::print, cb::graph, cb::alnum };
    std::printf("  classes");
    for (size_t m = 0; m < sizeof(masks) / sizeof(masks[0]); ++m) {
        int count = 0;
        for (int c = 0; c < 256; ++c) if (ct.is(masks[m], static_cast<char>(c))) ++count;
        std::printf(" %d", count);
    }
    unsigned up = 0, low = 0;
    for (int c = 0; c < 256; ++c) { up = up * 31 + static_cast<unsigned char>(ct.toupper(static_cast<char>(c))); low = low * 31 + static_cast<unsigned char>(ct.tolower(static_cast<char>(c))); }
    std::printf(" toupper %08X tolower %08X\n", up, low);
    char word[] = "Stra\xDF" "e \xE4\xF6\xFC \xC9t\xE9 42";
    const size_t len = std::strlen(word);
    ct.toupper(word, word + len);
    std::printf("  toupper range [%s]", shown(word).c_str());
    ct.tolower(word, word + len);
    std::printf(" tolower range [%s]\n", shown(word).c_str());
    const char* text = "\xE4\xF6 1\xA0x";
    cb::mask vec[6];
    ct.is(text, text + 6, vec);
    std::printf("  scan_is space at %d scan_not alpha at %d masks", static_cast<int>(ct.scan_is(cb::space, text, text + 6) - text), static_cast<int>(ct.scan_not(cb::alpha, text, text + 6) - text));
    for (int i = 0; i < 6; ++i) std::printf(" %d%d%d%d", (vec[i] & cb::alpha) ? 1 : 0, (vec[i] & cb::lower) ? 1 : 0, (vec[i] & cb::digit) ? 1 : 0, (vec[i] & cb::space) ? 1 : 0);
    std::printf("\n  convenience %d %d %d %02X %02X\n", std::isalpha('\xE4', loc) ? 1 : 0, std::isupper('\xC4', loc) ? 1 : 0, std::isspace('\xA0', loc) ? 1 : 0,
                static_cast<unsigned char>(std::toupper('\xE4', loc)), static_cast<unsigned char>(std::tolower('\xC4', loc)));

    const std::ctype<wchar_t>& wct = std::use_facet<std::ctype<wchar_t> >(loc);
    static const wchar_t wprobe[] = { L'a', 0xE4, 0xC4, 0xDF, 0xFF, 0x2003, 0x153, 0x3B1, 0x20AC, 0x431 };
    for (size_t i = 0; i < sizeof(wprobe) / sizeof(wprobe[0]); ++i) {
        const wchar_t c = wprobe[i];
        std::printf("  U+%04X alpha %d lower %d upper %d space %d punct %d print %d upper %04X lower %04X narrow [%s]\n", static_cast<unsigned>(c), wct.is(cb::alpha, c) ? 1 : 0, wct.is(cb::lower, c) ? 1 : 0,
                    wct.is(cb::upper, c) ? 1 : 0, wct.is(cb::space, c) ? 1 : 0, wct.is(cb::punct, c) ? 1 : 0, wct.is(cb::print, c) ? 1 : 0, static_cast<unsigned>(wct.toupper(c)), static_cast<unsigned>(wct.tolower(c)),
                    shown(wct.narrow(c, '?')).c_str());
    }
    std::printf("  widen");
    static const unsigned char narrow[] = { 'a', 0x80, 0x8A, 0x9C, 0xA0, 0xE4, 0xFF };
    for (size_t i = 0; i < sizeof(narrow); ++i) std::printf(" %04X", static_cast<unsigned>(wct.widen(static_cast<char>(narrow[i]))));
    wchar_t wword[] = L"Stra\x00DF" L"e \x00E4\x00F6\x00FC \x0153";
    const size_t wlen = sizeof(wword) / sizeof(wword[0]) - 1;
    wct.toupper(wword, wword + wlen);
    std::printf("\n  wide toupper range [%s]", shown(std::wstring(wword)).c_str());
    wct.tolower(wword, wword + wlen);
    std::printf(" tolower range [%s]\n", shown(std::wstring(wword)).c_str());
    char back[16];
    wct.narrow(wword, wword + wlen, '?', back);
    std::printf("  narrow range [%s] scan_is upper at %d\n", shown(std::string(back, wlen)).c_str(), static_cast<int>(wct.scan_is(cb::upper, wword, wword + wlen) - wword));

    // 22.2.1.5: the conversion between the code page and wide characters
    typedef std::codecvt<wchar_t, char, std::mbstate_t> convert;
    const convert& cv = std::use_facet<convert>(loc);
    std::mbstate_t state = std::mbstate_t();
    const char bytes[] = "a\x80\xE4\x9C z";
    const char* bytesNext = 0;
    wchar_t wide[8];
    wchar_t* wideNext = 0;
    convert::result r = cv.in(state, bytes, bytes + 6, bytesNext, wide, wide + 8, wideNext);
    std::printf("  codecvt in %d [%s] read %d; encoding %d max_length %d always_noconv %d length %d\n", static_cast<int>(r), shown(std::wstring(wide, wideNext)).c_str(), static_cast<int>(bytesNext - bytes),
                cv.encoding(), cv.max_length(), cv.always_noconv() ? 1 : 0, cv.length(state, bytes, bytes + 6, 4));
    static const wchar_t chars[] = { L'a', 0x20AC, 0xE4, 0x153, 0x431, L'z' };
    const wchar_t* charsNext = 0;
    char narrowed[16];
    char* narrowedNext = 0;
    state = std::mbstate_t();
    r = cv.out(state, chars, chars + 6, charsNext, narrowed, narrowed + 16, narrowedNext);
    std::printf("  codecvt out %d [%s] taken %d", static_cast<int>(r), shown(std::string(narrowed, narrowedNext)).c_str(), static_cast<int>(charsNext - chars));
    state = std::mbstate_t();
    r = cv.out(state, chars, chars + 4, charsNext, narrowed, narrowed + 2, narrowedNext);
    std::printf("; with room for two %d [%s] taken %d\n", static_cast<int>(r), shown(std::string(narrowed, narrowedNext)).c_str(), static_cast<int>(charsNext - chars));
}

// 22.2.4: the order of strings
static void collation(const std::locale& loc)
{
    const std::collate<char>& col = std::use_facet<std::collate<char> >(loc);
    static const char* const words[] = { "Zebra", "apfel", "\xC4pfel", "Apfel", "\xE4pfel", "c\xF4te", "cot\xE9", "cote", "c\xF4t\xE9", "a", "B", "Stra\xDF" "e", "Strasse", "", "apfel" };
    const int count = sizeof(words) / sizeof(words[0]);
    std::printf("  collate");
    for (int i = 0; i + 1 < count; ++i) {
        const char* a = words[i]; const char* b = words[i + 1];
        const int c = col.compare(a, a + std::strlen(a), b, b + std::strlen(b));
        const std::string ta = col.transform(a, a + std::strlen(a)), tb = col.transform(b, b + std::strlen(b));
        std::printf(" %d%s", c, sign(ta.compare(tb)) == c ? "" : "(transform differs)");
    }
    const char* x = words[1];
    std::printf(" hash equal %d locale() %d %d\n", col.hash(x, x + 5) == col.hash(words[count - 1], words[count - 1] + 5) ? 1 : 0, loc(std::string("\xE4pfel"), std::string("Zebra")) ? 1 : 0,
                loc(std::string("Zebra"), std::string("\xE4pfel")) ? 1 : 0);
    std::vector<std::string> v(words, words + count);
    std::sort(v.begin(), v.end(), loc);
    std::printf("  sorted");
    for (size_t i = 0; i < v.size(); ++i) std::printf(" [%s]", shown(v[i]).c_str());
    std::printf("\n");

    const std::collate<wchar_t>& wcol = std::use_facet<std::collate<wchar_t> >(loc);
    static const wchar_t* const wwords[] = { L"Zebra", L"apfel", L"\x00C4pfel", L"Apfel", L"\x00E4pfel", L"c\x00F4te", L"cot\x00E9", L"cote", L"a", L"B", L"\x0153uvre", L"oeuvre", L"" };
    const int wcount = sizeof(wwords) / sizeof(wwords[0]);
    std::printf("  wide collate");
    for (int i = 0; i + 1 < wcount; ++i) {
        const wchar_t* a = wwords[i]; const wchar_t* b = wwords[i + 1];
        const int c = wcol.compare(a, a + std::wcslen(a), b, b + std::wcslen(b));
        const std::wstring ta = wcol.transform(a, a + std::wcslen(a)), tb = wcol.transform(b, b + std::wcslen(b));
        std::printf(" %d%s", c, sign(ta.compare(tb)) == c ? "" : "(transform differs)");
    }
    std::printf(" locale() %d\n", loc(std::wstring(L"\x00E4pfel"), std::wstring(L"Zebra")) ? 1 : 0);
}

// 22.2.5: dates and times written and read
static std::tm someDay()
{
    std::tm t;
    std::memset(&t, 0, sizeof t);
    t.tm_year = 98; t.tm_mon = 2; t.tm_mday = 3; t.tm_wday = 2; t.tm_yday = 61; t.tm_hour = 14; t.tm_min = 5; t.tm_sec = 9;
    return t;
}
static std::string written(const std::locale& loc, const char* pattern, const std::tm& t)
{
    std::ostringstream o;
    o.imbue(loc);
    std::use_facet<std::time_put<char> >(loc).put(std::ostreambuf_iterator<char>(o), o, ' ', &t, pattern, pattern + std::strlen(pattern));
    return o.str();
}
static std::wstring wwritten(const std::locale& loc, const wchar_t* pattern, const std::tm& t)
{
    std::wostringstream o;
    o.imbue(loc);
    std::use_facet<std::time_put<wchar_t> >(loc).put(std::ostreambuf_iterator<wchar_t>(o), o, L' ', &t, pattern, pattern + std::wcslen(pattern));
    return o.str();
}
static void times(const std::locale& loc)
{
    const std::tm t = someDay();
    std::printf("  time_put [%s]", shown(written(loc, "%A|%a|%B|%b|%x|%X|%p", t)).c_str());
    std::printf(" [%s]", shown(written(loc, "%c", t)).c_str());
    std::printf(" [%s]\n", shown(written(loc, "%#x|%d.%m.%Y %H:%M|%j|%y|%%", t)).c_str());
    std::tm august = t;
    august.tm_mon = 7; august.tm_mday = 22; august.tm_wday = 6; august.tm_hour = 3;
    std::printf("  time_put [%s]\n", shown(written(loc, "%A %B %b %x %X %p", august)).c_str());
    std::printf("  wide time_put [%s]", shown(wwritten(loc, L"%A|%a|%B|%b|%x|%X", t)).c_str());
    std::printf(" [%s]\n", shown(wwritten(loc, L"%A %B %b %c", august)).c_str());

    typedef std::istreambuf_iterator<char> in;
    const std::time_get<char>& tg = std::use_facet<std::time_get<char> >(loc);
    std::printf("  date_order %s\n", order(tg.date_order()));
    static const char* const patterns[] = { "%A", "%a", "%B", "%b" };
    std::printf("  time_get names");
    for (int k = 0; k < 4; ++k) {
        for (int day = 0; day < 2; ++day) {
            const std::tm& from = day ? august : t;
            std::istringstream is(written(loc, patterns[k], from));
            is.imbue(loc);
            std::tm got;
            std::memset(&got, 0, sizeof got);
            got.tm_wday = -1; got.tm_mon = -1;
            std::ios_base::iostate err = std::ios_base::goodbit;
            if (k < 2) tg.get_weekday(in(is), in(), is, err, &got); else tg.get_monthname(in(is), in(), is, err, &got);
            std::printf(" %d/%d", k < 2 ? got.tm_wday : got.tm_mon, (err & std::ios_base::failbit) ? 1 : 0);
        }
    }
    std::printf("\n");
    // a date with the numbers in the locale's order and slashes between them (%x itself is read in main's own part:
    // cl's library does not take the dots of a German date)
    const std::time_base::dateorder o = tg.date_order();
    {
        std::istringstream is(written(loc, o == std::time_base::dmy ? "%d/%m/%Y" : o == std::time_base::mdy ? "%m/%d/%Y" : "%Y/%m/%d", august));
        is.imbue(loc);
        std::tm got;
        std::memset(&got, 0, sizeof got);
        std::ios_base::iostate err = std::ios_base::goodbit;
        tg.get_date(in(is), in(), is, err, &got);
        std::printf("  get_date %d-%d-%d fail %d", got.tm_year, got.tm_mon, got.tm_mday, (err & std::ios_base::failbit) ? 1 : 0);
    }
    {
        std::istringstream is("14:05:09 1998");
        is.imbue(loc);
        std::tm got;
        std::memset(&got, 0, sizeof got);
        std::ios_base::iostate err = std::ios_base::goodbit;
        in at = tg.get_time(in(is), in(), is, err, &got);
        if (at != in() && *at == ' ') ++at;
        tg.get_year(at, in(), is, err, &got);
        std::printf(" get_time %d:%d:%d get_year %d fail %d\n", got.tm_hour, got.tm_min, got.tm_sec, got.tm_year, (err & std::ios_base::failbit) ? 1 : 0);
    }
    typedef std::istreambuf_iterator<wchar_t> win;
    const std::time_get<wchar_t>& wtg = std::use_facet<std::time_get<wchar_t> >(loc);
    std::printf("  wide time_get %s", order(wtg.date_order()));
    {
        std::wistringstream is(wwritten(loc, L"%A", august));
        is.imbue(loc);
        std::tm got;
        std::memset(&got, 0, sizeof got);
        got.tm_wday = -1;
        std::ios_base::iostate err = std::ios_base::goodbit;
        wtg.get_weekday(win(is), win(), is, err, &got);
        std::printf(" weekday %d/%d", got.tm_wday, (err & std::ios_base::failbit) ? 1 : 0);
    }
    {
        std::wistringstream is(wwritten(loc, L"%B", august));
        is.imbue(loc);
        std::tm got;
        std::memset(&got, 0, sizeof got);
        got.tm_mon = -1;
        std::ios_base::iostate err = std::ios_base::goodbit;
        wtg.get_monthname(win(is), win(), is, err, &got);
        std::printf(" month %d/%d", got.tm_mon, (err & std::ios_base::failbit) ? 1 : 0);
    }
    {
        std::wistringstream is(wwritten(loc, o == std::time_base::dmy ? L"%d/%m/%y" : o == std::time_base::mdy ? L"%m/%d/%y" : L"%y/%m/%d", august));
        is.imbue(loc);
        std::tm got;
        std::memset(&got, 0, sizeof got);
        std::ios_base::iostate err = std::ios_base::goodbit;
        wtg.get_date(win(is), win(), is, err, &got);
        std::printf(" date %d-%d-%d fail %d\n", got.tm_year, got.tm_mon, got.tm_mday, (err & std::ios_base::failbit) ? 1 : 0);
    }
}

// 22.2.6: money
template <class Punct>
static void punct(const char* what, const Punct& mp)
{
    std::printf("  %s point [%s] sep [%s] grouping %s symbol [%s] + [%s] - [%s] frac %d pos %s neg %s\n", what, shown(mp.decimal_point()).c_str(), shown(mp.thousands_sep()).c_str(), sizes(mp.grouping()).c_str(),
                shown(mp.curr_symbol()).c_str(), shown(mp.positive_sign()).c_str(), shown(mp.negative_sign()).c_str(), mp.frac_digits(), format(mp.pos_format()).c_str(), format(mp.neg_format()).c_str());
}
static void money(const std::locale& loc)
{
    punct("moneypunct", std::use_facet<std::moneypunct<char, false> >(loc));
    punct("moneypunct intl", std::use_facet<std::moneypunct<char, true> >(loc));
    punct("wide moneypunct", std::use_facet<std::moneypunct<wchar_t, false> >(loc));
    punct("wide moneypunct intl", std::use_facet<std::moneypunct<wchar_t, true> >(loc));
    const std::money_put<char>& mput = std::use_facet<std::money_put<char> >(loc);
    const std::money_get<char>& mget = std::use_facet<std::money_get<char> >(loc);
    std::printf("  money_put");
    std::string texts[4];
    for (int k = 0; k < 4; ++k) {
        std::ostringstream o;
        o.imbue(loc);
        if (k != 2) o.setf(std::ios_base::showbase);
        const long double amount = k == 1 ? 123456789.0L : -123456789.0L;
        mput.put(std::ostreambuf_iterator<char>(o), k == 3, o, ' ', amount);
        texts[k] = o.str();
        std::printf(" [%s]", shown(texts[k]).c_str());
    }
    std::printf("\n  money_get");
    for (int k = 0; k < 4; ++k) {
        std::istringstream is(texts[k]);
        is.imbue(loc);
        if (k != 2) is.setf(std::ios_base::showbase);
        std::ios_base::iostate err = std::ios_base::goodbit;
        std::string digits;
        mget.get(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), k == 3, is, err, digits);
        std::printf(" [%s]/%d", digits.c_str(), (err & std::ios_base::failbit) ? 1 : 0);
    }
    std::wostringstream wo;
    wo.imbue(loc);
    wo.setf(std::ios_base::showbase);
    std::use_facet<std::money_put<wchar_t> >(loc).put(std::ostreambuf_iterator<wchar_t>(wo), false, wo, L' ', std::wstring(L"-123456789"));
    std::printf("\n  wide money_put [%s]", shown(wo.str()).c_str());
    std::wistringstream wis(wo.str());
    wis.imbue(loc);
    wis.setf(std::ios_base::showbase);
    std::ios_base::iostate err = std::ios_base::goodbit;
    long double units = 0;
    std::use_facet<std::money_get<wchar_t> >(loc).get(std::istreambuf_iterator<wchar_t>(wis), std::istreambuf_iterator<wchar_t>(), false, wis, err, units);
    std::printf(" money_get %.0f/%d\n", static_cast<double>(units), (err & std::ios_base::failbit) ? 1 : 0);
}

static void one(const char* name)
{
    if (!available(name)) { std::printf("%s: the C runtime has no such locale on this machine\n", name); return; }
    const std::locale loc(name);
    std::printf("%s: name %s, == itself %d, == another of the name %d, == classic %d\n", name, loc.name().c_str(), loc == loc ? 1 : 0, loc == std::locale(name) ? 1 : 0, loc == std::locale::classic() ? 1 : 0);
    numbers(loc);
    characters(loc);
    collation(loc);
    times(loc);
    money(loc);
}

// the facets by their names: each is the facet the locale of that name holds
static void byName(const char* name)
{
    if (!available(name)) return;
    const std::locale c = std::locale::classic();
    std::printf("%s by name:", name);
    const std::locale a(c, new std::numpunct_byname<char>(name));
    std::printf(" point [%s]", shown(std::use_facet<std::numpunct<char> >(a).decimal_point()).c_str());
    const std::locale b(c, new std::ctype_byname<char>(name));
    std::printf(" E4 alpha %d upper %02X", std::use_facet<std::ctype<char> >(b).is(std::ctype_base::alpha, '\xE4') ? 1 : 0, static_cast<unsigned char>(std::use_facet<std::ctype<char> >(b).toupper('\xE4')));
    const std::locale bw(c, new std::ctype_byname<wchar_t>(name));
    std::printf(" wide upper %04X", static_cast<unsigned>(std::use_facet<std::ctype<wchar_t> >(bw).toupper(static_cast<wchar_t>(0xE4))));
    const std::locale d(c, new std::collate_byname<char>(name));
    const char* x = "\xE4pfel"; const char* y = "Zebra";
    std::printf(" collate %d", std::use_facet<std::collate<char> >(d).compare(x, x + 5, y, y + 5));
    const std::locale e(c, new std::moneypunct_byname<char, false>(name));
    std::printf(" symbol [%s]", shown(std::use_facet<std::moneypunct<char, false> >(e).curr_symbol()).c_str());
    const std::locale f(c, new std::moneypunct_byname<char, true>(name));
    std::printf(" intl [%s]", shown(std::use_facet<std::moneypunct<char, true> >(f).curr_symbol()).c_str());
    const std::locale g(c, new std::time_put_byname<char>(name));
    const std::tm t = someDay();
    std::printf(" [%s]", shown(written(g, "%A %x", t)).c_str());
    const std::locale h(c, new std::time_get_byname<char>(name));
    std::printf(" %s", order(std::use_facet<std::time_get<char> >(h).date_order()));
    const std::locale i(c, new std::numpunct_byname<wchar_t>(name));
    std::printf(" wide sep [%s]", shown(std::use_facet<std::numpunct<wchar_t> >(i).thousands_sep()).c_str());
    const std::locale j(c, new std::messages_byname<char>(name));
    std::printf(" messages %d", std::has_facet<std::messages<char> >(j) ? 1 : 0);
    typedef std::codecvt<wchar_t, char, std::mbstate_t> convert;
    const std::locale k(c, new std::codecvt_byname<wchar_t, char, std::mbstate_t>(name));
    std::mbstate_t state = std::mbstate_t();
    const char byte[] = "\x80";
    const char* byteNext = 0;
    wchar_t wide[2] = { 0, 0 };
    wchar_t* wideNext = 0;
    std::use_facet<convert>(k).in(state, byte, byte + 1, byteNext, wide, wide + 2, wideNext);
    std::printf(" codecvt of 80 %04X\n", static_cast<unsigned>(wide[0]));
}

// a numpunct of the program's: groups of two
class Grouped : public std::numpunct<char> {
public:
    explicit Grouped(size_t refs = 0) : std::numpunct<char>(refs) {}
protected:
    virtual std::string do_grouping() const { return "\2"; }
};

static char pointOf(const std::locale& loc) { return std::use_facet<std::numpunct<char> >(loc).decimal_point(); }
static int alphaE4(const std::locale& loc) { return std::use_facet<std::ctype<char> >(loc).is(std::ctype_base::alpha, '\xE4') ? 1 : 0; }
static int collates(const std::locale& loc) { return loc(std::string("\xE4pfel"), std::string("Zebra")) ? 1 : 0; }
static std::string symbolOf(const std::locale& loc) { return shown(std::use_facet<std::moneypunct<char, false> >(loc).curr_symbol()); }
static std::string dayOf(const std::locale& loc) { const std::tm t = someDay(); return shown(written(loc, "%A", t)); }
static unsigned convertsOf(const std::locale& loc)
{
    std::mbstate_t state = std::mbstate_t();
    const char byte[] = "\x80";
    const char* byteNext = 0;
    wchar_t wide[2] = { 0, 0 };
    wchar_t* wideNext = 0;
    std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t> >(loc).in(state, byte, byte + 1, byteNext, wide, wide + 2, wideNext);
    return static_cast<unsigned>(wide[0]);
}
static void each(const char* what, const std::locale& loc)
{
    std::printf("  %s: point %c, E4 alpha %d, 80 converts to %04X, collates %d, symbol [%s], day %s\n", what, pointOf(loc), alphaE4(loc), convertsOf(loc), collates(loc), symbolOf(loc).c_str(), dayOf(loc).c_str());
}

// 22.1.1.2: a locale made of another's categories
static void combined(const char* germanName, const char* frenchName)
{
    if (!available(germanName) || !available(frenchName)) return;
    const std::locale c = std::locale::classic();
    const std::locale de(germanName), fr(frenchName);
    std::printf("combined:\n");
    each("classic", c);
    each("german", de);
    each("french", fr);
    each("classic, german name, numeric", std::locale(c, germanName, std::locale::numeric));
    each("classic, german, ctype", std::locale(c, de, std::locale::ctype));
    each("classic, german, collate", std::locale(c, de, std::locale::collate));
    each("classic, german, monetary", std::locale(c, de, std::locale::monetary));
    each("classic, german, time", std::locale(c, de, std::locale::time));
    each("german, classic, numeric | time", std::locale(de, c, std::locale::numeric | std::locale::time));
    each("german, french name, monetary | time", std::locale(de, frenchName, std::locale::monetary | std::locale::time));
    each("german, C, all", std::locale(de, "C", std::locale::all));
    each("german, french, none", std::locale(de, fr, std::locale::none));
    each("classic, german, all", std::locale(c, de, std::locale::all));
    // the names: 22.1.1.2 says only that the result has a name when both sources have; cl's library gives it the
    // name of the source of the categories, and so does cppparse's
    std::printf("  names: %s %s %s %s %s\n", std::locale(c, germanName, std::locale::numeric).name().c_str(), std::locale(c, de, std::locale::ctype).name().c_str(),
                std::locale(de, c, std::locale::numeric).name().c_str(), std::locale(de, fr, std::locale::none).name().c_str(), std::locale(de, frenchName, std::locale::all).name().c_str());
    const std::locale mine(c, new Grouped);
    std::printf("  with a locale without a name: %s %s %s %s\n", std::locale(mine, de, std::locale::numeric).name().c_str(), std::locale(de, mine, std::locale::numeric).name().c_str(),
                std::locale(de, mine, std::locale::none).name().c_str(), de.combine<std::numpunct<char> >(fr).name().c_str());
    std::printf("  equal: %d %d %d\n", std::locale(c, de, std::locale::numeric) == de ? 1 : 0, std::locale(de, fr, std::locale::none) == de ? 1 : 0, std::locale(de, mine, std::locale::numeric) == de ? 1 : 0);
    // the program's own facet goes with its category
    std::printf("  the program's numpunct with numeric: %s, with ctype: %s\n", sizes(std::use_facet<std::numpunct<char> >(std::locale(de, mine, std::locale::numeric)).grouping()).c_str(),
                sizes(std::use_facet<std::numpunct<char> >(std::locale(de, mine, std::locale::ctype)).grouping()).c_str());
    // a stream with the numbers of one locale and the rest of another
    std::ostringstream o;
    o.imbue(std::locale(c, de, std::locale::numeric));
    o << 1234567.5;
    std::printf("  stream with german numbers [%s]\n", o.str().c_str());
    int bad = 0;
    try { std::locale nope(c, "no-such-locale", std::locale::numeric); } catch (std::runtime_error&) { bad = 1; }
    std::printf("  a bad name among the categories throws %d\n", bad);
}

// 22.1.1.5: the global locale is the C library's too
static void global(const char* germanName, const char* frenchName)
{
    if (!available(germanName) || !available(frenchName)) return;
    const std::locale de(germanName);
    std::printf("global: printf %.2f, C locale %s\n", 1.5, std::setlocale(LC_ALL, 0));
    const std::locale old = std::locale::global(de);
    std::printf("  after global(german): previous %s, locale() %s, printf %.2f, C locale %s\n", old.name().c_str(), std::locale().name().c_str(), 1.5, std::setlocale(LC_ALL, 0));
    std::ostringstream o;
    o << 1234.5;
    std::istringstream i("9.876,5 0,25");
    double read1 = 0, read2 = 0;
    i >> read1 >> read2;
    std::printf("  a new stream writes [%s] and reads %.2f %.2f\n", o.str().c_str(), read1, read2);
    // the classic locale is the "C" locale still, whatever the C library's is
    const std::locale c = std::locale::classic();
    std::ostringstream co;
    co.imbue(c);
    co << 1234.5 << ' ' << 1234567;
    std::istringstream ci("9876.5 0.25e1");
    ci.imbue(c);
    read1 = read2 = 0;
    ci >> read1 >> read2;
    const std::ctype<char>& cct = std::use_facet<std::ctype<char> >(c);
    std::printf("  the classic locale: writes [%s], reads %.2f %.2f, E4 alpha %d upper %02X lower of C4 %02X, day %s\n", co.str().c_str(), read1, read2, cct.is(std::ctype_base::alpha, '\xE4') ? 1 : 0,
                static_cast<unsigned char>(cct.toupper('\xE4')), static_cast<unsigned char>(cct.tolower('\xC4')), dayOf(c).c_str());
    // making another named locale leaves the C locale alone
    const std::locale fr(frenchName);
    char day[64];
    const std::tm t = someDay();
    std::strftime(day, sizeof day, "%A", &t);
    std::printf("  with a french locale made: printf %.2f, strftime %s, french %s\n", 1.5, day, dayOf(fr).c_str());
    std::locale::global(old);
    std::printf("  after global(previous): locale() %s, printf %.2f, C locale %s\n", std::locale().name().c_str(), 1.5, std::setlocale(LC_ALL, 0));
}

// named locales made on eight threads at once, and one locale's facets used from all of them (its time_get asks the
// C runtime for the names at its first use, here from several threads together). cppparse's library makes a locale
// by setting the calling thread's own C locale for a moment: the program's stays "C" in every thread throughout,
// which the part of its own counts (disturbed)
extern "C" {
    void*         CreateThread(void* attributes, size_t stack, unsigned long (*start)(void*), void* param, unsigned long flags, unsigned long* id);
    unsigned long WaitForMultipleObjects(unsigned long count, void* const* handles, int all, unsigned long ms);
    unsigned long ResumeThread(void* thread);
    int           CloseHandle(void* handle);
}
struct Work {
    const char*        name;
    std::string        expected, day;
    const std::locale* shared;
    int                made, mismatches, disturbed;
};
static std::string numbersOf(const std::locale& loc)
{
    std::ostringstream o;
    o.imbue(loc);
    o << 1234567 << ' ' << 2.5;
    return o.str();
}
static unsigned long threadRun(void* p)
{
    Work* w = static_cast<Work*>(p);
    for (int r = 0; r < 100; ++r) {
        const std::locale loc(w->name);
        ++w->made;
        if (numbersOf(loc) != w->expected) ++w->mismatches;
        char text[32];
        std::sprintf(text, "%.1f", 1.5);
        if (std::strcmp(text, "1.5") != 0) ++w->disturbed;
        std::istringstream is(w->day);
        std::tm got;
        std::memset(&got, 0, sizeof got);
        got.tm_wday = -1;
        std::ios_base::iostate err = std::ios_base::goodbit;
        std::use_facet<std::time_get<char> >(*w->shared).get_weekday(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>(), is, err, &got);
        if (got.tm_wday != 2) ++w->mismatches;
        if (numbersOf(std::locale(*w->shared, loc, std::locale::numeric)) != w->expected) ++w->mismatches;
    }
    return 0;
}
static void threads(const char* const* names, bool own)
{
    for (int i = 0; i < 4; ++i) if (!available(names[i])) return;
    const std::locale shared(names[0]);
    const std::tm t = someDay();
    Work work[8];
    void* handles[8];
    for (int i = 0; i < 8; ++i) {
        work[i].name = names[i % 4];
        work[i].expected = numbersOf(std::locale(names[i % 4]));
        work[i].day = written(shared, "%A", t);
        work[i].shared = &shared;
        work[i].made = work[i].mismatches = work[i].disturbed = 0;
        handles[i] = CreateThread(0, 0, threadRun, &work[i], 4 /* CREATE_SUSPENDED */, 0);
    }
    for (int i = 0; i < 8; ++i) ResumeThread(handles[i]);
    WaitForMultipleObjects(8, handles, 1, 0xFFFFFFFFUL);
    int made = 0, mismatches = 0, disturbed = 0;
    for (int i = 0; i < 8; ++i) { made += work[i].made; mismatches += work[i].mismatches; disturbed += work[i].disturbed; CloseHandle(handles[i]); }
    if (own) std::printf("own: %d locales made on 8 threads, the C locale of a thread disturbed %d times\n", made, disturbed);
    else std::printf("threads: %d locales made on 8 threads, %d mismatches, printf %.2f, C locale %s\n", made, mismatches, 1.5, std::setlocale(LC_ALL, 0));
}

// what cppparse's library does on purpose where cl's does otherwise (docs/next-targets.md, row G28): printed for an
// argument, and checked by the test for cppparse's build alone
static int own(const char* const* names)
{
    for (int i = 0; i < 4; ++i) if (!available(names[i])) { std::printf("own: the C runtime has no locale %s on this machine\n", names[i]); return 0; }
    const char* name = names[0];
    const std::locale de(name), c = std::locale::classic();
    const std::tm t = someDay();
    // a date as %x writes it, dots and all (cl's library fails at the first dot)
    {
        typedef std::istreambuf_iterator<char> in;
        std::istringstream is(written(de, "%x", t));
        is.imbue(de);
        std::tm got;
        std::memset(&got, 0, sizeof got);
        std::ios_base::iostate err = std::ios_base::goodbit;
        std::use_facet<std::time_get<char> >(de).get_date(in(is), in(), is, err, &got);
        typedef std::istreambuf_iterator<wchar_t> win;
        std::wistringstream wis(wwritten(de, L"%x", t));
        wis.imbue(de);
        std::tm wgot;
        std::memset(&wgot, 0, sizeof wgot);
        std::ios_base::iostate werr = std::ios_base::goodbit;
        std::use_facet<std::time_get<wchar_t> >(de).get_date(win(wis), win(), wis, werr, &wgot);
        std::printf("own: get_date of %%x %d-%d-%d fail %d, wide %d-%d-%d fail %d\n", got.tm_year, got.tm_mon, got.tm_mday, (err & std::ios_base::failbit) ? 1 : 0, wgot.tm_year, wgot.tm_mon, wgot.tm_mday,
                    (werr & std::ios_base::failbit) ? 1 : 0);
    }
    // a range that holds a null character is compared beyond it (cl's library stops there: 0 and 0), and its key orders alike
    {
        const std::collate<char>& col = std::use_facet<std::collate<char> >(de);
        const char withNull[] = "ab\0\xE4", other[] = "ab\0z";
        std::printf("own: collate with a null character %d %d, by the keys %d\n", col.compare(withNull, withNull + 4, other, other + 4), col.compare(other, other + 4, withNull, withNull + 4),
                    sign(col.transform(withNull, withNull + 4).compare(col.transform(other, other + 4))));
    }
    // the classes of wide characters are the C runtime's (cl's library calls a tab and U+00A0 printable)
    {
        const std::ctype<wchar_t>& wct = std::use_facet<std::ctype<wchar_t> >(de);
        std::printf("own: wide print of a tab %d, of U+00A0 %d\n", wct.is(std::ctype_base::print, L'\t') ? 1 : 0, wct.is(std::ctype_base::print, static_cast<wchar_t>(0xA0)) ? 1 : 0);
    }
    // a name that is none throws from a facet too, and whatever the categories are (cl's library: neither)
    {
        int bad = 0;
        try { std::numpunct_byname<char>* f = new std::numpunct_byname<char>("no-such-locale"); std::locale holds(c, f); } catch (std::runtime_error&) { ++bad; }
        try { std::locale nope(c, "no-such-locale", std::locale::none); } catch (std::runtime_error&) { ++bad; }
        std::printf("own: bad names throw %d\n", bad);
    }
    // 22.1.1.2: a locale made of one without a name has none (cl's library names it for the locale named)
    {
        const std::locale mine(c, new Grouped);
        std::printf("own: (a locale without a name, %s, numeric) is named %s\n", name, std::locale(mine, name, std::locale::numeric).name().c_str());
    }
    // 22.1.1.5: global sets the C library's locale by the locale's name (cl's library sets the categories it was made with)
    {
        const std::locale old = std::locale::global(std::locale(c, name, std::locale::numeric));
        std::printf("own: global of (classic, %s, numeric) makes the C locale %s\n", name, std::setlocale(LC_ALL, 0));
        std::locale::global(old);
    }
    threads(names, true);
    return 0;
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, 0, _IONBF, 0);
    static const char* const names[] = { "de-DE", "German_Germany.1252", "en-US", "French_France.1252" };
    if (argc > 1 && std::strcmp(argv[1], "own") == 0) return own(names);
    // other names, given as arguments: each locale's facets, as for the four
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) { one(argv[i]); byName(argv[i]); }
        return 0;
    }
    for (int i = 0; i < 4; ++i) std::printf("%s%s %s", i ? ", " : "the C runtime's locales: ", names[i], available(names[i]) ? "yes" : "no");
    std::printf("\n");
    for (int i = 0; i < 4; ++i) one(names[i]);
    for (int i = 0; i < 4; ++i) byName(names[i]);
    combined(names[0], names[3]);
    global(names[0], names[3]);
    threads(names, false);
    // 22.1.1.2: a name that is none throws runtime_error, and so does a null pointer
    int bad = 0;
    try { std::locale nope("no-such-locale"); } catch (std::runtime_error&) { ++bad; }
    try { std::locale nope(static_cast<const char*>(0)); } catch (std::runtime_error&) { ++bad; }
    std::printf("bad names throw %d\n", bad);
    std::printf("C is classic %d, C by name point %c\n", std::locale("C") == std::locale::classic() ? 1 : 0, pointOf(std::locale(std::locale::classic(), new std::numpunct_byname<char>("C"))));
    // a locale is named as it was asked for; "" is the user's locale, the C runtime's ""
    if (available("German")) std::printf("German is named %s, equal to German_Germany.1252: %d\n", std::locale("German").name().c_str(), std::locale("German") == std::locale("German_Germany.1252") ? 1 : 0);
    {
        const std::locale user("");
        _locale_t h = _create_locale(LC_ALL, "");
        char day[64];
        const std::tm t = someDay();
        _strftime_l(day, sizeof day, "%A", &t, h);
        _free_locale(h);
        std::setlocale(LC_ALL, "");
        const char point = std::localeconv()->decimal_point[0];
        std::setlocale(LC_ALL, "C");
        std::printf("the user's locale is named [%s], equal to another of it %d; its point is the C runtime's %d, its day too %d\n", user.name().c_str(), user == std::locale("") ? 1 : 0,
                    point == pointOf(user) ? 1 : 0, written(user, "%A", t) == day ? 1 : 0);
    }
    return 0;
}
