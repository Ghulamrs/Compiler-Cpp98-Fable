// threads.cpp - the library's streams and strings on several threads at once (C3's thread-safety fix): eight threads
// start together, before any stream exists, and each builds strings, string streams and locales in a loop, so the
// global and classic locales are made by whichever thread comes first and their reference counts change from all
// of them. Each thread checks what it reads back and adds up a checksum; the program prints the totals, which do not
// depend on the order the threads run in. Built by cl with its own headers (the reference) and by cppparse
// --compile6 with include/, test 15 compares what the programs print at -O0 and -O2.
#include <cstdio>
#include <locale>
#include <sstream>
#include <string>

// the Win32 thread functions (declared here: test 15 builds the library programs with include/ alone)
extern "C" {
    void*         CreateThread(void* attributes, size_t stack, unsigned long (*start)(void*), void* param, unsigned long flags, unsigned long* id);
    unsigned long WaitForMultipleObjects(unsigned long count, void* const* handles, int all, unsigned long ms);
    unsigned long ResumeThread(void* thread);
    int           CloseHandle(void* handle);
}

struct Work {
    int           index;
    int           rounds;
    unsigned long checksum;
    int           mismatches;
};

static unsigned long hashText(const std::string& s)
{
    unsigned long h = 2166136261UL;
    for (size_t i = 0; i < s.size(); ++i) h = (h ^ static_cast<unsigned char>(s[i])) * 16777619UL;
    return h;
}

static unsigned long run(void* p)
{
    Work* w = static_cast<Work*>(p);
    for (int r = 0; r < w->rounds; ++r) {
        const int v = w->index * 100000 + r;
        // a string stream written and read back
        std::ostringstream out;
        out << "thread " << w->index << " round " << r << " value " << v << ' ' << (v % 7) * 0.5;
        const std::string text = out.str();
        std::istringstream in(text);
        std::string word1, word2, word3;
        int index = -1, round = -1, value = -1;
        double half = -1;
        in >> word1 >> index >> word2 >> round >> word3 >> value >> half;
        if (word1 != "thread" || index != w->index || round != r || value != v || half != (v % 7) * 0.5) ++w->mismatches;
        // strings built and compared
        std::string a = "name" + text.substr(0, 6);
        a += '#';
        a.append(word3);
        if (a.size() != 4 + 6 + 1 + word3.size() || a.compare(0, 4, "name") != 0) ++w->mismatches;
        // locales copied, compared and asked for their facets
        std::locale global;
        std::locale copy(global);
        const std::locale& classic = std::locale::classic();
        if (!(copy == classic) || !std::has_facet<std::ctype<char> >(copy)) ++w->mismatches;
        if (std::use_facet<std::ctype<char> >(copy).toupper('q') != 'Q') ++w->mismatches;
        std::locale assigned;
        assigned = copy;
        if (std::use_facet<std::numpunct<char> >(assigned).decimal_point() != '.') ++w->mismatches;
        w->checksum += hashText(text) % 1000003UL;
    }
    return 0;
}

int main()
{
    const int threads = 8;
    Work work[threads];
    void* handles[threads];
    int started = 0;
    // made suspended, then all resumed at once, so that they race for the first locale
    for (int t = 0; t < threads; ++t) {
        work[t].index = t;
        work[t].rounds = 1500;
        work[t].checksum = 0;
        work[t].mismatches = 0;
        handles[t] = CreateThread(0, 0, run, &work[t], 0x4 /* CREATE_SUSPENDED */, 0);
        if (handles[t]) ++started;
    }
    for (int t = 0; t < started; ++t) ResumeThread(handles[t]);
    WaitForMultipleObjects(static_cast<unsigned long>(started), handles, 1, 0xFFFFFFFFUL);
    unsigned long checksum = 0;
    int mismatches = 0, rounds = 0;
    for (int t = 0; t < started; ++t) {
        CloseHandle(handles[t]);
        checksum += work[t].checksum;
        mismatches += work[t].mismatches;
        rounds += work[t].rounds;
    }
    std::printf("threads %d, rounds %d, mismatches %d, checksum %lu\n", started, rounds, mismatches, checksum);
    // the streams still work on the main thread afterwards
    std::ostringstream last;
    last << "after the threads: " << 12345 << ' ' << std::string("ok");
    std::printf("%s\n", last.str().c_str());
    return 0;
}
