# C11 coverage improvement plan

## Current baseline

The review run reports 82.72% aggregate line coverage, while the implementation copy embedded by `test_bignum_is_prime_extra.c` reports 74.18% lines. The aggregate number is not a suitable sole production metric because the extra test includes `src/bignum_is_prime.c` a second time under a renamed symbol. The replacement script in `scripts/run_c11_coverage.sh` instruments and links the production C11 source exactly once.

## Unit-test vectors for lines 319–335

These vectors target `is_prime_u64`, including decomposition of `n-1`, the `value == 1`/`value == n-1` fast acceptance, repeated squaring, witness rejection, and final acceptance.

| Direct helper input | Expected result | Coverage intent |
|---|---:|---|
| `is_prime_u64(2)` | `1` | Smallest valid one-word prime; `powers == 0` and immediate base acceptance. |
| `is_prime_u64(3)` | `1` | Odd `n-1` decomposition with `powers == 1`. |
| `is_prime_u64(97)` | `1` | `n-1 = 3 * 2^5`; exercises repeated squaring and a witness path reaching `n-1`. |
| `is_prime_u64(UINT64_C(1022117))` | `0` | Fixed composite `1009 * 1013`; exercises deterministic one-word witness rejection without a small-prime factor. |
| `is_prime_u64(18,446,744,073,709,551,557)` | `1` | Largest commonly used 64-bit prime vector; exercises 64-bit modular multiplication and long exponentiation. |
| `is_prime_u64(UINT64_C(0xffffffffffffffff))` | `0` | High-bit composite and long `n-1` decomposition; exercises rejection near the uint64 limit. |

Use the exact fixed vector `UINT64_C(1022117)` from the stated factorization `1009 * 1013`; no symbolic or generated value should be used in the test.

## Unit-test vectors for lines 377–398

These vectors target public validation, normalization, values below two, the small-prime table, even rejection, small-divisor rejection, and dispatch into `is_prime_u64`.

| `num` representation | `rounds` | Initial `*is_prime` | Expected status/output | Coverage intent |
|---|---:|---:|---|---|
| `NULL` | `8` | `91` | `ERROR_NULL_ARG`, `91` unchanged | Null input. |
| Valid `{words={17}, len=1}` with `is_prime=NULL` | `8` | N/A | `ERROR_NULL_ARG` | Null output. |
| Valid `{words={17}, len=33}` | `8` | `91` | `ERROR_BAD_LENGTH`, `91` unchanged | Capacity guard. |
| Valid `{words={17}, len=1}` | `0` | `91` | `ERROR_ROUNDS`, `91` unchanged | Zero-round guard. |
| `{words={0}, len=0}` | `8` | `91` | `SUCCESS`, `0` | Canonical zero. |
| `{words={1}, len=1}` | `8` | `91` | `SUCCESS`, `0` | One, below two. |
| `{words={2}, len=1}` | `8` | `91` | `SUCCESS`, `1` | First small prime. |
| Each of `{3,5,7,11,13,17,19,23,29,31,37}` with `len=1` | `8` | `91` | `SUCCESS`, `1` | Every small-prime table entry. |
| `{words={25}, len=1}` | `8` | `91` | `SUCCESS`, `0` | Small-value table miss after scanning all entries. |
| `{words={0}, len=2}` | `8` | `91` | `SUCCESS`, `0` | Padded zero normalization. |
| `{words={2}, len=2}` | `8` | `91` | `SUCCESS`, `1` | Padded small-prime normalization. |
| `{words={0}, len=2}` with high non-zero words beyond logical value if representation allows | `8` | `91` | `SUCCESS`, `0` | Verify only normalized significant words affect the result. |
| `{words={0}, len=2}` where `words[0]` is even and `words[1]` is non-zero | `8` | `91` | `SUCCESS`, `0` | Multiword even fast rejection. |
| `{words={UINT64_C(1022117)}, len=1}` | `8` | `91` | `SUCCESS`, `0` | One-word dispatch to deterministic `is_prime_u64` after small-divisor prefilter. |
| `{words={41,1}, len=2}` | `8` | `91` | `SUCCESS`, `0` or `1` per independent oracle | Multiword prefilter and Miller–Rabin path; choose a known composite/prime pair and assert the oracle result. |

All invalid-call rows must assert byte-for-byte preservation of the output sentinel and input record. All success rows must assert that the input record remains unchanged.

## Recommended implementation order

First add the public validation and small-value table cases. Next add direct deterministic one-word helper vectors. Finally add multiword odd/even and witness vectors. Re-run `scripts/run_c11_coverage.sh` after each group and require the report to identify `src/bignum_is_prime.c` exactly once.
