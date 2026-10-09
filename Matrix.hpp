// Matrix.hpp - fixed-size matrix template built on Vector<T, N>: Matrix<T, R, C>
//
//   T : element type (int, float, double, ...)
//   R : number of rows    (compile-time constant, R > 0)
//   C : number of columns (compile-time constant, C > 0)
//
// A Matrix<T, R, C> is a Vector of Vectors: one Vector<row_type, R> object
// whose R elements are the rows, each a Vector<T, C>. The outer Vector is
// the second dimension, so element-wise arithmetic, negation, comparison
// and iteration all come straight from Vector with no extra loops here.
//
//     Matrix<T, R, C>
//       +-- Vector< Vector<T, C>, R >  rows_      (R rows)
//             +-- Vector<T, C>                     (C elements each)
//
// Example:
//   Matrix<double, 2, 3> m(Vector<double, 3>(1, 2, 3),
//                          Vector<double, 3>(4, 5, 6));
//   Vector<double, 3> v(1, 0, 1);
//   Vector<double, 2> r = m * v;                  // (4, 10)
//   Matrix<double, 3, 2> t = m.transpose();
//   Matrix<double, 3, 3> id = Matrix<double, 3, 3>::identity();
//
// Header-only. Strict C++98 - no C++11 or later features are used.

#ifndef MATRIX_HPP_INCLUDED
#define MATRIX_HPP_INCLUDED

#include "Vector.hpp"

#include <ostream>
#include <stdexcept>

template <typename T, int R, int C> class Matrix;

// Sub-matrix with row r and column c removed.
template <typename T, int R, int C>
Matrix<T, R - 1, C - 1> submatrix(const Matrix<T, R, C>& m, int r, int c);

// ---- small helpers (C++98 has no <type_traits>) ----------------------
namespace vecmath_detail {
    // True for float, double and long double; used to restrict inverse().
    template <typename T> struct IsFloatingPoint            { enum { value = 0 }; };
    template <>           struct IsFloatingPoint<float>       { enum { value = 1 }; };
    template <>           struct IsFloatingPoint<double>      { enum { value = 1 }; };
    template <>           struct IsFloatingPoint<long double> { enum { value = 1 }; };

    template <typename T>
    T absValue(T v) { return v < T() ? -v : v; }
}

// ---- determinant helper -----------------------------------------------
// Closed forms for 1x1..3x3; cofactor expansion along row 0 above that.
// A struct template is used because C++98 has no "if constexpr".
namespace vecmath_detail {
    template <typename T, int N>
    struct Determinant {
        static T compute(const Matrix<T, N, N>& m) {
            T det  = T();
            T sign = T(1);
            for (int c = 0; c < N; ++c) {
                det += sign * m(0, c) * Determinant<T, N - 1>::compute(submatrix(m, 0, c));
                sign = -sign;
            }
            return det;
        }
    };
    template <typename T>
    struct Determinant<T, 1> {
        static T compute(const Matrix<T, 1, 1>& m) { return m(0, 0); }
    };
    template <typename T>
    struct Determinant<T, 2> {
        static T compute(const Matrix<T, 2, 2>& m) {
            return m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0);
        }
    };
    template <typename T>
    struct Determinant<T, 3> {
        static T compute(const Matrix<T, 3, 3>& m) {
            return m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1))
                 - m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0))
                 + m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
        }
    };
}

template <typename T, int R, int C>
class Matrix {
    VECMATH_STATIC_ASSERT(R > 0);
    VECMATH_STATIC_ASSERT(C > 0);

public:
    typedef T                   value_type;
    typedef Vector<T, C>        row_type;      // one row: C elements
    typedef Vector<T, R>        col_type;      // one column: R elements
    typedef Vector<row_type, R> storage_type;  // the Vector of rows (2nd dimension)

    // ---- construction -------------------------------------------------

    // All elements zero-initialised (the outer Vector default-constructs
    // every row, and each row Vector zeroes its elements).
    Matrix() {}

    // Fill every element with the same value: the outer Vector's fill
    // constructor copies one filled row into every slot.
    explicit Matrix(T value) : rows_(row_type(value)) {}

    // Build directly from a Vector of rows.
    explicit Matrix(const storage_type& rows) : rows_(rows) {}

