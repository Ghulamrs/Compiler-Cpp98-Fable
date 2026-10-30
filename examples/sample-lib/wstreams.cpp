// wstreams.cpp - the streams as templates over a basic_streambuf (C3): wcout, wostream, wistream, the wide string
// and file streams, stringstream, the stream buffers and their iterators; built by cl with its own headers (the
// reference) and by cppparse --compile6 with include/, test 15 compares what the programs print at -O0 and -O2.
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

static void narrowOf(const char* what, const std::wstring& s)
{
    std::printf("%-12s [", what);
    for (size_t i = 0; i < s.size(); ++i) { unsigned u = static_cast<unsigned>(s[i]); if (u >= 32 && u < 127) std::printf("%c", static_cast<char>(u)); else std::printf("\\x%x", u); }
    std::printf("] %d\n", static_cast<int>(s.size()));
}

static void throughWcout()
{
    std::wstring w(L"wide string");
    std::wcout << L"wcout: " << w << L' ' << 42 << L' ' << 2.5 << L' ' << "narrow" << ' ' << true << std::endl;
    std::wcout << std::setw(8) << L"right" << L'|' << std::left << std::setw(8) << L"left" << L'|' << std::right << std::endl;
    std::wcout << std::hex << std::showbase << 255 << L' ' << std::oct << 8 << std::dec << std::noshowbase << L' ' << -17 << std::endl;
    std::wcout.flush();
    std::cout << "cout after wcout: " << std::string("narrow") << std::endl;
    std::wostream& wo = std::wcout;
    wo << L"through wostream&" << std::endl;
    // a character above 255 has no conversion in the "C" locale: badbit, and nothing more is written until clear()
    std::wcout << L"above 255: [" << L"\x263a" << L"]" << std::endl;
    const int bad = std::wcout.bad() ? 1 : 0;
    std::wcout.clear();
    std::wcout << std::endl << L"bad after it: " << bad << std::endl;
}

static void wideStrings()
{
    std::wostringstream o;
    o << L"n=" << 12345 << L" d=" << 3.25 << L" b=" << std::boolalpha << false << std::noboolalpha << L" c=" << L'x' << L" s=" << "abc";
    narrowOf("wostring", o.str());
    std::wostringstream f;
    f << std::fixed << std::setprecision(3) << 3.14159 << L' ' << std::scientific << 12345.678 << L' ' << std::setfill(L'*') << std::setw(7) << 42;
    narrowOf("formatted", f.str());
    std::wistringstream in(L"17 2.5 word\nsecond line\nthird");
    int i = 0; double d = 0; std::wstring word, line;
    in >> i >> d >> word;
    std::getline(in, line);
    std::wstring l2, l3;
    std::getline(in, l2);
    std::getline(in, l3, L'r');
    std::printf("wistring %d %.2f eof %d\n", i, d, in.eof() ? 1 : 0);
    narrowOf("word", word); narrowOf("rest", line); narrowOf("line 2", l2); narrowOf("to r", l3);
    std::wstringstream io;
    io << 100 << L' ' << L"two";
    int n = 0; std::wstring two;
    io >> n >> two;
    std::printf("wstringstream %d %d\n", n, two == L"two" ? 1 : 0);
    std::wostringstream w2;
    std::ostream_iterator<int, wchar_t> out(w2, L",");
    for (int k = 1; k <= 3; ++k) *out++ = k * k;
    narrowOf("ostream_iter", w2.str());
}

