/**
 * @file bignum_is_prime_benchmark_adapter.h
 * @brief Benchmark-framework adapter for the bignum_is_prime domain.
 */
#ifndef BIGNUM_IS_PRIME_BENCHMARK_ADAPTER_H
#define BIGNUM_IS_PRIME_BENCHMARK_ADAPTER_H

#include <benchmark_framework.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BIGNUM_IS_PRIME_BENCHMARK_STATUS_SUCCESS = 0,
    BIGNUM_IS_PRIME_BENCHMARK_STATUS_NULL_ARGUMENT = 1,
    BIGNUM_IS_PRIME_BENCHMARK_STATUS_INVALID_PROFILE = 2,
    BIGNUM_IS_PRIME_BENCHMARK_STATUS_OPERATION_ERROR = 3
} bignum_is_prime_benchmark_status_t;

/**
 * @brief Initializes the benchmark-framework binding for bignum_is_prime.
 * @param[out] adapter Receives the complete callback binding.
 * @return Named adapter status.
 */
bignum_is_prime_benchmark_status_t bignum_is_prime_benchmark_adapter_init(
    benchmark_adapter_t *adapter);

/**
 * @brief Validates all bignum-specific workload axes.
 * @param[in] workload Generic framework workload descriptor.
 * @return Named adapter status.
 */
bignum_is_prime_benchmark_status_t bignum_is_prime_benchmark_validate_workload(
    const benchmark_workload_t *workload);

#ifdef __cplusplus
}
#endif

#endif /* BIGNUM_IS_PRIME_BENCHMARK_ADAPTER_H */
