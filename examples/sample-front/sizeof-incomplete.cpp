// sizeof-incomplete.cpp - sizeof of incomplete types (test 17): cppparse --compile6 must reject every one, as cl
// does (C2027, C2070), instead of taking 8 bytes; and must write no object
#include <iosfwd>

struct Inc;
int declaredOnly() { return (int)sizeof(Inc); }                    // only declared
int libraryOnly() { return (int)sizeof(std::stringstream); }       // <iosfwd> declares the template, no definition
struct Self { enum { N = sizeof(Self) }; int x; };                  // its own members are still being read
int ofVoid() { return (int)sizeof(void); }

int main() { return declaredOnly() + libraryOnly() + ofVoid() + Self::N; }
