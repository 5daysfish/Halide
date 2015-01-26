// USAGE: halide_benchmarks <subroutine> <size>
//
// Benchmarks BLAS subroutines using Halide's implementation. Will
// construct random size x size matrices and/or size x 1 vectors
// to test the subroutine with.
//
// Accepted values for subroutine are:
//    L1: scal, copy, axpy, dot, nrm2
//    L2: gemv
//    L3: gemm
//

#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include "Halide.h"
#include "halide_blas.h"
#include "clock.h"

#define L1Benchmark(benchmark, type, code)                               \
  virtual void bench_##benchmark(int N) {                               \
        Scalar alpha = randomScalar();                                  \
        std::unique_ptr<Vector> x(randomVector(N));                     \
        std::unique_ptr<Vector> y(randomVector(N));                     \
                                                                        \
        double start = current_time();                                  \
        for (int i = 0; i < num_iters; ++i) {                           \
            code;                                                       \
        }                                                               \
        double end = current_time();                                    \
        double elapsed = end - start;                                   \
                                                                        \
        std::cout << std::setw(8) << name                               \
                  << std::setw(15) << type #benchmark                   \
                  << std::setw(8) << std::to_string(N)                  \
                  << std::setw(20) << std::to_string(elapsed)           \
                  << std::setw(20) << 1000 * N / elapsed                \
                  << std::endl;                                         \
    }                                                                   \

#define L2Benchmark(benchmark, type, code)                              \
    virtual void bench_##benchmark(int N) {                             \
        Scalar alpha = randomScalar();                                  \
        Scalar beta = randomScalar();                                   \
        std::unique_ptr<Vector> x(randomVector(N));                     \
        std::unique_ptr<Vector> y(randomVector(N));                     \
        std::unique_ptr<Matrix> A(randomMatrix(N));                     \
                                                                        \
        double start = current_time();                                  \
        for (int i = 0; i < num_iters; ++i) {                           \
            code;                                                       \
        }                                                               \
        double end = current_time();                                    \
        double elapsed = end - start;                                   \
                                                                        \
        std::cout << std::setw(8) << name                               \
                  << std::setw(15) << type #benchmark                   \
                  << std::setw(8) << std::to_string(N)                  \
                  << std::setw(20) << std::to_string(elapsed)           \
                  << std::setw(20) << 1000 * N / elapsed                \
                  << std::endl;                                         \
    }                                                                   \

#define L3Benchmark(benchmark, type, code)                              \
    virtual void bench_##benchmark(int N) {                             \
        Scalar alpha = randomScalar();                                  \
        Scalar beta = randomScalar();                                   \
        std::unique_ptr<Matrix> A(randomMatrix(N));                     \
        std::unique_ptr<Matrix> B(randomMatrix(N));                     \
        std::unique_ptr<Matrix> C(randomMatrix(N));                     \
                                                                        \
        double start = current_time();                                  \
        for (int i = 0; i < num_iters; ++i) {                           \
            code;                                                       \
        }                                                               \
        double end = current_time();                                    \
        double elapsed = end - start;                                   \
                                                                        \
        std::cout << std::setw(8) << name                               \
                  << std::setw(15) << type #benchmark                   \
                  << std::setw(8) << std::to_string(N)                  \
                  << std::setw(20) << std::to_string(elapsed)           \
                  << std::setw(20) << 1000 * N / elapsed                \
                  << std::endl;                                         \
    }                                                                   \


template<class T>
struct BenchmarksBase {
    typedef T Scalar;
    typedef Halide::Buffer Vector;
    typedef Halide::Buffer Matrix;

    std::random_device rand_dev;
    std::default_random_engine rand_eng{rand_dev()};

    std::string name;
    int num_iters;

    Scalar randomScalar() {
        std::uniform_real_distribution<T> uniform_dist(0.0, 1.0);
        return uniform_dist(rand_eng);
    }

    Vector *randomVector(int N) {
        Vector *buff = new Vector(Halide::type_of<T>(), N);
        Scalar *x = (Scalar*)buff->host_ptr();
        for (int i=0; i<N; ++i) {
            x[i] = randomScalar();
        }
        return buff;
    }

    Matrix *randomMatrix(int N) {
        Matrix *buff = new Matrix(Halide::type_of<T>(), N, N);
        Scalar *A = (Scalar*)buff->host_ptr();
        for (int i=0; i<N*N; ++i) {
            A[i] = randomScalar();
        }
        return buff;
    }

    BenchmarksBase(std::string n, int iters) :
            name(n), num_iters(iters)
    {}

    void run(std::string benchmark, int size) {
        if (benchmark == "copy") {
            bench_copy(size);
        } else if (benchmark == "scal") {
            bench_scal(size);
        } else if (benchmark == "axpy") {
            bench_axpy(size);
        } else if (benchmark == "dot") {
            bench_dot(size);
        } else if (benchmark == "asum") {
            bench_asum(size);
        } else if (benchmark == "gemv_notrans") {
            bench_gemv_notrans(size);
        } else if (benchmark == "gemv_trans") {
            bench_gemv_trans(size);
        } else if (benchmark == "gemm_notrans") {
            bench_gemm_notrans(size);
        } else if (benchmark == "gemm_trans_A") {
            bench_gemm_trans_A(size);
        } else if (benchmark == "gemm_trans_B") {
            bench_gemm_trans_B(size);
        } else if (benchmark == "gemm_trans_AB") {
            bench_gemm_trans_AB(size);
        }
    }

