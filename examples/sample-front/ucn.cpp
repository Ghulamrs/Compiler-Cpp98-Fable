// ucn.cpp - universal character names (C++98 2.2/2, Annex E): in identifiers they are characters of the name
// (\u00e9, \u00E9 and \U000000e9 are one character), # and -E keep the spelling written, and the decorated name
// holds the character as UTF-8, as cl's; in literals they are the character in the execution character set (cl:
// code page 1252 in a narrow literal, '?' for a character the page lacks; UTF-16 in a wide one). Test 17 compares
// the program's output with cl's build at -O0 and -O2, links the object with cl's object of the UCN_OTHER half
// (the names must decorate alike), and compares -E with the lines expected (with UCN_NO_STDIO, so that only this
// file's lines take part).
#define STR2(x) #x
#define STR(x) STR2(x)
#define CAT(a, b) a##b

#ifdef UCN_OTHER
// the half cl compiles: the definitions the other half refers to
int caf\u00e9 = 1;
namespace n\u03b1 {
    struct S\u00e9 { int m\u00e9; int f\u00e9(int x\u00e9) const; static int s\u4e2d; };
    int S\u00e9::f\u00e9(int x\u00e9) const { return x\u00e9 * 10 + m\u00e9; }
    int S\u00e9::s\u4e2d = 6;
    int \u00c0b\U000000C1(int a, double b) { return a + static_cast<int>(b); }
}
#else
#ifndef UCN_NO_STDIO
#include <cstdio>
#endif
extern int caf\u00e9;
namespace n\u03b1 {
    struct S\u00e9 { int m\u00e9; int f\u00e9(int x\u00e9) const; static int s\u4e2d; };
    int \u00c0b\U000000C1(int a, double b);
}
int \u0434\u0430 = 2;                           // Cyrillic, a name that starts with a universal character name
int CAT(x, \u00e9y) = 5;                        // x\u00e9y by ##
#define M\u00e9 7
template <class T\u00e9> struct Box\u00e9 { T\u00e9 v\u00e9; T\u00e9 get\u00e9() const { return v\u00e9; } };
enum E\u00e9 { one\u00e9 = 1, two\u00e9 };

int names()
{
    n\u03b1::S\u00e9 s; s.m\u00e9 = 3;
    Box\u00e9<int> b; b.v\u00e9 = two\u00e9;
    int local\u00e9 = caf\u00E9 + \u0434\u0430;   // \u00E9 and \u00e9 are one character
    return local\u00e9 * 100000 + s.f\u00e9(4) * 1000 + n\u03b1::\u00c0b\u00c1(1, 2.0) * 100 + n\u03b1::S\U000000e9::s\u4e2d * 10 + x\u00e9y + M\u00e9 + b.get\u00e9();
}

const char  n1[] = "\u00e9";                    // E9
const char  n2[] = "\u20ac";                    // 80: the euro sign of code page 1252
const char  n3[] = "\U0001F600";                // ?? : one '?' for each UTF-16 unit
const char  n4[] = "a\u00ffb\u0100c\u00a0";     // U+0100 is '?'
const char  n5[] = "\u201a\u0192\u201e\u2026\u2020\u2021\u02c6\u2030\u0160\u2039\u0152\u017d\u2018\u2019\u201c\u201d\u2022\u2013\u2014\u02dc\u2122\u0161\u203a\u0153\u017e\u0178";
const char  n6[] = "\u0024\u0040\u0060\u00a1\u00bf\uffff\ufffd\U0010FFFF" "\u00e9";
const wchar_t w1[] = L"\u00e9\u20ac\U0001F600\U00010000\U0010ffff\uffff\u0024";
const wchar_t w2[] = L"a" "\u20ac" "\U0001F600" "\u0100";   // the narrow pieces go through the code page
const char  s1[] = STR(caf\u00e9 "\u00e9" '\u00e9' L"\u20ac");
const char  s2[] = STR(caf\u00E9 \U000000e9 x\u00e9y);

#ifndef UCN_NO_STDIO
static void dump(const char* tag, const char* s, unsigned n) { std::printf("%s %u:", tag, n); for (unsigned i = 0; i < n; ++i) std::printf(" %02x", static_cast<unsigned>(static_cast<unsigned char>(s[i]))); std::printf("\n"); }
static void wdump(const char* tag, const wchar_t* s, unsigned n) { std::printf("%s %u:", tag, n); for (unsigned i = 0; i < n; ++i) std::printf(" %04x", static_cast<unsigned>(s[i])); std::printf("\n"); }

int main()
{
    std::printf("names %d\n", names());
    dump("n1", n1, sizeof n1); dump("n2", n2, sizeof n2); dump("n3", n3, sizeof n3); dump("n4", n4, sizeof n4);
    dump("n5", n5, sizeof n5); dump("n6", n6, sizeof n6);
    wdump("w1", w1, sizeof w1 / sizeof w1[0]); wdump("w2", w2, sizeof w2 / sizeof w2[0]);
    dump("s1", s1, sizeof s1); dump("s2", s2, sizeof s2);
    std::printf("c %d %d %d %d\n", '\u00e9', '\u00ff', '\u00a3', '\u0040');
    std::printf("c1252 %d %d %d %d\n", '\u20ac', '\u0152', '\u0178', '\u0100');
    std::printf("cmulti %d %d %d %d\n", 'a\u00e9', '\u00e9\u00e9', '\U0001F600', 'a\U0001F600');
    std::printf("wc %d %d %d %d\n", L'\u00e9', L'\u20ac', L'\uffff', static_cast<int>(L'\U0001F600'));
    // in a multicharacter constant a universal character name is no escape sequence of cl's rule (the second
    // escape sequence and what follows it go above) and is always shifted in
    std::printf("cesc %x %x %x %x\n", '\n\u00e9\tb', '\u00e9\n\tb', '\n\t\u00e9', '\n\U0001F600\t');
#if '\u00e9' == 0xC3A9 && '\u20ac' == 0xE282AC && L'\u20ac' == 0x20ac && L'\U0001F600' == 0xD83D && '\xe9' < 0
    std::printf("#if: a narrow universal character name is its UTF-8 bytes\n");   // cl's preprocessor (16.1/4)
#endif
    std::printf("sz %u %u %u\n", static_cast<unsigned>(sizeof("\U0001F600")), static_cast<unsigned>(sizeof(L"\U0001F600")), static_cast<unsigned>(sizeof("\u0100")));
    return 0;
}
#endif
#endif
