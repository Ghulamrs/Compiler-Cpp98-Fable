// dtor-catch.cpp - a destructor run by the stack unwinding throws, and catches the exception inside: the program goes
// on (15.2/3). terminate.cpp's sibling: there the exception leaves the destructor and the program must terminate. The
// runtime tells the two apart by the frame of the landing pad that runs the destructor, which it notes with
// _AddressOfReturnAddress (row P1 stage 2, docs/design-selfhost.md 4.2): a frame noted one too high would terminate
// here, or let terminate.cpp's exception be caught. The output and the exit code must equal cl's.
#include <cstdio>

struct Inner {
    int depth;
    explicit Inner(int d) : depth(d) {}
    ~Inner()
    {
        try { throw depth; }
        catch (int v) { std::printf("caught %d inside a destructor\n", v); }
    }
};

struct Outer {
    int id;
    explicit Outer(int i) : id(i) {}
    ~Outer()
    {
        Inner in(id * 10);
        std::printf("destroying %d\n", id);
    }
};

static void thrower(int n)
{
    Outer o(n);
    if (n == 0) throw 2.5;
    thrower(n - 1);
}

int main()
{
    try { thrower(2); }
    catch (double d) { std::printf("caught %g after the unwinding\n", d); }
    std::printf("the program goes on\n");
    return 0;
}
