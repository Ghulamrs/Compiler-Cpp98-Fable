// io.h - cppparse's declarations of the MSVC low-level file functions its own sources use.
#ifndef CPPPARSE_IO_H
#define CPPPARSE_IO_H

#include <cstddef>
#include <ctime>

struct _finddata_t {
    unsigned  attrib;
    time_t    time_create;
    time_t    time_access;
    time_t    time_write;
    unsigned long long size;
    char      name[260];
};

extern "C" {
    long long _findfirst64i32(const char* pattern, struct _finddata_t* data);
    int       _findnext64i32(long long handle, struct _finddata_t* data);
    int       _findclose(long long handle);
    int       _access(const char* path, int mode);
    int       _unlink(const char* path);
}

#define _findfirst _findfirst64i32
#define _findnext  _findnext64i32
#define _A_SUBDIR 0x10
#define _A_NORMAL 0x00

#endif
