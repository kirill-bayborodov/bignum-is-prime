# bignum-is-prime

[![C/ASM CI](https://github.com/kirill-bayborodov/bignum-is-prime/actions/workflows/ci.yml/badge.svg)](https://github.com/kirill-bayborodov/bignum-is-prime/actions/workflows/ci.yml)
[![GitHub release](https://img.shields.io/github/v/release/kirill-bayborodov/bignum-is-prime?label=release)](https://github.com/kirill-bayborodov/bignum-is-prime/releases/latest)

`bignum-is-prime` is a standalone C11/x86-64 YASM module that tests a non-negative fixed-capacity `bignum_t` for probable primality with the Miller–Rabin algorithm. The C11 implementation is the correctness reference; the assembly implementation is an independently optimized production path using bounded multiprecision arithmetic and a variable-precision Montgomery reduction kernel for operands up to 32 limbs.

The operation borrows its input, writes the result through caller-owned storage, performs no allocation or ownership transfer, and is safe for concurrent calls when independent output objects are used. A probable-prime result is not a mathematical proof for general multiword inputs.

## Features

- **C11 reference path:** portable fixed-capacity arithmetic and deterministic 64-bit Miller–Rabin bases.
- **x86-64 ASM path:** System V AMD64 ABI implementation with one-word fast paths, BMI2 `mulx`, ADX `adcx`/`adox`, bounded modular arithmetic, and Montgomery REDC support.
- **Explicit status contract:** named `bignum_is_prime_status_t` values distinguish null arguments, invalid lengths, and zero rounds.
- **Transactional output behavior:** invalid calls leave the output predicate unchanged; inputs are never modified.
- **Deterministic verification:** unit, extended, multithreaded, distribution-runner, adapter, randomized oracle, and boundary tests.
- **Reproducible benchmark protocol:** benchmark-framework-compatible ST/MT runners, JSON profiles, checksums, fingerprints, and baseline comparison reports.
- **Review documentation:** per-artifact Quality Gate checklist, Montgomery REDC trace, benchmark comparison, and cryptographic-core coverage assessment.

## Distribution and dependencies

The required `bignum-core` component is included as a Git submodule. The benchmark framework distribution is consumed from `libs/benchmark-framework/dist` and is already included in the repository workspace used by CI.

| Component | Location | Purpose |
|---|---|---|
| `bignum-core` | `libs/bignum-core` | Defines `bignum_t` and `BIGNUM_CAPACITY`. |
| `benchmark-framework` | `libs/benchmark-framework/dist` | C11 benchmark lifecycle, matrix execution, and statistics. |
| Project adapter | `benchmarks/adapter/` | Maps framework workload fields to Miller–Rabin operations. |

Clone with the submodule:

```bash
git clone --recurse-submodules https://github.com/kirill-bayborodov/bignum-is-prime.git
cd bignum-is-prime
```

For an existing clone, recover submodules with:

```bash
git submodule update --init --recursive
```

Required tools are GCC, YASM, GNU Make, cppcheck, pthreads, and the benchmark-framework distribution. Valgrind and a kernel-compatible `perf` binary are required only for their optional race and PMU workflows.

## API and contract

The public API is declared in `include/bignum_is_prime.h`:

```c
#include "bignum_is_prime.h"

bignum_is_prime_status_t bignum_is_prime(
    const bignum_t *num,
    size_t rounds,
    int *is_prime);
```

| Condition | Return status | Observable result |
|---|---|---|
| Valid input and positive rounds | `BIGNUM_IS_PRIME_SUCCESS` | `*is_prime` is `1` for probable prime or `0` for composite; `num` is unchanged. |
| `num == NULL` or `is_prime == NULL` | `BIGNUM_IS_PRIME_ERROR_NULL_ARG` | Output is unchanged. |
| `num->len > BIGNUM_CAPACITY` | `BIGNUM_IS_PRIME_ERROR_BAD_LENGTH` | Output is unchanged. |
| `rounds == 0` | `BIGNUM_IS_PRIME_ERROR_ROUNDS` | Output is unchanged. |

The function is reentrant and thread-safe for independent caller-owned objects. Concurrent mutation of a borrowed input or output requires external synchronization. Time complexity is bounded by the selected implementation and grows with the number of Miller–Rabin rounds and operand limbs.

A complete minimal caller checks the named status and needs no cleanup because the API does not allocate memory:

```c
#include "bignum_is_prime.h"
#include <stdio.h>

int main(void)
{
    bignum_t number = { .words = { 97U }, .len = 1U };
    int result = -1;
    bignum_is_prime_status_t status = bignum_is_prime(&number, 8U, &result);

    if (status != BIGNUM_IS_PRIME_SUCCESS) {
        return 1;
    }
    printf("probable_prime=%d\n", result);
    return 0;
}
```

Compile the example from the repository root after building the C11 object:

```bash
make build CONFIG=release USE_ASM=no
gcc example.c build/bignum_is_prime.o \
  -I./include -I./libs/bignum-core/include -o example -no-pie
./example
rm -f example
```

## Build and test

Build the C11 reference and assembly candidate separately:

```bash
make clean
make build CONFIG=release USE_ASM=no
make clean
make build CONFIG=release USE_ASM=yes
```

Run the complete deterministic, extended, multithreaded, integration-runner, and adapter suite for each implementation:

```bash
make test CONFIG=release USE_ASM=no
make test CONFIG=release USE_ASM=yes
```

Run static analysis and dynamic checks:

```bash
make lint
make clean
make test_sanitize SAN=address CONFIG=debug USE_ASM=no
make clean
make test_sanitize SAN=undefined CONFIG=debug USE_ASM=no
make clean
make test_helgrind CONFIG=debug USE_ASM=yes
```

The test sources are organized as follows:

| File | Scope |
|---|---|
| `tests/test_bignum_is_prime.c` | Deterministic public API, status, boundary, and normalization tests. |
| `tests/test_bignum_is_prime_extra.c` | Fixed-seed oracle tests, helper arithmetic, witness paths, preservation, and robustness checks. |
| `tests/test_bignum_is_prime_mt.c` | Concurrent calls on independent read-only inputs. |
| `tests/test_bignum_is_prime_runner.c` | Distribution integration smoke test. |
| `tests/benchmark_adapter/test_bignum_is_prime_benchmark_adapter.c` | Adapter validation, deterministic workload construction, callback lifecycle, and checksum checks. |

### C11 coverage

The C11 reference coverage run uses one instrumented `src/bignum_is_prime.c` object and a combined driver that executes deterministic, extra, and multithreaded suites. The current documented run reports **82.72% line coverage**, **56.72% branch coverage**, and **71.43% call coverage** for the C11 implementation. YASM instruction coverage is not represented by gcov; the assembly path is validated by ABI-facing, differential, randomized, and boundary tests.

## Benchmarks

The project-owned adapter in `benchmarks/adapter/` accepts the Miller–Rabin operation vocabulary `mr-quick`, `mr-standard`, `mr-strong`, and `mr-mixed`. It creates deterministic normalized odd operands, invokes the selected implementation, and preserves the required `benchmark=...` followed by `Benchmark finished.` protocol.

Run short, reproducible JSON matrices:

```bash
make bench_matrix CONFIG=release USE_ASM=no \
  REPORT_NAME=c11_baseline \
  BENCH_MATRIX_REPETITIONS=1 \
  BENCH_MATRIX_ITERATIONS=2000 \
  BENCH_MATRIX_MT_TOTAL_ITERATIONS=4000 \
  BENCH_MATRIX_WARMUP=10 \
  BENCH_MATRIX_DATA_COUNT=16

make bench_matrix CONFIG=release USE_ASM=yes \
  REPORT_NAME=asm_candidate \
  BENCH_MATRIX_REPETITIONS=1 \
  BENCH_MATRIX_ITERATIONS=2000 \
  BENCH_MATRIX_MT_TOTAL_ITERATIONS=4000 \
  BENCH_MATRIX_WARMUP=10 \
  BENCH_MATRIX_DATA_COUNT=16
```

Reports are written to `benchmarks/reports/`. Keep profile, seed, compiler, CPU affinity, thread count, total work, and configuration constant when comparing C11 and ASM. The Montgomery REDC before/after measurement protocol and results are documented in `docs/benchmark_montgomery_redc_fix.md`.

For repeated PMU-compatible measurements:

```bash
make bench_stat CONFIG=release REPORT_NAME=asm_candidate \
  DATA_MODE=mixed PERF_RUNS=7
```

When hardware PMU events are unavailable, use the cloud-compatible software-event workflow:

```bash
make bench_cl CONFIG=release REPORT_NAME=asm_candidate PERF_RUNS=7
```

## Profiles and JSON manifests

Each committed profile has an adjacent companion document:

| Manifest | Companion | Scope |
|---|---|---|
| `benchmarks/profiles/bignum_is_prime_standard.json` | `bignum_is_prime_standard.json.md` | Short standard workload. |
| `benchmarks/profiles/bignum_is_prime_full.json` | `bignum_is_prime_full.json.md` | Full ST/MT and size matrix. |

Validate the manifests with Python's JSON parser before a matrix run:

```bash
python3 -m json.tool benchmarks/profiles/bignum_is_prime_standard.json >/dev/null
python3 -m json.tool benchmarks/profiles/bignum_is_prime_full.json >/dev/null
```

The companion documents define schema version, vocabulary, profile semantics, commands, expected outputs, comparison policy, and failure handling.

## Installation and distribution

Create the release distribution:

```bash
make install CONFIG=release
make dist CONFIG=release
```

The distribution contains the public header, object/library artifacts, README, license, and integration runner. It does not contain internal `.git`, build, coverage, or temporary benchmark artifacts.

## Review and quality gates

The normative documentation standard is versioned at `docs/QUALITY_GATES_DOCUMENTATION_C11_JSON.md`. The current artifact-level review is summarized in `docs/QUALITY_REVIEW_COVERAGE_AND_DOCUMENTATION.md`; additional evidence is in `docs/CRYPTO_CORE_COVERAGE.md`, `docs/failing_mont_trace_n2.md`, and `docs/benchmark_montgomery_redc_fix.md`.

Before a release, run `git diff --check`, both implementation test modes, static analysis, sanitizer checks, JSON validation, the benchmark smoke matrix, and a final clean-tree check. Do not modify `Makefile` or `.github/workflows/`; required compatibility behavior belongs in project-owned adapters and documentation.

## Contributing

Changes must preserve the public status contract, input immutability, fixed-capacity bounds, System V AMD64 ABI, benchmark output protocol, and documented JSON vocabulary. Every new test must document its scenario, expected status/output, and oracle. Every changed header, source, test, benchmark adapter, profile, and README section must receive the applicable artifact-level Quality Gate review.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
