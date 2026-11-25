// cheaders.cpp - the C headers of Annex D.5 added on 2026-11-10 (assert.h, errno.h, float.h, iso646.h, limits.h,
// locale.h, setjmp.h, signal.h, time.h, wchar.h, wctype.h): each names its functions and macros in the global
// namespace. Test 15 compares the program's output with cl's build with MSVC's headers.
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <iso646.h>
#include <limits.h>
#include <locale.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

static jmp_buf g_env;
static int g_jumps = 0;

static void jumpBack(int value) { longjmp(g_env, value); }

int main()
{
    assert(1 + 1 == 2);
    errno = EDOM;
    printf("errno %d %d %d\n", errno == EDOM, EDOM, ERANGE);
    printf("float %d %d %d\n", FLT_MAX > 1.0f, DBL_DIG, FLT_RADIX);
    printf("iso646 %d %d\n", (1 and 1) ? 1 : 0, (0 or 1) ? 1 : 0);
    printf("limits %d %d %u\n", INT_MAX, CHAR_BIT, UINT_MAX);
    const char* loc = setlocale(LC_ALL, "C");
    printf("locale %s %s\n", loc ? loc : "(null)", localeconv()->decimal_point);
    int v = setjmp(g_env);
    if (v == 0) { ++g_jumps; jumpBack(7); }
    printf("setjmp %d %d\n", v, g_jumps);
    printf("signal %d %d\n", signal(SIGABRT, SIG_DFL) == SIG_DFL, SIGINT);
    printf("time %d %d\n", time(0) > 0, clock() >= 0);
    printf("wchar %d %d\n", (int)wcslen(L"abc"), wcscmp(L"a", L"b") < 0);
    printf("wctype %d %d %d\n", iswalpha(L'a') != 0, iswdigit(L'7') != 0, (int)towupper(L'q'));
    return 0;
}
