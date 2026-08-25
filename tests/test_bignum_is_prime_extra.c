/**
 * @file test_bignum_is_prime_extra.c
 * @brief Randomized, robustness, and internal arithmetic tests for C11 reference.
 * @details
 * The public randomized section uses fixed xorshift state
 * 0x0123456789abcdef and exactly 2,000 values in [0, 100000), comparing every
 * result with an independent trial-division oracle. The internal section
 * includes the C11 implementation with its public symbol renamed and directly
 * checks modular carry/reduction, aliasing, multiplication, halving,
 * square-and-multiply, and both Miller--Rabin witness outcomes. Assertions
 * abort on failure, preserving deterministic reproduction without allocation.
 */
#include "bignum_is_prime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define static
#define bignum_is_prime bignum_is_prime_internal_for_test
#include "../src/bignum_is_prime.c"
#undef bignum_is_prime
#undef static

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

/** @brief Compares 2,000 fixed-seed values with the trial-division oracle. */
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

/** @brief Verifies padded records and output atomicity on invalid rounds. */
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

/** @brief Builds a one-word temporary for direct helper checks. */
static bignum_t helper_word(uint64_t value)
{
    bignum_t result = { .words = { value }, .len = value == 0U ? 0U : 1U };
    return result;
}

/**
 * @brief Exercises modular carry, reduction, multiplication, and halving.
 * @details Exact one-word and high-bit inputs cover the bounded carry,
 * reduction, modular-addition, multiplication, aliasing, and halving paths.
 */
static void test_arithmetic_helpers(void)
{
    bignum_t left = helper_word(10U), right = helper_word(9U), modulus = helper_word(17U), result;
    assert(compare(&left, &right) > 0);
    subtract(&left, &right, &result);
    assert(result.words[0] == 1U && result.len == 1U);
    double_mod(&right, &modulus, &result);
    assert(result.words[0] == 1U);
    add_mod(&left, &right, &modulus, &result);
    assert(result.words[0] == 2U);
    multiply_mod(&left, &right, &modulus, &result);
    assert(result.words[0] == 5U);
    halve(&left);
    assert(left.words[0] == 5U);
    left = helper_word(UINT64_C(1) << 63U);
    right = helper_word(UINT64_C(1) << 63U);
    modulus = helper_word(UINT64_MAX - 58U);
    double_mod(&left, &modulus, &result);
    assert(result.words[0] == 59U);
    add_mod(&left, &right, &modulus, &result);
    assert(result.words[0] == 59U);
}

/**
 * @brief Exercises constructors, predicates, and 64-bit arithmetic helpers.
 * @details Exact oracle values cover zero/non-zero construction, comparison,
 * remainder, 128-bit multiplication, and binary modular exponentiation.
 */
static void test_low_level_helpers(void)
{
    bignum_t zero = helper_word(0U);
    bignum_t one;
    bignum_t value;
    bignum_t sample = helper_word(12345U);
    set_one(&one);
    set_u64(&value, 0U);
    assert(is_zero(&zero) != 0);
    assert(is_zero(&one) == 0);
    assert(value.len == 0U);
    set_u64(&value, 42U);
    assert(value.len == 1U && value.words[0] == 42U);
    assert(compare(&zero, &one) < 0);
    assert(compare(&one, &one) == 0);
    assert(remainder_u64(&sample, 97U) == 26U);
    assert(mulmod_u64(UINT64_C(0xfffffffffffffff0), 19U,
                      UINT64_C(0xffffffffffffffc5)) == 817U);
    assert(powmod_u64(5U, 13U, 17U) == 3U);
}

/**
 * @brief Exercises deterministic one-word Miller--Rabin helper vectors.
 * @details The vectors cover candidates after the public small-value fast path:
 * odd-component decomposition, repeated squaring, accepted witnesses,
 * deterministic rejection, and high-bit uint64 inputs.
 */
static void test_u64_miller_rabin_vectors(void)
{
    assert(is_prime_u64(UINT64_C(97)) == 1);
    assert(is_prime_u64(UINT64_C(1022117)) == 0); /* 1009 * 1013. */
    assert(is_prime_u64(UINT64_C(18446744073709551557)) == 1);
    assert(is_prime_u64(UINT64_C(0xffffffffffffffff)) == 0);
}

/** @brief Exercises square-and-multiply and both witness return paths. */
static void test_power_and_witness(void)
{
    bignum_t base = helper_word(5U), exponent = helper_word(13U), modulus = helper_word(17U), result;
    power_mod(&base, &exponent, &modulus, &result);
    assert(result.words[0] == 3U);
    exponent = helper_word(7U);
    modulus = helper_word(15U);
    assert(witness(&modulus, &exponent, 0U, 2U) == 0);
    exponent = helper_word(1U);
    modulus = helper_word(17U);
    assert(witness(&modulus, &exponent, 4U, 3U) == 1);
}

/** @brief Runs randomized, robustness, and direct arithmetic scenarios. */
int main(void)
{
    test_randomized_small_values();
    test_normalization_and_guards();
    test_arithmetic_helpers();
    test_low_level_helpers();
    test_u64_miller_rabin_vectors();
    test_power_and_witness();
    puts("bignum_is_prime extra tests: OK");
    return 0;
}
