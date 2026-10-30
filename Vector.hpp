// Vector.hpp - fixed-size vector template: Vector<T, N>
//
//   T : element type (int, float, double, ...)
//   N : number of elements (compile-time constant, N > 0)
//
// Example:
//   Vector<double, 3> a(1.0, 2.0, 3.0);
//   Vector<double, 3> b(4.0, 5.0, 6.0);
//   Vector<double, 3> c = a + b;      // (5, 7, 9)
//   double d = a.dot(b);              // 32
//
// Header-only. Strict C++98 - no C++11 or later features are used.

#ifndef VECTOR_HPP_INCLUDED
#define VECTOR_HPP_INCLUDED

#include <cmath>
#include <ostream>
#include <stdexcept>

// ---- compile-time assertion for C++98 --------------------------------
// StaticAssert<false> is declared but never defined, so sizeof() on it
// fails to compile. The macro turns that into a harmless anonymous enum,
// usable both at class scope and inside a function body.
namespace vecmath_detail {
    template <bool Condition> struct StaticAssert;
    template <> struct StaticAssert<true> {};
}
#define VECMATH_CONCAT_(a, b) a##b
#define VECMATH_CONCAT(a, b) VECMATH_CONCAT_(a, b)
#define VECMATH_STATIC_ASSERT(cond) \
    enum { VECMATH_CONCAT(vecmath_static_assert_, __LINE__) = sizeof(vecmath_detail::StaticAssert<(cond)>) }

template <typename T, int N>
class Vector {
    VECMATH_STATIC_ASSERT(N > 0);

public:
    typedef T        value_type;
    typedef int      size_type;
    typedef T*       iterator;
    typedef const T* const_iterator;

    // ---- construction -------------------------------------------------

    // All elements zero-initialised.
    Vector() {
        for (int i = 0; i < N; ++i) data_[i] = T();
    }

    // Fill every element with the same value.
    explicit Vector(T value) {
        for (int i = 0; i < N; ++i) data_[i] = value;
    }

    // Component constructors. Each only compiles when N matches
    // (member functions of a class template are checked on first use).
    Vector(T x, T y) {
        VECMATH_STATIC_ASSERT(N == 2);
        data_[0] = x; data_[1] = y;
    }
    Vector(T x, T y, T z) {
        VECMATH_STATIC_ASSERT(N == 3);
        data_[0] = x; data_[1] = y; data_[2] = z;
    }
    Vector(T x, T y, T z, T w) {
        VECMATH_STATIC_ASSERT(N == 4);
        data_[0] = x; data_[1] = y; data_[2] = z; data_[3] = w;
    }

    // Copy N values from a plain array or pointer.
    static Vector fromArray(const T* p) {
        Vector v;
        for (int i = 0; i < N; ++i) v.data_[i] = p[i];
        return v;
    }

    // ---- element access -----------------------------------------------

    // v[i] - the one and only index step of a Vector. No bounds check.
    // When T is itself a Vector (as in Matrix), v[i] yields that inner
    // Vector, and a second [] on it reaches the element: v[i][j].
    T&       operator[](int i)       { return data_[i]; }
    const T& operator[](int i) const { return data_[i]; }

    // Bounds-checked access.
    T& at(int i) {
        if (i < 0 || i >= N) throw std::out_of_range("Vector::at");
        return data_[i];
    }
    const T& at(int i) const {
        if (i < 0 || i >= N) throw std::out_of_range("Vector::at");
        return data_[i];
    }

    // Convenience accessors for small vectors (fail to compile if N is too small).
    T&       x()       { VECMATH_STATIC_ASSERT(N >= 1); return data_[0]; }
    T&       y()       { VECMATH_STATIC_ASSERT(N >= 2); return data_[1]; }
    T&       z()       { VECMATH_STATIC_ASSERT(N >= 3); return data_[2]; }
    T&       w()       { VECMATH_STATIC_ASSERT(N >= 4); return data_[3]; }
    const T& x() const { VECMATH_STATIC_ASSERT(N >= 1); return data_[0]; }
    const T& y() const { VECMATH_STATIC_ASSERT(N >= 2); return data_[1]; }
    const T& z() const { VECMATH_STATIC_ASSERT(N >= 3); return data_[2]; }
    const T& w() const { VECMATH_STATIC_ASSERT(N >= 4); return data_[3]; }

