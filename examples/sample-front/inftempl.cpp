// a template that instantiates itself without end: refused at depth 1000 (S148, cl C2999), not a stack overflow
template <class T> struct Inf { Inf<T*> next; };
int main() { Inf<int> i; return 0; }