    // Row constructors. Each only compiles when R matches.
    // (*this)[k] is step 1 of the index chain: the outer Vector's [].
    Matrix(const row_type& r0, const row_type& r1) {
        VECMATH_STATIC_ASSERT(R == 2);
        (*this)[0] = r0; (*this)[1] = r1;
    }
    Matrix(const row_type& r0, const row_type& r1, const row_type& r2) {
        VECMATH_STATIC_ASSERT(R == 3);
        (*this)[0] = r0; (*this)[1] = r1; (*this)[2] = r2;
    }
    Matrix(const row_type& r0, const row_type& r1, const row_type& r2, const row_type& r3) {
        VECMATH_STATIC_ASSERT(R == 4);
        (*this)[0] = r0; (*this)[1] = r1; (*this)[2] = r2; (*this)[3] = r3;
    }

    // Copy R*C values, row-major, from a plain array or pointer.
    static Matrix fromArray(const T* p) {
        Matrix m;
        for (int r = 0; r < R; ++r)
            for (int c = 0; c < C; ++c)
                m[r][c] = p[r * C + c];       // (Vector[])[]
        return m;
    }

    // Identity matrix (square only).
    static Matrix identity() {
        VECMATH_STATIC_ASSERT(R == C);
        Matrix m;
        for (int i = 0; i < R; ++i) m[i][i] = T(1);
        return m;
    }

    // ---- shape --------------------------------------------------------

    static int  rows()     { return R; }
    static int  cols()     { return C; }
    static int  size()     { return R * C; }
    static bool isSquare() { return R == C; }

    // ---- element access: (Vector[])[] ---------------------------------
    //
    // Indexing a Matrix is two Vector index steps chained together:
    //
    //   step 1   m[r]      Matrix::operator[]  ->  rows_[r]   outer Vector's []  ->  row_type&
    //   step 2   m[r][c]   row_type::operator[] on that row    inner Vector's []  ->  T&
    //
    // m(r, c) is only a shorthand for the same chain, m[r][c].

    // Step 1: m[r] is the outer Vector's [] applied to the Vector of rows.
    row_type&       operator[](int r)       { return rows_[r]; }
    const row_type& operator[](int r) const { return rows_[r]; }

    // Step 2: m(r, c) = (m[r])[c], the row Vector's [] applied to the row.
    T&       operator()(int r, int c)       { return (*this)[r][c]; }
    const T& operator()(int r, int c) const { return (*this)[r][c]; }

    // Bounds-checked access, then the same two steps.
    T& at(int r, int c) {
        if (r < 0 || r >= R || c < 0 || c >= C) throw std::out_of_range("Matrix::at");
        return (*this)[r][c];
    }
    const T& at(int r, int c) const {
        if (r < 0 || r >= R || c < 0 || c >= C) throw std::out_of_range("Matrix::at");
        return (*this)[r][c];
    }

    // Row r as a Vector<T, C> (reference, so it can be assigned to). Same as m[r].
    row_type&       row(int r)       { return (*this)[r]; }
    const row_type& row(int r) const { return (*this)[r]; }

    // The whole matrix as its underlying Vector of rows.
    storage_type&       rowsVector()       { return rows_; }
    const storage_type& rowsVector() const { return rows_; }

    // Column c as a Vector<T, R> (a copy, because columns are not contiguous).
    col_type col(int c) const {
        col_type v;
        for (int r = 0; r < R; ++r) v[r] = (*this)[r][c];
        return v;
    }
    void setCol(int c, const col_type& v) {
        for (int r = 0; r < R; ++r) (*this)[r][c] = v[r];
    }

    // Iterate over rows (the outer Vector's iterators).
    typedef typename storage_type::iterator       iterator;
    typedef typename storage_type::const_iterator const_iterator;
    iterator       begin()       { return rows_.begin(); }
    iterator       end()         { return rows_.end(); }
    const_iterator begin() const { return rows_.begin(); }
    const_iterator end()   const { return rows_.end(); }

    // ---- compound arithmetic (delegates to the Vector of rows) --------

    // Vector<row_type, R> += Vector<row_type, R> adds row by row, and each
    // row += row adds element by element. Same for -=.
    Matrix& operator+=(const Matrix& o) { rows_ += o.rows_; return *this; }
    Matrix& operator-=(const Matrix& o) { rows_ -= o.rows_; return *this; }

