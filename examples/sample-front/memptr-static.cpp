// memptr-static.cpp - must be rejected by --compile6 (S132): objects of static storage that hold a pointer to
// member (cl writes -1 for a null data member pointer where cppparse would zero it: H<P> gh gave 0)
struct P { int a; };
template <class T> struct H { int T::* f; int x; };
struct Outer { H<P> h; int y; };
H<P> gh;
static Outer go[2];
int f() { static H<P> sh; return sh.x; }
struct S { static Outer so; };
Outer S::so;

int main() { return gh.x + go[1].y + f() + S::so.y; }
