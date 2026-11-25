// sample-replace (scripts\replace.ps1): a replaceable function and its caller in one unit. The function's code is a
// default: it stands unless another object of the program defines the name, whatever a library holds
extern "C" int puts(const char*);
__declspec(replaceable) int probe_which() { return 1; }
int main() { puts(probe_which() == 1 ? "the default" : "the other definition"); return 0; }
