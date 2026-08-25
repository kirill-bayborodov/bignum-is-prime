# Quality review: test coverage and documentation

**Repository:** `bignum-is-prime`  
**Revision reviewed:** `48987d3`  
**Normative source:** `QUALITY_GATES_DOCUMENTATION_C11_JSON.md` supplied with the review request.

## 1. Executive result

The functional C11 test suite passes, and the reference implementation has measurable gcov evidence. The documentation review is **not fully passing** under the supplied QG standard. The main blocking gaps are missing file-level Doxygen in four test sources, incomplete contracts for public status enumerators and internal helpers, incomplete JSON companion sections, and insufficiently precise C/ASM boundary documentation.

The current ASM tests also pass, but this report evaluates documentation and coverage only; it does not change the previously identified semantic limitation that the ASM multiword path is not a complete Miller–Rabin implementation.

## 2. Test coverage evidence

| Evidence | Result | Assessment |
|---|---:|---|
| `make test CONFIG=debug USE_ASM=no` | `0 / 5 failed` | Pass |
| Deterministic C11 tests | Pass | Covers null, zero, small values, large one-word values, multiword values, invalid rounds/length and input immutability |
| Randomized C11 tests | 2,000 generated values | Pass; oracle is trial division for values below 100,000 |
| Multithread C11 test | 8 threads × 100 calls | Pass; independent read-only records |
| Adapter regression test | Pass | Valid and invalid profile vocabulary plus deterministic callback lifecycle |
| C11 line coverage | 61.36% of 176 lines | Evidence available; no threshold is specified by the supplied QG |
| C11 branch coverage | 62.69% of 134 branches executed; 55.97% taken at least once | Significant untested branch space remains |
| C11 call coverage | 51.43% of 35 calls | Internal helper paths remain partially unexercised |
| JSON matrix | 24/24 protocol-complete samples | Pass for benchmark protocol and current smoke workload |

The coverage command was executed with strict C11 flags, gcov instrumentation, and `make test`. Coverage is therefore evidence for the C11 reference only; it does not measure YASM instructions. The present result is useful baseline evidence but should not be described as complete coverage. Additional cases are needed for large multiword modular arithmetic, carry and reduction branches, every Miller–Rabin witness outcome, all error paths, and large-operand randomized differential testing.

## 3. Artifact-level documentation checklist

The acceptance labels below follow the artifact-level checklist required by DOC-1 through DOC-12 in the supplied QG document.

| Artifact | DOC-1 file block | DOC-2 declarations | DOC-3 complex fields | DOC-4 statuses | DOC-5 function contracts | DOC-6 rationale | Result |
|---|---|---|---|---|---|---|---|
| `include/bignum_is_prime.h` | Pass | Partial | N/A | **Fail**: enum values have no cause/output comments | **Fail**: missing explicit pre/post/complexity and complete ownership/NULL contract | Partial | Blocking gaps |
| `src/bignum_is_prime.c` | Pass | Partial | N/A | N/A | **Fail**: static helper blocks do not consistently document parameters/output semantics and algorithm complexity | Partial | Blocking gaps |
| `src/bignum_is_prime.asm` | Pass | Pass for symbol presence | N/A | Partial | **Fail**: ABI section does not fully specify stack alignment, preserved registers and all clobbers/error paths | Partial | Blocking gaps |
| `benchmarks/adapter/bignum_is_prime_benchmark_adapter.h` | Pass | Partial | N/A | **Fail**: adapter status enumerators lack complete cause/output/retry comments | **Fail**: public functions omit full pre/post/ownership/complexity contract | Partial | Blocking gaps |
| `benchmarks/adapter/bignum_is_prime_benchmark_adapter.c` | Pass | Partial | N/A | N/A | Partial | Partial | Needs strengthening |
| `benchmarks/bench_bignum_is_prime.c` | Pass | Partial | N/A | N/A | Partial: CLI, exit mapping and protocol need complete current contract | Partial | Needs strengthening |
| `benchmarks/bench_bignum_is_prime_mt.c` | Pass | Partial | N/A | N/A | Partial: ST/MT timing and worker contract need explicit documentation | Partial | Needs strengthening |
| `tests/test_bignum_is_prime.c` | **Fail**: no file Doxygen | **Fail**: test functions have no structured intent comments | N/A | N/A | **Fail**: oracle and expected invariants are not documented locally | Partial | Blocking |
| `tests/test_bignum_is_prime_extra.c` | **Fail**: no file Doxygen | **Fail**: test cases lack structured comments | N/A | N/A | **Fail**: seed, domain, 2,000 cases and trial-division oracle are undocumented in the file | Partial | Blocking |
| `tests/test_bignum_is_prime_mt.c` | **Fail**: no file Doxygen | **Fail**: worker/test intent is undocumented | N/A | N/A | **Fail**: concurrency scope and expected status are not documented locally | Partial | Blocking |
| `tests/test_bignum_is_prime_runner.c` | **Fail**: no file Doxygen | Partial | N/A | N/A | **Fail**: integration preconditions and named status contract are not documented | Partial | Blocking |
| `tests/benchmark_adapter/test_bignum_is_prime_benchmark_adapter.c` | Pass | Partial | N/A | Partial | Partial | Partial | Needs strengthening |

