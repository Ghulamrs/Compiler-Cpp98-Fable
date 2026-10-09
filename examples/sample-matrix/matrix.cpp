// matrix.cpp - mx::Matrix and the matrix checks the driver calls (mtx*). Every check returns a plain
// long or double so that no class crosses the boundary between cppparse's objects and the driver.
// Strict C++98: no C++11 or later features.
#include "matrix.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace mx {

Matrix::Matrix() : rows_(0), cols_(0), data_() {}

Matrix::Matrix(int rows, int cols, double fill)
    : rows_(rows), cols_(cols), data_()
{
    if (rows < 0 || cols < 0) throw std::invalid_argument("Matrix: negative size");
    data_.assign(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols), fill);
}

Matrix Matrix::identity(int n)
{
    Matrix m(n, n);
    for (int i = 0; i < n; ++i) m(i, i) = 1.0;
    return m;
}

Matrix Matrix::fromArray(int rows, int cols, const double* values)
{
    Matrix m(rows, cols);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            m(r, c) = values[r * cols + c];
    return m;
}

Matrix Matrix::transpose() const
{
    Matrix t(cols_, rows_);
    for (int r = 0; r < rows_; ++r)
        for (int c = 0; c < cols_; ++c)
            t(c, r) = (*this)(r, c);
    return t;
}

Matrix& Matrix::operator+=(const Matrix& o)
{
    if (!sameSize(o)) throw std::invalid_argument("Matrix::operator+=: size mismatch");
    for (std::size_t i = 0; i < data_.size(); ++i) data_[i] += o.data_[i];
    return *this;
}

Matrix& Matrix::operator*=(double s)
{
    for (std::vector<double>::iterator it = data_.begin(); it != data_.end(); ++it) *it *= s;
    return *this;
}

double Matrix::sum() const
{
    double s = 0.0;
    for (std::vector<double>::const_iterator it = data_.begin(); it != data_.end(); ++it) s += *it;
    return s;
}

double Matrix::maxAbsDiff(const Matrix& o) const
{
    if (!sameSize(o)) throw std::invalid_argument("Matrix::maxAbsDiff: size mismatch");
    double worst = 0.0;
    for (std::size_t i = 0; i < data_.size(); ++i) worst = std::max(worst, std::fabs(data_[i] - o.data_[i]));
    return worst;
}

Matrix operator+(Matrix a, const Matrix& b) { return a += b; }

Matrix operator*(Matrix a, double s) { return a *= s; }

Matrix operator*(const Matrix& a, const Matrix& b)
{
    if (a.cols() != b.rows()) throw std::invalid_argument("Matrix product: inner dimensions differ");
    Matrix p(a.rows(), b.cols());
    for (int i = 0; i < a.rows(); ++i)
        for (int k = 0; k < a.cols(); ++k) {
            const double aik = a(i, k);
            for (int j = 0; j < b.cols(); ++j) p(i, j) += aik * b(k, j);
        }
    return p;
}

Matrix power(const Matrix& m, int n)
{
    if (m.rows() != m.cols()) throw std::invalid_argument("power: matrix is not square");
    if (n < 0) throw std::invalid_argument("power: negative exponent");
    Matrix result = Matrix::identity(m.rows());
    Matrix base = m;
    while (n > 0) {
        if (n & 1) result = result * base;
        base = base * base;
        n >>= 1;
    }
    return result;
}

namespace {

template <typename T>
void swapRows(Matrix& m, int a, int b, T& sign)
{
    if (a == b) return;
    for (int c = 0; c < m.cols(); ++c) std::swap(m(a, c), m(b, c));
    sign = -sign;
}

int pivotRow(const Matrix& m, int col)       // row at or below col with the largest |m(row, col)|
{
    int best = col;
    for (int r = col + 1; r < m.rows(); ++r)
        if (std::fabs(m(r, col)) > std::fabs(m(best, col))) best = r;
    return best;
}

const double kSingular = 1e-12;

} // namespace

