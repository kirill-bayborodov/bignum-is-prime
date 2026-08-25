/**
 * @file bignum_is_prime_benchmark_adapter.h
 * @brief Project-owned benchmark-framework adapter for bignum_is_prime.
 * @details
 * The adapter translates validated benchmark_workload_t transport fields into
 * deterministic bignum_t datasets and Miller--Rabin round counts. Framework
 * owns the adapter object during a run; callers retain ownership of workload
 * strings and state buffers. No adapter function allocates or mutates global
 * state, so independent ST/MT runs are reentrant.
 */
#ifndef BIGNUM_IS_PRIME_BENCHMARK_ADAPTER_H
#define BIGNUM_IS_PRIME_BENCHMARK_ADAPTER_H

#include <benchmark_framework.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reports adapter validation and lifecycle outcomes.
 * @details Success guarantees valid callbacks or validated workload. Errors
 * leave caller-owned output state unchanged and are retryable after the input
 * contract is corrected.
 */
typedef enum bignum_is_prime_benchmark_status {
    BIGNUM_IS_PRIME_BENCHMARK_STATUS_SUCCESS = 0, /**< Adapter completed; all documented outputs are valid. */
    BIGNUM_IS_PRIME_BENCHMARK_STATUS_NULL_ARGUMENT = 1, /**< Required pointer is NULL; outputs are unchanged; caller may retry. */
    BIGNUM_IS_PRIME_BENCHMARK_STATUS_INVALID_PROFILE = 2, /**< Profile vocabulary/range is unsupported; no benchmark state is valid. */
    BIGNUM_IS_PRIME_BENCHMARK_STATUS_OPERATION_ERROR = 3 /**< Predicate lifecycle failed; the sample must be discarded and not retried unchanged. */
} bignum_is_prime_benchmark_status_t;

/**
 * @brief Initializes the benchmark-framework binding for bignum_is_prime.
 * @details Installs initialize, operation and checksum callbacks and the fixed
 * bignum_t state size; no memory allocation or ownership transfer occurs.
 * @param[out] adapter Caller-allocated writable framework object.
 * @return BIGNUM_IS_PRIME_BENCHMARK_STATUS_SUCCESS or NULL_ARGUMENT.
 * @pre adapter is non-NULL and remains alive for the benchmark run.
 * @post Success exposes a complete callback table; failure leaves adapter unchanged.
 * @thread_safety Safe when concurrent runs use distinct adapter objects.
 * @complexity O(1) time and O(1) space.
 */
bignum_is_prime_benchmark_status_t bignum_is_prime_benchmark_adapter_init(
    benchmark_adapter_t *adapter);

/**
 * @brief Validates all bignum-specific workload axes.
 * @details Validation is read-only and rejects unsupported operation, input,
 * measure, size, and capacity vocabulary before state initialization.
 * @param[in] workload Borrowed complete framework descriptor; string storage
 *                     remains owned by the caller for the duration of the call.
 * @return SUCCESS, NULL_ARGUMENT, or INVALID_PROFILE as named above.
 * @pre workload is non-NULL and points to initialized transport fields.
 * @post No input or output storage is modified.
 * @thread_safety Safe for concurrent calls on independent workload records.
 * @complexity O(1) time because all accepted token sets are fixed.
 */
bignum_is_prime_benchmark_status_t bignum_is_prime_benchmark_validate_workload(
    const benchmark_workload_t *workload);

#ifdef __cplusplus
}
#endif

#endif /* BIGNUM_IS_PRIME_BENCHMARK_ADAPTER_H */