static void charStreams()
{
    std::stringstream ss;
    ss << "alpha " << 7 << ' ' << 1.5;
    std::string a; int b = 0; double c = 0;
    ss >> a >> b >> c;
    std::printf("stringstream %s %d %.1f\n", a.c_str(), b, c);
    std::istringstream in("abcdef");
    char x = static_cast<char>(in.get());
    char y = static_cast<char>(in.peek());
    in.putback(x);
    char buf[8] = { 0 };
    in.read(buf, 3);
    std::streamsize got = in.gcount();
    in.unget();
    in.ignore(1);
    std::printf("get %c peek %c read %s gcount %d next %c\n", x, y, buf, static_cast<int>(got), static_cast<char>(in.get()));
    std::istringstream pos("0123456789");
    pos.seekg(4);
    char p4 = static_cast<char>(pos.get());
    pos.seekg(-2, std::ios_base::end);
    char p8 = static_cast<char>(pos.get());
    std::printf("seekg %c %c tellg %d\n", p4, p8, static_cast<int>(pos.tellg()));
    std::ostringstream o;
    o << "hello world";
    o.seekp(6);
    o << "WORLD";
    std::printf("seekp [%s] tellp %d\n", o.str().c_str(), static_cast<int>(o.tellp()));
    std::istringstream src("copied through rdbuf");
    std::ostringstream dst;
    dst << src.rdbuf();
    std::printf("rdbuf [%s]\n", dst.str().c_str());
    std::istringstream ws("   spaced");
    ws >> std::ws;
    std::printf("ws peek %c\n", static_cast<char>(ws.peek()));
    std::istringstream bad("x");
    int v = 5;
    bad >> v;
    std::printf("state fail %d eof %d bad %d rdstate %d\n", bad.fail() ? 1 : 0, bad.eof() ? 1 : 0, bad.bad() ? 1 : 0, static_cast<int>(bad.rdstate()));
    std::istringstream end("12");
    end >> v;
    std::printf("eof state %d rdstate %d value %d\n", end.eof() ? 1 : 0, static_cast<int>(end.rdstate()), v);
    int caught = 0;
    std::istringstream thrower("q");
    thrower.exceptions(std::ios_base::failbit);
    try { thrower >> v; } catch (std::ios_base::failure&) { caught = 1; }
    std::printf("failure thrown %d\n", caught);
    std::ostringstream ends;
    ends << "a" << std::ends << "b" << std::flush;
    std::printf("ends size %d\n", static_cast<int>(ends.str().size()));
    std::printf("flags %d after hex %d\n", static_cast<int>(std::ios_base::dec | std::ios_base::skipws), static_cast<int>((std::ostringstream() << std::hex).flags()));
}

static void buffers()
{
    std::stringbuf sb("buffer");
    const int avail = static_cast<int>(sb.in_avail());   // one call per statement: the order of a call's arguments is the compiler's
    const char c1 = static_cast<char>(sb.sgetc());
    const char c2 = static_cast<char>(sb.sbumpc());
    const char c3 = static_cast<char>(sb.snextc());
    std::printf("stringbuf in_avail %d sgetc %c sbumpc %c snextc %c\n", avail, c1, c2, c3);
    sb.sputbackc('u');
    char g[4] = { 0 };
    sb.sgetn(g, 3);
    std::printf("sputbackc and sgetn [%s]\n", g);
    std::ostream os(&sb);
    os << "!" << 9;
    std::printf("ostream over a stringbuf [%s]\n", sb.str().c_str());
    std::istringstream in("istreambuf");
    std::istreambuf_iterator<char> it(in), eos;
    std::string s;
    for (; it != eos; ++it) s += *it;
    std::ostringstream o;
    std::ostreambuf_iterator<char> oit(o);
    for (size_t i = 0; i < s.size(); ++i) *oit++ = static_cast<char>(s[i] - 32);
    std::printf("streambuf iterators [%s] [%s] failed %d\n", s.c_str(), o.str().c_str(), oit.failed() ? 1 : 0);
}

static void files()
{
    {
        std::wofstream out("out/wstreams-wide.txt");
        out << L"wide file " << 21 << L'\n' << L"second" << L'\n';
    }
    std::wifstream in("out/wstreams-wide.txt");
    std::wstring w1, w2, line;
    int n = 0;
    in >> w1 >> w2 >> n;
    std::getline(in, line);
    std::getline(in, line);
    std::printf("wifstream open %d n %d\n", in.is_open() ? 1 : 0, n);
    narrowOf("words", w1 + L"+" + w2); narrowOf("line", line);
    std::fstream both("out/wstreams-both.txt", std::ios_base::in | std::ios_base::out | std::ios_base::trunc);
    both << "fstream " << 33;
    both.seekg(0);
    std::string word; int m = 0;
    both >> word >> m;
    std::printf("fstream %s %d\n", word.c_str(), m);
    std::ifstream missing("out/no-such-file.txt");
    std::printf("missing open %d fail %d\n", missing.is_open() ? 1 : 0, missing.fail() ? 1 : 0);
}

int main()
{
    std::setvbuf(stdout, 0, _IONBF, 0);
    throughWcout();
    wideStrings();
    charStreams();
    buffers();
    files();
    std::wcout << L"wcout last " << 1 << std::endl;
    return 0;
}