double determinant(Matrix m)
{
    if (m.rows() != m.cols()) throw std::invalid_argument("determinant: matrix is not square");
    const int n = m.rows();
    double sign = 1.0;
    double det = 1.0;
    for (int k = 0; k < n; ++k) {
        swapRows(m, k, pivotRow(m, k), sign);
        const double pivot = m(k, k);
        if (std::fabs(pivot) < kSingular) return 0.0;
        det *= pivot;
        for (int r = k + 1; r < n; ++r) {
            const double f = m(r, k) / pivot;
            for (int c = k; c < n; ++c) m(r, c) -= f * m(k, c);
        }
    }
    return sign * det;
}

Matrix inverse(const Matrix& m)
{
    if (m.rows() != m.cols()) throw std::invalid_argument("inverse: matrix is not square");
    const int n = m.rows();
    Matrix a = m;
    Matrix inv = Matrix::identity(n);
    double unusedSign = 1.0;
    for (int k = 0; k < n; ++k) {
        const int p = pivotRow(a, k);
        swapRows(a, k, p, unusedSign);
        swapRows(inv, k, p, unusedSign);
        const double pivot = a(k, k);
        if (std::fabs(pivot) < kSingular) throw std::domain_error("inverse: matrix is singular");
        for (int c = 0; c < n; ++c) { a(k, c) /= pivot; inv(k, c) /= pivot; }
        for (int r = 0; r < n; ++r) {
            if (r == k) continue;
            const double f = a(r, k);
            if (f == 0.0) continue;
            for (int c = 0; c < n; ++c) { a(r, c) -= f * a(k, c); inv(r, c) -= f * inv(k, c); }
        }
    }
    return inv;
}

} // namespace mx

// ---- checks called by the driver ------------------------------------------------------------------

namespace {

double roundToLong(double v) { return std::floor(v + 0.5); }

mx::Matrix diagonallyDominant(int n)          // a(i,i) = n + 2, a(i,j) = 1 / (i + j + 1) elsewhere
{
    mx::Matrix a(n, n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            a(i, j) = (i == j) ? n + 2.0 : 1.0 / (i + j + 1);
    return a;
}

mx::Matrix patterned(int n, int mul1, int mul2, int mod, int shift)
{
    mx::Matrix a(n, n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            a(i, j) = static_cast<double>((i * mul1 + j * mul2) % mod - shift);
    return a;
}

} // namespace

double mtxMultiplyChecksum()                  // [[1,2,3],[4,5,6]] x [[7,8],[9,10],[11,12]] = [[58,64],[139,154]]
{
    const double a[] = { 1, 2, 3, 4, 5, 6 };
    const double b[] = { 7, 8, 9, 10, 11, 12 };
    mx::Matrix p = mx::Matrix::fromArray(2, 3, a) * mx::Matrix::fromArray(3, 2, b);
    return p(0, 0) + 10 * p(0, 1) + 100 * p(1, 0) + 1000 * p(1, 1);
}

long mtxIdentityHolds()
{
    mx::Matrix a = patterned(5, 3, 7, 9, 4);
    mx::Matrix i = mx::Matrix::identity(5);
    return (a * i).maxAbsDiff(a) == 0.0 && (i * a).maxAbsDiff(a) == 0.0 ? 1 : 0;
}

long mtxAssociative()                         // integer entries: (AB)C and A(BC) agree exactly
{
    mx::Matrix a = patterned(6, 2, 5, 7, 3);
    mx::Matrix b = patterned(6, 3, 1, 5, 2);
    mx::Matrix c = patterned(6, 4, 3, 11, 5);
    return ((a * b) * c).maxAbsDiff(a * (b * c)) == 0.0 ? 1 : 0;
}

long mtxTransposeRule()                       // (AB)^T = B^T A^T, also for non-square shapes
{
    mx::Matrix a(3, 4), b(4, 2);
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) a(r, c) = r * 4 + c - 5;
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 2; ++c) b(r, c) = (r + 1) * (c + 2);
    return (a * b).transpose().maxAbsDiff(b.transpose() * a.transpose()) == 0.0 ? 1 : 0;
}

double mtxDeterminant3()                      // [[2,-3,1],[2,0,-1],[1,4,5]] has determinant 49
{
    const double v[] = { 2, -3, 1, 2, 0, -1, 1, 4, 5 };
    return mx::determinant(mx::Matrix::fromArray(3, 3, v));
}

