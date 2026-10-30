/* main.cpp - drives the multi-file C sample; every result is printed so that two builds can be compared */
#include "cprog.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int cprog_version = 3;   /* external linkage: cprog.h declared it extern before this definition */
int cprog_calls = 0;

static int op_add(int a, int b) { return a + b; }
static int op_sub(int a, int b) { return a - b; }
static int op_mul(int a, int b) { return a * b; }
static int op_mod(int a, int b) { return b ? a % b : 0; }

static int next_id(void)
{
    static int id = 100;
    return id++;
}

static void section(const char* title)
{
    printf("\n== %s ==\n", title);
}

static void test_strings(void)
{
    char text[64];
    char csv[] = "red,green,,blue";
    char* parts[8];
    int n, i;
    section("strings");
    strcpy(text, "   Hello, C in C++ world   ");
    printf("trimmed length %u: [%s]\n", (unsigned)str_trim(text), text);
    printf("words: %d\n", str_count_words(text));
    str_reverse(text);
    printf("reversed: [%s]\n", text);
    str_reverse(text);
    str_to_upper(text);
    printf("upper: [%s]\n", text);
    str_caesar(text, 3);
    printf("caesar +3: [%s]\n", text);
    str_caesar(text, -3);
    printf("caesar -3: [%s]\n", text);
    n = str_split(csv, ',', parts, 8);
    for (i = 0; i < n; ++i) printf("part %d: [%s]\n", i, parts[i]);
    printf("stringized: %s = %s\n", CPROG_STR(CPROG_TABLE_SIZE), CPROG_XSTR(CPROG_TABLE_SIZE));
}

static void test_math(void)
{
    int primes[16], count, i;
    double data[] = { 2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0 };
    double a[3][3] = { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 } };
    double unit[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    double out[3][3];
    section("math");
    printf("10! = %ld, fib(46) = %ld, gcd(462, 1071) = %d\n", math_factorial(10), math_fib(46), math_gcd(462, 1071));
    count = math_sieve(100, primes, (int)CPROG_ARRAY_SIZE(primes));
    printf("primes up to 100: %d, first ones:", count);
    for (i = 0; i < 16 && i < count; ++i) printf(" %d", primes[i]);
    printf("\n");
    printf("mean %.4f stddev %.4f\n", math_mean(data, 8), math_stddev(data, 8));
    math_matmul(a, unit, out);
    printf("a * I row 2: %.1f %.1f %.1f\n", out[2][0], out[2][1], out[2][2]);
    math_matmul(a, a, out);
    printf("a * a row 0: %.1f %.1f %.1f\n", out[0][0], out[0][1], out[0][2]);
    printf("popcount(0xF0F0) = %u, rotl(0x80000001, 4) = 0x%08X\n", math_popcount(0xF0F0u), math_rotl(0x80000001u, 4));
    printf("pow(2, 10) = %.0f, sqrt(2) = %.6f, fabs(-3.5) = %.1f\n", pow(2.0, 10.0), sqrt(2.0), fabs(-3.5));
    printf("unsigned wrap: %u, signed division: %d %d\n", 0u - 1u, -7 / 2, -7 % 2);
}

static void test_shapes(void)
{
    Shape shapes[3];
    Shape copy;
    double total = 0.0;
    int i;
    section("shapes");
    shapes[0].kind = SHAPE_CIRCLE;
    shapes[0].u.circle.r = 1.5;
    shapes[1].kind = SHAPE_RECT;
    shapes[1].u.rect.w = 3.0;
    shapes[1].u.rect.h = 4.0;
    shapes[2].kind = SHAPE_TRIANGLE;
    shapes[2].u.tri.a = 3.0;
    shapes[2].u.tri.b = 4.0;
    shapes[2].u.tri.c = 5.0;
    for (i = 0; i < 3; ++i) {
        double area = shape_area(&shapes[i]);
        total += area;
        printf("shape %d (kind %d): area %.3f\n", i, (int)shapes[i].kind, area);
    }
    copy = shapes[1];   /* struct assignment copies the union too */
    copy.u.rect.w = 10.0;
    printf("copy area %.1f, original still %.1f, total %.3f\n", shape_area(&copy), shape_area(&shapes[1]), total);
    printf("sizeof(Shape) = %u, sizeof(Node) = %u, sizeof(Table) = %u\n", (unsigned)sizeof(Shape), (unsigned)sizeof(Node), (unsigned)sizeof(Table));
}

