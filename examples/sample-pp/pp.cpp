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

/* #if arithmetic (16.1/4): 64-bit values, unsigned when an operand is, short circuit and dead arms are not
   evaluated (1/0 in the arm not taken); the 2026-10-15 review's ppar.cpp and pp3.cpp */
#if (0xFFFFFFFF > 0) && (1 << 31 != 0) && (~0 == -1) && (-1 > 0u ? 1 : 0) && (0xFFFFFFFFFFFFFFFF == -1) && (4294967296 > 0) && (4294967296 / 2 == 2147483648) && (18446744073709551615u != 0)
int ppArith = 1;
#else
int ppArith = 0;
#endif
#if 1 ? 2 : (1/0)
int ppTern = 1;
#else
int ppTern = 0;
#endif
#if 0 && (1/0)
int ppDead = 0;
#elif 1 || (1 % 0)
int ppDead = 1;
#else
int ppDead = 2;
#endif
#if (-1 < 0) && (5 / 2 == 2) && (5 % 3 == 2) && (-7 / 2 == -3) && (-7 % 2 == -1) && (1u << 63 == 0x8000000000000000) && (0x8000000000000000 >> 63 == 1) && (-1 >> 1 == -1)
int ppDiv = 1;
#else
int ppDiv = 0;
#endif
/* #line (16.4): the line and, when given, the file of what follows */
#line 100 "renamed.cpp"
int ppLine = __LINE__; const char* ppFile = __FILE__;
#line 200
int ppLine2 = __LINE__;
int ppLine3 = __LINE__;