double mtxInverse2Weighted()                  // inverse of [[4,7],[2,6]] is [[0.6,-0.7],[-0.2,0.4]]
{
    const double v[] = { 4, 7, 2, 6 };
    mx::Matrix inv = mx::inverse(mx::Matrix::fromArray(2, 2, v));
    return inv(0, 0) + 10 * inv(0, 1) + 100 * inv(1, 0) + 1000 * inv(1, 1);
}

long mtxInverseExact()                        // unimodular: the inverse of [[2,3,1],[1,2,1],[1,1,1]] is
{                                             // [[1,-2,1],[0,1,-1],[-1,1,1]]; returns its digits as a checksum
    const double v[] = { 2, 3, 1, 1, 2, 1, 1, 1, 1 };
    mx::Matrix inv = mx::inverse(mx::Matrix::fromArray(3, 3, v));
    long checksum = 0;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            checksum = checksum * 10 + static_cast<long>(roundToLong(inv(r, c))) + 2;   // digits 0..4: 303231133
    return checksum;
}

double mtxRoundTripError(int n)               // max |A * inverse(A) - I| for a well-conditioned n x n matrix
{
    mx::Matrix a = diagonallyDominant(n);
    return (a * mx::inverse(a)).maxAbsDiff(mx::Matrix::identity(n));
}

double mtxInverseOfInverseError()
{
    mx::Matrix a = diagonallyDominant(4);
    a(0, 3) = -2.5;
    a(3, 1) = 1.75;
    return mx::inverse(mx::inverse(a)).maxAbsDiff(a);
}

long mtxPivotNeeded()                         // a zero on the diagonal: only partial pivoting finds the inverse
{
    const double swap2[] = { 0, 1, 1, 0 };
    const double v[] = { 0, 2, 1, 1, 1, 0, 3, 0, 1 };
    mx::Matrix s = mx::Matrix::fromArray(2, 2, swap2);
    mx::Matrix a = mx::Matrix::fromArray(3, 3, v);
    long ok = 0;
    if (mx::inverse(s).maxAbsDiff(s) == 0.0) ok += 1;
    if ((a * mx::inverse(a)).maxAbsDiff(mx::Matrix::identity(3)) < 1e-12) ok += 10;
    if (roundToLong(mx::determinant(a)) == -5.0) ok += 100;     // 0*(1-0) - 2*(1-0) + 1*(0-3) = -5
    return ok;                                                  // 111 when all three hold
}

long mtxSingularThrows()
{
    const double v[] = { 1, 2, 2, 4 };
    try {
        mx::inverse(mx::Matrix::fromArray(2, 2, v));
    } catch (const std::domain_error& e) {
        return std::string(e.what()).find("singular") != std::string::npos ? 1 : 2;
    } catch (...) {
        return 3;
    }
    return 0;
}

long mtxSizeMismatchThrows()
{
    try {
        mx::Matrix p = mx::Matrix(2, 3, 1.0) * mx::Matrix(2, 3, 1.0);
        return static_cast<long>(p.rows());
    } catch (const std::invalid_argument&) {
        return 1;
    }
}

long mtxFibonacci(int n)                      // [[1,1],[1,0]]^n holds F(n) at (0,1)
{
    const double q[] = { 1, 1, 1, 0 };
    return static_cast<long>(mx::power(mx::Matrix::fromArray(2, 2, q), n)(0, 1));
}

double mtxLargeProductChecksum()              // 24 x 24 integer product, weighted by (i+1)(j+1): exact in double
{
    mx::Matrix p = patterned(24, 7, 3, 11, 5) * patterned(24, 5, 2, 13, 6);
    double s = 0.0;
    for (int i = 0; i < 24; ++i)
        for (int j = 0; j < 24; ++j)
            s += p(i, j) * (i + 1) * (j + 1);
    return s;
}

double mtxScaledSum()                         // (2A + I) summed: exercises operator+, operator*(double)
{
    mx::Matrix a = patterned(4, 1, 2, 5, 2);
    return (a * 2.0 + mx::Matrix::identity(4)).sum();
}
