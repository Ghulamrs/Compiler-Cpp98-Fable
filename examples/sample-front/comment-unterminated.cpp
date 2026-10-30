// comment-unterminated.cpp - must be rejected: a comment that never ends swallowed the rest of the unit silently (cl C1071)
/* unterminated comment
int main() { return 0; }
