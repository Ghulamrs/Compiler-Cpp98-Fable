// new-refused.cpp - must be rejected by --compile6 (S132): allocations it cannot yet route through the class's own
// operators as cl does (a multidimensional array of a class with operator new[]; a class placement operator new is
// called since 2026-10-15, see front.cpp placement::)
typedef unsigned long long size_t;
extern "C" void* malloc(size_t n);
extern "C" void free(void* p);
struct S { int id; S() : id(1) {} ~S() {} static void* operator new[](size_t n) { return malloc(n); } static void operator delete[](void* p) { free(p); } };
int twoDims() { S (*p)[3] = new S[2][3]; int r = p[1][2].id; delete[] p; return r; }
int main() { return twoDims(); }
