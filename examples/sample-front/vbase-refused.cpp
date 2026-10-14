// vbase-refused.cpp - must be rejected by --compile6 (S132): a cast from a virtual base down to the derived class
// (ill-formed, cl C2635); a catch of a virtual base and a thrown pointer to a class with virtual bases, refused
// here before, are compiled (sample-eh ehCatchVbase)
struct V { int v; V() : v(7) {} };
struct A : virtual V { int a; };
struct B : virtual V { int b; };
struct D : A, B { int d; };

int down(V* p) { D* d = static_cast<D*>(p); return d->d; }

int main() { D d; return down(&d); }
