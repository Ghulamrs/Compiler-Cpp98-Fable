// alttok.cpp - the alternative tokens (2.5/2), the digraphs (2.5/1) and the trigraphs (2.3/1) of C++98: every
// spelling means its primary token, and # keeps the spelling written (2.5/2). cl reads them with /permissive-
// and /Zc:trigraphs. Test 17 compares the program's output with cl's build; test 10 compares -E with cl /EP
// (with ALTTOK_NO_STDIO defined, so that only this file's lines take part).
#ifndef ALTTOK_NO_STDIO
#include <cstdio>
#endif
#define STR(x) #x
%:define DIG_ONE 1
??=define TRI_TWO 2

int alt(int a, int b)
{
    int r = 0;
    if (a and b) r += 1;
    if (a or b) r += 2;
    if (not (a bitand 0)) r += 4;
    if ((a bitor b) == 3) r += 8;
    if ((a xor b) == 3) r += 16;
    if (compl a == ~a) r += 32;
    if (a not_eq b) r += 64;
    int c = a;
    c and_eq 1; c or_eq 4; c xor_eq 2;      // 1, 5, 7
    return r + c;
}

int dig()
<%
    int arr<:3:> = <% DIG_ONE, 2, 3 %>;
    return arr<:0:> + arr<:1:> * 10 + arr<:2:> * 100;
%>

int tri()
??<
    int arr??(2??) = ??< TRI_TWO, 3 ??>;
    int bits = 5 ??! 2;                     // the trigraph of |
    int ones = ??-0 bitand 0xFF;            // the trigraph of ~
    int c = 1 ??' 3;                        // the trigraph of ^
    return arr??(0??) + arr??(1??) * 10 + (bits == 7 ? 100 : 0) + (ones == 255 ? 1000 : 0) + (c == 2 ? 10000 : 0);
??>

const char* const strs[] = { STR(a and b), STR(x<:1:>), STR(%: y) };

#ifndef ALTTOK_NO_STDIO
int main()
{
    printf("alt %d\n", alt(1, 2));          // 1 + 2 + 4 + 8 + 16 + 32 + 64 + 7
    printf("dig %d\n", dig());              // 321
    printf("tri %d\n", tri());              // 11132
    printf("str %s|%s|%s\n", strs[0], strs[1], strs[2]);
    const char* s = "??/n";                 // phase 1 precedes the literal: the trigraph of the backslash makes "\n"
    printf("strlit %d\n", s[0] == 10 && s[1] == 0 ? 1 : 0);
    return 0;
}
#endif
