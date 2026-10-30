// TypeErrors.cpp - deliberate type errors for the version 3 type checker.
#include "Widget.h"

struct Account {
    int    balance;
    double rate;
    void   deposit(int amount) { balance += amount; }
    int    total() const { return balance; }
private:
    int    secret;
};

int sum(int a, int b) { return a + b; }
double scale(double x) { return x * 2.0; }

int typeErrors(const Account& acc, Account* ptr, int n)
{
    int    a = "text";                 // T104 cannot convert const char* to int
    double d = undefinedName + 1;      // T100 undeclared identifier
    int    s = sum(1);                 // T102 no viable overload: argument count
    int    t = sum("a", "b");          // T102 no viable overload: argument types
    Account* bad = &a;                 // T104 cannot convert int* to Account*
    n.deposit(5);                      // T110 member access on non-class type
    a[2] = 1;                          // T111 subscript on non-pointer
    int    v = *n;                     // T112 dereference of non-pointer
    acc.deposit(1);                    // T106 non-const method on const object
    acc.balance = 2;                   // T107 assignment to const
    ptr.total();                       // T117 '.' on pointer
    int    w = ptr->secret;            // T119 private member
    if (acc) { }                       // T109 condition not convertible to bool
    Account x = 5;                     // T104 no conversion from int
    ptr->nothing();                    // T101 no member
    5 = a;                             // T121 not an lvalue
    a + 1;                             // T118 expression result unused
    unsigned u = 3;
    if (a < u) { }                     // T115 signed/unsigned comparison
    int    k = d;                      // T116 narrowing
    break;                             // T122 break outside loop
    return acc;                        // T108 cannot return Account as int
}

void voidReturn()
{
    return 1;                          // T108 void function returns a value
}

int goodCode(int q)
{
    Account acc;
    acc.balance = q;
    acc.deposit(1);
    double r = scale(acc.total());
    return sum(q, 2) + (int)r;
}