    // Scalar scaling: the outer Vector's *= expects a row_type, so scale
    // each row Vector by the scalar here.
    Matrix& operator*=(T s) { for (int r = 0; r < R; ++r) (*this)[r] *= s; return *this; }
    Matrix& operator/=(T s) { for (int r = 0; r < R; ++r) (*this)[r] /= s; return *this; }

    // Negating the outer Vector negates each row Vector.
    Matrix operator-() const { return Matrix(-rows_); }

    // ---- matrix maths -------------------------------------------------

    Matrix<T, C, R> transpose() const {
        Matrix<T, C, R> t;
        for (int r = 0; r < R; ++r)
            for (int c = 0; c < C; ++c)
                t[c][r] = (*this)[r][c];
        return t;
    }

    // Trace (square only).
    T trace() const {
        VECMATH_STATIC_ASSERT(R == C);
        T sum = T();
        for (int i = 0; i < R; ++i) sum += (*this)[i][i];
        return sum;
    }

    // Determinant (square only).
    T determinant() const {
        VECMATH_STATIC_ASSERT(R == C);
        return vecmath_detail::Determinant<T, R>::compute(*this);
    }

    // ---- linear systems  A X = B  (square, floating-point T only) ------
    //
    // Gauss-Jordan elimination with partial pivoting on the augmented
    // system [A | B]. When it finishes the left half is the identity and
    // the right half is X. Every row operation is a Vector operation on a
    // whole row, which is where the Vector-of-Vectors layout pays off.
    //
    // B may have any number of columns K, so one call solves K systems that
    // share the same A. The inverse is the special case B = I.
    //
    // trySolve() writes X and returns true, or returns false (X unspecified)
    // when a pivot is no larger than epsilon, i.e. A is singular to within
    // that tolerance. solve() throws std::domain_error instead.

    template <int K>
    bool trySolve(const Matrix<T, R, K>& b, Matrix<T, R, K>& x, T epsilon = T()) const {
        VECMATH_STATIC_ASSERT(R == C);
        VECMATH_STATIC_ASSERT(vecmath_detail::IsFloatingPoint<T>::value != 0);
        typedef Vector<T, K> rhs_row_type;

        Matrix a(*this);          // left half  [A | .]
        x = b;                    // right half [. | B]

        for (int col = 0; col < R; ++col) {
            // Partial pivoting: pick the row with the largest |a[r][col]|.
            int pivotRow = col;
            T   best     = vecmath_detail::absValue(a[col][col]);
            for (int r = col + 1; r < R; ++r) {
                const T v = vecmath_detail::absValue(a[r][col]);
                if (v > best) { best = v; pivotRow = r; }
            }
            if (best <= epsilon) return false;   // singular

            if (pivotRow != col) {
                const row_type     tmpA = a[col]; a[col] = a[pivotRow]; a[pivotRow] = tmpA;
                const rhs_row_type tmpX = x[col]; x[col] = x[pivotRow]; x[pivotRow] = tmpX;
            }

            // Scale the pivot row so the pivot becomes 1.
            const T scale = T(1) / a[col][col];
            a[col] *= scale;
            x[col] *= scale;

            // Eliminate the pivot column from every other row.
            for (int r = 0; r < R; ++r) {
                if (r == col) continue;
                const T factor = a[r][col];
                if (factor == T()) continue;
                a[r] -= a[col] * factor;
                x[r] -= x[col] * factor;
            }
        }
        return true;
    }

    // Single right-hand side: A x = b with b and x as column Vectors.
    bool trySolve(const col_type& b, col_type& x, T epsilon = T()) const {
        Matrix<T, R, 1> bCol, xCol;
        for (int r = 0; r < R; ++r) bCol[r][0] = b[r];
        if (!trySolve(bCol, xCol, epsilon)) return false;
        for (int r = 0; r < R; ++r) x[r] = xCol[r][0];
        return true;
    }

    // Throwing versions.
    col_type solve(const col_type& b, T epsilon = T()) const {
        col_type x;
        if (!trySolve(b, x, epsilon)) throw std::domain_error("Matrix::solve: singular matrix");
        return x;
    }
    template <int K>
    Matrix<T, R, K> solve(const Matrix<T, R, K>& b, T epsilon = T()) const {
        Matrix<T, R, K> x;
        if (!trySolve(b, x, epsilon)) throw std::domain_error("Matrix::solve: singular matrix");
        return x;
    }

    // ---- inverse: the special case A X = I -----------------------------

