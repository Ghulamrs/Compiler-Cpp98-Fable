// VectorTest.cpp - quick sanity checks for Vector<T, N>  (C++98)
#include "Vector.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    // typedefs keep commas out of assert() arguments
    typedef Vector<int, 3>    V3;
    typedef Vector<double, 2> V2d;

    // Construction
    V3 a(1, 2, 3);
    V3 b(4, 5, 6);
    V3 zero;
    V3 fives(5);
    assert(zero == V3(0, 0, 0));
    assert(fives == V3(5, 5, 5));
    assert(a.size() == 3);

    const int raw[3] = { 7, 8, 9 };
    assert(V3::fromArray(raw) == V3(7, 8, 9));

    // Arithmetic
    assert((a + b) == V3(5, 7, 9));
    assert((b - a) == V3(3, 3, 3));
    assert((a * 2) == V3(2, 4, 6));
    assert((2 * a) == V3(2, 4, 6));
    assert((b / 2) == V3(2, 2, 3));
    assert((-a) == V3(-1, -2, -3));

    // Dot / cross / length
    assert(a.dot(b) == 32);
    assert(a.cross(b) == V3(-3, 6, -3));
    assert(a.lengthSquared() == 14);
    assert(std::fabs(V2d(3.0, 4.0).length() - 5.0) < 1e-12);

    // Accessors
    assert(a.x() == 1 && a.y() == 2 && a.z() == 3);
    a[0] = 10;
    assert(a.at(0) == 10);

    // Normalisation
    Vec3d n = Vec3d(0.0, 3.0, 4.0).normalized();
    assert(std::fabs(n.length() - 1.0) < 1e-12);
    assert(std::fabs(n.y() - 0.6) < 1e-12);

    // Iteration and streaming
    int sum = 0;
    for (V3::const_iterator it = b.begin(); it != b.end(); ++it) sum += *it;
    assert(sum == 15);

    // Bounds checking
    bool threw = false;
    try { (void)a.at(3); } catch (const std::out_of_range&) { threw = true; }
    assert(threw);

    std::cout << "a = " << a << ", b = " << b << ", a x b = " << a.cross(b) << '\n';
    std::cout << "All Vector<T, N> tests passed.\n";
    return 0;
}
