/**
 * @file test_bignum_is_prime_extra.c
 * @brief Randomized and robustness tests for the C11 primality reference.
 * @details
 * A fixed xorshift state starts at 0x0123456789abcdef and generates exactly
 * 2,000 values in [0, 100000). Each result is compared with the independent
 * trial-division oracle. A mismatch aborts through assert, preserving the
 * failing deterministic sequence for reproduction. Additional cases verify
 * padded input normalization and output atomicity on invalid arguments.
 */
#include "bignum_is_prime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/** @brief Advances the fixed deterministic pseudo-random generator. */
static uint64_t next_value(uint64_t *state)
{
    *state ^= *state << 13U;
    *state ^= *state >> 7U;
    *state ^= *state << 17U;
    return *state;
}

/** @brief Computes the trusted trial-division oracle for values below 100000. */
static int reference_u64(uint64_t number)
{
    if (number < 2U) return 0;
    if ((number & 1U) == 0U) return number == 2U;
    for (uint64_t divisor = 3U; divisor * divisor <= number; divisor += 2U) {
        if (number % divisor == 0U) return 0;
    }
    return 1;
}

/** @brief Compares 2,000 fixed-seed generated values with the trial-division oracle. */
static void test_randomized_small_values(void)
{
    uint64_t state = UINT64_C(0x0123456789abcdef);
    for (size_t index = 0U; index < 2000U; ++index) {
        const uint64_t value = next_value(&state) % 100000U;
        bignum_t number = { .words = { value }, .len = value == 0U ? 0U : 1U };
        int result = -1;
        assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
        assert(result == reference_u64(value));
    }
}

/** @brief Verifies padded records are accepted without modifying caller storage. */
static void test_normalization_and_guards(void)
{
    bignum_t number = { .words = { 101U }, .len = 4U };
    const bignum_t original = number;
    int result = -1;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 1);
    assert(memcmp(&number, &original, sizeof(number)) == 0);
    result = 91;
    assert(bignum_is_prime(&number, 0U, &result) == BIGNUM_IS_PRIME_ERROR_ROUNDS);
    assert(result == 91);
}

/** @brief Runs deterministic randomized and robustness scenarios. */
int main(void)
{
    test_randomized_small_values();
    test_normalization_and_guards();
    puts("bignum_is_prime extra tests: OK");
    return 0;
}
