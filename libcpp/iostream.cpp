// iostream.cpp - cppparse's C++98 library: the standard stream objects, the global allocation functions and the streams'
// calls of the num facets for char and wchar_t.
// Compiled by cppparse (--compile6 -I ../include) and linked with every program that uses <iostream>.
#include <iostream>
#include <new>
#include <cstdlib>

// the global allocation functions (18.4.1): a new-expression calls them by their MSVC names (??2@YAPEAX_K@Z and
// so on), so a program that links this unit gets these instead of the C runtime's, whose failure is reported
// through MSVC's exception handling, which no cppparse frame can catch; these throw std::bad_alloc through
// cppparse's runtime, the nothrow forms give null. When the storage cannot be had, operator new calls the handler
// that std::set_new_handler installed and tries again, for as long as there is one (18.4.1.1/3: the handler frees
// storage, installs another handler or none, throws bad_alloc or ends the program); the array form returns what
// operator new returns, and a nothrow form what its throwing form returns, or null when that one throws (18.4.1.2,
// 18.4.1.1/7).
// A program may define any of the eight itself (18.4.1.1/2), so each definition here is a default:
// __declspec(replaceable) makes cppparse define the code under a name of its own and the function's name a weak
// external that stands for it, which the linker gives to the program's definition when there is one (and to this
// one, not to the C runtime's, when there is none). The calls below go through the names, so they reach the
// program's functions: the array forms and the nothrow forms call the plain ones, as the C runtime's do, and a
// program that replaces only operator new and operator delete has replaced them all. cl has no such attribute:
// its build of this unit (sandbox/scripts/mix.ps1) has plain definitions, which no program can replace.
#ifdef __cppparse__
#define __REPLACEABLE __declspec(replaceable)
#else
#define __REPLACEABLE
#endif
static std::new_handler __new_handler = 0;

namespace std {

new_handler set_new_handler(new_handler h) throw()
{
    new_handler old = __new_handler;
    __new_handler = h;
    return old;
}

}

__REPLACEABLE void* operator new(size_t n) throw(std::bad_alloc)
{
    for (;;) {
        void* p = malloc(n ? n : 1);
        if (p) return p;
        std::new_handler h = __new_handler;
        if (!h) throw std::bad_alloc();
        h();
    }
}
__REPLACEABLE void* operator new[](size_t n) throw(std::bad_alloc) { return ::operator new(n); }
__REPLACEABLE void operator delete(void* p) throw() { free(p); }
__REPLACEABLE void operator delete[](void* p) throw() { ::operator delete(p); }
__REPLACEABLE void* operator new(size_t n, const std::nothrow_t&) throw()
{
    try { return ::operator new(n); }
    catch (...) { return 0; }
}
__REPLACEABLE void* operator new[](size_t n, const std::nothrow_t&) throw()
{
    try { return ::operator new[](n); }
    catch (...) { return 0; }
}
__REPLACEABLE void operator delete(void* p, const std::nothrow_t&) throw() { ::operator delete(p); }
__REPLACEABLE void operator delete[](void* p, const std::nothrow_t&) throw() { ::operator delete[](p); }

namespace std {

// the buffers over the C runtime's FILEs, then the streams over them (27.3): constructed in this order before main,
// destroyed after it in the reverse order, so the streams go before their buffers
static filebuf  __stdin_buf(stdin);
static filebuf  __stdout_buf(stdout);
static filebuf  __stderr_buf(stderr);
static wfilebuf __wstdin_buf(stdin);
static wfilebuf __wstdout_buf(stdout);
static wfilebuf __wstderr_buf(stderr);

istream  cin(&__stdin_buf);
ostream  cout(&__stdout_buf);
ostream  cerr(&__stderr_buf);
ostream  clog(&__stderr_buf);
wistream wcin(&__wstdin_buf);
wostream wcout(&__wstdout_buf);
wostream wcerr(&__wstderr_buf);
wostream wclog(&__wstderr_buf);

// cin and cerr are tied to cout, wcin and wcerr to wcout; cerr and wcerr flush after each output (27.3.1, 27.3.2)
struct __init_streams {
    __init_streams()
    {
        cin.tie(&cout);
        cerr.tie(&cout);
        cerr.setf(ios_base::unitbuf);
        wcin.tie(&wcout);
        wcerr.tie(&wcout);
        wcerr.setf(ios_base::unitbuf);
    }
};
static __init_streams __init_streams_now;

// the streams' calls of the num facets for char and wchar_t (declared in <ostream> and <istream>): the facets' code is
// compiled here once, not in every unit that writes or reads a number
bool __num_put_out(basic_ostream<char, char_traits<char> >& os, const locale::facet* f, char fill, bool v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<char, char_traits<char> >& os, const locale::facet* f, char fill, long v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<char, char_traits<char> >& os, const locale::facet* f, char fill, unsigned long v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<char, char_traits<char> >& os, const locale::facet* f, char fill, __cppparse_int64 v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<char, char_traits<char> >& os, const locale::facet* f, char fill, __cppparse_uint64 v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<char, char_traits<char> >& os, const locale::facet* f, char fill, double v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<char, char_traits<char> >& os, const locale::facet* f, char fill, long double v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<char, char_traits<char> >& os, const locale::facet* f, char fill, const void* v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<wchar_t, char_traits<wchar_t> >& os, const locale::facet* f, wchar_t fill, bool v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<wchar_t, char_traits<wchar_t> >& os, const locale::facet* f, wchar_t fill, long v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<wchar_t, char_traits<wchar_t> >& os, const locale::facet* f, wchar_t fill, unsigned long v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<wchar_t, char_traits<wchar_t> >& os, const locale::facet* f, wchar_t fill, __cppparse_int64 v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<wchar_t, char_traits<wchar_t> >& os, const locale::facet* f, wchar_t fill, __cppparse_uint64 v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<wchar_t, char_traits<wchar_t> >& os, const locale::facet* f, wchar_t fill, double v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<wchar_t, char_traits<wchar_t> >& os, const locale::facet* f, wchar_t fill, long double v) { return __num_put_call(os, f, fill, v); }
bool __num_put_out(basic_ostream<wchar_t, char_traits<wchar_t> >& os, const locale::facet* f, wchar_t fill, const void* v) { return __num_put_call(os, f, fill, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, bool& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, long& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, unsigned short& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, unsigned int& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, unsigned long& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, __cppparse_int64& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, __cppparse_uint64& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, float& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, double& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, long double& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<char, char_traits<char> >& is, ios_base::iostate& err, void*& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, bool& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, long& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, unsigned short& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, unsigned int& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, unsigned long& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, __cppparse_int64& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, __cppparse_uint64& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, float& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, double& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, long double& v) { __num_get_call(is, err, v); }
void __num_get_in(basic_istream<wchar_t, char_traits<wchar_t> >& is, ios_base::iostate& err, void*& v) { __num_get_call(is, err, v); }

}
