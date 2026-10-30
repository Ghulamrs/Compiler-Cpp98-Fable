// explspec-refused.cpp - must be rejected: an explicit specialisation defined twice (cl C2766); front.cpp explspec:: has
// the valid forms
template <class T> struct Box { int k() { return 0; } };
template <> struct Box<char> { int k() { return 2; } };
template <> struct Box<char>;
template <> struct Box<char> { int k() { return 3; } };
int main() { Box<char> b; return b.k(); }
