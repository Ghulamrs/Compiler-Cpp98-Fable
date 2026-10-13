// vbase-refused.cpp - must be rejected by --compile6 (S132): constructs with virtual bases it cannot generate
// as cl does yet, or that are ill-formed (a cast from a virtual base down to the derived class, cl C2635)
struct V { int v; V() : v(7) {} };
struct A : virtual V { int a; };
struct B : virtual V { int b; };
struct D : A, B { int d; };

int down(V* p) { D* d = static_cast<D*>(p); return d->d; }
int memberPointer(D* d) { int D::* p = &D::d; return d->*p; }
int handler() { try { throw D(); } catch (V& x) { return x.v; } return 0; }

int main() { D d; return down(&d) + memberPointer(&d) + handler(); }
