/**
 * @file bignum_is_prime_benchmark_adapter.c
 * @brief Deterministic benchmark-framework adapter for Miller–Rabin testing.
 */
#include "bignum_is_prime_benchmark_adapter.h"
#include "bignum_is_prime.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define FNV_OFFSET UINT64_C(1469598103934665603)
#define FNV_PRIME UINT64_C(1099511628211)

/** @brief Compares a workload field with one allowed token. */
static int equals(const char *left, const char *right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

/** @brief Tests membership in a NULL-terminated token list. */
static int allowed(const char *value, const char *const *tokens)
{
    if (value == NULL || tokens == NULL) return 0;
    for (size_t index = 0U; tokens[index] != NULL; ++index) {
        if (equals(value, tokens[index])) return 1;
    }
    return 0;
}

/** @brief Advances the deterministic adapter PRNG. */
static uint64_t next_value(uint64_t *state)
{
    if (*state == 0U) *state = UINT64_C(0x9E3779B97F4A7C15);
    *state ^= *state << 7U;
    *state ^= *state >> 9U;
    *state ^= *state << 8U;
    return *state;
}

/** @brief Chooses an operand length from the generic size profile. */
static size_t choose_length(const benchmark_workload_t *workload, uint64_t *state)
{
    if (equals(workload->size_profile, "one")) return 1U;
    if (equals(workload->size_profile, "quarter")) return BIGNUM_CAPACITY / 4U;
    if (equals(workload->size_profile, "half")) return BIGNUM_CAPACITY / 2U;
    if (equals(workload->size_profile, "near-capacity") ||
        equals(workload->capacity_profile, "near-capacity")) return BIGNUM_CAPACITY - 1U;
    return 1U + (size_t)(next_value(state) % (BIGNUM_CAPACITY / 2U));
}

/** @brief Reports whether a mixed input row is zero. */
static int row_is_zero(const char *input_kind, uint64_t index)
{
    return equals(input_kind, "zero") || (equals(input_kind, "mixed") && (index % 2U) == 0U);
}

/** @brief Initializes one deterministic bignum operand. */
static benchmark_adapter_status_t initialize(
    void *state, uint64_t sequence_index, const benchmark_workload_t *workload,
    void *adapter_context)
{
    bignum_t *number = state;
    uint64_t random_state;
    size_t length;
    (void)adapter_context;
    if (number == NULL || workload == NULL ||
        bignum_is_prime_benchmark_validate_workload(workload) !=
            BIGNUM_IS_PRIME_BENCHMARK_STATUS_SUCCESS) {
        return BENCHMARK_ADAPTER_STATUS_INPUT_ERROR;
    }
    memset(number, 0, sizeof(*number));
    if (row_is_zero(workload->input_kind, sequence_index)) return BENCHMARK_ADAPTER_STATUS_SUCCESS;
    random_state = workload->seed ^ (sequence_index + UINT64_C(0x9E3779B97F4A7C15));
    length = choose_length(workload, &random_state);
    if (length == 0U || length > BIGNUM_CAPACITY) return BENCHMARK_ADAPTER_STATUS_INPUT_ERROR;
    number->len = length;
    for (size_t index = 0U; index < length; ++index) number->words[index] = next_value(&random_state);
    number->words[length - 1U] |= UINT64_C(1) << 63U;
    number->words[0] |= 1U;
    return BENCHMARK_ADAPTER_STATUS_SUCCESS;
}

/** @brief Maps operation_kind to a deterministic Miller–Rabin round count. */
static size_t rounds_for(const char *operation, uint64_t iteration)
{
    if (equals(operation, "mr-quick")) return 1U;
    if (equals(operation, "mr-standard")) return 8U;
    if (equals(operation, "mr-strong")) return 16U;
    return 4U + (size_t)(iteration % 9U);
}

/** @brief Executes one non-mutating primality test. */
static benchmark_adapter_status_t operation(
    void *state, uint64_t iteration, const benchmark_workload_t *workload,
    void *adapter_context)
{
    bignum_t *number = state;
    int is_prime;
    bignum_is_prime_status_t status;
    (void)adapter_context;
    if (number == NULL || workload == NULL) return BENCHMARK_ADAPTER_STATUS_INPUT_ERROR;
    status = bignum_is_prime(number, rounds_for(workload->operation_kind, iteration), &is_prime);
    return status == BIGNUM_IS_PRIME_SUCCESS
        ? BENCHMARK_ADAPTER_STATUS_SUCCESS
        : BENCHMARK_ADAPTER_STATUS_OPERATION_ERROR;
}

/** @brief Produces an observable checksum for the immutable operand state. */
static uint64_t checksum(const void *state, uint64_t iteration, void *adapter_context)
{
    const bignum_t *number = state;
    uint64_t value = FNV_OFFSET;
    (void)adapter_context;
    if (number == NULL) return 0U;
    for (size_t index = 0U; index < BIGNUM_CAPACITY; ++index) {
        value ^= number->words[index];
        value *= FNV_PRIME;
    }
    value ^= number->len;
    value *= FNV_PRIME;
    value ^= iteration;
    return value * FNV_PRIME;
}

bignum_is_prime_benchmark_status_t bignum_is_prime_benchmark_validate_workload(
    const benchmark_workload_t *workload)
{
    static const char *const inputs[] = { "zero", "nonzero", "mixed", NULL };
    static const char *const operations[] = { "mr-quick", "mr-standard", "mr-strong", "mr-mixed", NULL };
    static const char *const measures[] = { "end-to-end", "kernel-only", NULL };
    static const char *const sizes[] = { "one", "quarter", "half", "variable", "near-capacity", NULL };
    static const char *const capacities[] = { "normal", "near-capacity", NULL };
    if (workload == NULL) return BIGNUM_IS_PRIME_BENCHMARK_STATUS_NULL_ARGUMENT;
    if (!allowed(workload->input_kind, inputs) || !allowed(workload->operation_kind, operations) ||
        !allowed(workload->measure_mode, measures) || !allowed(workload->size_profile, sizes) ||
        !allowed(workload->capacity_profile, capacities)) {
        return BIGNUM_IS_PRIME_BENCHMARK_STATUS_INVALID_PROFILE;
    }
    return BIGNUM_IS_PRIME_BENCHMARK_STATUS_SUCCESS;
}

bignum_is_prime_benchmark_status_t bignum_is_prime_benchmark_adapter_init(
    benchmark_adapter_t *adapter)
{
    if (adapter == NULL) return BIGNUM_IS_PRIME_BENCHMARK_STATUS_NULL_ARGUMENT;
    *adapter = (benchmark_adapter_t){
        .benchmark_name = "bignum_is_prime",
        .state_size = sizeof(bignum_t),
        .success_code = BENCHMARK_ADAPTER_STATUS_SUCCESS,
        .adapter_context = NULL,
        .initialize = initialize,
        .operation = operation,
        .checksum = checksum
    };
    return BIGNUM_IS_PRIME_BENCHMARK_STATUS_SUCCESS;
}
