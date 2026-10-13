// memptr-global.cpp - must be rejected by --compile6 (S132): a namespace-scope and a static member pointer to
// member (cl: 4 bytes, null -1; they were 8 bytes of 0)
struct P { int a; };
int P::* gmp;
struct S { static int P::* smp; };
int P::* S::smp = 0;

int main() { return 0; }
