// sample-replace (scripts\replace.ps1): the caller alone; the function is default.cpp's, replaceable there, which this
// unit cannot know. That is how every unit of a program names libcpp's operator new
extern "C" int puts(const char*);
int probe_which();
int main() { puts(probe_which() == 1 ? "the default" : "the other definition"); return 0; }
