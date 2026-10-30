// libnames.cpp - functions and variables whose types are library class templates (C3): test 15 builds the unit with
// cl and its own headers and with cppparse and include/, and requires the decorated names of everything it defines
// to be the same, so that the names cppparse gives std::string and the others are the ones cl gives them.
#include <complex>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>
#include <streambuf>
#include <string>
#include <strstream>
#include <valarray>

namespace names {

// the class templates and classes of <complex>, <valarray>, <locale> and <strstream>
void numbers(std::complex<double>&, const std::complex<float>*, std::valarray<int>&, const std::valarray<double>*, std::slice, const std::gslice&) {}
void locales(const std::locale&, const std::ctype<char>*, const std::ctype<wchar_t>*, const std::numpunct<char>&, const std::collate<wchar_t>*, const std::ctype_base*) {}
void arrayStreams(std::strstreambuf*, std::istrstream&, std::ostrstream&, std::strstream*) {}

// the stream types (the stream objects themselves are libcpp's, referenced here with cl's names)
void streams(std::ostream&, std::wostream&, std::istream&, std::wistream&, std::iostream*) {}
void stringStreams(std::ostringstream&, std::wistringstream*, std::stringstream&, std::wstringbuf*) {}
void fileStreams(std::ofstream&, std::wifstream*, std::fstream&, std::filebuf*) {}
std::streambuf* buffers(std::wstreambuf*, std::ios&, std::wios*, std::ios_base&) { return 0; }
std::ios_base::fmtflags flagsOf(std::ios_base::iostate, std::ios_base::openmode, std::streamsize) { return 0; }
const void* standardStreams()
{
    const void* p[8] = { &std::cout, &std::cin, &std::cerr, &std::clog, &std::wcout, &std::wcin, &std::wcerr, &std::wclog };
    return p[0];
}

void byRef(const std::string&) {}
std::string byValue(std::string s) { return s; }
void wide(std::wstring&, const std::wstring*) {}
void spelled(std::basic_string<char, std::char_traits<char>, std::allocator<char> >&) {}
void both(const std::string&, const std::wstring&, std::string*, std::wstring) {}
std::char_traits<char>* traitsOf(std::char_traits<wchar_t>*) { return 0; }
std::allocator<char>* allocatorOf(std::allocator<wchar_t>*) { return 0; }
std::string::size_type sizeOf(const std::string& s, std::wstring::size_type) { return s.size(); }

struct Holder {
    std::string  name;
    std::wstring wname;
    void set(const std::string& s, const std::wstring& w);
    std::string get() const;
    static std::string label;
};
void Holder::set(const std::string& s, const std::wstring& w) { name = s; wname = w; }
std::string Holder::get() const { return name; }
std::string Holder::label;

std::string  g_text;
std::wstring g_wide;
std::string* g_textPtr = 0;

}
