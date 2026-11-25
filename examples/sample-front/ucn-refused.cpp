// ucn-refused.cpp - universal character names that C++98 2.2/2 and Annex E do not allow: each line is an error
// (cl C3850, C3851, C3872; in literals cl applies C++11's rule and accepts lines 9 and 10, clang -std=c++98
// -pedantic-errors refuses them), and the lines of the skipped group are not looked at.
int a\u0041 = 1;                // a member of the basic source character set
int b\u001f = 1;                // a control character
int c\u0085 = 1;                // a control character (0x7F to 0x9F)
int d\u00d7 = 1;                // the multiplication sign is not one of Annex E's characters
int e\u2030 = 1;                // nor is the per mille sign
const char* f = "\u0041";       // 2.2/2 holds in a literal too
const char* g = "\u001f";
const char* h = "\ud800";       // a surrogate is no character
const wchar_t* i = L"\U00110000";
const char* j = "\u00e";        // \u takes four digits
char k = '\U0000e9';            // \U takes eight
#define L\u0040 1
#if 0
int m\u0041 = "\u0041\u00e";
#endif
int ok\u00e9 = '\u0040' + sizeof("\u0024\u0060\\u0041");
