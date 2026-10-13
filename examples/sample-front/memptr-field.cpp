// memptr-field.cpp - must be rejected by --compile6 (S132): a class with a member of pointer-to-member type
// (cl's null data member pointer is -1, its size 4; copies and value-initialisation are not generated yet)
struct P { int a; int b; };
struct Holder { int P::* field; int x; };

int main() { Holder h = Holder(); return h.x; }
