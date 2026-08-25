# Optimization review

## Current state

The C11 reference uses binary double-and-add modular multiplication. Its multiword path is therefore dominated by repeated word/bit scans and bounded subtraction, with approximately cubic word complexity per modular multiply and a further factor for exponent bits and Miller–Rabin rounds. The one-word path is substantially faster because it uses deterministic uint64 arithmetic with `__int128`.

The current x86_64 implementation is an ABI-safe fast path. It validates pointers, length and rounds, rejects even values, performs one-word trial division, and computes streaming remainders for multiword divisors 3 through 37. It does not yet implement full multiword Miller–Rabin modular exponentiation. Consequently its protocol benchmark numbers are not a valid correctness-preserving speedup comparison for arbitrary multiword values.

## Recommendations

| Priority | Recommendation | Expected effect | Risk / validation |
|---|---|---|---|
| P0 | Implement full multiword modular exponentiation in ASM or expose a separately linked helper object. | Required for semantic equivalence before performance claims. | Differential tests against C11 across random 2–2048-bit values. |
| P0 | Fix arithmetic aliasing by copying inputs before in-place reduction. | Prevents silent corruption in `double_mod` and `subtract`. | Unit tests with `result == input`; sanitizers. |
| P1 | Replace binary double-and-add with Montgomery multiplication using precomputed modulus parameters. | Reduces reduction overhead and removes repeated generic comparisons. | Validate odd-modulus setup and final conversion; compare exact outputs. |
| P1 | Use BMI2 `mulx` plus `adcx/adox` when CPU feature detection permits, with a portable baseline fallback. | Higher instruction-level parallelism for wide multiplication. | CPUID dispatch, non-BMI2 fallback, ABI register audit. |
| P1 | Keep hot operands in registers and unroll two or four limbs per iteration. | Fewer loop branches and better carry-chain throughput. | Benchmark per operand size; inspect generated code and perf counters. |
| P2 | Use fixed-base precomputation for the selected Miller–Rabin bases. | Fewer modular multiplications across rounds. | Verify table representation and constant-time indexing policy. |
| P2 | Separate validation, small-prime sieve and full MR kernel. | Better branch locality and clearer benchmark attribution. | Matrix must cover each path independently. |
| P2 | Add semantic differential benchmark mode before timing. | Prevents invalid ASM samples from entering performance reports. | Every sample must match C11 output for the same dataset. |
| P3 | Add CPU affinity, repeated runs, median/MAD comparison and perf-compatible host capture. | Reduces noise in C11-vs-ASM conclusions. | Host-specific; never compare incompatible environments. |

## Review conclusion

The highest-value optimization is not further branch shaving in the current ASM prefilter; it is completing the same modular arithmetic contract as C11. After equivalence, Montgomery reduction, BMI2 carry chains, limb unrolling, fixed-base precomputation and differential benchmark gating are the recommended sequence. Until then, report the ASM path as a fast-path candidate rather than a production-equivalent optimized implementation.