    static int size() { return N; }

    T*       data()       { return data_; }
    const T* data() const { return data_; }

    iterator       begin()       { return data_; }
    iterator       end()         { return data_ + N; }
    const_iterator begin() const { return data_; }
    const_iterator end()   const { return data_ + N; }

    // ---- compound arithmetic -----------------------------------------

    Vector& operator+=(const Vector& o) { for (int i = 0; i < N; ++i) data_[i] += o.data_[i]; return *this; }
    Vector& operator-=(const Vector& o) { for (int i = 0; i < N; ++i) data_[i] -= o.data_[i]; return *this; }
    Vector& operator*=(T s)             { for (int i = 0; i < N; ++i) data_[i] *= s;          return *this; }
    Vector& operator/=(T s)             { for (int i = 0; i < N; ++i) data_[i] /= s;          return *this; }

    Vector operator-() const {
        Vector r;
        for (int i = 0; i < N; ++i) r.data_[i] = -data_[i];
        return r;
    }

    // ---- vector maths -------------------------------------------------

    T dot(const Vector& o) const {
        T sum = T();
        for (int i = 0; i < N; ++i) sum += data_[i] * o.data_[i];
        return sum;
    }

    T lengthSquared() const { return dot(*this); }

    // Length as double (works for integer T as well).
    double length() const { return std::sqrt(static_cast<double>(lengthSquared())); }

    // Unit vector. Only meaningful for floating-point T.
    Vector normalized() const {
        const T len = static_cast<T>(length());
        if (len == T()) throw std::domain_error("Vector::normalized: zero-length vector");
        return *this / len;
    }

    // Cross product, 3-D only.
    Vector cross(const Vector& o) const {
        VECMATH_STATIC_ASSERT(N == 3);
        return Vector(data_[1] * o.data_[2] - data_[2] * o.data_[1],
                      data_[2] * o.data_[0] - data_[0] * o.data_[2],
                      data_[0] * o.data_[1] - data_[1] * o.data_[0]);
    }

    // ---- comparison ---------------------------------------------------

    bool operator==(const Vector& o) const {
        for (int i = 0; i < N; ++i) if (data_[i] != o.data_[i]) return false;
        return true;
    }
    bool operator!=(const Vector& o) const { return !(*this == o); }

private:
    T data_[N];
};

// ---- free operators ---------------------------------------------------

template <typename T, int N>
Vector<T, N> operator+(Vector<T, N> a, const Vector<T, N>& b) { return a += b; }

template <typename T, int N>
Vector<T, N> operator-(Vector<T, N> a, const Vector<T, N>& b) { return a -= b; }

template <typename T, int N>
Vector<T, N> operator*(Vector<T, N> a, T s) { return a *= s; }

template <typename T, int N>
Vector<T, N> operator*(T s, Vector<T, N> a) { return a *= s; }

template <typename T, int N>
Vector<T, N> operator/(Vector<T, N> a, T s) { return a /= s; }

template <typename T, int N>
std::ostream& operator<<(std::ostream& os, const Vector<T, N>& v) {
    os << '(';
    for (int i = 0; i < N; ++i) {
        if (i) os << ", ";
        os << v[i];
    }
    return os << ')';
}

// ---- common aliases ---------------------------------------------------

typedef Vector<int, 2>    Vec2i;
typedef Vector<int, 3>    Vec3i;
typedef Vector<float, 2>  Vec2f;
typedef Vector<float, 3>  Vec3f;
typedef Vector<float, 4>  Vec4f;
typedef Vector<double, 2> Vec2d;
typedef Vector<double, 3> Vec3d;
typedef Vector<double, 4> Vec4d;

#endif // VECTOR_HPP_INCLUDED
