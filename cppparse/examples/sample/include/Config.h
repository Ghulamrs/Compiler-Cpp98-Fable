// Config.h - build configuration macros for the sample project.
// Exercises: include guard, object-like and function-like macros, line
// continuation, #if/#elif/#else chains, #ifdef, #pragma.
#ifndef SAMPLE_CONFIG_H
#define SAMPLE_CONFIG_H

#define SAMPLE_VERSION_MAJOR 1
#define SAMPLE_VERSION_MINOR 2
#define SAMPLE_MAX(a, b) ((a) > (b) ? (a) : (b))
#define SAMPLE_LONG_MACRO(x) \
    do {                     \
        (void)(x);           \
    } while (0)

#if defined(_WIN32) && !defined(SAMPLE_STATIC)
#  define SAMPLE_API __declspec(dllexport)
#elif defined(__GNUC__)
#  define SAMPLE_API __attribute__((visibility("default")))
#else
#  define SAMPLE_API
#endif

#ifdef _DEBUG
#  define SAMPLE_TRACE(msg) sample::trace(msg)
#else
#  define SAMPLE_TRACE(msg) ((void)0)
#endif

#pragma warning(disable : 4996)

namespace sample {
    void trace(const char* msg);
}

#endif // SAMPLE_CONFIG_H
