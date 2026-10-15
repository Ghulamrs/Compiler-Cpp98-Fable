// lib4.cpp - exercises the headers added for the review of 2026-10-15 (B6): <limits> <climits> <cfloat>,
// <iterator>, <functional>, <numeric>, <memory>, <deque>, <list>, <stack>, <queue>, <bitset>, <iomanip>, the generic
// <algorithm>, <exception>'s handlers and <new>'s nothrow forms; compiled by cppparse --compile6 and checked by
// examples/sample-lib-driver/driver-lib.cpp.
#include <algorithm>
#include <bitset>
#include <cerrno>
#include <cfloat>
#include <climits>
#include <cstdio>
#include <deque>
#include <exception>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <new>
#include <numeric>
#include <queue>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

int limitsBasics()                         // 1111: numeric_limits against the <climits>/<cfloat> macros, infinity and NaN
{
    int r = 0;
    if (std::numeric_limits<int>::max() == INT_MAX && std::numeric_limits<int>::min() == INT_MIN && std::numeric_limits<int>::digits == 31) r += 1000;
    if (std::numeric_limits<unsigned char>::max() == UCHAR_MAX && std::numeric_limits<long long>::max() == LLONG_MAX && std::numeric_limits<unsigned long long>::digits == 64) r += 100;
    if (std::numeric_limits<double>::max() == DBL_MAX && std::numeric_limits<float>::epsilon() == FLT_EPSILON && std::numeric_limits<double>::digits10 == 15 && std::numeric_limits<double>::is_iec559) r += 10;
    double inf = std::numeric_limits<double>::infinity();
    double nan = std::numeric_limits<double>::quiet_NaN();
    if (inf > DBL_MAX && -inf < -DBL_MAX && nan != nan && !std::numeric_limits<int>::has_infinity && std::numeric_limits<bool>::max() == true && CHAR_BIT == 8) r += 1;
    return r;
}

static int twice(int v) { return 2 * v; }
static bool odd(int v) { return v % 2 != 0; }
struct Acc { int sum; Acc() : sum(0) {} void operator()(int v) { sum += v; } };
struct Cnt { int v; Cnt(int x) : v(x) {} int get() const { return v; } int add(int d) { return v += d; } };

int iteratorsAndFunctors()                 // 3421: reverse iterators, back_inserter, distance/advance, bind2nd, not1, mem_fun, ptr_fun
{
    std::vector<int> v;
    for (int i = 1; i <= 5; ++i) v.push_back(i);                // 1 2 3 4 5
    int r = 0;
    std::string rev;
    for (std::vector<int>::reverse_iterator i = v.rbegin(); i != v.rend(); ++i) rev += static_cast<char>('0' + *i);
    if (rev == "54321") r += 3000;
    std::vector<int> w;
    std::copy(v.begin(), v.end(), std::back_inserter(w));
    std::list<int> l(v.begin(), v.end());
    std::list<int>::iterator li = l.begin();
    std::advance(li, 3);
    if (w == v && std::distance(l.begin(), l.end()) == 5 && *li == 4) r += 400;
    if (std::count_if(v.begin(), v.end(), std::bind2nd(std::less<int>(), 3)) == 2 && std::count_if(v.begin(), v.end(), std::not1(std::ptr_fun(odd))) == 2) r += 20;
    std::vector<Cnt> cs; cs.push_back(Cnt(1)); cs.push_back(Cnt(2));
    std::vector<int> got;
    std::transform(cs.begin(), cs.end(), std::back_inserter(got), std::mem_fun_ref(&Cnt::get));
    std::for_each(cs.begin(), cs.end(), std::bind2nd(std::mem_fun_ref(&Cnt::add), 10));
    if (got[0] == 1 && got[1] == 2 && cs[0].v == 11 && cs[1].v == 12) r += 1;
    return r;
}

int numericOps()                           // 2460: accumulate, inner_product, partial_sum, adjacent_difference, max_element on a list
{
    int a[5] = { 1, 2, 3, 4, 5 };
    std::list<int> l(a, a + 5);
    int r = std::accumulate(l.begin(), l.end(), 0) * 100;                   // 1500
    r += std::inner_product(a, a + 5, a, 0) * 10;                           // 550
    int ps[5]; std::partial_sum(a, a + 5, ps);                               // 1 3 6 10 15
    int ad[5]; std::adjacent_difference(ps, ps + 5, ad);                     // 1 2 3 4 5
    if (ps[4] == 15 && std::equal(ad, ad + 5, a)) r += 400;
    if (*std::max_element(l.begin(), l.end()) == 5 && *std::min_element(l.begin(), l.end()) == 1) r += 10;
    return r;
}

