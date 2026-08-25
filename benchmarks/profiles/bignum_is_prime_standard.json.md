# How-to: `bignum_is_prime_standard.json`

## Назначение

`bignum_is_prime_standard.json` — компактная versioned matrix для функциональной проверки и baseline операции `bignum_is_prime`. Manifest использует schema version `1`, которую читает C11-инструмент `bench_matrix` из pinned `benchmark-framework v1.0.0`.

> Manifest не описывает generic byte-transform. Он переносит bignum primality semantics через нейтральные transport fields benchmark framework.

| JSON field | Значение в manifest | Bignum interpretation |
|---|---|---|
| `input_kind` | `zero`, `nonzero`, `mixed` | Форма исходного `bignum_t` dataset |
| `operation_kind` | `mr-quick`, `mr-standard`, `mr-strong`, `mr-mixed` | Number and schedule of Miller–Rabin rounds |
| `measure_mode` | `end-to-end`, `kernel-only` | Includes or excludes preparation copy from the timed interval |
| `size_profile` | `one`, `quarter`, `half`, `variable`, `near-capacity` | Logical word length of input `bignum_t` |
| `capacity_profile` | `normal`, `near-capacity` | Storage-boundary workload condition |

## Smoke run

```bash
make bench_matrix CONFIG=release USE_ASM=no \
  REPORT_NAME=c11_standard_smoke \
  BENCH_MATRIX_REPETITIONS=1 \
  BENCH_MATRIX_ITERATIONS=1000 \
  BENCH_MATRIX_MT_TOTAL_ITERATIONS=2000 \
  BENCH_MATRIX_WARMUP=10 \
  BENCH_MATRIX_DATA_COUNT=16
```

The expected matrix contains **8 profiles × 2 modes × repetitions** samples. Every accepted sample has exactly one `benchmark=...` line before its `Benchmark finished.` marker.

## Aggregation and comparison

Aggregate a candidate with the framework statistics tool through the Makefile matrix target. Later C11 and ASM comparisons must use identical manifests, seed, compiler configuration, measurement modes, thread count and total work.

A changed profile set is intentionally not a valid baseline. The statistics tool reports missing or extra profiles rather than treating a partial comparison as success.

## Boundary case

`near-capacity` uses a valid normalized source operand near `BIGNUM_CAPACITY`. It measures primality testing on large operands; it does not intentionally time invalid-length or error paths. Invalid argument behavior belongs in deterministic API tests, not performance aggregates.
