// sums.h - sample-dbg: a header with code of its own, an inline function and the members of a class template, so
// that the object's file table names it beside the unit (test 22: the SHA-256 of each source file in a -g object,
// read back from the PDB through dbghelp).
#ifndef SAMPLE_DBG_SUMS_H
#define SAMPLE_DBG_SUMS_H

inline int addUp(const int* values, int count)
{
    int sum = 0;
    for (int i = 0; i < count; ++i) sum += values[i];
    return sum;
}

template <class T> class Pile {
public:
    Pile() : m_held() {}
    void put(const T& value) { m_held = m_held + value; }
    const T& held() const { return m_held; }

private:
    T m_held;
};

#endif
