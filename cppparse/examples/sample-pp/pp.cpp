// examples/sample-pp/pp.cpp - exercises the version 6 preprocessor; the test compares
// cppparse -E with cl /EP on this file (whitespace ignored).
#include "pp.h"
#include "inc/inner.h"
#include "pp.h"             /* second time: the include guard keeps it out */
#include <inner.h>          /* found through -I inc */

#define SQ(x) ((x) * (x))
#define CAT(a, b) a##b
#define STR(x) #x
#define XSTR(x) STR(x)
#define EMPTY
#define N 3
#define TWICE(x) x x
#define APPLY(f, v) f(v)
#define VERSION_MAJOR 2
#define VERSION_MINOR 10
#define VERSION (VERSION_MAJOR * 100 + VERSION_MINOR)

#if N > 2 && defined(SQ) && !defined(UNDEFINED_THING)
int a = SQ(N + 1);
int CAT(va, lue) = 7;
const char* s1 = STR(hello   world);
const char* s2 = XSTR(N);
const char* s3 = STR("quoted \n text");
#elif N
int b;
#else
int c;
#endif

#define f(x) x + f
int z = f(f(1));                     /* f is not re-expanded inside its own expansion */

#define g(x) (x EMPTY)
int w = g(1 EMPTY 2);
int t = TWICE(SQ(2));
int applied = APPLY(SQ, 5);
int ver = VERSION;

#ifdef VERSION
#  if VERSION >= 210 && (VERSION_MINOR % 2 == 0 || VERSION_MAJOR == 1)
int selected = 1;
#  elif VERSION > 100
int selected = 2;
#  else
int selected = 3;
#  endif
#endif

#ifndef INNER_VALUE
#error inner.h was not included
#endif
int inner = INNER_VALUE + PP_H_CONSTANT;

#if 'A' == 65 && (1 ? 2 : 3) == 2 && (0x10 == 16) && (010 == 8) && ~0 == -1 && (1 << 4) == 16
int chars = 1;
#endif

#define LINE_INFO __LINE__
int lineBefore = __LINE__;
int lineMacro = LINE_INFO;
const char* file = __FILE__;

#define SUM(a, b) (a + b)
int spanning = SUM(1,
                   2);

#undef N
#ifdef N
int n_still_defined;
#else
int n_gone;
#endif

#define GLUE3(a, b, c) a ## b ## c
int GLUE3(x, y, z) = 9;
#define CALL_EMPTY_ARG(a, b) a##b
int CALL_EMPTY_ARG(left, ) = 1;
int CALL_EMPTY_ARG(, right) = 2;
#define NOARGS() 42
int noargs = NOARGS();
#define PAREN (
#define CALL_SQ SQ PAREN 3)
int called = CALL_SQ;
long big = __cplusplus;
