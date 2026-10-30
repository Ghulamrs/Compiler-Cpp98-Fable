// solver.cpp - linear equation solvers on the fixed-size Vector<T, N> of cpp/Vector.hpp: Gaussian
// elimination with partial pivoting, Cramer's rule through cross and dot products, and Jacobi
// iteration, cross-checked against mx::inverse from matrix.cpp. The checks the driver calls are lin*.
// A system of N equations is N row vectors: Vector<Vector<double, N>, N>.
// Strict C++98: no C++11 or later features.
#include "Vector.hpp"
#include "matrix.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace lin {

template <int N>
struct System {
    typedef Vector<double, N> Row;
    typedef Vector<Row, N>    Rows;
    Rows a;                                     // a[i] is the coefficient row of equation i
    Row  b;                                     // right-hand sides
};

template <int N>
System<N> makeSystem(const double (&coefficients)[N][N], const double (&rhs)[N])
{
    System<N> s;
    for (int i = 0; i < N; ++i) {
        s.a[i] = Vector<double, N>::fromArray(coefficients[i]);
        s.b[i] = rhs[i];
    }
    return s;
}

// Gaussian elimination with partial pivoting; the system is taken by value and reduced in place.
template <int N>
Vector<double, N> solveGauss(System<N> s)
{
    for (int k = 0; k < N; ++k) {
        int p = k;
        for (int i = k + 1; i < N; ++i)
            if (std::fabs(s.a[i][k]) > std::fabs(s.a[p][k])) p = i;
        if (std::fabs(s.a[p][k]) < 1e-12) throw std::domain_error("solveGauss: singular system");
        if (p != k) {
            Vector<double, N> row = s.a[p]; s.a[p] = s.a[k]; s.a[k] = row;
            double rhs = s.b[p];            s.b[p] = s.b[k]; s.b[k] = rhs;
        }
        for (int i = k + 1; i < N; ++i) {
            const double f = s.a[i][k] / s.a[k][k];
            s.a[i] -= s.a[k] * f;               // Vector operator* (Vector, T) and operator-=
            s.b[i] -= f * s.b[k];
        }
    }
    Vector<double, N> x;
    for (int i = N - 1; i >= 0; --i) {
        double v = s.b[i];
        for (int j = i + 1; j < N; ++j) v -= s.a[i][j] * x[j];
        x[i] = v / s.a[i][i];
    }
    return x;
}

// Cramer's rule for three equations: with columns c0, c1, c2 the determinant is c0 . (c1 x c2).
inline Vec3d solveCramer(const System<3>& s)
{
    Vec3d c0, c1, c2;
    for (int i = 0; i < 3; ++i) { c0[i] = s.a[i][0]; c1[i] = s.a[i][1]; c2[i] = s.a[i][2]; }
    const double det = c0.dot(c1.cross(c2));
    if (std::fabs(det) < 1e-12) throw std::domain_error("solveCramer: singular system");
    return Vec3d(s.b.dot(c1.cross(c2)) / det,
                 c0.dot(s.b.cross(c2)) / det,
                 c0.dot(c1.cross(s.b)) / det);
}

// Jacobi iteration from x = 0 until no component moves by more than tol; returns the iteration count,
// or -1 when maxIterations pass first. Converges for strictly diagonally dominant systems.
template <int N>
int solveJacobi(const System<N>& s, Vector<double, N>& x, double tol, int maxIterations)
{
    x = Vector<double, N>();
    for (int it = 1; it <= maxIterations; ++it) {
        Vector<double, N> next;
        for (int i = 0; i < N; ++i) {
            double v = s.b[i];
            for (int j = 0; j < N; ++j)
                if (j != i) v -= s.a[i][j] * x[j];
            next[i] = v / s.a[i][i];
        }
        double moved = 0.0;
        for (int i = 0; i < N; ++i)
            if (std::fabs(next[i] - x[i]) > moved) moved = std::fabs(next[i] - x[i]);
        x = next;
        if (moved < tol) return it;
    }
    return -1;
}

template <int N>
double residual(const System<N>& s, const Vector<double, N>& x)   // max |a[i] . x - b[i]|
{
    double worst = 0.0;
    for (int i = 0; i < N; ++i) {
        const double r = std::fabs(s.a[i].dot(x) - s.b[i]);
        if (r > worst) worst = r;
    }
    return worst;
}

