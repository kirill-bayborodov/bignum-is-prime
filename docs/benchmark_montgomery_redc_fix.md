# Montgomery REDC benchmark: baseline vs fixed

The benchmark was run on the same host, compiler, operands, and 20,000 calls per size. The baseline object was assembled from `HEAD`; the fixed object was assembled from the working tree containing the `T[2n]` overflow check.

| Limbs | Baseline median, ns/call | Fixed median, ns/call | Delta |
|---:|---:|---:|---:|
| 2 | 60.82 | 57.18 | -6.0% |
| 4 | 107.30 | 114.58 | +6.8% |
| 8 | 267.14 | 275.73 | +3.2% |
| 16 | 911.87 | 921.74 | +1.1% |
| 32 | 3,424.96 | 3,406.83 | -0.5% |

These figures are noisy wall-clock measurements rather than cycle-counter measurements. The fix adds one `T[2n]` load/branch to the final reduction; the expected steady-state cost is therefore negligible relative to the O(n²) REDC kernel, while overflow cases now produce correct results. All benchmark runs returned `rc=0`.

The exact raw output is retained at `/tmp/mont_benchmark_before_after.txt`.
