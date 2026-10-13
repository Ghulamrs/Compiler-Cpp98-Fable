// memptr-refused.cpp - must be rejected by --compile6 (S132): pointers to members are not generated yet (cl's
// representations: a data member pointer is a 4-byte offset, null -1; a member function pointer a code pointer,
// with a this adjustment under multiple inheritance and vcall thunks ??_9 for virtual functions). Each of these
// once compiled to wrong code: ->* read garbage, a null data member pointer was 0, the types mangled as void*
struct P { int a; int b; int f() const { return a + b; } };
struct M { int m; };
struct Q : M, P { int q; };

int readData(P* p) { int P::* pm = &P::b; return p->*pm; }
bool isNull() { int P::* pm = 0; return pm == 0; }
int derivedMember(Q* q) { int Q::* pm = &P::b; return q->*pm; }

int main()
{
    P p; p.a = 1; p.b = 2;
    Q q; q.b = 3;

    return readData(&p) + isNull() + derivedMember(&q);
}