template <int N>
long weightedRound(const Vector<double, N>& x)                   // round(x0) + 10 round(x1) + 100 round(x2) ...
{
    long sum = 0, weight = 1;
    for (int i = 0; i < N; ++i, weight *= 10) sum += weight * static_cast<long>(std::floor(x[i] + 0.5));
    return sum;
}

template <int N>
mx::Matrix toMatrix(const System<N>& s)
{
    mx::Matrix m(N, N);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            m(i, j) = s.a[i][j];
    return m;
}

// 2x + y - z = 8, -3x - y + 2z = -11, -2x + y + 2z = -3: x = 2, y = 3, z = -1
System<3> classic3()
{
    const double a[3][3] = { { 2, 1, -1 }, { -3, -1, 2 }, { -2, 1, 2 } };
    const double b[3] = { 8, -11, -3 };
    return makeSystem(a, b);
}

// a 4 x 4 system built so that x = (1, -2, 3, 4): b = A x
System<4> known4()
{
    const double a[4][4] = { { 10, -1, 2, 0 }, { -1, 11, -1, 3 }, { 2, -1, 10, -1 }, { 0, 3, -1, 8 } };
    const double x[4] = { 1, -2, 3, 4 };
    double b[4];
    for (int i = 0; i < 4; ++i) {
        b[i] = 0.0;
        for (int j = 0; j < 4; ++j) b[i] += a[i][j] * x[j];
    }
    return makeSystem(a, b);
}

} // namespace lin

// ---- checks called by the driver ------------------------------------------------------------------

long linSolve3Rounded()                       // x + 10 y + 100 z = 2 + 30 - 100 = -68
{
    return lin::weightedRound(lin::solveGauss(lin::classic3()));
}

double linSolve3Residual()
{
    lin::System<3> s = lin::classic3();
    return lin::residual(s, lin::solveGauss(s));
}

double linCramerVsGauss()                     // largest difference between the two methods' solutions
{
    lin::System<3> s = lin::classic3();
    Vec3d d = lin::solveCramer(s) - lin::solveGauss(s);
    double worst = 0.0;
    for (int i = 0; i < 3; ++i) if (std::fabs(d[i]) > worst) worst = std::fabs(d[i]);
    return worst;
}

long linSolve4Rounded()                       // 1 - 20 + 300 + 4000 = 4281
{
    return lin::weightedRound(lin::solveGauss(lin::known4()));
}

double linSolve4Residual()
{
    lin::System<4> s = lin::known4();
    return lin::residual(s, lin::solveGauss(s));
}

long linJacobiIterations()                    // iterations to 1e-10 on the diagonally dominant known4
{
    lin::System<4> s = lin::known4();
    Vector<double, 4> x;
    const int it = lin::solveJacobi(s, x, 1e-10, 500);
    return it > 0 && lin::weightedRound(x) == 4281 ? it : -1;
}

double linInverseVsGauss()                    // x from mx::inverse(A) * b against Gaussian elimination
{
    lin::System<4> s = lin::known4();
    mx::Matrix bm(4, 1);
    for (int i = 0; i < 4; ++i) bm(i, 0) = s.b[i];
    mx::Matrix xm = mx::inverse(lin::toMatrix(s)) * bm;
    Vector<double, 4> g = lin::solveGauss(s);
    double worst = 0.0;
    for (int i = 0; i < 4; ++i) if (std::fabs(xm(i, 0) - g[i]) > worst) worst = std::fabs(xm(i, 0) - g[i]);
    return worst;
}

long linSingularThrows()                      // third equation = first + second: no unique solution
{
    const double a[3][3] = { { 1, 2, 3 }, { 4, 5, 6 }, { 5, 7, 9 } };
    const double b[3] = { 1, 2, 3 };
    long r = 0;
    try {
        lin::solveGauss(lin::makeSystem(a, b));
    } catch (const std::domain_error& e) {
        if (std::string(e.what()).find("singular") != std::string::npos) r += 1;
    }
    try {
        lin::solveCramer(lin::makeSystem(a, b));
    } catch (const std::domain_error&) {
        r += 10;
    }
    return r;                                 // 11 when both methods refuse
}

double linDeterminantAgrees()                 // c0 . (c1 x c2) for classic3 against mx::determinant
{
    lin::System<3> s = lin::classic3();
    Vec3d c0, c1, c2;
    for (int i = 0; i < 3; ++i) { c0[i] = s.a[i][0]; c1[i] = s.a[i][1]; c2[i] = s.a[i][2]; }
    return std::fabs(c0.dot(c1.cross(c2)) - mx::determinant(lin::toMatrix(s)));
}
