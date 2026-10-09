// driver-matrix.cpp - runs the checks of examples/sample-matrix (matrix.cpp, solver.cpp) and prints one
// line per result. The whole output is compared with the oracle files in this folder:
//   oracle-clang-cpp98.txt   every unit built by clang++ -std=c++98 -pedantic-errors
//   oracle-cl.txt            every unit built by cl.exe
// and must be identical when matrix.cpp and solver.cpp are compiled by cppparse --compile6 --msvc-mangling
// at any optimisation level (see sandbox/scripts/matrix.ps1). Exact results are also checked against
// their known values here; inexact ones are printed with 10 significant digits or as a tolerance verdict.
// Strict C++98: no C++11 or later features.
#include <cmath>
#include <cstdio>

double mtxMultiplyChecksum();
long   mtxIdentityHolds();
long   mtxAssociative();
long   mtxTransposeRule();
double mtxDeterminant3();
double mtxInverse2Weighted();
long   mtxInverseExact();
double mtxRoundTripError(int n);
double mtxInverseOfInverseError();
long   mtxPivotNeeded();
long   mtxSingularThrows();
long   mtxSizeMismatchThrows();
long   mtxFibonacci(int n);
double mtxLargeProductChecksum();
double mtxScaledSum();

long   linSolve3Rounded();
double linSolve3Residual();
double linCramerVsGauss();
long   linSolve4Rounded();
double linSolve4Residual();
long   linJacobiIterations();
double linInverseVsGauss();
long   linSingularThrows();
double linDeterminantAgrees();

static int g_failures = 0;

static void exact(const char* what, long got, long expected)
{
    std::printf("%-6s %-28s = %ld\n", got == expected ? "ok" : "FAIL", what, got);
    if (got != expected) { std::printf("       expected %ld\n", expected); ++g_failures; }
}

static void exactD(const char* what, double got, double expected)
{
    std::printf("%-6s %-28s = %.10g\n", got == expected ? "ok" : "FAIL", what, got);
    if (got != expected) { std::printf("       expected %.10g\n", expected); ++g_failures; }
}

static void value(const char* what, double got)              // no closed form: the oracle holds the value
{
    std::printf("%-6s %-28s = %.10g\n", "value", what, got);
}

static void small(const char* what, double got, double tol)  // an error that must stay below tol
{
    const bool ok = got >= 0.0 && got < tol;
    std::printf("%-6s %-28s < %g\n", ok ? "ok" : "FAIL", what, tol);
    if (!ok) { std::printf("       got %.17g\n", got); ++g_failures; }
}

int main()
{
    std::setvbuf(stdout, 0, _IONBF, 0);   // every line reaches the console even if a check crashes
    std::printf("== matrices ==\n");
    exactD("multiply 2x3 by 3x2", mtxMultiplyChecksum(), 168598.0);
    exact("A*I = I*A = A", mtxIdentityHolds(), 1);
    exact("(AB)C = A(BC)", mtxAssociative(), 1);
    exact("(AB)^T = B^T A^T", mtxTransposeRule(), 1);
    const double det = mtxDeterminant3();                  // pivoting leaves 48.99999999999999
    exact("det [[2,-3,1],[2,0,-1],..]", static_cast<long>(std::floor(det + 0.5)), 49);
    small("det rounding error", std::fabs(det - 49.0), 1e-12);
    value("inverse 2x2 weighted", mtxInverse2Weighted());
    exact("inverse unimodular 3x3", mtxInverseExact(), 303231133L);
    small("A * inv(A) - I, n = 3", mtxRoundTripError(3), 1e-12);
    small("A * inv(A) - I, n = 8", mtxRoundTripError(8), 1e-12);
    small("A * inv(A) - I, n = 16", mtxRoundTripError(16), 1e-12);
    small("inv(inv(A)) - A", mtxInverseOfInverseError(), 1e-12);
    exact("pivoting on a zero diagonal", mtxPivotNeeded(), 111);
    exact("singular matrix throws", mtxSingularThrows(), 1);
    exact("size mismatch throws", mtxSizeMismatchThrows(), 1);
    exact("Fibonacci by power, F(30)", mtxFibonacci(30), 832040L);
    exact("power 0 is identity, F(0)", mtxFibonacci(0), 0);
    value("24x24 product checksum", mtxLargeProductChecksum());
    value("sum of 2A + I", mtxScaledSum());

    std::printf("== linear equations (Vector.hpp) ==\n");
    exact("Gauss 3x3: x+10y+100z", linSolve3Rounded(), -68);
    small("Gauss 3x3 residual", linSolve3Residual(), 1e-12);
    small("Cramer vs Gauss", linCramerVsGauss(), 1e-12);
    exact("Gauss 4x4: weighted x", linSolve4Rounded(), 4281);
    small("Gauss 4x4 residual", linSolve4Residual(), 1e-12);
    value("Jacobi iterations to 1e-10", static_cast<double>(linJacobiIterations()));
    small("inverse(A)*b vs Gauss", linInverseVsGauss(), 1e-12);
    exact("singular system throws", linSingularThrows(), 11);
    small("cross/dot det vs det()", linDeterminantAgrees(), 1e-12);

    std::printf(g_failures ? "%d FAILED\n" : "ALL OK\n", g_failures);
    return g_failures ? 1 : 0;
}
