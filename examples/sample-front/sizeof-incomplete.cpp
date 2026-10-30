// sizeof-incomplete.cpp - sizeof of incomplete types (test 17): cppparse --compile6 must reject every one, as cl
// does (C2027, C2070), instead of taking 8 bytes; and must write no object
namespace std { template <class T> class basic_declared_only; }   // a library template declared, no definition (<iosfwd>
                                                               // declared the streams so before C3; it defines them now)

struct Inc;
int declaredOnly() { return (int)sizeof(Inc); }                    // only declared
int libraryOnly() { return (int)sizeof(std::basic_declared_only<char>); }
struct Self { enum { N = sizeof(Self) }; int x; };                  // its own members are still being read
int ofVoid() { return (int)sizeof(void); }

int main() { return declaredOnly() + libraryOnly() + ofVoid() + Self::N; }
