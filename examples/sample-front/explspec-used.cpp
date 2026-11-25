// explspec-used.cpp - must be rejected: an explicit specialisation declared after a use that instantiated the
// template for the same arguments (14.7.3/6; cl C2908 on the same eight lines). front2.cpp explorder:: has the
// valid forms: a specialisation declared before the first use, also after the class was only named
template <class T> struct Obj { int k() { return 1; } };
template <class T> struct InBody { int k() { return 1; } };
template <class T> struct Sized { int k() { return 1; } };
template <class T> struct Base { int k() { return 1; } };
template <class T> struct Mem { int f(); static int s; };
template <class T> int Mem<T>::f() { return 1; }
template <class T> int Mem<T>::s = 1;
template <class T> int fn(T) { return 1; }
template <class T> int addr(T) { return 1; }

Obj<int> obj;                                   // a namespace-scope object: Obj<int> is instantiated here
int inBody() { InBody<int> a; return a.k(); }   // in a function body
int sized = sizeof(Sized<int>);
struct Derived : Base<int> {};
int callsMember() { Mem<int> m; return m.f(); }
int readsStatic() { return Mem<char>::s; }
int callsFn() { return fn(1); }
int (*takes)(long) = &addr<long>;

template <> struct Obj<int> { int k() { return 2; } };
template <> struct InBody<int> { int k() { return 2; } };
template <> struct Sized<int> { int k() { return 2; } };
template <> struct Base<int> { int k() { return 2; } };
template <> int Mem<int>::f() { return 2; }
template <> int Mem<char>::s = 2;
template <> int fn<int>(int) { return 2; }
template <> int addr<long>(long) { return 2; }

int main() { return obj.k() + inBody() + sized + callsMember() + readsStatic() + callsFn() + takes(0); }
