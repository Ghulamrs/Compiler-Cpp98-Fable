// if-unterminated.cpp - must be rejected: a #if without its #endif (cl C1070); the preprocessor reported it, the
// compile path did not print the preprocessor's diagnostics
#if 1
int main() { return 0; }
