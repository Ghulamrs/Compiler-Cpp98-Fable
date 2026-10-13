// goto-refused.cpp - must be rejected by --compile6 (S132): ill-formed jumps (C++98 6.7/3, 15/2; cl C2362,
// C2041). A goto past the initialisation of an object with a destructor, and a goto into a try block
struct L { int v; L(int x) : v(x) {} ~L() {} };

int pastInit(int n) { if (n) goto x; { } L a(1); x: return n; }
int intoTry(int n) { if (n) goto y; try { y: n += 2; } catch (...) {} return n; }

int main() { return pastInit(0) + intoTry(0); }
