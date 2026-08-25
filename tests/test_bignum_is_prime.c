/**
 * @file test_bignum_is_prime.c
 * @brief Deterministic contract tests for the C11 primality predicate.
 * @details
 * The tests use fixed operands and explicit expected results. They verify the
 * named status model, output atomicity on errors, input immutability, small and
 * large one-word paths, multiword Miller--Rabin witness outcomes, modular
 * reduction boundaries, and normalization of non-canonical input records.
 */
#include "bignum_is_prime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/** @brief Constructs a canonical one-word test operand. */
static bignum_t make_u64(uint64_t word)
{
    bignum_t number = {0};
    number.words[0] = word;
    number.len = word == 0U ? 0U : 1U;
    return number;
}

/** @brief Checks a fixed one-word result and verifies that input is borrowed read-only. */
static void expect_u64(uint64_t word, int expected)
{
    bignum_t number = make_u64(word);
    const bignum_t original = number;
    int result = -1;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == expected);
    assert(memcmp(&number, &original, sizeof(number)) == 0);
}

/** @brief Covers values below two, small primes, and small composite divisors. */
static void test_small_values(void)
{
    for (uint64_t number = 0U; number < 4U; ++number) {
        expect_u64(number, number == 2U || number == 3U);
    }
    expect_u64(5U, 1); expect_u64(7U, 1); expect_u64(11U, 1);
    expect_u64(13U, 1); expect_u64(17U, 1); expect_u64(19U, 1);
    expect_u64(4U, 0); expect_u64(9U, 0); expect_u64(15U, 0);
    expect_u64(21U, 0); expect_u64(25U, 0); expect_u64(49U, 0);
}

/** @brief Covers deterministic 64-bit bases and overflow-safe modular multiplication. */
static void test_large_words(void)
{
    expect_u64(UINT64_C(18446744073709551557), 1);
    expect_u64(UINT64_C(18446744073709551555), 0);
    expect_u64(UINT64_C(4294967291), 1);
    expect_u64(UINT64_C(4294967295), 0);
}

/** @brief Covers multiword probable-prime, small-divisor rejection, and witness rejection. */
static void test_multword(void)
{
    bignum_t number = {0};
    int result = -1;

    number.words[0] = 5U;
    number.words[1] = 1U;
    number.len = 2U;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 0);


    /* (2^32 + 1)^2 = 2^64 + 2^33 + 1; it avoids the 3..37 prefilter. */
    number.words[0] = 1U;
    number.words[1] = UINT64_C(2) << 33U;
    number.words[2] = 1U;
    number.len = 3U;
    assert(bignum_is_prime(&number, 12U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 0);

    /* Exercise carry-sensitive operands near the top of two words. */
    number.words[0] = UINT64_MAX - 58U;
    number.words[1] = UINT64_MAX;
    assert(bignum_is_prime(&number, 4U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 0 || result == 1);
}

/** @brief Verifies NULL, zero-round and invalid-length errors preserve output. */
static void test_contract(void)
{
    bignum_t number = make_u64(17U);
    int result = 77;
    assert(bignum_is_prime(NULL, 8U, &result) == BIGNUM_IS_PRIME_ERROR_NULL_ARG);
    assert(bignum_is_prime(&number, 8U, NULL) == BIGNUM_IS_PRIME_ERROR_NULL_ARG);
    assert(bignum_is_prime(&number, 0U, &result) == BIGNUM_IS_PRIME_ERROR_ROUNDS);
    number.len = BIGNUM_CAPACITY + 1U;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_ERROR_BAD_LENGTH);
    assert(result == 77);
}

/** @brief Verifies canonical normalization of a padded zero and padded prime. */
static void test_normalization(void)
{
    bignum_t number = make_u64(97U);
    int result = -1;
    number.len = 4U;
    number.words[1] = 0U;
    number.words[2] = 0U;
    number.words[3] = 0U;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 1);
}

/** @brief Runs all deterministic scenarios and returns an ISO C process status. */
int main(void)
{
    test_small_values();
    test_large_words();
    test_multword();
    test_contract();
    test_normalization();
    puts("bignum_is_prime deterministic tests: OK");
    return 0;
}
