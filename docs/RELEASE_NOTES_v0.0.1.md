# bignum-is-prime v0.0.1

## Summary

This release improves the review readiness of `bignum-is-prime`, a C11/x86-64 YASM Miller–Rabin probable-primality module. It updates the documentation to follow the `bignum-bit-test` template, adds the normative documentation Quality Gates artifact, and strengthens C11 helper coverage in the existing extra test suite.

## Validation

| Check | Result |
|---|---:|
| C11 release tests | 0/5 failed |
| ASM release tests | 0/5 failed |
| AddressSanitizer C11 suite | 5/5 pass, 0 issues |
| UndefinedBehaviorSanitizer C11 suite | 5/5 pass, 0 issues |
| C11 line coverage | 82.72% |
| C11 branch coverage | 56.72% |
| C11 call coverage | 71.43% |
| Doxygen with warnings-as-errors | Pass, zero warnings |
| JSON profile parsing and companion presence | Pass |
| `git diff --check` | Pass |

The C11 coverage figures measure the C11 reference implementation only. YASM instruction coverage is validated through public contract tests, direct randomized kernel tests, ABI checks, and boundary tests rather than gcov.

## Documentation

`README.md` now follows the template structure with Features, Dependencies, API Contract, Build and Test, Coverage, Benchmarks, JSON profile guidance, Installation and Distribution, Review Gates, Contributing, and License sections. The README contains a complete minimal caller example, current project-specific names, submodule recovery instructions, and current benchmark commands.

The release includes:

- `docs/QUALITY_GATES_DOCUMENTATION_C11_JSON.md`
- `docs/QUALITY_REVIEW_COVERAGE_AND_DOCUMENTATION.md`
- `docs/CRYPTO_CORE_COVERAGE.md`
- `docs/benchmark_montgomery_redc_fix.md`
- `docs/failing_mont_trace_n2.md`

## Compatibility

The public API, status values, fixed-capacity representation, System V AMD64 ABI, benchmark protocol, Makefile, and CI workflows are unchanged. No Makefile or CI changes are included.

## Distribution

The release distribution is produced with:

```bash
make dist CONFIG=release
```

It contains the public header, static library, README, license, and integration runner.
