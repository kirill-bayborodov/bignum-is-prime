# How-to: `bignum_is_prime_full.json`

## Purpose, location and lifecycle

This committed manifest is the controlled full benchmark source for `bignum_is_prime`. It is consumed by benchmark-framework v1.0.0 `bench_matrix`; generated matrix and summary JSON files belong under `benchmarks/reports/`. Project owners edit the manifest, and every edit must update this companion table and rerun JSON parsing plus C11/ASM smoke commands. Only `schema_version: 1` is supported.

## Schema and compatibility

The root object contains required integer `schema_version` and required array `profiles`. Each profile is a required object containing unique string `id`, `input_kind`, `operation_kind`, `measure_mode`, `size_profile`, and `capacity_profile`. No field has an implicit default. Version 1 rejects unknown schema versions, malformed JSON, missing fields and duplicate IDs; field additions or removals require framework compatibility review and a schema revision.

## Vocabulary

| Field | Allowed values | Semantics |
|---|---|---|
| `input_kind` | `zero`, `nonzero`, `mixed` | Deterministic dataset shape |
| `operation_kind` | `mr-quick`, `mr-standard`, `mr-strong`, `mr-mixed` | One, eight, sixteen, or rotating four-to-twelve rounds |
| `measure_mode` | `end-to-end`, `kernel-only` | Preparation included or excluded from timing |
| `size_profile` | `one`, `quarter`, `half`, `variable`, `near-capacity` | Logical word length |
| `capacity_profile` | `normal`, `near-capacity` | Normal or boundary-near storage |

## Complete profile table

| ID | Input | Operation | Measurement | Size | Capacity |
|---|---|---|---|---|---|
| `zero-one-end-to-end` | zero | mr-quick | end-to-end | one | normal |
| `nonzero-one-quick-kernel` | nonzero | mr-quick | kernel-only | one | normal |
| `nonzero-one-standard-kernel` | nonzero | mr-standard | kernel-only | one | normal |
| `nonzero-quarter-standard-kernel` | nonzero | mr-standard | kernel-only | quarter | normal |
| `nonzero-quarter-strong-kernel` | nonzero | mr-strong | kernel-only | quarter | normal |
| `nonzero-half-strong-kernel` | nonzero | mr-strong | kernel-only | half | normal |
| `nonzero-half-mixed-kernel` | nonzero | mr-mixed | kernel-only | half | normal |
| `nonzero-variable-random-end-to-end` | nonzero | mr-standard | end-to-end | variable | normal |
| `mixed-variable-mixed-end-to-end` | mixed | mr-mixed | end-to-end | variable | normal |
| `near-capacity-standard-kernel` | nonzero | mr-standard | kernel-only | near-capacity | near-capacity |
| `near-capacity-standard-end-to-end` | nonzero | mr-standard | end-to-end | near-capacity | near-capacity |
| `near-capacity-strong-kernel` | nonzero | mr-strong | kernel-only | near-capacity | near-capacity |

A run with `R` repetitions produces `12 × 2 × R` samples: one ST and one MT process per profile and repetition.

## Complete valid example

```json
{
  "schema_version": 1,
  "profiles": [
    {
      "id": "near-capacity-strong-kernel",
      "input_kind": "nonzero",
      "operation_kind": "mr-strong",
      "measure_mode": "kernel-only",
      "size_profile": "near-capacity",
      "capacity_profile": "near-capacity"
    }
  ]
}
```

## How to run and expected outputs

```bash
make bench_matrix CONFIG=release USE_ASM=no \
  REPORT_NAME=c11_full_smoke \
  BENCH_MATRIX_REPETITIONS=1 \
  BENCH_MATRIX_ITERATIONS=1000 \
  BENCH_MATRIX_MT_TOTAL_ITERATIONS=2000 \
  BENCH_MATRIX_WARMUP=10 \
  BENCH_MATRIX_DATA_COUNT=16
```

The command writes `benchmarks/reports/c11_full_smoke_matrix.json` and `benchmarks/reports/c11_full_smoke_matrix_summary.json`. Every accepted sample has a `benchmark=...` line before `Benchmark finished.`, plus elapsed seconds and nanoseconds per call.

## How to modify

Copy a profile, assign a unique ID, select values from the vocabulary table, update the complete table, run `jq empty benchmarks/profiles/bignum_is_prime_full.json`, then run both C11 and ASM matrices with identical seed, repetitions, work and thread settings. Review raw and summary JSON before using a result as a baseline.

## Baseline, comparison and failures

Baseline comparison requires the same schema and complete profile ID set. Missing or extra IDs, malformed JSON, unsupported schema, missing fields, duplicate IDs or invalid vocabulary are hard failures before accepted samples. A failed process or missing protocol marker invalidates the sample. A single smoke number is not a performance conclusion; compare medians across identical platform, build, profile, repetition and workload conditions.