## 4. JSON companion review

Both committed manifests have adjacent companion documents and both JSON files parse successfully with `jq`.

| Manifest | Companion | Syntax | Required companion content | Result |
|---|---|---|---|---|
| `benchmarks/profiles/bignum_is_prime_full.json` | Present | Pass | Missing explicit schema/root/nested field types and requiredness/defaults; no complete JSON fragment; no systematic how-to-modify sequence; failure semantics are incomplete | **Fail DOC-9** |
| `benchmarks/profiles/bignum_is_prime_standard.json` | Present | Pass | Same gaps; profile table and exact profile-by-profile scenario mapping are incomplete | **Fail DOC-9** |

The profile guides correctly explain the purpose, framework consumer, operation vocabulary, matrix cardinality and basic commands. They do not yet satisfy the supplied minimum structure for `*.json.md`: schema, location/lifecycle, complete example, modification procedure, unsupported schema behavior, malformed JSON behavior and invalid-vocabulary behavior must be stated explicitly.

## 5. README and examples

`README.md` describes the purpose, toolchain, dependencies, submodule recovery, build/test commands, distribution and benchmark workflow. However, the supplied QG requires a **minimal executable API usage example** with complete include/link flags, caller ownership, named status checking and cleanup. The current README contains only a declaration snippet and no complete compiling application example.

The coverage paragraph also says to inspect `src/bignum_is_prime.c.gcov`, while the current gcov run produced `bignum_is_prime.c.gcov` in the repository working directory. The documented path must be corrected or the command must be changed to place the report at the documented path.

## 6. C/ASM boundary

The assembly file states the System V AMD64 intent and argument registers, but the QG requires the complete boundary contract: representation of `bignum_t`, stack alignment, caller- versus callee-saved registers, all clobbers, return status mapping, output-pointer preservation, and error-path behavior. The current comments do not provide that complete reviewable contract. This is a blocking DOC-5/DOC-6/DOC-11 issue for the assembly artifact.

## 7. Recommended remediation order

First, add file-level Doxygen and structured test-case comments to the four test sources, including the fixed seed/domain/oracle and concurrency invariants. Second, complete the public header status and function contracts, then document every static helper in `src/bignum_is_prime.c` with parameters, output semantics, invariants and complexity. Third, add the missing schema/example/failure/how-to-modify sections to both JSON companion documents. Fourth, add a complete README application example and correct the gcov path. Finally, expand the ASM boundary block and rerun Doxygen; the review environment currently has `gcov` and `cppcheck`, but no `doxygen` executable, so DOC-11 cannot yet be formally evidenced in this environment.

## 8. Conclusion

**Testing:** functionally passing with reproducible C11 coverage evidence, but coverage is partial at 61.36% lines and 55.97% branches taken at least once.  
**Documentation:** partially prepared, but **not accepted by the supplied documentation QG** until the blocking artifact-level gaps above are corrected.