    virtual void bench_copy(int N) =0;
    virtual void bench_scal(int N) =0;
    virtual void bench_axpy(int N) =0;
    virtual void bench_dot(int N)  =0;
    virtual void bench_asum(int N) =0;
    virtual void bench_gemv_notrans(int N) =0;
    virtual void bench_gemv_trans(int N) =0;
    virtual void bench_gemm_notrans(int N) =0;
    virtual void bench_gemm_trans_A(int N) =0;
    virtual void bench_gemm_trans_B(int N) =0;
    virtual void bench_gemm_trans_AB(int N) =0;
};

struct BenchmarksFloat : public BenchmarksBase<float> {
    BenchmarksFloat(std::string n, int iters) :
            BenchmarksBase(n, iters),
            result(Halide::Float(32), 1)
    {}

    Halide::Buffer result;

    L1Benchmark(copy, "s", halide_scopy(x->raw_buffer(), y->raw_buffer()))
    L1Benchmark(scal, "s", halide_sscal(alpha, x->raw_buffer()))
    L1Benchmark(axpy, "s", halide_saxpy(alpha, x->raw_buffer(), y->raw_buffer()))
    L1Benchmark(dot,  "s", halide_sdot(x->raw_buffer(), y->raw_buffer(), result.raw_buffer()))
    L1Benchmark(asum, "s", halide_sasum(x->raw_buffer(), result.raw_buffer()))

    L2Benchmark(gemv_notrans, "s", halide_sgemv(false, alpha, A->raw_buffer(), x->raw_buffer(),
                                                beta, y->raw_buffer()))

    L2Benchmark(gemv_trans, "s", halide_sgemv(true, alpha, A->raw_buffer(), x->raw_buffer(),
                                              beta, y->raw_buffer()))

    L3Benchmark(gemm_notrans, "s", halide_sgemm(false, false, alpha, A->raw_buffer(),
                                                B->raw_buffer(), beta, C->raw_buffer()))

    L3Benchmark(gemm_trans_A, "s", halide_sgemm(true, false, alpha, A->raw_buffer(),
                                                B->raw_buffer(), beta, C->raw_buffer()))

    L3Benchmark(gemm_trans_B, "s", halide_sgemm(false, true, alpha, A->raw_buffer(),
                                                B->raw_buffer(), beta, C->raw_buffer()))

    L3Benchmark(gemm_trans_AB, "s", halide_sgemm(true, true, alpha, A->raw_buffer(),
                                                 B->raw_buffer(), beta, C->raw_buffer()))
};

struct BenchmarksDouble : public BenchmarksBase<double> {
    BenchmarksDouble(std::string n, int iters) :
            BenchmarksBase(n, iters),
            result(Halide::Float(64), 1)
    {}

    Halide::Buffer result;

    L1Benchmark(copy, "d", halide_dcopy(x->raw_buffer(), y->raw_buffer()))
    L1Benchmark(scal, "d", halide_dscal(alpha, x->raw_buffer()))
    L1Benchmark(axpy, "d", halide_daxpy(alpha, x->raw_buffer(), y->raw_buffer()))
    L1Benchmark(dot,  "d", halide_ddot(x->raw_buffer(), y->raw_buffer(), result.raw_buffer()))
    L1Benchmark(asum, "d", halide_dasum(x->raw_buffer(), result.raw_buffer()))

    L2Benchmark(gemv_notrans, "d", halide_dgemv(false, alpha, A->raw_buffer(), x->raw_buffer(),
                                                beta, y->raw_buffer()))

    L2Benchmark(gemv_trans, "d", halide_dgemv(true, alpha, A->raw_buffer(), x->raw_buffer(),
                                              beta, y->raw_buffer()))

    L3Benchmark(gemm_notrans, "d", halide_dgemm(false, false, alpha, A->raw_buffer(),
                                                B->raw_buffer(), beta, C->raw_buffer()))

    L3Benchmark(gemm_trans_A, "d", halide_dgemm(true, false, alpha, A->raw_buffer(),
                                                B->raw_buffer(), beta, C->raw_buffer()))

    L3Benchmark(gemm_trans_B, "d", halide_dgemm(false, true, alpha, A->raw_buffer(),
                                                B->raw_buffer(), beta, C->raw_buffer()))

    L3Benchmark(gemm_trans_AB, "d", halide_dgemm(true, true, alpha, A->raw_buffer(),
                                                B->raw_buffer(), beta, C->raw_buffer()))
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "USAGE: halide_benchmarks <subroutine> <size>\n";
        return 0;
    }

    std::string subroutine = argv[1];
    char type = subroutine[0];
    int  size = std::stoi(argv[2]);

    subroutine = subroutine.substr(1);
    if (type == 's') {
        BenchmarksFloat ("Halide", 1000).run(subroutine, size);
    } else if (type == 'd') {
        BenchmarksDouble("Halide", 1000).run(subroutine, size);
    }

    return 0;
}
