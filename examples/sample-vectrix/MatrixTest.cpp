// MatrixTest.cpp - quick sanity checks for Matrix<T, R, C>  (C++98)
#include "Matrix.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

// Element-wise comparison with a tolerance, for floating-point matrices.
template <typename T, int R, int C>
bool approxEqual(const Matrix<T, R, C>& a, const Matrix<T, R, C>& b, T tol) {
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c)
            if (std::fabs(a(r, c) - b(r, c)) > tol) return false;
    return true;
}

int main() {
    typedef Vector<int, 2>    V2;
    typedef Vector<int, 3>    V3;
    typedef Matrix<int, 2, 2> M22;
    typedef Matrix<int, 2, 3> M23;
    typedef Matrix<int, 3, 2> M32;
    typedef Matrix<int, 3, 3> M33;
    typedef Matrix<int, 4, 4> M44;

    // Construction and shape
    M23 m(V3(1, 2, 3),
          V3(4, 5, 6));
    assert(m.rows() == 2 && m.cols() == 3 && m.size() == 6);
    assert(!m.isSquare());
    assert(m(0, 0) == 1 && m(1, 2) == 6);
    assert(m[1][1] == 5);
    assert(m.at(0, 2) == 3);

    // Index chain (Vector[])[]: step 1 gives the row Vector, step 2 the element.
    V3& row1 = m[1];                          // step 1: outer Vector's []
    assert(&row1 == &m.rowsVector()[1]);      // same object as indexing the Vector of rows
    assert(&row1 == &m.row(1));
    int& e12 = row1[2];                       // step 2: row Vector's []
    assert(&e12 == &m[1][2]);                 // chained form
    assert(&e12 == &m(1, 2));                 // m(r, c) is the same chain
    assert(&e12 == &m.at(1, 2));
    e12 = 60;                                 // write through the chain
    assert(m(1, 2) == 60);
    m[1][2] = 6;

    M22 zero;
    assert(zero == M22(V2(0, 0), V2(0, 0)));
    M22 sevens(7);
    assert(sevens == M22(V2(7, 7), V2(7, 7)));

    const int raw[6] = { 1, 2, 3, 4, 5, 6 };
    assert(M23::fromArray(raw) == m);

    // Rows are Vectors, columns come back as Vectors
    assert(m.row(0) == V3(1, 2, 3));
    assert(m.col(1) == V2(2, 5));
    m.setCol(2, V2(30, 60));
    assert(m(0, 2) == 30 && m(1, 2) == 60);
    m.setCol(2, V2(3, 6));

    // Element-wise arithmetic
    M23 n(V3(6, 5, 4),
          V3(3, 2, 1));
    assert((m + n) == M23(V3(7, 7, 7), V3(7, 7, 7)));
    assert((n - m) == M23(V3(5, 3, 1), V3(-1, -3, -5)));
    assert((m * 2) == M23(V3(2, 4, 6), V3(8, 10, 12)));
    assert((2 * m) == (m * 2));
    assert((-m) == M23(V3(-1, -2, -3), V3(-4, -5, -6)));

    // Transpose
    M32 t = m.transpose();
    assert(t == M32(V2(1, 4), V2(2, 5), V2(3, 6)));
    assert(t.transpose() == m);

    // Matrix * vector and vector * matrix
    V3 v(1, 0, 1);
    assert((m * v) == V2(4, 10));
    V2 u(1, 1);
    assert((u * m) == V3(5, 7, 9));

    // Matrix * matrix  (2x3) * (3x2) = (2x2)
    M22 p = m * t;
    assert(p == M22(V2(14, 32), V2(32, 77)));

    // Identity, trace, determinant, sub-matrix
    M33 id = M33::identity();
    assert(id == M33(V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1)));
    assert(id.trace() == 3);
    assert(id.determinant() == 1);
    M33 s(V3(1, 2, 3),
          V3(4, 5, 6),
          V3(7, 8, 10));
    assert(s * id == s);
    assert(s.determinant() == -3);
    assert(submatrix(s, 0, 0) == M22(V2(5, 6), V2(8, 10)));

    const int diag[16] = { 2, 0, 0, 0,
                           0, 3, 0, 0,
                           0, 0, 4, 0,
                           0, 0, 0, 5 };
    M44 q = M44::fromArray(diag);
    assert(q.determinant() == 120);

    // Inverse (floating-point only)
    {
        typedef Vector<double, 2>    V2d;
        typedef Vector<double, 3>    V3d;
        const double tol = 1e-12;

        // 2x2 with a known inverse: [[4,7],[2,6]]^-1 = [[0.6,-0.7],[-0.2,0.4]]
        Mat2d a(V2d(4, 7), V2d(2, 6));
        Mat2d aInv = a.inverse();
        assert(approxEqual(aInv, Mat2d(V2d(0.6, -0.7), V2d(-0.2, 0.4)), tol));
        assert(approxEqual(a * aInv, Mat2d::identity(), tol));
        assert(approxEqual(aInv * a, Mat2d::identity(), tol));

        // 3x3 (det = -3): check A * A^-1 = I both ways
        Mat3d b(V3d(1, 2, 3), V3d(4, 5, 6), V3d(7, 8, 10));
        Mat3d bInv = b.inverse();
        assert(approxEqual(b * bInv, Mat3d::identity(), tol));
        assert(approxEqual(bInv * b, Mat3d::identity(), tol));
        assert(approxEqual(bInv.inverse(), b, 1e-9));
        assert(std::fabs(bInv.determinant() - 1.0 / b.determinant()) < tol);

        // Needs pivoting: zero in the top-left corner
        Mat3d c(V3d(0, 1, 0), V3d(2, 0, 0), V3d(0, 0, 4));
        Mat3d cInv = c.inverse();
        assert(approxEqual(cInv, Mat3d(V3d(0, 0.5, 0), V3d(1, 0, 0), V3d(0, 0, 0.25)), tol));

        // 4x4 diagonal via the cofactor path for determinant, GJ for inverse
        const double diag4[16] = { 2, 0, 0, 0,
                                   0, 4, 0, 0,
                                   0, 0, 8, 0,
                                   0, 0, 0, 16 };
        Mat4d d = Mat4d::fromArray(diag4);
        assert(approxEqual(d * d.inverse(), Mat4d::identity(), tol));

        // Identity is its own inverse
        assert(approxEqual(Mat3d::identity().inverse(), Mat3d::identity(), tol));

        // Singular: invert() reports false, inverse() throws
        Mat2d sing(V2d(1, 2), V2d(2, 4));
        Mat2d unused;
        assert(!sing.invert(unused));
        bool threwSingular = false;
        try { (void)sing.inverse(); } catch (const std::domain_error&) { threwSingular = true; }
        assert(threwSingular);

        // Nearly singular: passes with epsilon 0, rejected with a tolerance
        Mat2d nearly(V2d(1, 2), V2d(2, 4.0 + 1e-15));
        assert(nearly.invert(unused));
        assert(!nearly.invert(unused, 1e-9));
    }

    // Outer product
    assert(outer(V2(1, 2), V3(1, 10, 100)) == M23(V3(1, 10, 100), V3(2, 20, 200)));

    // Iterate over rows
    int total = 0;
    for (M23::const_iterator it = m.begin(); it != m.end(); ++it) total += it->dot(V3(1, 1, 1));
    assert(total == 21);

    // The matrix really is one Vector of row Vectors
    typedef M23::storage_type RowsOfM23;      // Vector<V3, 2>
    RowsOfM23 rv = m.rowsVector();
    assert(rv.size() == 2);
    assert(rv[0] == V3(1, 2, 3) && rv[1] == V3(4, 5, 6));
    rv[1] = V3(0, 0, 1);
    M23 fromRows(rv);
    assert(fromRows.row(0) == m.row(0));
    assert(fromRows(1, 2) == 1 && fromRows(1, 0) == 0);
    assert((m + fromRows).rowsVector() == (m.rowsVector() + rv));

    // Bounds checking
    bool threw = false;
    try { (void)m.at(2, 0); } catch (const std::out_of_range&) { threw = true; }
    assert(threw);

    std::cout << "m =\n" << m << "\nm^T =\n" << t << "\nm * m^T =\n" << p << '\n';
    std::cout << "All Matrix<T, R, C> tests passed.\n";
    return 0;
}
