// lib3.cpp - exercises cppparse's own stream headers (ostringstream, istringstream, ofstream,
// ifstream, cout); compiled by cppparse --compile6 and checked by examples/sample-lib-driver/driver-lib.cpp.
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int streamFormatting()                     // 1731: "x=42 ab3.5" (10 chars), fixed 2.50, padded, negative with a double
{
    std::ostringstream o;
    o << "x=" << 42 << ' ' << std::string("ab") << 3.5;
    std::string s = o.str();
    int r = static_cast<int>(s.size()) * 100;
    std::ostringstream f;
    f.setf(std::ios::fixed);
    f.precision(2);
    f << 2.5;
    if (f.str() == "2.50") r += 700;
    std::ostringstream w;
    w.width(6);
    w.fill('*');
    w << 42;
    if (w.str() == "****42") r += 30;
    std::ostringstream m;
    m << -7 << 1.5 << std::endl;
    if (m.str() == "-71.5\n") r += 1;
    std::cout << "  (cout from cppparse's own iostream: " << s << ")" << std::endl;
    return r;
}

int streamParsing()                        // 1234: 12 and 34 parsed, then a word and a double
{
    std::istringstream in("12 34 abc 5.5");
    int a = 0, b = 0;
    std::string word;
    double d = 0;
    in >> a >> b >> word >> d;
    if (word != "abc" || d != 5.5 || in.fail()) return -1;   // the end of the input was reached: eof set, no failure
    int rest = 0;
    in >> rest;
    if (!in.fail()) return -2;
    return a * 100 + b;
}

int fileRoundTrip()                        // 77: two lines written, read back with getline and >>
{
    {
        std::ofstream out("out/lib3-roundtrip.txt");
        if (!out) return -1;
        out << "hello" << '\n' << "world " << 77 << '\n';
    }
    std::ifstream in("out/lib3-roundtrip.txt");
    if (!in.is_open()) return -2;
    std::string line;
    if (!std::getline(in, line) || line != "hello") return -3;
    std::string word;
    int value = 0;
    in >> word >> value;
    if (word != "world") return -4;
    if (std::getline(in, line) && std::getline(in, line)) return -5;   // the rest of line two, then the end
    return value;
}

int streamManipulators()                   // 2551: hex and dec manipulators (a call through a function pointer whose
{                                          // Base& parameter takes the stream's ios part), a stream as a condition and cast
    std::ostringstream o;
    o << std::hex << 255 << std::dec << " " << 255;
    int r = o.str() == "ff 255" ? 2000 : 0;
    std::istringstream in("1 2 3 x");
    int v, sum = 0, count = 0;
    while (in >> v) { sum += v; ++count; }
    if (sum == 6 && count == 3) r += 500;
    std::istringstream in2("7");
    bool ok = (bool)(in2 >> v);                       // a cast through operator void*
    const int got = v;
    bool ok2 = static_cast<bool>(in2 >> v);           // the stream has failed now (and stored 0, as cl)
    if (ok && !ok2 && got == 7) r += 50;
    std::istringstream in3("9");
    if (in3 >> v) r += 1;
    return r;
}
