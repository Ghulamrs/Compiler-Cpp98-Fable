/* cprog.h - shared declarations of the multi-file C sample. The sources are C (C99) written in the subset
   that C++ accepts too, so they carry the .cpp extension: malloc results are cast, string literals are
   const char*, enum values are assigned only from enumerators, and the shared const object is declared
   extern (a const at file scope has internal linkage in C++). */
#ifndef CPROG_H
#define CPROG_H

#include <stddef.h>

#define CPROG_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define CPROG_STR(x) #x
#define CPROG_XSTR(x) CPROG_STR(x)
#define CPROG_TABLE_SIZE 17

extern const int cprog_version;
extern int cprog_calls;

typedef enum { SHAPE_CIRCLE, SHAPE_RECT, SHAPE_TRIANGLE } ShapeKind;

typedef struct {
    ShapeKind kind;
    union {
        struct { double r; } circle;
        struct { double w, h; } rect;
        struct { double a, b, c; } tri;
    } u;
} Shape;

typedef struct Node {
    int value;
    struct Node* next;
} Node;

typedef struct {
    char key[32];
    int count;
} Entry;

typedef struct Bucket {
    Entry entry;
    struct Bucket* next;
} Bucket;

typedef struct {
    Bucket* slots[CPROG_TABLE_SIZE];
    int size;
} Table;

typedef int (*BinaryOp)(int, int);

/* strutil.cpp */
size_t str_trim(char* s);
void str_reverse(char* s);
int str_count_words(const char* s);
void str_to_upper(char* s);
void str_caesar(char* s, int shift);
int str_split(char* s, char sep, char** parts, int max);

/* mathutil.cpp */
long math_factorial(int n);
long math_fib(int n);
int math_gcd(int a, int b);
int math_sieve(int limit, int* primes, int max);
void math_matmul(double a[][3], double b[][3], double out[][3]);
double math_mean(const double* v, int n);
double math_stddev(const double* v, int n);
unsigned math_popcount(unsigned v);
unsigned math_rotl(unsigned v, int k);
double shape_area(const Shape* s);

/* list.cpp */
Node* list_push(Node* head, int value);
Node* list_reverse(Node* head);
int list_sum(const Node* head);
void list_free(Node* head);
int compare_ints(const void* a, const void* b);
int compare_strings(const void* a, const void* b);
int binary_search(const int* v, int n, int key);

/* table.cpp */
void table_init(Table* t);
void table_add(Table* t, const char* key);
int table_get(const Table* t, const char* key);
void table_free(Table* t);

#endif
