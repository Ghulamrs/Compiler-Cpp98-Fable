// probe-mix.cpp - the middle frame of ehMixed (probe.cpp). eh.ps1 also compiles it with the other exception model
// than probe.cpp, so a throw passes from table-based frames through setjmp frames and back (and the other way).
// Strict C++98.

extern int g_destroyed;
int mixInner(int n);

struct MixGuard {
    int id;
    explicit MixGuard(int i) : id(i) {}
    ~MixGuard() { g_destroyed += id; }
};

int mixMiddle(int n)
{
    MixGuard g(40);
    try {
        return mixInner(n) + 1;
    } catch (int v) {
        return -v;
    }
}
