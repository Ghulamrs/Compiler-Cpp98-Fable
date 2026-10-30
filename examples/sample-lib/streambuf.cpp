// streambuf.cpp - <streambuf> (C3): stream buffers a program derives from basic_streambuf, used by ostream and
// istream; built by cl with its own headers (the reference) and by cppparse --compile6 with include/, test 15
// compares what the programs print at -O0 and -O2.
#include <cctype>
#include <cstdio>
#include <cstring>
#include <istream>
#include <ostream>
#include <streambuf>
#include <string>

// (the bases are named through the typedefs std::streambuf and std::wstreambuf, one as the template-id: row B15)
// an unbuffered sink: every character comes through overflow, upper-cased and counted
class UpperBuf : public std::streambuf {
public:
    UpperBuf() : count(0) {}
    std::string text;
    int count;
protected:
    virtual int_type overflow(int_type c)
    {
        if (traits_type::eq_int_type(c, traits_type::eof())) return traits_type::not_eof(c);
        text += static_cast<char>(std::toupper(c));
        ++count;
        return c;
    }
};

// a put area over a fixed array: overflow empties it into a string, sync too
class ChunkBuf : public std::streambuf {
public:
    ChunkBuf() : flushes(0) { setp(m_buf, m_buf + sizeof m_buf); }
    std::string out;
    int flushes;
protected:
    virtual int_type overflow(int_type c)
    {
        drain();
        if (!traits_type::eq_int_type(c, traits_type::eof())) { *pptr() = traits_type::to_char_type(c); pbump(1); }
        return traits_type::not_eof(c);
    }
    virtual int sync() { drain(); return 0; }
private:
    char m_buf[4];
    void drain() { out.append(pbase(), pptr() - pbase()); setp(m_buf, m_buf + sizeof m_buf); ++flushes; }
};

// a get area over a string, refilled one line at a time through underflow
class LineSource : public std::basic_streambuf<char> {
public:
    explicit LineSource(const char* text) : m_text(text), m_refills(0) {}
    int refills() const { return m_refills; }
protected:
    virtual int_type underflow()
    {
        if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
        if (*m_text == 0) return traits_type::eof();
        const char* nl = std::strchr(m_text, '\n');
        size_t n = nl ? static_cast<size_t>(nl - m_text) + 1 : std::strlen(m_text);
        if (n > sizeof m_line) n = sizeof m_line;
        std::memcpy(m_line, m_text, n);
        m_text += n;
        ++m_refills;
        setg(m_line, m_line, m_line + n);
        return traits_type::to_int_type(*gptr());
    }
private:
    const char* m_text;
    char        m_line[32];
    int         m_refills;
};

// a wide sink through overflow
class WideCount : public std::wstreambuf {
public:
    WideCount() : count(0), last(0) {}
    int count;
    wchar_t last;
protected:
    virtual int_type overflow(int_type c)
    {
        if (traits_type::eq_int_type(c, traits_type::eof())) return traits_type::not_eof(c);
        ++count;
        last = traits_type::to_char_type(c);
        return c;
    }
};

int main()
{
    UpperBuf up;
    std::ostream o1(&up);
    o1 << "mixed Case " << 42 << ' ' << 1.5 << std::endl;
    std::printf("upper [%s] %d chars, good %d\n", up.text.c_str(), up.count, o1.good() ? 1 : 0);

    ChunkBuf chunks;
    std::ostream o2(&chunks);
    o2 << "abcdefghij";
    const int before = chunks.flushes;
    o2.flush();
    std::printf("chunks [%s] flushes before sync %d after %d\n", chunks.out.c_str(), before, chunks.flushes);

    LineSource src("first line\nsecond 22\nthird\n");
    std::istream in(&src);
    std::string line, word;
    int n = 0;
    std::getline(in, line);
    in >> word >> n;
    std::printf("line [%s] word [%s] n %d refills %d\n", line.c_str(), word.c_str(), n, src.refills());
    in.ignore(1);
    std::getline(in, line);
    std::printf("third [%s] eof next %d\n", line.c_str(), in.peek() == std::char_traits<char>::eof() ? 1 : 0);

    LineSource src2("copy me\nall\n");
    UpperBuf sink;
    std::ostream o3(&sink);
    o3 << &src2;                                   // the rest of a stream buffer
    std::printf("copied [%s]\n", sink.text.c_str());

    std::ostream o4(0);                           // no buffer: badbit
    o4 << "lost";
    std::printf("null buffer bad %d\n", o4.bad() ? 1 : 0);
    o4.rdbuf(&up);
    o4 << "!";
    std::printf("rdbuf set good %d [%s]\n", o4.good() ? 1 : 0, up.text.c_str());

    WideCount wide;
    std::wostream wo(&wide);
    wo << L"wide" << 7 << L'!';
    std::printf("wide %d chars, last %c\n", wide.count, static_cast<char>(wide.last));
    std::streambuf* sb = &up;
    const char put = static_cast<char>(sb->sputc('z'));
    const int putn = static_cast<int>(sb->sputn("xyz", 3));
    std::printf("sputc %c sputn %d\n", put, putn);
    std::printf("after [%s]\n", up.text.c_str());
    return 0;
}
