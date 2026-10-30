// bigfile.cpp - 64-bit stream positions (C3): streamoff and fpos carry offsets beyond 2 GB, and the file buffers seek
// and tell there (tellp, seekp, tellg, seekg on fstream, ofstream and ifstream). The file is made sparse first, so
// writing 3 GB in takes no space. Built by cl with its own headers (the reference) and by cppparse --compile6 with
// include/, test 15 compares what the programs print at -O0 and -O2.
#include <cstdio>
#include <fstream>
#include <ios>
#include <string>

// the Win32 functions that make the file sparse (declared here: test 15 builds the library programs with include/ alone)
extern "C" {
    void* CreateFileA(const char* name, unsigned long access, unsigned long share, void* security, unsigned long disposition, unsigned long flags, void* templateFile);
    int   DeviceIoControl(void* device, unsigned long code, void* in, unsigned long inSize, void* out, unsigned long outSize, unsigned long* returned, void* overlapped);
    int   CloseHandle(void* handle);
}

static const char* kPath = "out/bigfile.bin";

static bool makeSparse()
{
    void* h = CreateFileA(kPath, 0x80000000UL | 0x40000000UL /* GENERIC_READ | GENERIC_WRITE */, 0, 0, 2 /* CREATE_ALWAYS */, 0x80 /* NORMAL */, 0);
    if (h == reinterpret_cast<void*>(static_cast<long long>(-1))) return false;
    unsigned long got = 0;
    const int ok = DeviceIoControl(h, 0x000900C4UL /* FSCTL_SET_SPARSE */, 0, 0, 0, 0, &got, 0);
    CloseHandle(h);
    return ok != 0;
}

static void showPos(const char* what, std::streamoff off) { std::printf("%-14s %lld\n", what, static_cast<long long>(off)); }

int main()
{
    const std::streamoff gb = 1024LL * 1024 * 1024;
    // the position types: an fpos holds a 64-bit offset, moves by one and subtracts to one
    std::streampos p(3 * gb + 5);
    p += 10;
    std::streampos q = p - static_cast<std::streamoff>(gb);
    std::streamoff d = p - q;
    showPos("fpos", p); showPos("fpos - off", q); showPos("fpos - fpos", d);
    std::printf("sparse %d\n", makeSparse() ? 1 : 0);
    {
        std::fstream f(kPath, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
        std::printf("open %d\n", f.is_open() ? 1 : 0);
        f.seekp(3 * gb + 5);
        f << "far away";
        showPos("tellp", f.tellp());
        f.seekg(3 * gb + 9);
        std::string w;
        f >> w;
        std::printf("read [%s]\n", w.c_str());
        f.clear();
        f.seekg(0, std::ios_base::end);
        showPos("size", f.tellg());
        f.seekg(-8, std::ios_base::end);
        showPos("end - 8", f.tellg());
        f.seekg(2 * gb + 7, std::ios_base::beg);
        f.seekg(gb, std::ios_base::cur);
        showPos("beg + cur", f.tellg());
        char c = 'x';
        f.get(c);
        std::printf("byte at 3 GB + 7 [%c]\n", c);
    }
    {
        // a buffered ofstream (output only) appending past 3 GB
        std::ofstream o(kPath, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
        o.seekp(0, std::ios_base::end);
        showPos("ofstream end", o.tellp());
        o << "tail";
        showPos("after tail", o.tellp());
    }
    {
        std::ifstream in(kPath, std::ios_base::in | std::ios_base::binary);
        in.seekg(3 * gb + 5);
        char buf[13] = {0};
        in.read(buf, 12);
        std::printf("ifstream [%s] tellg %lld\n", buf, static_cast<long long>(in.tellg()));
        in.seekg(-4, std::ios_base::end);
        std::string t;
        in >> t;
        std::printf("last [%s]\n", t.c_str());
    }
    std::printf("removed %d\n", std::remove(kPath) == 0 ? 1 : 0);
    return 0;
}
