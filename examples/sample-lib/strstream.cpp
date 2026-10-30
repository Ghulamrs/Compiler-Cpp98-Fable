// strstream.cpp - <strstream> (C3, annex D.7); built by cl with its own headers (the reference) and by cppparse
// --compile6 with include/, test 15 compares what the programs print at -O0 and -O2.
#include <cstdio>
#include <cstring>
#include <strstream>

int main()
{
    // a dynamic ostrstream: str() freezes the array and hands it out; freeze(false) gives it back to the stream
    std::ostrstream o;
    o << "value " << 42 << ' ' << 2.5 << std::ends;
    char* s = o.str();
    std::printf("ostrstream [%s] pcount %d\n", s, static_cast<int>(o.pcount()));
    o.freeze(false);

    // an ostrstream over the program's array, which does not grow
    char fixed[16];
    std::memset(fixed, 0, sizeof fixed);
    std::ostrstream f(fixed, 8);
    f << "abcdefghij";
    std::printf("fixed [%.8s] pcount %d bad %d\n", fixed, static_cast<int>(f.pcount()), f.bad() ? 1 : 0);

    // appending to the program's string
    char app[32] = "start:";
    std::ostrstream a(app, sizeof app, std::ios_base::app);
    a << "more" << std::ends;
    std::printf("app [%s]\n", app);

    // istrstream: up to the terminator, or n characters
    std::istrstream in("12 34.5 word");
    int i = 0; double d = 0; char w[8] = { 0 };
    in >> i >> d >> w;
    std::printf("istrstream %d %.1f %s eof %d\n", i, d, w, in.eof() ? 1 : 0);
    const char* digits = "987654";
    std::istrstream part(digits, std::streamsize(3));
    int p = 0;
    part >> p;
    std::printf("istrstream n %d\n", p);

    // strstream: written, then read back
    std::strstream io;
    io << 5 << ' ' << "five";
    int n = 0; char word[8] = { 0 };
    io >> n >> word;
    std::printf("strstream %d %s\n", n, word);
    io.freeze(false);

    // the buffer itself
    std::strstreambuf sb;
    sb.sputn("xyz", 3);
    sb.sputc('!');
    std::printf("strstreambuf pcount %d sgetc %c\n", static_cast<int>(sb.pcount()), static_cast<char>(sb.sgetc()));
    char* t = sb.str();
    std::printf("strstreambuf str %.4s\n", t);
    sb.freeze(false);
    return 0;
}
