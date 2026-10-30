// cstrovl-refused.cpp - must be rejected: the search functions of a const argument return a pointer to const (21.4,
// cl C2440); front.cpp cstrovl:: has the valid forms
#include <cstring>
#include <cwchar>
int main()
{
    const char* cs = "abc";
    const wchar_t* ws = L"abc";
    char* bad = std::strchr(cs, 'b');
    wchar_t* bad2 = std::wcsstr(ws, L"b");
    void* bad3 = std::memchr(cs, 'c', 3);
    return bad == 0 && bad2 == 0 && bad3 == 0;
}
