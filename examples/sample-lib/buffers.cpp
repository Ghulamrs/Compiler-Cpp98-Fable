// buffers.cpp - the edges of the library's fast paths: strings at the size where the characters leave the object
// for the heap (15 chars, 7 wchar_t) and back, a string appended, inserted or replaced from itself, strings moved
// bytewise by vector; a string stream's put area as it grows, after seekp and with in|out; a file read through a
// buffer of its own (binary input), longer than that buffer, by getline, >>, read, tellg and seekg; a file written
// through a buffer of its own and read back; a whole file into a string stream with << rdbuf(). Built by cl with its
// own headers (the reference) and by cppparse --compile6 with include/, test 15 compares what the programs print at
// -O0 and -O2.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static void show(const char* what, const std::string& s) { std::printf("%-12s %2d [%s]\n", what, static_cast<int>(s.size()), s.c_str()); }

int main()
{
    // the local buffer's edge: 15 characters stay in the object, 16 do not
    std::string a(15, 'a'), b(16, 'b'), c;
    show("15", a); show("16", b);
    a += 'x';                     // 15 -> 16: to the heap
    b.resize(3);                  // stays on the heap, short
    show("a+x", a); show("b.resize", b);
    c = a; c.erase(2);            // assigned long, erased short
    show("c", c);
    std::string s("0123456789");
    s += s;                       // appended from itself across the edge
    show("s+=s", s);
    s.insert(5, s);               // inserted from itself
    show("insert", s);
    s.replace(2, 30, s.c_str() + 10, 6);   // replaced from itself, shorter
    show("replace", s);
    s.assign(s.c_str() + 3, 4);   // assigned from itself
    show("assign", s);
    std::string l(40, 'L'), m("mm");
    l.swap(m);                    // a heap string and a local one swap
    show("swap l", l); show("swap m", m);
    m.swap(m);
    show("self swap", m);
    std::string t = std::string("abc") + "defghijklmno" + 'p' + std::string("q");
    show("+ chain", t);
    std::printf("find %d %d %d %d rfind %d\n", static_cast<int>(t.find("ghi")), static_cast<int>(t.find('q')), static_cast<int>(t.find("x")),
                static_cast<int>(t.find("pq", 10)), static_cast<int>(t.rfind('c')));
    std::printf("compare %d %d %d == %d\n", t.compare("abc") > 0 ? 1 : 0, std::string("ab").compare("abc") < 0 ? 1 : 0, t.compare(0, 3, "abc"),
                t == "abcdefghijklmnopq" ? 1 : 0);
    // vector moves its strings bytewise as it grows: local and heap ones alike
    std::vector<std::string> v;
    for (int i = 0; i < 40; ++i) v.push_back(std::string(static_cast<size_t>(i % 20), static_cast<char>('a' + i % 26)));
    v.erase(v.begin() + 3);
    v.insert(v.begin() + 1, std::string(17, 'Z'));
    size_t total = 0;
    for (size_t i = 0; i < v.size(); ++i) total += v[i].size() * (i + 1);
    std::printf("vector %d strings, weighted size %d, [%s] [%s]\n", static_cast<int>(v.size()), static_cast<int>(total), v[1].c_str(), v[38].c_str());
    std::wstring w(7, L'w');
    w += L"xy";                   // 7 -> 9 wide characters: to the heap
    std::printf("wide %d %d\n", static_cast<int>(w.size()), w[8] == L'y' ? 1 : 0);

    // a string stream's put area: grown past the local buffer, overwritten after seekp, read after writing
    std::ostringstream o;
    for (int i = 0; i < 30; ++i) o << i << ',';
    show("ostream", o.str());
    o.seekp(3);
    o << "XY";
    std::printf("tellp %d\n", static_cast<int>(o.tellp()));
    show("seekp", o.str());
    std::ostringstream big;
    big << std::string(5000, 'q') << "end";
    std::printf("big %d %s\n", static_cast<int>(big.str().size()), big.str().substr(4998).c_str());
    std::stringstream io;
    io << "alpha beta";
    std::string w1, w2, w3;
    io >> w1;
    io << " gamma";
    io >> w2 >> w3;
    std::printf("in|out [%s] [%s] [%s] eof %d\n", w1.c_str(), w2.c_str(), w3.c_str(), io.eof() ? 1 : 0);
    std::ostringstream app("start", std::ios_base::app);
    app << "+more";
    show("app", app.str());
    std::ostringstream ate("start", std::ios_base::ate);
    ate << "+more";
    show("ate", ate.str());

    // a file longer than the buffers: written through ofstream, read back in several ways
    {
        std::ofstream out("out/buffers.txt", std::ios::out | std::ios::binary);
        for (int i = 0; i < 1200; ++i) out << "line " << i << " of the file\n";
        out.write("tail", 4);
    }
    std::ifstream in("out/buffers.txt", std::ios::in | std::ios::binary);
    std::string line;
    int lines = 0;
    size_t chars = 0;
    while (std::getline(in, line)) { ++lines; chars += line.size(); }
    std::printf("getline %d lines %d chars, last [%s]\n", lines, static_cast<int>(chars), line.c_str());
    in.clear();
    in.seekg(0, std::ios::end);
    const long size = static_cast<long>(in.tellg());
    in.seekg(5000);
    std::string word;
    in >> word;
    std::printf("size %ld, at 5000 [%s] tellg %d\n", size, word.c_str(), static_cast<int>(in.tellg()));
    in.seekg(-10, std::ios::cur);
    char buf[16] = {0};
    in.read(buf, 12);
    std::printf("read [%s] gcount %d\n", buf, static_cast<int>(in.gcount()));
    in.seekg(4090);
    in.read(buf, 12);             // across the end of the first buffer
    std::printf("read2 [%.12s]\n", buf);
    int words = 0;
    in.seekg(0);
    while (in >> word) ++words;
    std::printf("words %d last [%s]\n", words, word.c_str());
    in.close();
    std::ifstream whole("out/buffers.txt", std::ios::in | std::ios::binary);
    std::ostringstream all;
    all << whole.rdbuf();
    std::printf("rdbuf %d chars, ends [%s]\n", static_cast<int>(all.str().size()), all.str().substr(all.str().size() - 10).c_str());
    std::ifstream text("out/buffers.txt");   // text mode: read unbuffered
    std::getline(text, line);
    std::printf("text [%s] tellg %d\n", line.c_str(), static_cast<int>(text.tellg()));
    return 0;
}
