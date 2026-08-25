# Coverage review: remaining cryptographic core modules

## Scope

The repository contains one remaining cryptographic core dependency besides the `bignum-is-prime` module: `libs/bignum-core`. Its implementation is `libs/bignum-core/src/bignum_core.asm`; there is no C11 implementation file in that module.

## Module-level results

| Module | Implementation | Tests discovered | Test result | gcov line coverage |
|---|---|---:|---|---|
| `bignum-core` | YASM assembly, currently an empty object / ABI placeholder | 2 | 2/2 PASS | Not applicable |

The two tests are `test_bignum_core_runner` and `test_bignum_t`. Both verify the `bignum_t` layout and size invariant. The core's assembly source contains no exported arithmetic or cryptographic routines and therefore has no executable function paths that can be covered by these tests.

## Sanitizer results

AddressSanitizer and UndefinedBehaviorSanitizer were both run through the module's existing `test_sanitize` target. Each mode completed with 2/2 tests passed and zero reported sanitizer issues. Since the implementation is YASM and the tests exercise only structure layout, these runs validate the C test harness and ABI-level object integration; they do not provide instruction-level sanitizer coverage for assembly.

## Assessment

The module has **100% structural/API smoke-test coverage** for the currently exposed `bignum_t` contract: capacity, word type, length field, and `sizeof` invariant are exercised. It has **0 executable-function coverage**, because `bignum_core.asm` intentionally defines no executable functions. gcov artifacts are not produced, which is expected for a YASM-only module and not a test failure.

There are no cryptographic arithmetic functions in `bignum-core` to fuzz or cover at this revision. A meaningful coverage target can be established only after arithmetic APIs are added. At that point the module should add deterministic tests, randomized reference-oracle tests, boundary tests for zero/max capacity/overflow, and an assembly-aware trace or differential harness; gcov should still not be used as the coverage metric for YASM instruction paths.

## Commands and results

```text
make test CONFIG=debug
Summary: 0 / 2 failed

make test_sanitize SAN=address CONFIG=debug
Summary: tests=2, failed=0, sanitizer_issues=0

make test_sanitize SAN=undefined CONFIG=debug
Summary: tests=2, failed=0, sanitizer_issues=0
```
