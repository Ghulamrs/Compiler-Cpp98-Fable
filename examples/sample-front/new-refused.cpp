// new-refused.cpp - must be rejected by --compile6 (S132): allocations it cannot yet route through the class's own
// operators as cl does (a class placement operator new, a multidimensional array of a class with operator new[])
typedef unsigned long long size_t;
extern "C" void* malloc(size_t n);
extern "C" void free(void* p);
struct Q { int id; Q(int x) : id(x) {} static void* operator new(size_t n, int) { return malloc(n); } static void operator delete(void* p) { free(p); } };
struct S { int id; S() : id(1) {} ~S() {} static void* operator new[](size_t n) { return malloc(n); } static void operator delete[](void* p) { free(p); } };
int placement() { Q* q = new (3) Q(1); int r = q->id; delete q; return r; }
int twoDims() { S (*p)[3] = new S[2][3]; int r = p[1][2].id; delete[] p; return r; }
int main() { return placement() + twoDims(); }
