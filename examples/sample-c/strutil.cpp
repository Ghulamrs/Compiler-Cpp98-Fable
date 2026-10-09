/* strutil.cpp - string helpers of the multi-file C sample */
#include "cprog.h"
#include <ctype.h>
#include <string.h>

/* table.cpp has a static function of the same name with another meaning: internal linkage keeps them apart */
static int is_separator(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == ',';
}

size_t str_trim(char* s)
{
    size_t len = strlen(s), start = 0, i;
    while (start < len && isspace((unsigned char)s[start])) ++start;
    while (len > start && isspace((unsigned char)s[len - 1])) --len;
    for (i = start; i < len; ++i) s[i - start] = s[i];
    s[len - start] = '\0';
    ++cprog_calls;
    return len - start;
}

void str_reverse(char* s)
{
    char* end = s + strlen(s);
    while (s < end) {
        char t = *s;
        *s++ = *--end;
        *end = t;
    }
}

int str_count_words(const char* s)
{
    int words = 0, inWord = 0;
    for (; *s; ++s) {
        if (is_separator((unsigned char)*s)) inWord = 0;
        else if (!inWord) { inWord = 1; ++words; }
    }
    return words;
}

void str_to_upper(char* s)
{
    for (; *s; ++s) *s = (char)toupper((unsigned char)*s);
}

void str_caesar(char* s, int shift)
{
    shift = ((shift % 26) + 26) % 26;
    for (; *s; ++s) {
        if (isupper((unsigned char)*s)) *s = (char)('A' + (*s - 'A' + shift) % 26);
        else if (islower((unsigned char)*s)) *s = (char)('a' + (*s - 'a' + shift) % 26);
    }
}

/* splits s in place at every sep; empty fields are kept */
int str_split(char* s, char sep, char** parts, int max)
{
    int n = 0;
    char* p = s;
    if (max <= 0) return 0;
    parts[n++] = p;
    while (*p) {
        if (*p == sep) {
            *p = '\0';
            if (n == max) break;
            parts[n++] = p + 1;
        }
        ++p;
    }
    return n;
}