    bool invert(Matrix& out, T epsilon = T()) const {
        return trySolve(identity(), out, epsilon);
    }

    Matrix inverse(T epsilon = T()) const {
        Matrix out;
        if (!invert(out, epsilon)) throw std::domain_error("Matrix::inverse: singular matrix");
        return out;
    }

    // ---- comparison ---------------------------------------------------

    // Vector<row_type, R> == compares row Vectors, which compare elements.
    bool operator==(const Matrix& o) const { return rows_ == o.rows_; }
    bool operator!=(const Matrix& o) const { return rows_ != o.rows_; }

private:
    storage_type rows_;   // ONE Vector object holding all R rows: the 2nd dimension
};

// ---- sub-matrix -------------------------------------------------------

template <typename T, int R, int C>
Matrix<T, R - 1, C - 1> submatrix(const Matrix<T, R, C>& m, int r, int c) {
    Matrix<T, R - 1, C - 1> out;
    int outR = 0;
    for (int i = 0; i < R; ++i) {
        if (i == r) continue;
        int outC = 0;
        for (int j = 0; j < C; ++j) {
            if (j == c) continue;
            out(outR, outC++) = m(i, j);
        }
        ++outR;
    }
    return out;
}

// ---- free operators ---------------------------------------------------

template <typename T, int R, int C>
Matrix<T, R, C> operator+(Matrix<T, R, C> a, const Matrix<T, R, C>& b) { return a += b; }

template <typename T, int R, int C>
Matrix<T, R, C> operator-(Matrix<T, R, C> a, const Matrix<T, R, C>& b) { return a -= b; }

template <typename T, int R, int C>
Matrix<T, R, C> operator*(Matrix<T, R, C> a, T s) { return a *= s; }

template <typename T, int R, int C>
Matrix<T, R, C> operator*(T s, Matrix<T, R, C> a) { return a *= s; }

template <typename T, int R, int C>
Matrix<T, R, C> operator/(Matrix<T, R, C> a, T s) { return a /= s; }

// Matrix * column vector  ->  column vector      (R x C) * (C) = (R)
template <typename T, int R, int C>
Vector<T, R> operator*(const Matrix<T, R, C>& m, const Vector<T, C>& v) {
    Vector<T, R> out;
    for (int r = 0; r < R; ++r) out[r] = m.row(r).dot(v);
    return out;
}

// row vector * Matrix  ->  row vector            (R) * (R x C) = (C)
template <typename T, int R, int C>
Vector<T, C> operator*(const Vector<T, R>& v, const Matrix<T, R, C>& m) {
    Vector<T, C> out;
    for (int r = 0; r < R; ++r) out += m.row(r) * v[r];
    return out;
}

// Matrix * Matrix                                 (R x K) * (K x C) = (R x C)
template <typename T, int R, int K, int C>
Matrix<T, R, C> operator*(const Matrix<T, R, K>& a, const Matrix<T, K, C>& b) {
    Matrix<T, R, C> out;
    for (int r = 0; r < R; ++r)
        for (int k = 0; k < K; ++k)
            out.row(r) += b.row(k) * a(r, k);   // row r of result = sum_k a(r,k) * row k of b
    return out;
}

// Outer product of two vectors                    (R) x (C) = (R x C)
template <typename T, int R, int C>
Matrix<T, R, C> outer(const Vector<T, R>& a, const Vector<T, C>& b) {
    Matrix<T, R, C> out;
    for (int r = 0; r < R; ++r) out.row(r) = b * a[r];
    return out;
}

template <typename T, int R, int C>
std::ostream& operator<<(std::ostream& os, const Matrix<T, R, C>& m) {
    os << '[';
    for (int r = 0; r < R; ++r) {
        if (r) os << ",\n ";
        os << m.row(r);
    }
    return os << ']';
}

// ---- common aliases ---------------------------------------------------

typedef Matrix<int, 2, 2>    Mat2i;
typedef Matrix<int, 3, 3>    Mat3i;
typedef Matrix<float, 2, 2>  Mat2f;
typedef Matrix<float, 3, 3>  Mat3f;
typedef Matrix<float, 4, 4>  Mat4f;
typedef Matrix<double, 2, 2> Mat2d;
typedef Matrix<double, 3, 3> Mat3d;
typedef Matrix<double, 4, 4> Mat4d;

#endif // MATRIX_HPP_INCLUDED
