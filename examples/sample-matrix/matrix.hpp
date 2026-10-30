// matrix.hpp - dense matrices of double: products, transposes, powers, determinants and inverses.
// Compiled by cppparse --compile6 (matrix.cpp, solver.cpp) and checked by
// examples/sample-matrix-driver/driver-matrix.cpp against the oracle files next to it.
// Strict C++98: no C++11 or later features.
#ifndef SAMPLE_MATRIX_HPP_INCLUDED
#define SAMPLE_MATRIX_HPP_INCLUDED

#include <cstddef>
#include <vector>

namespace mx {

class Matrix {
public:
    Matrix();
    Matrix(int rows, int cols, double fill = 0.0);
    static Matrix identity(int n);
    static Matrix fromArray(int rows, int cols, const double* values);

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    double& operator()(int r, int c)       { return data_[index(r, c)]; }
    double  operator()(int r, int c) const { return data_[index(r, c)]; }

    Matrix transpose() const;
    Matrix& operator+=(const Matrix& o);
    Matrix& operator*=(double s);

    double sum() const;
    double maxAbsDiff(const Matrix& o) const;     // largest |a(i,j) - b(i,j)|; sizes must match
    bool   sameSize(const Matrix& o) const { return rows_ == o.rows_ && cols_ == o.cols_; }

private:
    std::size_t index(int r, int c) const { return static_cast<std::size_t>(r) * cols_ + c; }

    int rows_;
    int cols_;
    std::vector<double> data_;
};

Matrix operator+(Matrix a, const Matrix& b);
Matrix operator*(const Matrix& a, const Matrix& b);   // throws std::invalid_argument on a size mismatch
Matrix operator*(Matrix a, double s);
Matrix power(const Matrix& m, int n);                 // m^n by squaring, n >= 0
double determinant(Matrix m);                         // Gaussian elimination with partial pivoting
Matrix inverse(const Matrix& m);                      // Gauss-Jordan; throws std::domain_error if singular

} // namespace mx

#endif // SAMPLE_MATRIX_HPP_INCLUDED