int containersAdaptors()                   // 1234: deque, list operations, stack, queue, priority_queue
{
    std::deque<int> d;
    for (int i = 0; i < 5; ++i) { d.push_back(i); d.push_front(-i); }     // -4 -3 -2 -1 0 0 1 2 3 4
    d.pop_front(); d.pop_back();                                            // -3 .. 3
    d.insert(d.begin() + 2, 99);
    d.erase(d.begin() + 2);
    int r = 0;
    if (d.size() == 8 && d.front() == -3 && d.back() == 3 && d[3] == 0) r += 1000;
    std::list<int> l;
    l.push_back(3); l.push_back(1); l.push_back(2); l.push_back(2); l.push_front(5);
    l.sort(); l.unique(); l.reverse();                                      // 5 3 2 1
    std::list<int> m; m.push_back(4);
    l.splice(++l.begin(), m);                                               // 5 4 3 2 1
    std::string s;
    for (std::list<int>::iterator i = l.begin(); i != l.end(); ++i) s += static_cast<char>('0' + *i);
    if (s == "54321" && m.empty() && l.size() == 5) r += 200;
    std::stack<int> st; st.push(1); st.push(2); st.push(3);
    std::queue<int> q; q.push(1); q.push(2); q.push(3);
    std::priority_queue<int> pq; pq.push(2); pq.push(9); pq.push(5);
    if (st.top() == 3 && q.front() == 1 && q.back() == 3 && pq.top() == 9) { st.pop(); q.pop(); pq.pop(); if (st.top() == 2 && q.front() == 2 && pq.top() == 5 && st.size() == 2) r += 30; }
    std::list<int> toSort; for (int i = 0; i < 20; ++i) toSort.push_back((i * 7) % 20);
    toSort.sort();
    bool sorted = true; int prev = -1;
    for (std::list<int>::iterator i = toSort.begin(); i != toSort.end(); ++i) { if (*i < prev) sorted = false; prev = *i; }
    if (sorted && toSort.size() == 20) r += 4;
    return r;
}

int bitsAndManipulators()                  // 5321: bitset, iomanip, generic algorithms on a deque, heap, permutations
{
    std::bitset<10> b(5ul);                                                 // 0000000101
    b.set(9); b.flip(0);
    int r = 0;
    if (b.count() == 2 && b.to_string() == "1000000100" && b.to_ulong() == 516 && (b << 1).test(3) && (~b).count() == 8) r += 5000;
    std::ostringstream o;
    o << std::setw(6) << std::setfill('*') << 42 << '|' << std::setprecision(3) << 3.14159 << '|' << std::setbase(16) << 255;
    if (o.str() == "****42|3.14|ff") r += 300;
    std::deque<int> d; d.push_back(3); d.push_back(1); d.push_back(2);
    std::sort(d.begin(), d.end());
    std::vector<int> h; h.push_back(1); h.push_back(8); h.push_back(3);
    std::make_heap(h.begin(), h.end()); std::sort_heap(h.begin(), h.end());
    int p[3] = { 1, 2, 3 }; int perms = 1;
    while (std::next_permutation(p, p + 3)) ++perms;
    std::rotate(p, p + 1, p + 3);                                           // 2 3 1
    if (d[0] == 1 && d[2] == 3 && h[2] == 8 && perms == 6 && p[0] == 2 && p[2] == 1) r += 20;
    if (std::lower_bound(d.begin(), d.end(), 2) == d.begin() + 1 && std::binary_search(d.begin(), d.end(), 3)) r += 1;
    return r;
}

static int g_unwinding = -1;
struct Probe { ~Probe() { g_unwinding = std::uncaught_exception() ? 1 : 0; } };
static void throwing() { Probe p; throw 7; }

int exceptionsAndNew()                     // 2731: uncaught_exception during unwinding, nothrow new, auto_ptr, set_terminate's return, bad_alloc
{
    int r = 0;
    try { throwing(); } catch (int e) { if (e == 7 && g_unwinding == 1) r += 700; }
    Probe outside; (void)outside;
    int* p = new (std::nothrow) int(5);
    if (p && *p == 5) r += 30;
    delete p;
    std::auto_ptr<Cnt> ap(new Cnt(1));
    std::auto_ptr<Cnt> bp = ap;
    if (ap.get() == 0 && bp->get() == 1) r += 1;
    std::terminate_handler old = std::set_terminate(0);
    std::set_terminate(old);
    // the library's own operator new throws bad_alloc when malloc fails (the C runtime's reports it through MSVC's
    // exception handling, which no cppparse frame catches)
    try { char* big = new char[0x7fffffffffffULL]; delete[] big; } catch (std::bad_alloc& e) { if (e.what()[0] == 'b') r += 2000; }
    return r;
}
