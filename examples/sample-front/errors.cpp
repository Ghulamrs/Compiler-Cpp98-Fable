// errors.cpp - a unit with semantic errors (test 17): cppparse --compile6 must report them, exit non-zero and
// write no object, listing or map (cl: C3861 and C2065, no .obj)
int main() { return undeclared_fn(3) + nosuchvar; }
