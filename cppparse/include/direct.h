// direct.h - cppparse's declarations of the MSVC directory functions its own sources use.
#ifndef CPPPARSE_DIRECT_H
#define CPPPARSE_DIRECT_H

extern "C" {
    char* _getcwd(char* buffer, int size);
    int   _mkdir(const char* path);
    int   _rmdir(const char* path);
    int   _chdir(const char* path);
}

#endif
