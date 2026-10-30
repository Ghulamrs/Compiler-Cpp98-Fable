/* mathutil.cpp - arithmetic, number theory, matrices, statistics and shapes of the multi-file C sample */
#include "cprog.h"
#include <math.h>
#include <stdlib.h>

long math_factorial(int n)
{
    return n <= 1 ? 1L : n * math_factorial(n - 1);
}

long math_fib(int n)
{
    static long memo[47];   /* static storage starts as zero, which marks "not computed yet" */
    if (n < 2) return n;
    if (memo[n]) return memo[n];
    memo[n] = math_fib(n - 1) + math_fib(n - 2);
    return memo[n];
}

int math_gcd(int a, int b)
{
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a < 0 ? -a : a;
}

int math_sieve(int limit, int* primes, int max)
{
    char* composite = (char*)calloc((size_t)limit + 1, 1);
    int count = 0, i, j;
    if (!composite) return 0;
    for (i = 2; i <= limit; ++i) {
        if (composite[i]) continue;
        if (count < max) primes[count] = i;
        ++count;
        for (j = i * i; j <= limit; j += i) composite[j] = 1;
    }
    free(composite);
    return count;
}

void math_matmul(double a[][3], double b[][3], double out[][3])
{
    int i, j, k;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j) {
            double sum = 0.0;
            for (k = 0; k < 3; ++k) sum += a[i][k] * b[k][j];
            out[i][j] = sum;
        }
}

double math_mean(const double* v, int n)
{
    double sum = 0.0;
    int i;
    for (i = 0; i < n; ++i) sum += v[i];
    return n > 0 ? sum / n : 0.0;
}

double math_stddev(const double* v, int n)
{
    double mean = math_mean(v, n), acc = 0.0;
    int i;
    for (i = 0; i < n; ++i) acc += (v[i] - mean) * (v[i] - mean);
    return n > 1 ? sqrt(acc / (n - 1)) : 0.0;
}

unsigned math_popcount(unsigned v)
{
    unsigned count = 0;
    while (v) {
        v &= v - 1;
        ++count;
    }
    return count;
}

unsigned math_rotl(unsigned v, int k)
{
    k &= 31;
    return k ? (v << k) | (v >> (32 - k)) : v;
}

double shape_area(const Shape* s)
{
    switch (s->kind) {
    case SHAPE_CIRCLE:
        return 3.14159265358979323846 * s->u.circle.r * s->u.circle.r;
    case SHAPE_RECT:
        return s->u.rect.w * s->u.rect.h;
    case SHAPE_TRIANGLE: {
        double p = (s->u.tri.a + s->u.tri.b + s->u.tri.c) / 2.0;   /* Heron's formula */
        return sqrt(p * (p - s->u.tri.a) * (p - s->u.tri.b) * (p - s->u.tri.c));
    }
    }
    return 0.0;
}
