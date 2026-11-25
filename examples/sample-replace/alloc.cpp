// sample-replace (scripts\replace.ps1): a program that only allocates. Its operator new and delete, plain and array,
// must be libcpp's and not the C runtime's library's
#include <cstdio>
struct Node { int v; Node* next; };
int main() {
    Node* n = new Node; n->v = 7; n->next = 0;
    int* a = new int[4]; a[3] = n->v;
    std::printf("%d\n", a[3]);
    delete[] a; delete n;
    return 0;
}
