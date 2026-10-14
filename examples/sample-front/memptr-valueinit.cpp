// memptr-valueinit.cpp - must be rejected by --compile6 (S132): value-initialisation of an object that holds a
// pointer to member (a class template instantiation, a member, an array element) and brace initialisation of
// one: zeroing does not give cl's null data member pointer (-1; cl itself gives -1 or 0 by the nesting)
struct P { int a; };
template <class T> struct H { int T::* f; int x; };
struct Outer { H<P> h; int y; };
struct M { H<P> h; M() : h() {} };
int g() { H<P> h = H<P>(); return h.x; }
int k() { Outer* o = new Outer(); int r = o->y; delete o; return r; }
int n() { Outer* a = new Outer[2](); int r = a[1].y; delete[] a; return r; }
int b() { Outer o = { }; return o.y; }
int m() { M v; return v.h.x; }

int main() { return g() + k() + n() + b() + m(); }
