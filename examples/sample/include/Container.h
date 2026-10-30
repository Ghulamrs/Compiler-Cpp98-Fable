// Container.h - templates for the sample project.
// Exercises: class template with default argument, out-of-line template
// member definitions, nested class inside a template, explicit and partial
// specializations, function template, template base class, throw in a
// template body, nested template arguments "> >".
#ifndef SAMPLE_CONTAINER_H
#define SAMPLE_CONTAINER_H

#include <stdexcept>
#include <cstddef>
#include <vector>

namespace util {

template <typename T, int N = 16>
class FixedStack {
public:
    typedef T value_type;

    FixedStack() : m_size(0) {}
    void push(const T& v);
    T    pop();
    bool empty() const { return m_size == 0; }
    int  size() const { return m_size; }

    class Iterator {
    public:
        explicit Iterator(const FixedStack* owner, int pos) : m_owner(owner), m_pos(pos) {}
        const T&  operator*() const { return m_owner->m_items[m_pos]; }
        Iterator& operator++() { ++m_pos; return *this; }
        bool      operator!=(const Iterator& other) const { return m_pos != other.m_pos; }
    private:
        const FixedStack* m_owner;
        int               m_pos;
    };

    Iterator begin() const { return Iterator(this, 0); }
    Iterator end() const { return Iterator(this, m_size); }

private:
    T   m_items[N];
    int m_size;
};

template <typename T, int N>
void FixedStack<T, N>::push(const T& v)
{
    if (m_size >= N) throw std::overflow_error("FixedStack is full");
    m_items[m_size++] = v;
}

template <typename T, int N>
T FixedStack<T, N>::pop()
{
    if (empty()) throw std::underflow_error("FixedStack is empty");
    T result = m_items[--m_size];
    return result;
}

template <>
class FixedStack<bool, 8> {
public:
    FixedStack() : m_bits(0), m_size(0) {}
    void push(bool v) { if (v) m_bits |= (1u << m_size); ++m_size; }
    bool pop() { --m_size; return (m_bits >> m_size) & 1u; }
private:
    unsigned m_bits;
    int      m_size;
};

template <typename T>
class FixedStack<T*, 4> {
public:
    FixedStack() : m_size(0) {}
    void push(T* p) { m_items[m_size++] = p; }
private:
    T*  m_items[4];
    int m_size;
};

template <typename T>
const T& maxOf(const T& a, const T& b)
{
    return a < b ? b : a;
}

template <typename T, int N>
class Matrix2 : public FixedStack<T, N * N> {
public:
    std::vector<std::vector<T> > rows() const;     // declared only: reported by the health check
};

} // namespace util

#endif // SAMPLE_CONTAINER_H
