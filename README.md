# bignum-is-prime

`bignum-is-prime` is a standalone C11/x86-64 YASM module that tests a non-negative `bignum_t` for probable primality with the Miller–Rabin algorithm. The public operation is read-only with respect to its input and writes the Boolean result through a caller-owned output parameter.

## API

```c
#include "bignum_is_prime.h"

bignum_is_prime_status_t bignum_is_prime(
    const bignum_t *num,
    size_t rounds,
    int *is_prime);
```

`rounds` must be positive. The function returns `BIGNUM_IS_PRIME_SUCCESS` and writes `1` for probable prime or `0` for composite. Values below two, even values, and values rejected by the deterministic small-prime prefilter are composite. `NULL` arguments, an invalid `len`, or zero rounds return a named error and leave the output result unchanged. The C11 implementation uses fixed-capacity storage and no mutable global state.

A complete caller owns the input and output records, checks the named status, and performs no cleanup because the API does not allocate memory:

```c
#include "bignum_is_prime.h"
#include <stdio.h>

int main(void) {
    bignum_t number = { .words = { 97U }, .len = 1U };
    int result = 0;
    bignum_is_prime_status_t status = bignum_is_prime(&number, 8U, &result);
    if (status != BIGNUM_IS_PRIME_SUCCESS) return 1;
    printf("probable_prime=%d\\n", result);
    return 0;
}
```

Build and run it from the repository root after `make build CONFIG=release USE_ASM=no`:

```bash
gcc example.c build/bignum_is_prime.o -I./include -I./libs/bignum-core/include \
  -o example -no-pie && ./example
```

The caller retains ownership of `number` and `result`; the process owns and releases only their automatic storage.

## Repository and dependencies

The repository follows the structure and conventions of `bignum-bit-test`. The required core is the `libs/bignum-core` submodule. The current CI delivers the pinned benchmark-framework distribution; it must be unpacked into `libs/benchmark-framework/dist` and used as a library. The project-owned compatibility links under `benchmarks/framework/` and `libs/benchmark-framework/build/` allow the protected Makefile to consume the downloaded tools and default profile without changing CI or Makefile.

```bash
git submodule update --init --recursive
make build CONFIG=release USE_ASM=no
```

Required tools are GCC, YASM, Make, cppcheck, Valgrind, pthreads, and the CI-compatible benchmark-framework distribution. Hardware `perf` support is host-dependent and is not required by the JSON matrix benchmark.

## Implementation stages

The C11 source in `src/bignum_is_prime.c` is the correctness reference and baseline implementation. It contains bounded modular arithmetic and deterministic 64-bit Miller–Rabin bases, while larger operands use the fixed-capacity Miller–Rabin path. The assembly source in `src/bignum_is_prime.asm` is selected with `USE_ASM=yes`; it preserves the System V AMD64 ABI and provides an optimized validation, small-operand, parity, and small-divisor path.

The C11 implementation is not synchronized with the assembly candidate. Both implementations are tested against the same public contract. Any future optimization must preserve the API, input immutability, status codes, and benchmark protocol.

## Tests and coverage

Run the complete deterministic, extended, multithreaded, integration, and adapter suite for either implementation:

```bash
make test CONFIG=release USE_ASM=no
make test CONFIG=release USE_ASM=yes
```

Run static analysis and the template quality gates:

```bash
make lint
make clean
make test_sanitize SAN=address CONFIG=debug USE_ASM=no
make clean
make test_sanitize SAN=undefined CONFIG=debug USE_ASM=no
make clean
make test_helgrind CONFIG=debug USE_ASM=yes
```

The deterministic tests cover values below two, known small primes and composites, large one-word values, multiword values, invalid arguments, invalid lengths, zero rounds, input immutability, and randomized small values against a trial-division oracle. The multithread test proves concurrent read-only calls on independent records.

For a C11 line-coverage run, build with gcov instrumentation using the normal include paths and execute `make test CONFIG=debug USE_ASM=no`; then inspect the generated `bignum_is_prime.c.gcov` report in the repository root. Coverage is a reference-quality indicator, not a substitute for randomized differential testing.

## Benchmarks

The project-owned adapter in `benchmarks/adapter/` maps benchmark-framework transport fields to Miller–Rabin workloads. Valid `operation_kind` values are `mr-quick`, `mr-standard`, `mr-strong`, and `mr-mixed`. Input, size, capacity, and measurement axes remain compatible with benchmark-framework v1.0.0. The adapter generates deterministic normalized odd operands, invokes `bignum_is_prime`, and preserves the required machine-readable protocol markers.

Run a short JSON matrix for the C11 baseline:

```bash
make bench_matrix CONFIG=release USE_ASM=no \
  REPORT_NAME=c11_baseline \
  BENCH_MATRIX_REPETITIONS=1 \
  BENCH_MATRIX_ITERATIONS=2000 \
  BENCH_MATRIX_MT_TOTAL_ITERATIONS=4000 \
  BENCH_MATRIX_WARMUP=10 \
  BENCH_MATRIX_DATA_COUNT=16
```

Run the same matrix for the assembly candidate:

```bash
make bench_matrix CONFIG=release USE_ASM=yes \
  REPORT_NAME=asm_candidate \
  BENCH_MATRIX_REPETITIONS=1 \
  BENCH_MATRIX_ITERATIONS=2000 \
  BENCH_MATRIX_MT_TOTAL_ITERATIONS=4000 \
  BENCH_MATRIX_WARMUP=10 \
  BENCH_MATRIX_DATA_COUNT=16
```

Reports are written to `benchmarks/reports/`. A successful matrix must contain successful ST and MT samples with `benchmark`, `elapsed_seconds`, and `ns_per_call` protocol fields. Keep profile, seed, compiler, CPU affinity, thread count, and total work constant when comparing C11 and ASM results.

The perf targets remain available through the protected Makefile. They require a `perf` binary compatible with the running kernel; if the host does not provide compatible PMU tools, use `bench_matrix` for reproducible software-independent measurements.

## Distribution

```bash
make install CONFIG=release
make dist CONFIG=release
```

The generated distribution contains the public header, object/library artifacts, README, license, and integration runner. Do not modify `.github/workflows/` or `Makefile`; if a future naming mismatch cannot be solved with project-owned compatibility artifacts, document the required protected-file change for owner review instead.

## Review checklist

Before publication, review every project-owned file under `include/`, `src/`, `benchmarks/adapter/`, `benchmarks/profiles/`, and `tests/`. Confirm that no template or shift-specific symbol remains, all Doxygen `@brief` descriptions are concise, all public parameters and return statuses are documented, and `git diff --check`, `make test`, `make lint`, sanitizer checks, matrix checks, and the final clean-tree check succeed.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
