# How-to: `bignum_is_prime_full.json`

## Назначение

`bignum_is_prime_full.json` — расширенная domain-specific matrix для анализа производительности predicate проверки простоты алгоритмом Miller–Rabin. Она предназначена для controlled run, а не для быстрого CI smoke. Manifest сохраняет meaningful bignum axes: zero/mixed input, число раундов Miller–Rabin, operand word length, measurement boundary и near-capacity state.

The C11 `bench_matrix` runner from pinned `benchmark-framework v1.0.0` accepts the JSON document and launches project-owned ST/MT bignum adapter binaries. The runner writes a raw samples document; the C11 `benchmark_stats` tool parses it through the public framework library and emits a metrics/regression summary.

## Coverage

| Family | Profiles | What it isolates |
|---|---:|---|
| Zero path | 1 | Composite zero fast path |
| One-word paths | 2 | Quick and standard round counts |
| Quarter/half lengths | 4 | Standard, strong and mixed round costs at bounded multi-word sizes |
| Variable/mixed | 2 | Reproducible randomized and branch-diverse workload behavior |
| Near-capacity | 3 | Valid large operands with standard and strong round counts |

The document declares **12 profiles**. A run with `R` repetitions therefore produces `12 × 2 × R` samples: one ST and one MT process per profile/repetition.

## Controlled full run

Use fixed seed, thread count, data-count and iteration counts when a result will become a baseline.

```bash
make bench_matrix CONFIG=release USE_ASM=no \
  REPORT_NAME=c11_baseline \
  BENCH_MATRIX_REPETITIONS=7 \
  BENCH_MATRIX_ITERATIONS=200000000 \
  BENCH_MATRIX_MT_TOTAL_ITERATIONS=320000000 \
  BENCH_MATRIX_WARMUP=10000 \
  BENCH_MATRIX_DATA_COUNT=4096
```

Do not compare this result to data collected with different manifest contents, compiler configuration, CPU affinity, thread count or benchmark boundary. The JSON report records profile text, command/protocol outputs and individual timing samples so the conditions remain auditable.

## Review candidate metrics

Create a candidate summary first:

```bash
make bench_matrix CONFIG=release USE_ASM=yes REPORT_NAME=asm_candidate
```

After review, preserve the raw matrix JSON as the baseline because it contains profile metadata and protocol samples. Candidate comparison must use the same profile, seed, compiler configuration, thread count and total work.

## Bignum transport vocabulary

`operation_kind` must use one of `mr-quick`, `mr-standard`, `mr-strong`, or `mr-mixed`. Generic example values such as `xor` or `rotate` are not legal. The adapter validates these values before it initializes bignum state, so malformed profiles fail before their data become benchmark samples.

| `operation_kind` | Adapter semantics |
|---|---|
| `mr-quick` | One Miller–Rabin round |
| `mr-standard` | Eight fixed-base Miller–Rabin rounds |
| `mr-strong` | Sixteen fixed-base Miller–Rabin rounds |
| `mr-mixed` | Deterministic rotation through four to twelve rounds |