static void test_lists(void)
{
    int values[] = { 42, 7, 19, 3, 88, 21, 56, 7 };
    const char* names[] = { "pear", "apple", "fig", "banana", "cherry" };
    Node* list = NULL;
    Node* p;
    size_t i;
    section("lists and sorting");
    for (i = 0; i < CPROG_ARRAY_SIZE(values); ++i) list = list_push(list, values[i]);
    printf("list sum %d, first %d\n", list_sum(list), list->value);
    list = list_reverse(list);
    printf("reversed:");
    for (p = list; p; p = p->next) printf(" %d", p->value);
    printf("\n");
    list_free(list);
    qsort(values, CPROG_ARRAY_SIZE(values), sizeof(values[0]), compare_ints);
    printf("sorted:");
    for (i = 0; i < CPROG_ARRAY_SIZE(values); ++i) printf(" %d", values[i]);
    printf("\nindex of 21: %d, index of 5: %d\n", binary_search(values, (int)CPROG_ARRAY_SIZE(values), 21),
           binary_search(values, (int)CPROG_ARRAY_SIZE(values), 5));
    qsort((void*)names, CPROG_ARRAY_SIZE(names), sizeof(names[0]), compare_strings);   /* the pointers move, not the strings */
    printf("names:");
    for (i = 0; i < CPROG_ARRAY_SIZE(names); ++i) printf(" %s", names[i]);
    printf("\n");
}

static void test_table(void)
{
    const char* text = "the quick brown fox jumps over the lazy dog the end";
    char buffer[128];
    char* words[16];
    Table t;
    int n, i;
    section("hash table");
    strcpy(buffer, text);
    n = str_split(buffer, ' ', words, 16);
    table_init(&t);
    for (i = 0; i < n; ++i) table_add(&t, words[i]);
    printf("%d words, %d distinct; the=%d fox=%d cat=%d\n", n, t.size, table_get(&t, "the"), table_get(&t, "fox"), table_get(&t, "cat"));
    table_free(&t);
}

static void test_control(void)
{
    BinaryOp ops[4];
    const char* symbols = "+-*%";
    int i, total = 0, attempts = 0, first, second, third, day, month, year;
    unsigned char bytes[4];
    unsigned value = 0x12345678u;
    char line[64];
    long parsed;
    char* end;
    section("control flow and function pointers");
    ops[0] = op_add;
    ops[1] = op_sub;
    ops[2] = op_mul;
    ops[3] = op_mod;
    for (i = 0; i < 4; ++i) printf("17 %c 5 = %d\n", symbols[i], ops[i](17, 5));
    i = 0;
    do {
        switch (i % 4) {
        case 0: total += 1; break;
        case 1: total += 10;   /* falls through */
        case 2: total += 100; break;
        default: total += 1000; break;
        }
        ++i;
    } while (i < 8);
    printf("switch total %d\n", total);
retry:
    ++attempts;
    if (attempts < 3) goto retry;
    first = next_id();   /* separate statements: the order of evaluation of call arguments is unspecified */
    second = next_id();
    third = next_id();
    printf("goto attempts %d, ids %d %d %d\n", attempts, first, second, third);
    memcpy(bytes, &value, sizeof(value));
    printf("little-endian bytes: %02X %02X %02X %02X\n", bytes[0], bytes[1], bytes[2], bytes[3]);
    parsed = strtol("  -0x1F rest", &end, 16);
    printf("strtol: %ld, rest [%s], atoi: %d\n", parsed, end, atoi("314abc"));
    sprintf(line, "%d-%s-%.2f", 7, "seven", 7.0 / 3.0);
    printf("sprintf: %s\n", line);
    if (sscanf("09/10/2026", "%d/%d/%d", &day, &month, &year) == 3) printf("sscanf: %d %d %d\n", year, month, day);
}

static void test_memory_and_files(void)
{
    int* grow = NULL;
    int capacity = 0, size = 0, i;
    FILE* f;
    char line[80];
    const char* path = "cprog_sample.tmp";
    section("memory and files");
    for (i = 0; i < 100; ++i) {
        if (size == capacity) {
            int* bigger;
            capacity = capacity ? capacity * 2 : 4;
            bigger = (int*)realloc(grow, (size_t)capacity * sizeof(int));
            if (!bigger) {
                free(grow);
                return;
            }
            grow = bigger;
        }
        grow[size++] = i * i;
    }
    printf("realloc: size %d capacity %d last %d\n", size, capacity, grow[size - 1]);
    free(grow);
    f = fopen(path, "w");
    if (!f) {
        printf("cannot write %s\n", path);
        return;
    }
    for (i = 1; i <= 3; ++i) fprintf(f, "line %d of %d\n", i, 3);
    fclose(f);
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, (int)sizeof(line), f)) printf("read: %s", line);
    fclose(f);
    remove(path);
}

int main(void)
{
    int r1, r2;
    printf("multi-file C sample, version %d\n", cprog_version);
    test_strings();
    test_math();
    test_shapes();
    test_lists();
    test_table();
    test_control();
    test_memory_and_files();
    srand(12345u);
    r1 = rand();
    r2 = rand();
    printf("\nrand after srand(12345): %d %d\nstr_trim calls: %d\n", r1, r2, cprog_calls);
    return 0;
}
